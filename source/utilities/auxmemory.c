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

void *recalloc(void *old_p, size_t old_count, size_t new_count, size_t size)
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

void *aux_allocate_array(int recordsize, int size, int reserved)
{
    return lmt_memory_malloc(recordsize * ((size_t) size + reserved + 1));
}

void *aux_reallocate_array(void *p, int recordsize, int size, int reserved)
{
    return lmt_memory_realloc(p, recordsize * ((size_t) size + reserved + 1));
}

void *aux_allocate_clear_array(int recordsize, int size, int reserved)
{
    return lmt_memory_calloc((size_t) size + reserved + 1, recordsize);
}

# if 0

    void *aux_reallocate_clear_array(void *p, int recordsize, int size, int reserved, int oldsize)
    {
        size_t newsize = ((size_t) size + reserved + 1);
        void *q = lmt_memory_realloc(p, recordsize * newsize);
        // Cast q to (char *) so pointer arithmetic adds bytes correctly
        memset((char *) q + (size_t) oldsize * recordsize, 0, (newsize - oldsize) * recordsize);
        return q;
    }

# else

    void *aux_reallocate_clear_array(void *p, int recordsize, int size, int reserved, int oldsize)
    {
        size_t newsize = ((size_t) size + reserved + 1);
        return lmt_memory_recalloc(p, oldsize, newsize, recordsize);
    }

# endif

void aux_deallocate_array(void *p)
{
    lmt_memory_free(p);
}
