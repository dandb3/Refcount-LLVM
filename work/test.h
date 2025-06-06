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

struct {
    refcount_t val1;
};