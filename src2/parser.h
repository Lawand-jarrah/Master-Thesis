#ifndef __PARSER_H
#define __PARSER_H

#include <stdio.h>
#include <string>
#include "utils.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

class Parser {
    FILE *p = nullptr;
    Node *parseNode();
    uint64_t parseInt();
    Node* parseNode(const json& tree);

    public: 
    Parser();
    Node *parseTree(const std::string &fname);
    vector<Node*> parseForest(const std::string &fname);
};

#endif 
