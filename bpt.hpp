#ifndef BPT_HPP
#define BPT_HPP

#include <fstream>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>

const int M = 100; // Order of B+ tree (max children for internal node)
const int L = 100; // Max entries for leaf node

// Key type: string with max 64 bytes
struct Key {
    char data[65];

    Key() { memset(data, 0, sizeof(data)); }
    Key(const char* s) { strncpy(data, s, 64); data[64] = '\0'; }

    bool operator<(const Key& other) const {
        return strcmp(data, other.data) < 0;
    }
    bool operator==(const Key& other) const {
        return strcmp(data, other.data) == 0;
    }
    bool operator<=(const Key& other) const {
        return strcmp(data, other.data) <= 0;
    }
};

// Value type: int
typedef int Value;

// Key-Value pair
struct Pair {
    Key key;
    Value value;

    Pair() : value(0) {}
    Pair(const Key& k, const Value& v) : key(k), value(v) {}

    bool operator<(const Pair& other) const {
        if (key == other.key) return value < other.value;
        return key < other.key;
    }
};

// Node types
enum NodeType { INTERNAL, LEAF };

// File position type
typedef long long FilePos;
const FilePos NULL_POS = -1;

// Internal node structure
struct InternalNode {
    int n; // Number of keys
    Key keys[M];
    FilePos children[M + 1];

    InternalNode() : n(0) {
        for (int i = 0; i <= M; i++) children[i] = NULL_POS;
    }
};

// Leaf node structure
struct LeafNode {
    int n; // Number of pairs
    Pair pairs[L];
    FilePos next; // Next leaf node

    LeafNode() : n(0), next(NULL_POS) {}
};

class BPlusTree {
private:
    std::fstream file;
    std::string filename;
    FilePos root;
    FilePos freePos; // Next free position for allocation

    // Read/Write nodes
    void writeInternal(FilePos pos, const InternalNode& node) {
        file.seekp(pos);
        int type = INTERNAL;
        file.write((char*)&type, sizeof(int));
        file.write((char*)&node, sizeof(InternalNode));
        file.flush();
    }

    void writeLeaf(FilePos pos, const LeafNode& node) {
        file.seekp(pos);
        int type = LEAF;
        file.write((char*)&type, sizeof(int));
        file.write((char*)&node, sizeof(LeafNode));
        file.flush();
    }

    NodeType readNodeType(FilePos pos) {
        file.seekg(pos);
        int type;
        file.read((char*)&type, sizeof(int));
        return (NodeType)type;
    }

    void readInternal(FilePos pos, InternalNode& node) {
        file.seekg(pos + sizeof(int));
        file.read((char*)&node, sizeof(InternalNode));
    }

    void readLeaf(FilePos pos, LeafNode& node) {
        file.seekg(pos + sizeof(int));
        file.read((char*)&node, sizeof(LeafNode));
    }

    FilePos allocateNode() {
        FilePos pos = freePos;
        freePos += sizeof(int) + std::max(sizeof(InternalNode), sizeof(LeafNode));
        return pos;
    }

    // Insert into leaf node
    void insertIntoLeaf(LeafNode& leaf, const Pair& pair) {
        int i = leaf.n - 1;
        while (i >= 0 && pair < leaf.pairs[i]) {
            leaf.pairs[i + 1] = leaf.pairs[i];
            i--;
        }
        leaf.pairs[i + 1] = pair;
        leaf.n++;
    }

    // Split leaf node
    FilePos splitLeaf(FilePos leafPos, LeafNode& leaf, Key& upKey) {
        FilePos newLeafPos = allocateNode();
        LeafNode newLeaf;

        int mid = (L + 1) / 2;
        newLeaf.n = leaf.n - mid;
        for (int i = 0; i < newLeaf.n; i++) {
            newLeaf.pairs[i] = leaf.pairs[mid + i];
        }
        leaf.n = mid;

        newLeaf.next = leaf.next;
        leaf.next = newLeafPos;

        upKey = newLeaf.pairs[0].key;

        writeLeaf(leafPos, leaf);
        writeLeaf(newLeafPos, newLeaf);

        return newLeafPos;
    }

    // Split internal node
    FilePos splitInternal(FilePos nodePos, InternalNode& node, Key& upKey) {
        FilePos newNodePos = allocateNode();
        InternalNode newNode;

        int mid = (M + 1) / 2;
        upKey = node.keys[mid];

        newNode.n = node.n - mid - 1;
        for (int i = 0; i < newNode.n; i++) {
            newNode.keys[i] = node.keys[mid + 1 + i];
        }
        for (int i = 0; i <= newNode.n; i++) {
            newNode.children[i] = node.children[mid + 1 + i];
        }
        node.n = mid;

        writeInternal(nodePos, node);
        writeInternal(newNodePos, newNode);

        return newNodePos;
    }

