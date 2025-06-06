#include "test.h"

typedef struct {
    int counter;
} atomic_t;

struct {
    struct {
        atomic_t val1;
    };
};

// typedef struct {
// 	/*
// 	 * ctx_id uniquely identifies this mm_struct.  A ctx_id will never
// 	 * be reused, and zero is not a valid ctx_id.
// 	 */
// 	u64 ctx_id;

// 	/*
// 	 * Any code that needs to do any sort of TLB flushing for this
// 	 * mm will first make its changes to the page tables, then
// 	 * increment tlb_gen, then flush.  This lets the low-level
// 	 * flushing code keep track of what needs flushing.
// 	 *
// 	 * This is not used on Xen PV.
// 	 */
// 	atomic64_t tlb_gen;

// 	struct rw_semaphore	ldt_usr_sem;
// 	struct ldt_struct	*ldt;
// 	unsigned short ia32_compat;

// 	struct mutex lock;
// 	void __user *vdso;			/* vdso base address */
// 	const struct vdso_image *vdso_image;	/* vdso image in use */

// 	atomic_t perf_rdpmc_allowed;	/* nonzero if rdpmc is allowed */
// 	u16 pkey_allocation_map;
// 	s16 execute_only_pkey;
// } mm_context_t;

typedef union {
    atomic_t val1;
} wow0_t;
