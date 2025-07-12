#include "test.h"

struct outside {
    struct inside {
        atomic_t val;
        int val0;
    } in;
};

typedef struct elearn {
    int val0;
} elearn_t;

typedef struct wow {
    atomic_long_t val0;
    atomic64_t val1;
    struct {
        struct kref val2;
        int val3;
        atomic_t val4;
    };
    struct {
        struct {
            refcount_t val5;
        };
    };
    struct {
        int val6;
    };
} wow_t;

struct {
    struct {
        int val0;
    };
} st0;

struct test {
    union {
        atomic_t val0;
        unsigned long val1;
    };
};

typedef unsigned us;

struct testt {
    unsigned val0:1;
    unsigned val1:1;
    unsigned val2:2;
    unsigned val3:5;
};

union dump {
    struct atomic_contain {
        atomic_t val0;
    } val0;
    unsigned long val1;
} ud;

void foo() {
    wow_t val0;
    struct outside val1;
    struct test val2;
    struct testt val3;
}