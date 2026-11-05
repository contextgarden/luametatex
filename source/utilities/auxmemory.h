/*
    See license.txt in the root of this project.
*/

/*
    Some operating systems come with |allocarray| so we use more verbose names. We cannot define
    them because on some bsd/apple platforms |CLANG| cannot resolve them.

*/

# ifndef LMT_UTILITIES_MEMORY_H
# define LMT_UTILITIES_MEMORY_H

# define lmt_memory_force_align 1

# define LMT_MEMORY_ALIGNMENT 64

/*tex
    This is an experiment. The impact of using an alternative allocator on native Windows makes a
    native version some 5% faster than a cross compiled one. Otherwise the cross compiled version
    outperforms the native one a bit. In \TEX\ and \METAPOST\ we already do something like this
    but there we don't reclaim memory.

*/

# include <stdlib.h>
# include <string.h>
# include <stdint.h>

# if defined(LUAMETATEX_USE_MIMALLOC)

    # include "libraries/mimalloc/include/mimalloc.h"

    /* mimalloc regular wrappers */

    # define lmt_memory_malloc   mi_malloc
    # define lmt_memory_calloc   mi_calloc
    # define lmt_memory_realloc  mi_realloc
 // # define lmt_memory_recalloc mi_recalloc
    # define lmt_memory_free     mi_free
    # define lmt_memory_strdup   mi_strdup

    # define lmt_memory_recalloc(p,oldcount,newcount,size) ((void) (oldcount), mi_recalloc(p,newcount,size))

    /* mimalloc aligned wrappers */

    # define lmt_memory_malloc_aligned(size, alignment)         mi_malloc_aligned(size, alignment)
    # define lmt_memory_calloc_aligned(count, size, alignment)  mi_calloc_aligned(count, size, alignment)
    # define lmt_memory_realloc_aligned(p, newsize, alignment)  mi_realloc_aligned(p, newsize, alignment)
    # define lmt_memory_free_aligned(p)                         mi_free(p)

    /* mimalloc doesn't have a direct mi_recalloc_aligned, so we simulate it or use standard pattern */

    static inline void *lmt_memory_recalloc_aligned (
        void   *p,
        size_t  oldcount,
        size_t  newcount,
        size_t  size,
        size_t  alignment
    )
    {
        size_t old_bytes = oldcount * size;
        size_t new_bytes = newcount * size;
        void *new_p = mi_realloc_aligned(p, new_bytes, alignment);
        if (new_p && new_bytes > old_bytes) {
            memset((char *) new_p + old_bytes, 0, new_bytes - old_bytes);
        }
        return new_p;
    }

 // # include "libraries/mimalloc/include/mimalloc-override.h"

