/*
    See license.txt in the root of this project.
*/

# ifndef LMT_UTILITIES_FILE_H
# define LMT_UTILITIES_FILE_H

/*tex

    We have to deal with wide characters on windows when it comes to filenames. The same is true for
    the commandline and environment variables. Basically we go from utf8 to wide and back.

    \starttyping
    libraries/zlib/crc32.c          : fopen -> minimalistic, goes via lua anyway
    libraries/zlib/trees.c          : fopen -> minimalistic, goes via lua anyway
    libraries/zlib/zutil.h          : fopen -> minimalistic, goes via lua anyway

    lua/llualib.c                   : fopen -> utf8_fopen
    lua/lenginelib.c                : fopen -> utf8_fopen

    luacore/lua55/src/lauxlib.c     : fopen -> see below
    luacore/lua55/src/liolib.c      : fopen -> see below
    luacore/lua55/src/loadlib.c     : fopen -> see below

    luaffi/call.c                   : fopen -> not used

    mp/mpw/mp.w                     : fopen -> overloaded by callback

    libraries/pplib/ppload.c        : fopen -> will be abstraction (next pplib)

    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used
    libraries/pplib/util/utiliof.c  : fopen -> not used

    tex/texfileio.c     12:         : fopen -> utf8_fopen
    \stoptyping

    Furthermore:

    \starttyping
    - system commands (execute) : done
    - popen                     : done

    - lua rename                : done
    - lua remove                : done

    - command line argv         : done
    - lua setenv                : done
    - lua getenv                : done

    - lfs attributes            : done
    - lfs chdir                 : done
    - lfs currentdir            : done
    - lfs dir                   : done
    - lfs mkdir                 : done
    - lfs rmdir                 : done
    - lfs touch                 : done
    - lfs link                  : done
    - lfs symlink               : done
    - lfs setexecutable         : done (needs testing)
    - lfs isdir                 : done
    - lfs isfile                : done
    - lfs iswritabledir         : done
    - lfs iswritablefile        : done
    - lfs isreadabledir         : done
    - lfs isreadablefile        : done
    \stoptyping

    Kind of tricky because quite some code (indirectness):

    \starttyping
    - lua load                  : via overload ?
    - lua dofile                : via overload -> loadstring
    - lua require               : via overload ?
    \stoptyping

    So: do we patch lua (fopen) or just copy? We can actually assume flat ascii files for libraries
    and such so there is no real need unless we load job related files.

    I will probably reshuffle some code and maybe more some more here; once I'm sure all works out
    well.


    canonicalize
        resolves symlinks, .., ., and duplicate slashes natively via the OS filesystem:
            Windows: relies on GetFinalPathNameByHandleW
            POSIX  : uses realpath()

    expandpath
        resolves .., ., and duplicate slashes without resolving symlinks, even if the target file
        does not exist:
            Windows: relies on GetFullPathNameW
            POSIX  : uses aux_utf8_normalize_posix_path()

*/

# ifdef _WIN32

    # include <windows.h>
    # include <ctype.h>
    # include <stdio.h>

    extern LPWSTR  aux_utf8_to_wide     (const char *utf8str);
    extern char   *aux_utf8_from_wide   (LPWSTR widestr);

    extern FILE   *aux_utf8_fopen       (const char *path, const char *mode);
    extern FILE   *aux_utf8_popen       (const char *path, const char *mode);
    extern int     aux_utf8_system      (const char *cmd);

    extern int     aux_utf8_remove      (const char *name);
    extern int     aux_utf8_rename      (const char *oldname, const char *newname);

    extern int     aux_utf8_setargv     (char * **av, char **argv, int argc);
    extern char   *aux_utf8_getownpath  (const char *file);
    extern char   *aux_utf8_readlink    (const char *file);
    extern char   *aux_utf8_canonicalize(const char *file);
    extern char   *aux_utf8_expandpath  (const char *file);

# else

    # include <libgen.h>
    # include <stdio.h>

    # define       aux_utf8_fopen       fopen
    # define       aux_utf8_popen       popen
    # define       aux_utf8_system      system

    extern int     aux_utf8_remove      (const char *name);
    extern int     aux_utf8_rename      (const char *oldname, const char *newname);

    extern int     aux_utf8_setargv     (char * **av, char **argv, int argc);
    extern char   *aux_utf8_getownpath  (const char *file);
    extern char   *aux_utf8_readlink    (const char *file);
    extern char   *aux_utf8_canonicalize(const char *file);
    extern char   *aux_utf8_expandpath  (const char *file);

# endif


