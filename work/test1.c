#include "test.h"

struct inside {
    atomic_t val;
};

struct outside {
    struct inside in;
};

struct outside1 {
    atomic_t val0;
    struct {
        struct {
            atomic_t val2;
        };
    };
};

void foo() {
    struct outside out;
}