#include "server.h"
#include "parser.h"
#include <cassert>


using namespace std;
using namespace seal;

TreeEvaluationServer::TreeEvaluationServer()
{
}

void TreeEvaluationServer::initialize_model(
    string model_filename,
    QueryParameters* query_parameters
){
    Parser p;
    this->decision_forest = p.parseForest(model_filename, query_parameters);
    //this->tree_root = p.parseTree(model_filename);
    //query_parameters->set_tree_params(this->tree_root);
}

void TreeEvaluationServer::
    initialize_params_with_input(stringstream& parms_stream, stringstream& pk_stream)
{
    this->parms = new EncryptionParameters();
    this->parms->load(parms_stream);

    parms_stream.seekg(0, parms_stream.beg);
    context = new SEALContext(*(this->parms));

    this->evaluator = new Evaluator(*context);
    this->batch_encoder = new BatchEncoder(*context);

    PublicKey pk;
    pk.load(*context, pk_stream);

    this->enc = new Encryptor(*context, pk);

}

void TreeEvaluationServer::load_keys(stringstream& data_stream)
{
    this->rlk_server = new RelinKeys();
    this->gal_keys_server = new GaloisKeys();
    this->rlk_server->load(*context, data_stream);
    this->gal_keys_server->load(*context, data_stream);
}

void TreeEvaluationServer::set_parameters_used_for_debug(stringstream& sk_stream)
{
    SecretKey sk;
    sk.load(*context, sk_stream);
    this->noise_calculator = new Decryptor(*context, sk);
}

// Constant-weight Equality Operator
vector<Ciphertext> TreeEvaluationServer::constant_weight_arith(
    vector<vector<Plaintext>>& cts_x_bits_,
    vector<vector<Ciphertext>>& cts_y_bits_,
    QueryParameters* query_parameters)
{
    assert(cts_x_bits_.size() == cts_y_bits_.size() && "Size mismatch");
    int num_cts = cts_x_bits_.size();
    int code_length = query_parameters->code_length;
    int hamming_weight = query_parameters->k;

    vector<vector<Ciphertext>> operands(num_cts, vector<Ciphertext>(0));

    #pragma omp parallel for collapse(2)
    for (int ct_ind=0; ct_ind < num_cts; ct_ind++){
        for (int i = 0; i < code_length; i++) {
            if (!cts_x_bits_[ct_ind][i].is_zero()){
                Ciphertext temp;
                this->evaluator->multiply_plain(cts_y_bits_[ct_ind][i], cts_x_bits_[ct_ind][i], temp);
                #pragma omp critical
                {
                    operands[ct_ind].push_back(temp);
                }
            }
        }
    }

    vector<vector<Ciphertext>> cts_ops(num_cts, vector<Ciphertext>(hamming_weight));
    #pragma omp parallel for
    for (int ct_ind=0; ct_ind < num_cts; ct_ind++){
        this->evaluator->add_many(operands[ct_ind], cts_ops[ct_ind][0]);
    }

    #pragma omp parallel for collapse(2)
    for (int ct_ind=0; ct_ind < num_cts; ct_ind++){
        for (int i = 1; i < hamming_weight; i++) {
            Plaintext plain_matrix;
            vector<uint64_t> pod_matrix(batch_encoder->slot_count(), i);
            this->batch_encoder->encode(pod_matrix, plain_matrix);
            this->evaluator->sub_plain(cts_ops[ct_ind][0], plain_matrix, cts_ops[ct_ind][i]);
        }
    }

    vector<Ciphertext> result_cts(num_cts);

    if (hamming_weight > 1) {
        uint64_t ceil_log_k = ceil(log2(hamming_weight));

        for (int i = 0; i < ceil_log_k; i++) {
            #pragma omp parallel for collapse(2)
            for (int ct_ind=0; ct_ind < num_cts; ct_ind++){
                for (int j = 0; j < 1 << (ceil_log_k - 1 - i); j++) {

                    if (j + (1 << (ceil_log_k - 1 - i)) < cts_ops[ct_ind].size()) {
                        this->evaluator->multiply_inplace(cts_ops[ct_ind][j],
                            cts_ops[ct_ind][j + (1 << (ceil_log_k - 1 - i))]);
                        this->evaluator->relinearize_inplace(cts_ops[ct_ind][j],
                            *(rlk_server));
                    }
                    
                }
            }
        }

        // find multiplicative inverse of k! mod p
        uint64_t inv = prime_mod_inverse(iter_factorial(hamming_weight), *(this->parms->plain_modulus().data()));

        Plaintext pt;
        vector<uint64_t> pod_matrix(this->batch_encoder->slot_count(),
            inv);
        this->batch_encoder->encode(pod_matrix, pt);
        
        #pragma omp parallel for
        for (int ct_ind=0; ct_ind < num_cts; ct_ind++){
            this->evaluator->multiply_plain(cts_ops[ct_ind][0], pt, result_cts[ct_ind]);
        }
    } else {
        #pragma omp parallel for
        for (int ct_ind=0; ct_ind < num_cts; ct_ind++){
            result_cts[ct_ind] = cts_ops[ct_ind][0];
        }
    }

    return result_cts;
}

