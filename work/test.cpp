#include <fstream>
#include <iostream>
#include <sstream>

int main() {
    std::ifstream ifs("./test.txt");

    if (!ifs.is_open()) {
        exit(1);
    }

    std::string typeName, fileLine, filename;
    unsigned int line, col;
    char sep1, sep2;

    while (std::getline(ifs, typeName)) {
        if (typeName.empty())
            continue;

        if (!std::getline(ifs, fileLine)) {
            std::cerr << "위치 정보가 부족합니다.\n";
            break;
        }

        std::istringstream iss(fileLine);

        if (std::getline(iss, filename, ':') &&
            (iss >> line >> sep1 >> col) &&
            sep1 == ':') {
            std::cout << "Type: "     << typeName
                      << ", File: "   << filename
                      << ", Line: "   << line
                      << ", Column: " << col
                      << "\n";
        } else {
            std::cerr << "파싱 실패: " << fileLine << "\n";
        }
    }
}