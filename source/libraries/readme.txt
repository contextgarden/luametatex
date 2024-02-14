Nota bene,

The currently embedded libcerf library might become an optional one as soon as we decide to provide
it as such. It doesn't put a dent in filesize but as it's used rarely (and mostly as complement to
the complex math support) that makes sense. The library was added because some users wanted it as
companion the other math libraries and because TeX is often about math it sort of feels okay. But
it looks like there will never be support for the MSVC compiler. Mojca and I (Hans) adapted the
sources included here to compile out of the box, but that didn't make it back into the original.

The pplib library has a few patches with respect to memory allocation and zip compression so that
we can hook in the minizip and mimalloc alternatives.

The avl and hnj libraries are adapted to Lua(Meta)TeX and might get some more adaptations depending
on our needs. The decnumber library that is also used in mplib is unchanged.

In mimalloc we need to patch init.c: #if defined(_M_X64) || defined(_M_ARM64) to get rid of a link
error as well as in options.c some snprint issue with the mingw64 cross compiler: 

/* HH */ snprintf(tprefix, sizeof(tprefix), "%sthread 0x%x: ", prefix, (unsigned) _mi_thread_id()); /* HH: %z is unknown */

In decNumber.c this got added: 

# include "../../utilities/auxmemory.h"
# define malloc lmt_memory_malloc
# define free   lmt_memory_free

In softposit/source/include/softposit_types.h we have to comment the initializations in the unions
bcause the compiler complains about it (we're not using c++). So: 

uint32_t ui;    // =0;                  // patched by HH because the compilers don't like this 
uint64_t ui[2]; // ={0,0};              // idem 
uint64_t ui[8]; // ={0,0,0,0, 0,0,0,0}; // idme 
uint64_t ui[8]; // ={0,0,0,0, 0,0,0,0}; // idem
uint64_t ui[8]; // ={0,0,0,0, 0,0,0,0}; // idem 

We only include a subset of the potrace files. There is one patch for uint64_t because mingw doesn't like 
it. It's a playground anyway, and it might eventually go away (become an optional library but then we need
to figure out how to get windows dll's.) We did add this: 

# if defined(LUAMETATEX_USE_MIMALLOC)
    # include "libraries/mimalloc/include/mimalloc-override.h"
# endif 

Nice site : https://mserdarsanli.github.io/FloatInfo
Nice video: https://youtu.be/Ae9EKCyI1xU (GradIEEEnt half decent)

Hans