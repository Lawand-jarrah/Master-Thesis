#ifndef __PARSER_H
#define __PARSER_H

#include <stdio.h>
#include <string>
#include "tree_utils.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class Parser {
    FILE *p = nullptr;
    DecisionTreeNode *parseNode();
    DecisionTreeNode* parseNode(const json& tree);
    int parseNode(const json& tree, DecisionTreeNode*& outNode);
    uint64_t parseInt();

    public: 
    Parser();
    DecisionTreeNode *parseTree(const std::string &fname);
    vector<DecisionTreeNode*> parseForest(const std::string &fname, QueryParameters* query_parameters);
};

#endif 