vector<Plaintext> TreeEvaluationServer::prepare_server_comparison_encoding(
    vector<uint64_t>& server_values,
    QueryParameters* query_parameters
){

    int n = query_parameters->n;
    int k = query_parameters->k;

    // inner is the array from previous code
    vector<vector<uint64_t>> paths(server_values.size(), vector<uint64_t>());

    for (int i=0; i < server_values.size(); i++) {
        uint64_t cur = server_values[i];
        for (int j = 0; j < n; j++) {
            paths[i].push_back(cur);
            cur /= 2;
        }
        paths[i].push_back(0);
    }

    uint64_t m = query_parameters->code_length;

    // middle is for array in old code
    std::vector<std::vector<std::vector<uint64_t>>> pathcodes;

    for (int j = 0; j < paths.size(); ++j) {
        vector<vector<uint64_t>> pathcode;
        pathcode.reserve(n + 1);
        for (int i = n; i >= 0; --i) {
            pathcode.insert(pathcode.begin(),
                get_OP_CHaW_encoding(paths[j][i], m, k, true));
        }
        pathcodes.push_back(pathcode);
    }

    std::vector<Plaintext> cts_range_bits_pathcode;

    Plaintext plain_matrix;
    Ciphertext ct;
    for (int i = 0; i < m; ++i) {
        vector<uint64_t> pod_matrix(query_parameters->slot_count, 0ULL);
        for (int k = 0; k < pathcodes.size(); k++) {
            for (int j = 0; j < n + 1; ++j) {
                if (pathcodes[k][j].size() <= i) {
                    continue;
                } else {
                    pod_matrix[k * query_parameters->num_slots_per_element + j] = pathcodes[k][j][i];
                    pod_matrix[k * query_parameters->num_slots_per_element + j + query_parameters->row_count] = pathcodes[k][j][i];
                }
            }
        }
        batch_encoder->encode(pod_matrix, plain_matrix);
        cts_range_bits_pathcode.push_back(plain_matrix);
    }
    return cts_range_bits_pathcode;
}


/*
    INPUT: the client input is mentioned in the client file
           the server input is each element that needs to be compared with the corresponding (encrypted) client element
        
    this function: encode each server value and constuct a plaintext, compare that with the client input.

    Output: If each client input take up (n + BUFFER) bits then the result of the comparison should be in bit (n+1)
*/