# else
    
    # define lmt_memory_malloc   malloc
    # define lmt_memory_calloc   calloc
    # define lmt_memory_realloc  realloc
    # define lmt_memory_recalloc recalloc
    # define lmt_memory_free     free
    # define lmt_memory_strdup   strdup

    # if defined(_MSC_VER) || defined(__MINGW32__)

        # define lmt_memory_malloc_aligned(size, alignment)         _aligned_malloc(size, alignment)
        # define lmt_memory_calloc_aligned(count, size, alignment)  lmt_fallback_calloc_aligned((count) * (size), alignment)
        # define lmt_memory_realloc_aligned(p, newsize, alignment)  _aligned_realloc(p, newsize, alignment)
        # define lmt_memory_free_aligned(p)                         _aligned_free(p)

        static inline void *lmt_memory_recalloc_aligned (
            void   *p,
            size_t  oldcount,
            size_t  newcount,
            size_t  size,
            size_t  alignment
        )
        {
            size_t old_bytes = oldcount * size;
            size_t new_bytes = newcount * size;
            void *new_p = _aligned_realloc(p, new_bytes, alignment);
            if (new_p && new_bytes > old_bytes) {
                memset((char *) new_p + old_bytes, 0, new_bytes - old_bytes);
            }
            return new_p;
        }

    # else

        static inline void *lmt_memory_fallback_malloc_aligned (
            size_t size,
            size_t alignment
        )
        {
            size_t header_size = sizeof(size_t);
            size_t total_size = size + alignment + header_size;
            void *raw_ptr = NULL;
            if (posix_memalign(&raw_ptr, alignment < sizeof(void *) ? sizeof(void *) : alignment, total_size) != 0) {
                return NULL;
            }
            char *aligned_ptr = (char *) (((uintptr_t) raw_ptr + header_size + alignment - 1) & ~(uintptr_t)(alignment - 1));
            ((size_t *) aligned_ptr)[-1] = size;
            ((void **) aligned_ptr)[-2] = raw_ptr;
            return (void *) aligned_ptr;
        }

        static inline void lmt_memory_fallback_free_aligned (
            void *p
        )
        {
            if (p) {
                void *raw_ptr = ((void **) p)[-2];
                free(raw_ptr);
            }
        }

        static inline void *lmt_memory_fallback_realloc_aligned (
            void *p,
            size_t newsize,
            size_t alignment
        )
        {
            if (!p) {
                return lmt_memory_fallback_malloc_aligned(newsize, alignment);
            }
            if (newsize == 0) {
                lmt_memory_fallback_free_aligned(p);
                return NULL;
            }
            size_t old_size = ((size_t *) p)[-1];
            void *new_p = lmt_memory_fallback_malloc_aligned(newsize, alignment);
            if (!new_p) {
                return NULL;
            }
            memcpy(new_p, p, old_size < newsize ? old_size : newsize);
            lmt_memory_fallback_free_aligned(p);
            return new_p;
        }

        static inline void *lmt_memory_fallback_calloc_aligned (
            size_t count,
            size_t size,
            size_t alignment
        )
        {
            size_t bytes = count * size;
            void *ptr = lmt_memory_fallback_malloc_aligned(bytes, alignment);
            if (ptr) {
                memset(ptr, 0, bytes);
            }
            return ptr;
        }

        static inline void *lmt_memory_fallback_recalloc_aligned (
            void   *p,
            size_t  oldcount,
            size_t  newcount,
            size_t  size,
            size_t  alignment
        )
        {
            size_t old_bytes = oldcount * size;
            size_t new_bytes = newcount * size;
            void *new_p = lmt_memory_fallback_realloc_aligned(p, new_bytes, alignment);
            if (! new_p) {
                return NULL;
            }
            if (new_bytes > old_bytes) {
                memset((char *) new_p + old_bytes, 0, new_bytes - old_bytes);
            }
            return new_p;
        }

        # define lmt_memory_malloc_aligned(size, alignment)         lmt_memory_fallback_malloc_aligned(size, alignment)
        # define lmt_memory_calloc_aligned(count, size, alignment)  lmt_memory_fallback_calloc_aligned(count, size, alignment)
        # define lmt_memory_realloc_aligned(p, newsize, alignment)  lmt_memory_fallback_realloc_aligned(p, newsize, alignment)
        # define lmt_memory_free_aligned(p)                         lmt_memory_fallback_free_aligned(p)
        # define lmt_memory_recalloc_aligned(p, oldcount, newcount, size, alignment) \
            lmt_memory_fallback_recalloc_aligned(p, oldcount, newcount, size, alignment)

    # endif

# endif

# define lmt_generic_malloc   malloc
# define lmt_generic_calloc   calloc
# define lmt_generic_realloc  realloc
# define lmt_generic_recalloc recalloc
# define lmt_generic_free     free
# define lmt_generic_strdup   strdup

extern void *recalloc (
    void   *p, 
    size_t  old_count, 
    size_t  new_count, 
    size_t  size
);

/*tex
   
   The regular (large) \TEX\ memory pools and stacks are managed by the next collection of 
   helpers. 

*/

extern void *aux_allocate_array         (int recordsize, int size, int reserved);
extern void *aux_reallocate_array       (void *p, int recordsize, int size, int reserved);
extern void *aux_allocate_clear_array   (int recordsize, int size, int reserved);
extern void *aux_reallocate_clear_array (void *p, int recordsize, int size, int reserved, int oldsize);
extern void  aux_deallocate_array       (void *p);

# endif