#include <iostream>
#include <vector>
class MyClass {
    std::vector<MyClass> children;
public:
    MyClass() { std::cout << "default\n"; }

    // 복사 생성자
    MyClass(const MyClass &other) {
        std::cout << "copy\n";
    }

    // 이동 생성자
    MyClass(MyClass &&other) noexcept {
        std::cout << "move\n";
    }

    // 복사 대입 연산자
    MyClass& operator=(const MyClass &other) {
        std::cout << "copy assign\n";
        return *this;
    }
    
    // 이동 대입 연산자
    MyClass& operator=(MyClass &&other) noexcept {
        std::cout << "move assign\n";
        return *this;
    }

    void add(MyClass &&node) {
        children.push_back(std::move(node));
    }
};

int main() {
    MyClass a;
    a.add(std::move(MyClass()));
    return 0;
}