/*
    See license.txt in the root of this project.
*/

# ifndef LMT_AUXSTREAM_H
# define LMT_AUXSTREAM_H

typedef FILE *dumpstream;

/*tex

    I was curious if the loading a format via a file or string made a difference even if loading
    is extremely fast: on a 2018 laptop a \CONTEXT\ format takes 0.03 seconds via a file and 0.02
    via a string, so an not so impressive difference. When we started with \LUATEX\ we actually
    went for a compressed format (gz) because expecially over networks in 2005 it made a difference
    but with files being cached by the operating system less so. So in \LUAMETATEX|\ we not only
    dropped compression but also optimized saving and loading the format file. And  ... it might
    still matter on virtual machines (or on slow usb drives e.g. on the Arduino UNO Q).

    We keep the code but both code paths are good enough for our purpose. After all, it's the
    \LUA\ initialization that determines start up time. Format generation is not that critital
    and this experiment demonstrates that we should not waste time on doing that in memory. It
    anyway made the undump code already a bit more abstract (with its own |undumpstream|). 

    The experiment was actually triggered by two use cases. We have a large test suite that gets
    run often (in parallel) and while small gains are irrelevant for single runs on many thousands
    they might add up. Various optimizations not putting a dent in e.g.\ a 15 second 750 page
    document run, when applied on the test suite, brought runtime down from 540 to 370 seconds
    (and below 200 in linux) for at that moment 2200 test files processed in 8 threads (2018
    windows laptop).

    A second possible use case is doing multiple passes within a run in which case we need to
    reinitialize and then we can best keep the format data in memory, but for now we delay that
    till I have a use case.

*/

# define undump_in_memory 1

# if undump_in_memory == 1

    typedef struct lmt_undump_info {
        unsigned char *buffer;
        unsigned char *ptr;
        unsigned char *end;
        size_t         size;
    } lmt_undump_info;

    typedef lmt_undump_info *undumpstream;

    /* We (need to) use macros and we don't want to clash with |n| so we use a funny |_n_| here. */

    # define undump_items(u,p,item_size,nitems) do { \
        size_t _n_ = (size_t) (item_size) * (nitems); \
        if lmt_unlikely((u)->ptr + _n_ > (u)->end) { \
            tex_fatal_undump_error("truncated file"); \
        } \
        memcpy((void *) (p), (u)->ptr, _n_); \
        (u)->ptr += _n_; \
    } while (0)

    # define undump_scalar(u,x) do { \
        size_t _n_ = sizeof(x); \
        if lmt_unlikely((u)->ptr + _n_ > (u)->end) { \
            tex_fatal_undump_error("truncated file"); \
        } \
        memcpy((void *) &(x), (u)->ptr, _n_); \
        (u)->ptr += _n_; \
    } while (0)

    # define undump_things(u,base,len) undump_items(u, (char *) &(base), sizeof (base), (len))

    # define undump_mem(u,x)           undump_scalar(u, x)
    # define undump_int(u,x)           undump_scalar(u, x)

    # define undump_char(u,x) do { \
        if lmt_unlikely((u)->ptr >= (u)->end) { \
            tex_fatal_undump_error("truncated file"); \
        } \
        x = (char) *(u)->ptr++; \
    } while (0)

    # define undump_uchar(u,x) do { \
        if lmt_unlikely((u)->ptr >= (u)->end) { \
            tex_fatal_undump_error("truncated file"); \
        } \
        x = (unsigned char) *(u)->ptr++; \
    } while (0)

    # define undump_short(u,x)         undump_scalar(u, x)
    # define undump_ushort(u,x)        undump_scalar(u, x)

    # define undump_total(u)           (u->ptr - u->buffer)

# else

    typedef FILE *undumpstream;

    # define undump_items(f,p,item_size,nitems) lmt_fread ((void *) p, (size_t) item_size, (size_t) nitems, f)
    # define undump_things(f,base,len)          undump_items(f, (char *) &(base), sizeof (base), (len))

    # define undump_mem(f,x)                    undump_things(f,x,1)
    # define undump_int(f,x)                    undump_things(f,x,1)
    # define undump_char(f,x)                   x = (char) lmt_getc(f)
    # define undump_uchar(f,x)                  x = (unsigned char) lmt_getc(f)
    # define undump_short(f,x)                  undump_things(f,x,1)
    # define undump_ushort(f,x)                 undump_things(f,x,1)

    # define undump_total(f)                    lmt_ftell(f)

# endif

# endif