    // Insert helper (returns new child if split occurred)
    FilePos insertHelper(FilePos nodePos, const Pair& pair, Key& upKey, bool& split) {
        if (nodePos == NULL_POS) {
            // Create new leaf
            FilePos newLeafPos = allocateNode();
            LeafNode newLeaf;
            newLeaf.pairs[0] = pair;
            newLeaf.n = 1;
            writeLeaf(newLeafPos, newLeaf);
            split = false;
            return newLeafPos;
        }

        NodeType type = readNodeType(nodePos);

        if (type == LEAF) {
            LeafNode leaf;
            readLeaf(nodePos, leaf);

            // Check if pair already exists
            for (int i = 0; i < leaf.n; i++) {
                if (leaf.pairs[i].key == pair.key && leaf.pairs[i].value == pair.value) {
                    split = false;
                    return NULL_POS;
                }
            }

            insertIntoLeaf(leaf, pair);

            if (leaf.n <= L) {
                writeLeaf(nodePos, leaf);
                split = false;
                return NULL_POS;
            } else {
                split = true;
                return splitLeaf(nodePos, leaf, upKey);
            }
        } else {
            InternalNode node;
            readInternal(nodePos, node);

            int i = 0;
            while (i < node.n && !(pair.key < node.keys[i])) {
                i++;
            }

            bool childSplit;
            Key childUpKey;
            FilePos newChild = insertHelper(node.children[i], pair, childUpKey, childSplit);

            if (!childSplit) {
                split = false;
                return NULL_POS;
            }

            // Insert new child into internal node
            for (int j = node.n; j > i; j--) {
                node.keys[j] = node.keys[j - 1];
                node.children[j + 1] = node.children[j];
            }
            node.keys[i] = childUpKey;
            node.children[i + 1] = newChild;
            node.n++;

            if (node.n <= M) {
                writeInternal(nodePos, node);
                split = false;
                return NULL_POS;
            } else {
                split = true;
                return splitInternal(nodePos, node, upKey);
            }
        }
    }

    // Find all values for a key in a leaf
    void findInLeaf(const LeafNode& leaf, const Key& key, std::vector<Value>& result) {
        for (int i = 0; i < leaf.n; i++) {
            if (leaf.pairs[i].key == key) {
                result.push_back(leaf.pairs[i].value);
            }
        }
    }

    // Delete from leaf node
    bool deleteFromLeaf(LeafNode& leaf, const Pair& pair) {
        for (int i = 0; i < leaf.n; i++) {
            if (leaf.pairs[i].key == pair.key && leaf.pairs[i].value == pair.value) {
                for (int j = i; j < leaf.n - 1; j++) {
                    leaf.pairs[j] = leaf.pairs[j + 1];
                }
                leaf.n--;
                return true;
            }
        }
        return false;
    }

    // Delete helper
    void deleteHelper(FilePos nodePos, const Pair& pair) {
        if (nodePos == NULL_POS) return;

        NodeType type = readNodeType(nodePos);

        if (type == LEAF) {
            LeafNode leaf;
            readLeaf(nodePos, leaf);
            if (deleteFromLeaf(leaf, pair)) {
                writeLeaf(nodePos, leaf);
            }
        } else {
            InternalNode node;
            readInternal(nodePos, node);

            int i = 0;
            while (i < node.n && !(pair.key < node.keys[i])) {
                i++;
            }

            deleteHelper(node.children[i], pair);
        }
    }

    // Find helper
    void findHelper(FilePos nodePos, const Key& key, std::vector<Value>& result) {
        if (nodePos == NULL_POS) return;

        NodeType type = readNodeType(nodePos);

        if (type == LEAF) {
            LeafNode leaf;
            readLeaf(nodePos, leaf);
            findInLeaf(leaf, key, result);
        } else {
            InternalNode node;
            readInternal(nodePos, node);

            int i = 0;
            while (i < node.n && !(key < node.keys[i])) {
                i++;
            }

            findHelper(node.children[i], key, result);
        }
    }

public:
    BPlusTree(const std::string& fname) : filename(fname), root(NULL_POS) {
        // Try to open existing file
        file.open(filename, std::ios::in | std::ios::out | std::ios::binary);

        if (!file.is_open()) {
            // Create new file
            file.open(filename, std::ios::out | std::ios::binary);
            file.close();
            file.open(filename, std::ios::in | std::ios::out | std::ios::binary);

            // Initialize metadata
            freePos = 2 * sizeof(FilePos);
            writeMetadata();
        } else {
            // Read metadata
            readMetadata();
        }
    }

    ~BPlusTree() {
        if (file.is_open()) {
            writeMetadata();
            file.close();
        }
    }

    void writeMetadata() {
        file.seekp(0);
        file.write((char*)&root, sizeof(FilePos));
        file.write((char*)&freePos, sizeof(FilePos));
        file.flush();
    }

    void readMetadata() {
        file.seekg(0);
        file.read((char*)&root, sizeof(FilePos));
        file.read((char*)&freePos, sizeof(FilePos));
    }

    void insert(const Key& key, const Value& value) {
        Pair pair(key, value);
        Key upKey;
        bool split;
        FilePos newChild = insertHelper(root, pair, upKey, split);

        if (split) {
            // Create new root
            FilePos newRootPos = allocateNode();
            InternalNode newRoot;
            newRoot.n = 1;
            newRoot.keys[0] = upKey;
            newRoot.children[0] = root;
            newRoot.children[1] = newChild;
            writeInternal(newRootPos, newRoot);
            root = newRootPos;
        } else if (root == NULL_POS) {
            root = newChild;
        }

        writeMetadata();
    }

    void remove(const Key& key, const Value& value) {
        Pair pair(key, value);
        deleteHelper(root, pair);
        writeMetadata();
    }

    std::vector<Value> find(const Key& key) {
        std::vector<Value> result;
        findHelper(root, key, result);
        std::sort(result.begin(), result.end());
        return result;
    }
};

#endif // BPT_HPP