# ifdef _WIN32

    extern char *aux_basename (const char *name);
    extern char *aux_dirname  (const char *name);

# else

    # define aux_basename basename
    # define aux_dirname  dirname

# endif

extern int aux_is_readable (const char *filename);

/*tex

    We support unix and windows. In fact, we could stick to |/| only. When
    scanning filenames entered in \TEX\ we can actually enforce a |/| as
    convention.

*/

# ifndef IS_DIR_SEP
    # ifdef _WIN32
        # define IS_DIR_SEP(ch) ((ch) == '/' || (ch) == '\\')
    # else
        # define IS_DIR_SEP(ch) ((ch) == '/')
    # endif
# endif

# ifndef R_OK
    # define F_OK 0x0
    # define W_OK 0x2
    # define R_OK 0x4
# endif

# ifndef S_ISREG
    # define S_ISREG(mode) (mode & _S_IFREG)
# endif

/*tex

    At some point I tested this and it had little to no impact but after replacing multiple
    |getc| hy a |fread| it made sense to also use this level of indirection. Not that we need
    these often. We use the |lmt_| namespace to indicate that we're in this single thread
    environment. (In the past we tested this on format loading and it had zero practical
    advantages, but nowadays we also play with tiny devices so \unknown

    As a note: \LUA\ also defaults to unlocked but only on \POSIX. The more
    specific |MSVC| test is needed for the tell and seek. There is some overlap
    with the file lib defined.

*/

/*tex
    At some point I tested this and it had little to no impact but after replacing multiple |getc|
    by a |fread| it made sense to also use this level of indirection. Not that we need these often.
    We use the |lmt_| namespace to indicate that we're in this single thread environment. In the 
    past we tested this on format loading and it had zero practical advantages, but nowadays we 
    also play with tiny devices so \unknown. We also unify platform-specific stream positioning 
    types, standard seek/tell wrappers, and unlocked/nolock I/O primitives here so they are 
    available globally across helper files. So we also moved some code from elsewhere here. We 
    actually only use this in the extended IO lib and format (un)dump. 
*/

# if defined(__GLIBC__) || defined(LUA_USE_POSIX)

    # define lmt_seeknum             off_t
    # define lmt_fseek(f, o, w)      fseeko(f, o, w)
    # define lmt_ftell(f)            ftello(f)

    # define lmt_getc(f)             getc_unlocked(f)
    # define lmt_putc(c, f)          putc_unlocked(c, f)
    # define lmt_fread(p, sz, n, f)  fread_unlocked(p, sz, n, f)
    # define lmt_fwrite(p, sz, n, f) fwrite_unlocked(p, sz, n, f)
    # define lmt_ungetc(c, f)        ungetc(c, f)

# elif defined(_WIN32) && !defined(_CRTIMP_TYPEINFO) && defined(_MSC_VER) && (_MSC_VER >= 1400)

    # define lmt_seeknum             __int64
    # define lmt_fseek(f, o, w)      _fseeki64_nolock(f, o, w)
    # define lmt_ftell(f)            _ftelli64_nolock(f)

    # define lmt_getc(f)             _fgetc_nolock(f)
    # define lmt_putc(c, f)          _fputc_nolock(c, f)
    # define lmt_fread(p, sz, n, f)  _fread_nolock(p, sz, n, f)
    # define lmt_fwrite(p, sz, n, f) _fwrite_nolock(p, sz, n, f)
    # define lmt_ungetc(c, f)        ungetc(c, f)

# elif defined(__MINGW32__)

    # define lmt_seeknum             int64_t
    # define lmt_fseek(f, o, w)      fseeko64(f, o, w)
    # define lmt_ftell(f)            ftello64(f)

    # define lmt_getc(f)             getc(f)
    # define lmt_putc(c, f)          putc(c, f)
    # define lmt_fread(p, sz, n, f)  fread(p, sz, n, f)
    # define lmt_fwrite(p, sz, n, f) fwrite(p, sz, n, f)
    # define lmt_ungetc(c, f)        ungetc(c, f)

# else

    # define lmt_seeknum             long
    # define lmt_fseek(f, o, w)      fseek(f, o, w)
    # define lmt_ftell(f)            ftell(f)

    # define lmt_getc(f)             getc(f)
    # define lmt_putc(c, f)          putc(c, f)
    # define lmt_fread(p, sz, n, f)  fread(p, sz, n, f)
    # define lmt_fwrite(p, sz, n, f) fwrite(p, sz, n, f)
    # define lmt_ungetc(c, f)        ungetc(c, f)

# endif

# endif
