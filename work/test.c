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
    };
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
};

// struct wow5 {
//     struct wow4 val1;
// };

union {
    struct wow6 {
        atomic_t val1;
    } w;
};

int func() {
    return 0;
}