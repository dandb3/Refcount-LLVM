#include "test.h"

void atomic_set(atomic_t *target, int val) {
    target->counter = val;
}

struct test0 {
    atomic_t count;
};

struct test1 {
    struct test0 val1[3];
};

int main() {
    struct test1 obj;
    atomic_t count[3];

    atomic_set(&obj.val1[1].count, 1);
    atomic_set(&count[0], 1);
}