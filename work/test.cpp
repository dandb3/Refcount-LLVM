#include <iostream>
#include <vector>

int main() {
    std::vector<std::string> wow(3, "e");

    for (size_t i = 0; i < wow.size(); ++i) {
        std::cout << wow[i] << "\n";
    }
    return 0;
}