#include "parser.h"



Parser::Parser() { }

inline void consume(FILE *p, int cnt) {
    while(cnt--) {
        getc(p);
    }
}

uint64_t Parser::parseInt() {
    uint64_t ret = 0;
    char ch = 0;
    while(true) {
        ch = getc(p);
        if(ch < '0' || ch > '9') {
            ungetc(ch, p);
            return ret;
        }
        ret = (ret * 10) + (ch - '0');
    }
}

DecisionTreeNode *Parser::parseNode() {
    DecisionTreeNode *ret = NULL;

    consume(p, 10);
    char ch = getc(p);
    
    if(ch == 's') {
        consume(p, 20);
        uint64_t threshold = parseInt();
        consume(p, 13);
        uint64_t feature = parseInt();
        consume(p, 10);
        auto left = parseNode();
        consume(p, 11);
        auto right = parseNode();
        ret = new DecisionTreeNode(threshold, feature);
        ret->left_child = left;
        ret->right_child = right;
        getc(p);
    } else {
        consume(p, 15);
        uint64_t value = parseInt();
        ret = new DecisionTreeNode(value, -1);
        getc(p);
    }

    return ret;
}

DecisionTreeNode *Parser::parseNode(const json& tree) {
    DecisionTreeNode *node = NULL;

    if(tree["type"] == "split") {
        node = new DecisionTreeNode(tree["threshold"], tree["feature"]);
        node->left_child = parseNode(tree["left"]);
        node->right_child = parseNode(tree["right"]);
    } else {
        node = new DecisionTreeNode(tree["value"], -1);
    }
    return node;
}

vector<DecisionTreeNode*> Parser::parseForest(const std::string &fname) {
    std::ifstream f(fname);
    if(!f.is_open()) {
        std::cerr << "Failed to open file.\n";
        return {};
    }

    vector<DecisionTreeNode*> forest;

    json data = json::parse(f);

    for(const auto& tree : data) {
        forest.push_back(parseNode(tree));
    }

    return forest;
}

DecisionTreeNode *Parser::parseTree(const std::string &fname) {
    p = fopen(fname.c_str(), "r");
    assert(p);
    
    auto ret = parseNode();

    fclose(p);
    p = nullptr;
    
    return ret;
}