#ifndef __SERVER__H
#define __SERVER__H

#include "seal/seal.h"
#include "utils.h"

#include <unordered_set>

class Server {
    public:
    void load_parameters_and_keys(stringstream& params_stream, bool debug_mode, bool _verbose);
    void do_server_computation(stringstream& data_stream, size_t input_size, Metrics *metrics, size_t num_ctxt_per_value, bool mult_path, bool _verbose);
    void fetch_model(const string &fname);
    uint64_t evaluate_plain(const vector<uint64_t> &attr_vec);
    int n_trees = 10;

    private:

    seal::EncryptionParameters *enc_params = nullptr;
    seal::SEALContext *context = nullptr;

    seal::Evaluator *evaluator = nullptr;
    seal::RelinKeys *relin_keys = nullptr;
    seal::GaloisKeys *galois_keys = nullptr;
    seal::PublicKey *public_key = nullptr;
    seal::Encryptor *encryptor = nullptr;
    seal::BatchEncoder *batch_encoder = nullptr;

    size_t poly_modulus_degree = 0;
    uint64_t plain_modulus = 0;
    uint64_t inv = 0; // (inv * poly_modulus_degree) mod plain_modulus = 1 
    unordered_set<int> random_tree_indices;
    //int n_trees = 2;

    // Requires secret key
    // Only available in debug mode
    seal::Decryptor *decryptor = nullptr;

    std::vector<Node*> inner_nodes, leaves;
    std::vector<vector<Node*>> forest_inner_nodes, forest_leaves;
    //seal::Ciphertext labels;
    vector<Ciphertext> forest_labels;
    
    std::vector<seal::Ciphertext> load_inputs(stringstream &data_stream, size_t size);
    void send_results(const vector<seal::Ciphertext> &results, stringstream &data_stream);

    // math

    // computes 1{a > b} with one-ciphertext-comparison
    void xcmp(const seal::Ciphertext &enc_a, uint64_t b, seal::Ciphertext &dest);

    // computes 1{x = y}
    void xcmp_eq(const seal::Ciphertext &enc_a, uint64_t b, seal::Ciphertext &dest);

    // compute 1{a > b} with two-ciphertext-comparison
    void two_ctxt_xcmp(const seal::Ciphertext &enc_a1, const seal::Ciphertext &enc_a2, uint64_t b1, uint64_t b2, seal::Ciphertext &dest);

    void make_room_for_packing(seal::Ciphertext &ctxt);

    // // all of the following functions are used in multpath only (and therefore not important)
    // // computes 1{a > b} * p with one-ciphertext-comparison
    // void xcmp_param(const seal::Ciphertext &enc_a, uint64_t b, seal::Ciphertext &dest, uint64_t p);
    // // computes 1{a > b} * p with two-ciphertext-comparison
    // void two_ctxt_xcmp_param(const seal::Ciphertext &enc_a1, const seal::Ciphertext &enc_a2, uint64_t b1, uint64_t b2, seal::Ciphertext &dest, uint64_t p);

    // void extract_const_coeff(seal::Ciphertext &ctxt);
    // void mult_aggregate(Node *node, vector<seal::Ciphertext> &stack);
};

#endif