vector<Ciphertext> TreeEvaluationServer::batched_comparison(
    vector<Ciphertext>& client_input,
    vector<vector<uint64_t>>& server_comp_values,
    QueryParameters* q_params
){

    int num_cts = ceil((float)q_params->max_repetitions/q_params->reps_in_ct_per_attr);
    vector<Ciphertext> comparisonResult(num_cts);
    int n = q_params->n;

    vector<vector<Ciphertext>> client_input_vec(num_cts, client_input);

    int k = q_params->k;

    vector<vector<Plaintext>> cts_range_bits_pathcode_vec;

    for (int i=0;i<num_cts;i++)
        cts_range_bits_pathcode_vec.push_back(prepare_server_comparison_encoding(server_comp_values[i], q_params));

    vector<Ciphertext> batchedComparison = constant_weight_arith(cts_range_bits_pathcode_vec, client_input_vec,
        q_params);

    vector<vector<Ciphertext>> ops(num_cts, vector<Ciphertext>(n));

    #pragma omp parallel for collapse(2)
    for (int ct_ind=0;ct_ind<num_cts;ct_ind++){
        for (int i = 1; i <= n; ++i) {
            // Ciphertext rotations;
            evaluator->rotate_rows(batchedComparison[ct_ind], -i,
                *(this->gal_keys_server), ops[ct_ind][i-1]);
        }
    }

    #pragma omp parallel for
    for (int ct_ind=0;ct_ind<num_cts;ct_ind++){
        ops[ct_ind].push_back(batchedComparison[ct_ind]);
        evaluator->add_many(ops[ct_ind], comparisonResult[ct_ind]);
    }
    
    return comparisonResult;
}

vector<Ciphertext> TreeEvaluationServer::bfs_path_summing(vector<Ciphertext>& reference_ciphertext, QueryParameters* query_parameters)
{
    int n = query_parameters->n;

    vector<uint64_t> pod_matrix(query_parameters->slot_count, 0);

    Plaintext pt;

    batch_encoder->encode(pod_matrix, pt);

    Ciphertext zero_ciphertext;
    this->enc->encrypt(pt, zero_ciphertext);
    vector<Ciphertext> finished_ciphers;

    vector<DecisionTreeNode*> all_queue_elements_rotation_amount;

    std::queue<DecisionTreeNode*> traversal_queue;
    traversal_queue.push(this->tree_root);
    while (!traversal_queue.empty()) {
        DecisionTreeNode* current_element = traversal_queue.front();
        traversal_queue.pop();

        if (!current_element->is_leaf()) {
            all_queue_elements_rotation_amount.push_back(current_element);
            traversal_queue.push(current_element->left_child);
            traversal_queue.push(current_element->right_child);
        }
    }

    vector<Ciphertext> rotated_ciphs(2*all_queue_elements_rotation_amount.size());

    #pragma omp parallel
    {
        Plaintext pt;
        batch_encoder->encode(vector<uint64_t>(query_parameters->poly_mod_degree,1), pt);
        #pragma omp for
        for (int i = 0; i < all_queue_elements_rotation_amount.size(); ++i) {
            
            int ct_ind = all_queue_elements_rotation_amount[i]->second_index / query_parameters->reps_in_ct_per_attr;
            int rotation_amount = (all_queue_elements_rotation_amount[i]->second_index % query_parameters->reps_in_ct_per_attr
                                + all_queue_elements_rotation_amount[i]->first_index * query_parameters->reps_in_ct_per_attr)
                                * query_parameters->num_slots_per_element
                                + n;

            evaluator->rotate_rows(
                reference_ciphertext[ct_ind], rotation_amount,
                *gal_keys_server, rotated_ciphs[2*i]
            );
            evaluator->negate(rotated_ciphs[2*i], rotated_ciphs[2*i+1]);
            evaluator->add_plain_inplace(rotated_ciphs[2*i+1], pt);
        }
    }

    int current_index = 0;
    std::queue<BFSQueueElement> bfs_queue;
    bfs_queue.push(BFSQueueElement(this->tree_root, zero_ciphertext, 0));
    while (!bfs_queue.empty()) {
        BFSQueueElement current_element = bfs_queue.front();
        bfs_queue.pop();
        if (current_element.tree_node->is_leaf()) {
            // both children are NULL

            vector<uint64_t> pod_matrix(query_parameters->slot_count, current_element.path_length);

            Plaintext pt;
            batch_encoder->encode(pod_matrix, pt);
            this->evaluator->sub_plain_inplace(current_element.current_node_ciphertext, pt);
            finished_ciphers.push_back(current_element.current_node_ciphertext);
        } else {

            // accumulate in the n-th slot
            Ciphertext left_child_ciph;
            evaluator->add(rotated_ciphs[current_index], current_element.current_node_ciphertext, left_child_ciph);
            BFSQueueElement left_child_element(current_element.tree_node->left_child, left_child_ciph, current_element.path_length + 1);
            current_index += 1;

            Ciphertext right_child_ciph;
            evaluator->add(rotated_ciphs[current_index], current_element.current_node_ciphertext, right_child_ciph);
            BFSQueueElement right_child_element(current_element.tree_node->right_child, right_child_ciph, current_element.path_length + 1);
            current_index += 1;

            bfs_queue.push(left_child_element);
            bfs_queue.push(right_child_element);
        }
    }
    return finished_ciphers;
}

