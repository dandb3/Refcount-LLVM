#include "test.h"

struct wow1 {
    int val1;
    int val2;
};

struct wow0 {
    union {
        struct {
            atomic_long_t val1;
        } s1;
        struct wow1 s2;
    };
};

int main() {
    struct wow0 eng;
    struct wow0 {
        atomic_t val1;
    };
    eng.s1.val1 = 3;
    eng.s2.val1 = 3;
}