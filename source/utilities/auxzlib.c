/*
    See license.txt in the root of this project.
*/

# include "auxzlib.h"
# include "auxmemory.h"

void *lmt_zlib_alloc(void *opaque, size_t items, size_t size)
{
    (void) opaque;   
    /* prevent multiplication overflow */
    if (items != 0 && size > SIZE_MAX / items) {
        return NULL; 
    }    
    size_t total_bytes = items * size;
# if lmt_force_align == 1
    return lmt_memory_malloc_aligned(total_bytes, LMT_MEMORY_ALIGNMENT);
# else 
    return lmt_memory_malloc(total_bytes);
# endif 
}

void  lmt_zlib_free(void *opaque, void *p)
{
    (void) opaque;
# if lmt_force_align == 1
    lmt_memory_free_aligned(p);
# else 
    lmt_memory_free(p);
# endif 
}