vector<Ciphertext> TreeEvaluationServer::prepare_client_result(
    std::vector<Ciphertext>& batched_result,
    QueryParameters* query_parameters
){

    Ciphertext ret_vec;
    Timer time_server_aggregation;
    
    int batch_p_num = *(this->parms->plain_modulus().data());

        std::vector<uint64_t> leaf_classification_values = get_leaf_classification_values(this->tree_root);

        int n_classes = query_parameters->n_classes;
        vector<vector<uint64_t>> one_hot_vecs(n_classes, vector<uint64_t>(query_parameters->poly_mod_degree,0));
        vector<int> one_hot_flag(n_classes, 0);
        #pragma omp parallel
        {
            Plaintext pt_mask; 
            int classification = 0;
            #pragma omp for
            for (int i=0;i<batched_result.size();i++){
                vector<int> to_keep({i,i+ query_parameters->row_count});
                //vector<int> to_keep_vals({(rand()%(batch_p_num-1))+1,(rand()%(batch_p_num-1))+1});
                vector<int> to_keep_vals({1,1});
                pt_mask=create_near_zero_mask(
                    to_keep,
                    to_keep_vals,
                    query_parameters
                );
                evaluator->rotate_rows_inplace(batched_result[i], -i, *gal_keys_server);
                evaluator->multiply_plain_inplace(batched_result[i], pt_mask);

                classification = leaf_classification_values[i];

                for (int j = 0; j < n_classes; j++) {
                    if (j == classification) {
                        one_hot_vecs[j][i] = 1;
                        #pragma omp atomic write
                        one_hot_flag[j] = 1;
                    }
                }

            }
        }

        time_server_aggregation.start();

        evaluator->add_many(batched_result, ret_vec);

        int depth = this->tree_root->depth;
        vector<Ciphertext> ops(depth);
        #pragma omp parallel
        {
            Plaintext plain_matrix;
            #pragma omp for
            for(int i = 1; i <= depth; i++) {
                vector<uint64_t> pod_matrix(batch_encoder->slot_count(), i);
                this->batch_encoder->encode(pod_matrix, plain_matrix);
                evaluator->add_plain(ret_vec, plain_matrix, ops[i-1]);
            }
        }

        Ciphertext result;
        evaluator->multiply_many(ops, *rlk_server, result);

        uint64_t inv = prime_mod_inverse(iter_factorial(depth), *(this->parms->plain_modulus().data()));
        Plaintext pt;
        vector<uint64_t> pod_matrix(this->batch_encoder->slot_count(), inv);
        batch_encoder->encode(pod_matrix, pt);

        evaluator->multiply_plain_inplace(result, pt);

        vector<Ciphertext> one_hot_ciph(n_classes);

        #pragma omp parallel
        {   
            Plaintext plain_matrix;
            Ciphertext zero_cipher;
            #pragma omp for
            for (int i = 0; i < n_classes; i++) {
                if(one_hot_flag[i]) {
                    this->batch_encoder->encode(one_hot_vecs[i], plain_matrix);
                    evaluator->multiply_plain(result, plain_matrix, one_hot_ciph[i]);
                } else {
                    this->enc->encrypt_zero(result.parms_id(), zero_cipher);
                    one_hot_ciph[i] = zero_cipher;
                }
            }
        }

        query_parameters->metrics_["time_server_aggregation"] += time_server_aggregation.end_and_get();

    return one_hot_ciph;
}

