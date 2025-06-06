union wow1 {
    int val1;
    int val2;
    double val3;
};

struct wow2 {
    int val1;
    int val2;
};

union wow3 {
    int val1;
    int val2;
};

struct wow4 {
    union un1 {
        int val1;
    } u;
};

struct wow5 {
    struct {
        struct {
            int val1;
            int val2;
        };
    };
};

struct wow6 {
    union {
        int val1;
    };
};