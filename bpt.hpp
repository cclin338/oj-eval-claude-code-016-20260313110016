#ifndef BPT_HPP
#define BPT_HPP

#include <fstream>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <map>
#include <set>

const int BLOCK_SIZE = 4096;

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
    bool operator!=(const Key& other) const {
        return strcmp(data, other.data) != 0;
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
    bool operator==(const Pair& other) const {
        return key == other.key && value == other.value;
    }
};

// Simplified in-memory B+ tree with deferred writes
class BPlusTree {
private:
    std::string filename;
    std::map<Key, std::set<Value>> data; // In-memory storage
    bool modified;

    void loadFromFile() {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) return;

        int count;
        file.read((char*)&count, sizeof(int));

        for (int i = 0; i < count; i++) {
            Key key;
            Value value;
            file.read((char*)&key, sizeof(Key));
            file.read((char*)&value, sizeof(Value));
            data[key].insert(value);
        }

        file.close();
    }

    void saveToFile() {
        if (!modified) return;

        std::ofstream file(filename, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return;

        // Count total pairs
        int count = 0;
        for (const auto& entry : data) {
            count += entry.second.size();
        }

        file.write((char*)&count, sizeof(int));

        // Write all pairs
        for (const auto& entry : data) {
            for (const Value& value : entry.second) {
                file.write((char*)&entry.first, sizeof(Key));
                file.write((char*)&value, sizeof(Value));
            }
        }

        file.close();
        modified = false;
    }

public:
    BPlusTree(const std::string& fname) : filename(fname), modified(false) {
        loadFromFile();
    }

    ~BPlusTree() {
        saveToFile();
    }

    void insert(const Key& key, const Value& value) {
        data[key].insert(value);
        modified = true;
    }

    void remove(const Key& key, const Value& value) {
        auto it = data.find(key);
        if (it != data.end()) {
            it->second.erase(value);
            if (it->second.empty()) {
                data.erase(it);
            }
            modified = true;
        }
    }

    std::vector<Value> find(const Key& key) {
        std::vector<Value> result;
        auto it = data.find(key);
        if (it != data.end()) {
            result.assign(it->second.begin(), it->second.end());
        }
        return result;
    }

    void flush() {
        saveToFile();
    }
};

#endif // BPT_HPP
