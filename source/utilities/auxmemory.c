/*
    See license.txt in the root of this project.
*/

# include "auxmemory.h"
# include <stdint.h>

/*tex
    In |mimalloc| we have this natively, so we need a fallback. Actually, because |mimalloc|
    stores the old size, we only pass the count and record size there. So, one always has
    to wrap it. In our usage we already remember the old size.
*/

void *recalloc(
    void   *old_p, 
    size_t  old_count, 
    size_t  new_count, 
    size_t  size
)
{
    if (size <= 0) {
        return NULL;
    } else if (new_count > SIZE_MAX / size || old_count > SIZE_MAX / size) {
        return NULL; // overflow
    } else {
        size_t old_bytes = old_count * size;
        size_t new_bytes = new_count * size;
        void *new_p = realloc(old_p, new_bytes);
        if (! new_p) {
            return NULL;
        } else {
            if (new_bytes > old_bytes) {
                memset((char *) new_p + old_bytes, 0, new_bytes - old_bytes);
            }
            return new_p;
        }
    }
}

/* calculate aligned byte size (it has to be a multiple of LMT_MEMORY_ALIGNMENT) */

static inline size_t lmt_memory_aligned_total_bytes(int recordsize, int size, int reserved)
{
    size_t bytes = (size_t) recordsize * ((size_t) size + reserved + 1);
# if lmt_memory_force_align == 1
    if (bytes % LMT_MEMORY_ALIGNMENT != 0) {
        bytes = (bytes + (LMT_MEMORY_ALIGNMENT - 1)) & ~(size_t) (LMT_MEMORY_ALIGNMENT - 1);
    }
# endif
    return bytes;
}
void *aux_allocate_array(int recordsize, int size, int reserved)
{
# if lmt_memory_force_align == 1
    return lmt_memory_malloc_aligned(lmt_memory_aligned_total_bytes(recordsize, size, reserved), LMT_MEMORY_ALIGNMENT);
# else
    return lmt_memory_malloc((size_t) recordsize * ((size_t) size + reserved + 1));
# endif
}

void *aux_reallocate_array(void *p, int recordsize, int size, int reserved)
{
# if lmt_memory_force_align == 1
    return lmt_memory_realloc_aligned(p, lmt_memory_aligned_total_bytes(recordsize, size, reserved), LMT_MEMORY_ALIGNMENT);
# else
    return lmt_memory_realloc(p, (size_t) recordsize * ((size_t) size + reserved + 1));
# endif
}

void *aux_allocate_clear_array(int recordsize, int size, int reserved)
{
# if lmt_memory_force_align == 1
    /* 
        For |calloc_aligned|, |count * size| needs to be a multiple of LMT_MEMORY_ALIGNMENT. Passing 1 as 
        count and the rounded-up total bytes ensures strict alignment compliance.
    */
    return lmt_memory_calloc_aligned(1, lmt_memory_aligned_total_bytes(recordsize, size, reserved), LMT_MEMORY_ALIGNMENT);
# else
    return lmt_memory_calloc((size_t) size + reserved + 1, recordsize);
# endif
}

void *aux_reallocate_clear_array(void *p, int recordsize, int size, int reserved, int oldsize)
{
# if lmt_memory_force_align == 1
    size_t new_bytes = lmt_memory_aligned_total_bytes(recordsize, size, reserved);
    /* 
        This |recalloc_aligned| expects old count or old bytes depending on the wrapper, so
        here we use |new_bytes| for |realloc_aligned| and manual zeroing of the newly added 
        delta.
    */
    void *q = lmt_memory_realloc_aligned(p, new_bytes, LMT_MEMORY_ALIGNMENT);
    if (q) {
        size_t old_bytes = (size_t) oldsize * recordsize;
        if (new_bytes > old_bytes) {
            memset((char *) q + old_bytes, 0, new_bytes - old_bytes);
        }
    }
    return q;
# else
    size_t newsize = ((size_t) size + reserved + 1);
    return lmt_memory_recalloc(p, oldsize, newsize, recordsize);
# endif
}

void aux_deallocate_array(void *p)
{
# if lmt_memory_force_align == 1
    lmt_memory_free_aligned(p);
# else
    lmt_memory_free(p);
# endif
}
