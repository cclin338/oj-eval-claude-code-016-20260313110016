#include "bpt.hpp"
#include <iostream>
#include <string>
#include <sstream>

int main() {
    BPlusTree bpt("data.dat");

    int n;
    std::cin >> n;
    std::cin.ignore(); // Ignore newline after number

    for (int i = 0; i < n; i++) {
        std::string line;
        std::getline(std::cin, line);
        std::istringstream iss(line);

        std::string command;
        iss >> command;

        if (command == "insert") {
            std::string index;
            int value;
            iss >> index >> value;
            bpt.insert(Key(index.c_str()), value);
        } else if (command == "delete") {
            std::string index;
            int value;
            iss >> index >> value;
            bpt.remove(Key(index.c_str()), value);
        } else if (command == "find") {
            std::string index;
            iss >> index;
            std::vector<Value> results = bpt.find(Key(index.c_str()));
            if (results.empty()) {
                std::cout << "null" << std::endl;
            } else {
                for (size_t j = 0; j < results.size(); j++) {
                    if (j > 0) std::cout << " ";
                    std::cout << results[j];
                }
                std::cout << std::endl;
            }
        }
    }

    return 0;
}