Plaintext TreeEvaluationServer::create_near_zero_mask(vector<int>& indices_to_keep, vector<int>& values_for_kept_indices, QueryParameters* q_params)
{
    Plaintext mask;
    vector<uint64_t> mask_matrix(batch_encoder->slot_count(), 0);

    for (int i = 0; i < indices_to_keep.size(); i++) {
        mask_matrix[indices_to_keep[i]] = values_for_kept_indices[i];
    }
    this->batch_encoder->encode(mask_matrix, mask);
    return mask;
}

// Annotate the nodes of the tree with their position in the plaintext vector
void traverse_and_fill(DecisionTreeNode* root, vector<vector<uint64_t>>& arrs, map<int, int>& first_unused_index, int max_reps)
{
    if (!root->is_leaf()) {
        int first_index = root->attribute_used;
        int second_index = first_unused_index[root->attribute_used];

        arrs[second_index/max_reps][first_index*max_reps+(second_index%max_reps)] = root->threshold_value;
        first_unused_index[root->attribute_used] += 1;

        // root->set_position(first_index * max_reps + second_index);
        root->set_position(first_index, second_index);

        traverse_and_fill(root->left_child, arrs, first_unused_index, max_reps);
        traverse_and_fill(root->right_child, arrs, first_unused_index, max_reps);
    }
}

vector<vector<uint64_t>> TreeEvaluationServer::generate_server_comp_values(QueryParameters* query_parameters){

    uint64_t number_of_attributes = query_parameters->num_attr;
    int num_cts = ceil((float)query_parameters->max_repetitions/query_parameters->reps_in_ct_per_attr);
    vector<vector<uint64_t>> server_comp_values(num_cts, vector<uint64_t>(number_of_attributes*query_parameters->reps_in_ct_per_attr,0));

    map<int, int> first_unused_indices;
    for (int i = 0; i < number_of_attributes; i++) {
        first_unused_indices[i] = 0;
    }

    traverse_and_fill(this->tree_root, server_comp_values, first_unused_indices, query_parameters->reps_in_ct_per_attr);

    return server_comp_values;
}

