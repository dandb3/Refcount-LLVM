#include "test.h"

struct wow {
    union {
        int val2;
        atomic_t val1;
    };
};

struct wow1 {
    union {
        refcount_t val1;
    } a, b;
};

struct wow2 {
    refcount_t val1;
};

struct wow3 {
    struct wow2 val1;
    refcount_t val2;
};

struct {
    struct {
        union {
            atomic_long_t val1;
            int val2;
        };
    };
} st1;

// struct wow5 {
//     struct wow4 val1;
// };

union {
    struct wow6 {
        atomic_t val1;
        atomic_t val2;
        atomic_t val3;
        atomic_t val4;
        atomic_long_t val5;
        struct kref val6;
    } w;
} st2;

int func() {
    struct wow v0;
    struct wow1 v1;
    struct wow2 v2;
    struct wow3 v3;
    struct wow6 v6;
    struct wow6 {
        int val1;
    } v7;
    return 0;
}