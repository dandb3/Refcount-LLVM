typedef struct {
    int counter;
} atomic_t;

typedef struct {
    long long counter;
} atomic64_t;

typedef atomic64_t atomic_long_t;

typedef struct refcount_struct {
    atomic_t refs;
} refcount_t;

struct kref {
    refcount_t refcount;
};

struct wow {
    union {
        int val2;
        atomic_t val1;
    };
};

int func() {
    struct wow val1;
    struct kref val2;
    atomic_long_t val3;
    return 0;
}