void TreeEvaluationServer::respond_to_classification(
    stringstream& data_stream,
    QueryParameters* query_parameters,
    bool _verbose
){

    // Setting threads
    if (query_parameters->num_threads == 0) query_parameters->num_threads = omp_get_max_threads();
    omp_set_num_threads(query_parameters->num_threads);
    if (_verbose)
        cout << "\tNumber of Threads: " << query_parameters->num_threads << endl
             << "--------------------------------------------------------------------"
             << endl;
    
    Timer time_server_crypto, time_server_batch_compare, time_server_aggregation, time_server_total;
    time_server_total.start();
    
    Timer time_load_data;
    time_load_data.start();
    ////////////////////////////////////////////////////////
        // Loading Keys, then data
        this->load_keys(data_stream);
        vector<Ciphertext> encrypted_query(query_parameters->num_input_ciphers);
        for (int i = 0; i < query_parameters->num_input_ciphers; i++) {
            encrypted_query[i].load(*context, data_stream);
        }
    ////////////////////////////////////////////////////////    
    query_parameters->metrics_["time_load_data"] = time_load_data.end_and_get();
    //query_parameters->metrics_["noise_budget_after_comparison"] = this->noise_calculator->invariant_noise_budget(encrypted_query[0]);
    
    
    /*
    srand(time(0));
    int forest_size = this->decision_forest.size();
    
    while(random_tree_indices.size() < query_parameters->n_trees) {
        int idx = rand() % (forest_size);
        random_tree_indices.insert(idx);
    }
    */
    
    int n_classes = query_parameters->n_classes;
    vector<vector<Ciphertext>> tree_result_ciphers(n_classes);

    //for(int idx : random_tree_indices)
    for(auto tree : decision_forest) {
        //this->tree_root = decision_forest.at(idx);
        this->tree_root = tree;
        query_parameters->set_tree_params(this->tree_root);
        
        time_server_crypto.start();

        // Generate the server values to compare with the clients values
        vector<vector<uint64_t>> server_comp_values = generate_server_comp_values(query_parameters);

        Timer time_batched_comparison;
        time_batched_comparison.start();
        ////////////////////////////////////////////////////////
            vector<Ciphertext> comparisonResult = batched_comparison(encrypted_query, server_comp_values, query_parameters);
        ////////////////////////////////////////////////////////
        query_parameters->metrics_["time_batched_comparison"] += time_batched_comparison.end_and_get();
        query_parameters->metrics_["noise_budget_after_comparison"] = this->noise_calculator->invariant_noise_budget(comparisonResult[0]);
        
        // Sum up the path
        Timer time_summing;
        time_summing.start();
        ////////////////////////////////////////////////////////
            vector<Ciphertext> comparisonResultList;
            if (query_parameters->path_eval == SUM) {
                comparisonResultList = bfs_path_summing(comparisonResult, query_parameters);
            }
        ////////////////////////////////////////////////////////
        query_parameters->metrics_["time_summing"] += time_summing.end_and_get();
        query_parameters->metrics_["noise_budget_after_summing"] = this->noise_calculator->invariant_noise_budget(comparisonResultList[0]);

        // Sum up the path
        Timer time_prepare_result;
        time_prepare_result.start();
        ////////////////////////////////////////////////////////
        // Mask everything except required result
        vector<Ciphertext> client_result = prepare_client_result(comparisonResultList, query_parameters);
        ////////////////////////////////////////////////////////
        for(int i = 0; i < n_classes; i++) {
            tree_result_ciphers[i].push_back(client_result[i]);
        }
        
        query_parameters->metrics_["time_prepare_result"] += time_prepare_result.end_and_get();
        query_parameters->metrics_["noise_budget_after_prepare_result"] = this->noise_calculator->invariant_noise_budget(client_result[0]);
        
        query_parameters->metrics_["time_server_crypto"] += time_server_crypto.end_and_get();        

    }
    
    time_server_aggregation.start();

    vector<Ciphertext> ct_vec(n_classes);
    for(int i = 0; i < n_classes; i++) {
        evaluator->add_many(tree_result_ciphers[i], ct_vec[i]);
    }

    int max_step = (int)pow(2, ceil(log2(query_parameters->n_leaves)));
    for (int i = 1; i < max_step; i *= 2) {
        for(int j = 0; j < n_classes; j++) {
            Ciphertext rotated;
            evaluator->rotate_rows(
                ct_vec[j], i,
                *gal_keys_server, rotated
            );
            evaluator->add_inplace(ct_vec[j], rotated);
        }
    }

    vector<uint64_t> mask(query_parameters->poly_mod_degree, 0);
    mask[0] = 1;
    Plaintext pt;
    batch_encoder->encode(mask, pt);
    evaluator->multiply_plain_inplace(ct_vec[0], pt);
    for (int i = 1; i < n_classes; i++) {
        evaluator->multiply_plain_inplace(ct_vec[i], pt);
        evaluator->rotate_rows_inplace(ct_vec[i], -i, *gal_keys_server);
    }

    Ciphertext result;
    evaluator->add_many(ct_vec, result);
    
    query_parameters->metrics_["time_server_aggregation"] = time_server_aggregation.end_and_get();

    auto size_encrypted_answer = 0;
    size_encrypted_answer += result.save(data_stream);
    
    query_parameters->metrics_["time_server_total"] = time_server_total.end_and_get();  
    if (_verbose)
            cout << "--- End of process ---" << endl;  
}
