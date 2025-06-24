#include "test.h"

struct outside {
    struct inside {
        atomic_t val;
    } in;
};

void foo() {
    struct outside out;
}