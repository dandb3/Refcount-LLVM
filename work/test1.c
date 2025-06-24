#include "test.h"

struct inside {
    atomic_t val;
};

struct outside {
    struct inside in;
};

void foo() {
    struct outside out;
}