#
# import dataset
#

import os
import json
import numpy
from pandas import DataFrame
from scipy.io.arff import loadarff

from sklearn.preprocessing import LabelEncoder
from sklearn.model_selection import train_test_split 
from sklearn.metrics import average_precision_score
from sklearn.metrics import precision_score
from concrete.ml.sklearn import DecisionTreeClassifier as ConcreteDecisionTreeClassifier
from concrete.ml.sklearn import RandomForestClassifier as ConcreteRandomForestClassifier

import random
CLASSIFICATION_VALUE_BITLENGTH=8

class Leaf(object):
    def __init__(self, v):
        self.type = "leaf"
        self.value = int(v)

class Internal(object):
    def __init__(self, threshold, feature, left, right):
        self.type = "split"
        self.threshold = int(threshold)
        self.feature = int(feature)
        # stored as dict so that we can get a json.dump easily
        self.left = left.__dict__
        self.right = right.__dict__

def build_forest(forest):
    all_trees = []
    total_nodes = 0

    for i, estimator in enumerate(forest.estimators_):
        t, count = build_tree(estimator)
        all_trees.append(t)
        total_nodes += count

    return all_trees, total_nodes

def build_tree(tree):
    node_count = [0]
    return (build_tree_rec(tree, 0, node_count), node_count[0])

def build_tree_rec(tree, node_id, node_count):
    left_child = tree.tree_.children_left[node_id]
    right_child = tree.tree_.children_right[node_id]
    is_split_node = left_child != right_child

    if is_split_node:
        left = build_tree_rec(tree, left_child, node_count)
        right = build_tree_rec(tree, right_child, node_count)
        # always use <= comparison
        node_count[0] += 1
        t = Internal(tree.tree_.threshold[node_id], tree.tree_.feature[node_id], left, right)
    else:
        max_index = tree.tree_.value[node_id].argmax()
        t = Leaf(max_index)

    return t

def generate_balanced_tree_rec(max_depth, bitlength, num_attributes, seed=None):

    if max_depth>0:
        left = generate_balanced_tree_rec(max_depth-1, bitlength, num_attributes, seed)
        right = generate_balanced_tree_rec(max_depth-1, bitlength, num_attributes, seed)
        threshold = random.randint(0, 2**bitlength-1)
        feature = random.randint(0, num_attributes-1)
        t = Internal(threshold, feature, left, right)
    else:
        t = Leaf(random.randint(0, 2**CLASSIFICATION_VALUE_BITLENGTH-1))

    return t

def generate_input(path, bitlength, num_attributes, seed=None):
    if seed is not None:
        random.seed(seed)
    with open(path, 'w') as output_file:
        for _ in range(num_attributes):
            attribute_value = random.randint(0, 2**bitlength-1)
            output_file.write(f'{attribute_value}\n')

WORKSPACE_DIR = os.path.dirname(os.path.abspath(__file__))

if __name__ == '__main__':

    for dataset_name in ['breast', 'heart', 'spam', 'steel']:

        write_path = os.path.join('datasets_quantized', dataset_name)
        if not os.path.exists(os.path.join(WORKSPACE_DIR, write_path)):
            os.makedirs(os.path.join(WORKSPACE_DIR, write_path))

        data = loadarff(os.path.join(WORKSPACE_DIR,"datasets",dataset_name+".arff"))
        df = DataFrame(data[0])

        df.columns = [label if label != 'class' else 'Class' for label in df.columns]

        X = df.drop(['Class'], axis=1)
        y = df['Class']

        X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.25, stratify=y)

        encoder = LabelEncoder()
        y_train = encoder.fit_transform(y_train)
        #y_test = encoder.fit_transform(y_test)
        y_test = encoder.transform(y_test)

        with open(os.path.join(WORKSPACE_DIR, write_path, f'{dataset_name}.config'), 'w') as f:
            f.write(str(len(X_train.columns)))

        for bitlength in range(10,17):
            model = ConcreteRandomForestClassifier(
                n_estimators = 10,
                random_state = 42,
                n_bits = bitlength
            )

            model, sklearn_model = model.fit_benchmark(X_train, y_train)

            if dataset_name != 'heart': 
                y_pred_concrete = model.predict_proba(X_test)[:, 1]
                y_pred_sklearn = sklearn_model.predict_proba(X_test)[:, 1]
                concrete_average_precision = average_precision_score(y_test, y_pred_concrete)
                sklearn_average_precision = average_precision_score(y_test, y_pred_sklearn)
                print(f"Sklearn average precision score: {sklearn_average_precision:0.2f}")
                print(f"Concrete average precision score: {concrete_average_precision:0.2f}")
            else: 
                y_pred_concrete = numpy.argmax(model.predict_proba(X_test), axis=1)
                y_pred_sklearn = numpy.argmax(sklearn_model.predict_proba(X_test), axis=1)
                concrete_average_precision = precision_score(y_test, y_pred_concrete, average='micro')
                sklearn_average_precision = precision_score(y_test, y_pred_sklearn, average='micro')
                print(f"Sklearn average precision score: {sklearn_average_precision:0.2f}")
                print(f"Concrete average precision score: {concrete_average_precision:0.2f}")    

            (forest_trees, total_nodes) = build_forest(model.sklearn_model)

            # print(f"Number of internal nodes: {count}")
            # print(f"Depth: {model.sklearn_model.get_depth()}")

            forest_json = {
                "num_classes": model.sklearn_model.n_classes_,
                "trees": [t.__dict__ for t in forest_trees]
            }

            with open(os.path.join(WORKSPACE_DIR, write_path,
                    f'forest_{dataset_name}_n_{bitlength}.json'), 'w+') as f:
                json.dump(forest_json, f)

        print(f"Done {dataset_name}")

    ########################################
