/*

    See license.txt in the root of this project.

    In \LUATEX\ we use the lfs library from the kepler project and we started from that one. In
    the meantime all the code has been replaced, we added some more and made sure it works well
    on msvs, posix and cross compilation. The original library offers the following functions
    and we try to remain compatible with these as users might expect them.

        lfs.attributes(filepath [, attributename | attributetable])
        lfs.chdir(path)
        lfs.currentdir()
        lfs.dir(path)
        lfs.link(old, new[, symlink])
        lfs.mkdir(path)
        lfs.rmdir(path)
        lfs.symlinkattributes(filepath [, attributename])
        lfs.touch(filepath [, atime [, mtime]])

    Because \TEX| is multi-platform we try to provide a consistent interface. So, for instance
    block size and inode number are not relevant for us, nor are user and group ids. The lock
    functions have been removed as they serve no purpose in a \TEX\ system and devices make no
    sense either. The iterator could be improved. I also fixed some abnormalities. Permissions
    are not useful either. As this is a bit hairy we occasionally come back to it, and in 2026
    did some checking with codex (luna max) and gemini flash and as a result updated some calls
    to libraries as we can assume to run on modern versions of the operating systems as well as
    use recent iterations of C. Double (tripple) checking this definitely made sense.

    We might move some more file(system) related code here.

    % sudo sync && echo 3 | sudo tee /proc/sys/vm/drop_caches
    % RAMMap.exe -Es   (EmptyStandbyList)
    % RAMMap.exe -Et   (EmptySystemWorkingSet)

*/

# include "../lua/lmtinterface.h"
# include "../utilities/auxmemory.h"
# include "../utilities/auxfile.h"

/* Standard access mode constants */

# ifndef R_OK

    enum {
        F_OK = 0,
        W_OK = 2,
        R_OK = 4
    };

# endif

# ifndef _WIN32
    # ifndef _FILE_OFFSET_BITS
        # define _FILE_OFFSET_BITS 64
    # endif
# endif

# ifdef _WIN32
    # ifndef WINVER
        # define WINVER       0x0601
        # undef  _WIN32_WINNT
        # define _WIN32_WINNT 0x0601
    # endif
# endif

# define _LARGEFILE64_SOURCE 1

# include <errno.h>
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <time.h>
# include <sys/stat.h>

/*tex 
    See |lmtinterface.h| for |DIR_HANDLE_INSTANCE|. here it's not really needed to have a high 
    performance lookup but we are consistent so let's have it. We seldom have more than a few 
    tens of thousands iterations. 
*/

# if defined(PATH_MAX)
    # define MY_MAXPATHLEN PATH_MAX
# elif defined(MAXPATHLEN)
    # define MY_MAXPATHLEN MAXPATHLEN
# else
    # define MY_MAXPATHLEN 255
# endif

# ifdef _WIN32

    # include <direct.h>
    # include <windows.h>
    # include <io.h>
    # include <fileapi.h>
    # include <sys/locking.h>
    # include <sys/utime.h>
    # include <fcntl.h>

    # ifndef FIND_FIRST_EX_LARGE_FETCH
        # define FIND_FIRST_EX_LARGE_FETCH 0x02
    # endif

    # ifndef mode_t
        typedef unsigned short mode_t;
    # endif

    static inline int win_S_ISDIR(mode_t mode) { return (mode & _S_IFDIR) != 0; }
    static inline int win_S_ISREG(mode_t mode) { return (mode & _S_IFREG) != 0; }
    static inline int win_S_ISLNK(mode_t mode) { (void) mode; return 0; }

    # ifndef S_ISDIR
        # define S_ISDIR(mode) win_S_ISDIR(mode)
    # endif

    # ifndef S_ISREG
        # define S_ISREG(mode) win_S_ISREG(mode)
    # endif

    # ifndef S_ISLNK
        # define S_ISLNK(mode) win_S_ISLNK(mode)
    # endif

    typedef struct _stati64    info_struct;
    typedef struct __utimbuf64 utime_struct;

    static const int exec_mode_flag = _S_IEXEC;

    static inline int get_stat(const char *s, info_struct *i)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wstati64(w, i);
        lmt_memory_free(w);
        return r;
    }

    static inline int mk_dir(const char *s)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wmkdir(w);
        lmt_memory_free(w);
        return r;
    }

    static inline int ch_dir(const char *s)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wchdir(w);
        lmt_memory_free(w);
        return r;
    }

    static inline int rm_dir(const char *s)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wrmdir(w);
        lmt_memory_free(w);
        return r;
    }

    /* POSIX symlink(target, linkpath) -> Symbolic Link */

    static inline int mk_symlink(const char *t, const char *f)
    {
        LPWSTR wt = aux_utf8_to_wide(t); /* Existing Target */
        LPWSTR wf = aux_utf8_to_wide(f); /* New Link Name  */
        /* Win32 takes (NewLink, ExistingTarget, Flags) */
        /* 0x2 = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE */
        int r = CreateSymbolicLinkW(wf, wt, 0x2) != 0;
        lmt_memory_free(wt);
        lmt_memory_free(wf);
        return r;
    }

    /* POSIX link(target, linkpath) -> Hard Link */

    static inline int mk_link(const char *t, const char *f)
    {
        LPWSTR wt = aux_utf8_to_wide(t); /* Existing Target */
        LPWSTR wf = aux_utf8_to_wide(f); /* New Link Name  */
        /* Win32 takes (NewLink, ExistingTarget, Reserved) */
        int r = CreateHardLinkW(wf, wt, NULL) != 0;
        lmt_memory_free(wt);
        lmt_memory_free(wf);
        return r;
    }

    static inline int ch_to_exec(const char *s, mode_t n)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wchmod(w, n);
        lmt_memory_free(w);
        return r;
    }

    static inline int set_utime(const char *s, utime_struct *b)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wutime64(w, b);
        lmt_memory_free(w);
        return r;
    }

# else

    /* POSIX */

    # include <unistd.h>
    # include <dirent.h>
    # include <fcntl.h>
    # include <sys/types.h>
    # include <utime.h>
    # include <sys/param.h>

    typedef struct stat   info_struct;
    typedef struct utimbuf utime_struct;

    static const mode_t exec_mode_flag = (S_IXUSR | S_IXGRP | S_IXOTH);

    static inline int get_stat(const char *s, info_struct *i)
    {
        return stat(s, i);
    }

    static inline int mk_dir(const char *s)
    {
        return mkdir(s, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
    }

    static inline int ch_dir(const char *s)
    {
        return chdir(s);
    }

    static inline int rm_dir(const char *s)
    {
        return rmdir(s);
    }

    static inline int mk_symlink(const char *target, const char *linkpath)
    {
        return symlink(target, linkpath) != -1;
    }

    static inline int mk_link(const char *target, const char *linkpath)
    {
        return link(target, linkpath) != -1;
    }

    static inline int ch_to_exec(const char *s, mode_t mode)
    {
        return chmod(s, mode);
    }

    static inline int set_utime(const char *s, utime_struct *b)
    {
        return utime(s, b);
    }

# endif

# include <lua.h>
# include <lauxlib.h>
# include <lualib.h>

/*
    This function changes the current directory.

    success = chdir(name)
*/

static int filelib_chdir(lua_State *L) {
    if (lua_type(L, 1) == LUA_TSTRING) {
        lua_pushboolean(L, ! ch_dir(luaL_checkstring(L, 1)));
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

/*
    This function returns the current directory or false.

    name = currentdir()
*/

# ifdef _WIN32

    static int filelib_currentdir(lua_State *L)
    {
        LPWSTR wpath = NULL;
        int size = 256;
        while (1) {
            LPWSTR temp = lmt_memory_realloc(wpath, size * sizeof(WCHAR));
            wpath = temp;
            if (! wpath) {
                lua_pushboolean(L, 0);
                break;
            } else if (_wgetcwd(wpath, size)) {
                char *path = aux_utf8_from_wide(wpath);
                lua_pushstring(L, path);
                lmt_memory_free(path);
                break;
            } else if (errno != ERANGE) {
                lua_pushboolean(L, 0);
                break;
            } else {
                size *= 2;
            }
        }
        lmt_memory_free(wpath);
        return 1;
    }

# else

    static int filelib_currentdir(lua_State *L)
    {
        char *path = NULL;
        size_t size = MY_MAXPATHLEN;
        while (1) {
            char *next_path = lmt_memory_realloc(path, size);
            if (! next_path) {
                if (path) {
                    lmt_memory_free(path); /* Free original buffer if realloc fails */
                }
                lua_pushboolean(L, 0);
                return 1;
            }
            path = next_path;

            if (getcwd(path, size)) {
                lua_pushstring(L, path);
                lmt_memory_free(path); /* Clean up allocated buffer before returning */
                return 1;
            }

            if (errno != ERANGE) {
                lmt_memory_free(path);
                lua_pushboolean(L, 0);
                return 1;
            }
            size *= 2;
        }
    }

# endif

/*
    This functions create a link:

    success = link(target,name,[true=symbolic])
    success = symlink(target,name)
*/

static int filelib_link(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TSTRING && lua_type(L, 2) == LUA_TSTRING) {
        const char *oldpath = lua_tostring(L, 1);
        const char *newpath = lua_tostring(L, 2);
        lua_pushboolean(L, lua_toboolean(L, 3) ? mk_symlink(oldpath, newpath) : mk_link(oldpath, newpath));
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

static int filelib_symlink(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TSTRING && lua_type(L, 2) == LUA_TSTRING) {
        const char *oldpath = lua_tostring(L, 1);
        const char *newpath = lua_tostring(L, 2);
        lua_pushboolean(L, mk_symlink(oldpath, newpath));
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

/*
    This function creates a directory.

    success = mkdir(name)
*/

static int filelib_mkdir(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TSTRING) {
        lua_pushboolean(L, mk_dir(lua_tostring(L, 1)) != -1);
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

/*
    This function removes a directory (non-recursive).

    success = mkdir(name)
*/

static int filelib_rmdir(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TSTRING) {
        lua_pushboolean(L, rm_dir(luaL_checkstring(L, 1)) != -1);
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

static int filelib_moreweird(lua_State *L)
{
    static const char *weird = "~`!#$%^&*()={}[]:;\"'|<>,?\n\r\t";
    const char *str = lua_tostring(L, 1);
    lua_pushboolean(L, str ? str[0] == '.' || strpbrk(str, weird) != NULL : 1);
    return 1;
}

static int filelib_lessweird(lua_State *L)
{
    static const char *weird = "~`#$%^&*:;\"\'||<>,?\n\r\t";
    const char *str = lua_tostring(L, 1);
    lua_pushboolean(L, str ? str[0] == '.' || strpbrk(str, weird) != NULL : 1);
    return 1;
}

/*tex

    It happens that one can run into complaints about generating the \CONTEXT\ file databases,
    often by non \CONTEXT\ users. The arguments are kind of weird and make little sense when
    you consider what goes on. Also, it doesn't concern the normal distibution but a large
    setup like texlive. Here is a break down. The numbers are on a wsl subsystem so on a bare
    metal setup we have even less of an issue. In the \CONTEXT\ distribution we're talking about
    neglectable times, when disk I/O is cached, second digits after the comma.

    A 2026 tl setup has 241.000 files on about 17,600 directories and an |mtxrun --generate|
    takes 2.75 seconds (cached I/O on WSL) or 7.36 seconds (uncached disk I/O) and according
    to Gemini that is \quotation {an exceptionally strong result}.

    When we run |mktexlsr| after that, it needs 0.700 seconds to complete the job. It just
    lists the files and directories and where |mtxrun| prepares the dabase for lookups, that
    is delegated to kpse when \CONTEXT\ is not used. Also, serializing and compiling to
    bytecode add overhead. Actually, the 2 more seconds are used for that. We're using a 2018
    laptop (windows 11 with wsl) when testing this. Both the \LUA\ and plain text files that
    contain the information are quite large.

    When we do a cold run, that is: uncached disk I/O, a |mktexlsr| needs over 5 seconds which
    isn't nothing either. Now here is the thing. Say that we update texlive. If after an update
    we first run |mtxrun| than that one will will populate the cache, in our case we talk of
    directories and attributes. A later |mktexlsr| will benefit from that! If we first run
    |mktexlsr| that one does the heavy lifting and |mtxrun| might benefit. Order matters here
    and that is never taken into account when I read complaints. In fact, not running
    |mtxrun --generate| and delaying that actually makes it worse. If \CONTEXT, because it
    comes early in the alphabet is done first it is always the loser!

    A similar argument can be held for making formats. This also involves lookups and files
    and again caching plays a role here. Seeing what format generation does (logging) can give
    the impression of it being slow, and the few seconds needed then gain attention, while a
    hidden generation of other formats taking minutes seems to give the impression of speed.

    In the end, and this is the bottom line here, it all depends on order and caching, and the
    code involved in the scanning is not really the problem: it's efficient and fast. Messing
    with it, assuming an improvement, is asking for problems and obsuring the facts. It also
    sort of demonstrates ignorance. But then, reading about performance issues in \TEX\ and
    its ecosystem are often kind of weird (and off).

    As a side note: there are reasons why, right from the start of \LUATEX, and even in the
    former \PDFTEX\ times, in the \CONTEXT\ runners we found ways to optimize file lookups:
    there was time that pre-ssd disk access really forced us to get around the multi-second
    engine file lookups on large distributions. It actually is why we started with the
    minimal ones.

    The iterator provides the name, status and optionally (when we want detail) attributes.

*/

# ifdef _WIN32

    # include <windows.h>
    # include <fileapi.h>

    typedef struct dir_data {
        HANDLE           handle;
        int              closed;
        int              details;
        WIN32_FIND_DATAW fd;
        wchar_t          pattern[MY_MAXPATHLEN + 5];
    } dir_data;

    static inline void push_utf8_filename(lua_State *L, const wchar_t *wstr)
    {
        char utf8buf[MY_MAXPATHLEN * 4];
        /* WideCharToMultiByte returns 0 on failure (e.g. buffer overflow or bad UTF-16) */
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8buf, sizeof(utf8buf), NULL, NULL);
        if (len > 1) {
            lua_pushlstring(L, utf8buf, (size_t) (len - 1));
        } else {
            lua_pushliteral(L, "");
        }
    }

    static inline int push_entry(lua_State *L, const WIN32_FIND_DATAW *fd, int details)
    {
        push_utf8_filename(L, fd->cFileName);
        if (fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            lua_push_key(directory);
        } else {
            lua_push_key(file);
        }
        if (details) {
            /* convert FILETIME to 64-bit Unix epoch time */
            ULARGE_INTEGER ull;
            ull.LowPart = fd->ftLastWriteTime.dwLowDateTime;
            ull.HighPart = fd->ftLastWriteTime.dwHighDateTime;
            /* the magic number is 100 nanoseconds between 1601 and 1970 */
            lua_Integer mtime = (lua_Integer) ((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
            lua_Integer size = ((lua_Integer)fd->nFileSizeHigh << 32) | fd->nFileSizeLow;
            lua_pushinteger(L, size);
            lua_pushinteger(L, mtime);
            return 4;
        } else {
            return 2;
        }
    }

    static int filelib_aux_dir_next(lua_State *L, dir_data *d)
    {
        if (! d || d->closed) {
            return 0;
        }
        if (d->handle == INVALID_HANDLE_VALUE) {
            /* first entry */
            d->handle = FindFirstFileExW(
                d->pattern,
                FindExInfoBasic, /* skips short 8.3 filename retrieval */
                &d->fd,
                FindExSearchNameMatch,
                NULL,
                FIND_FIRST_EX_LARGE_FETCH /* instead of 0, is faster in a network or usb */
            );
            if (d->handle == INVALID_HANDLE_VALUE) {
                d->closed = 1;
                return 0;
            }
        } else {
            if (! FindNextFileW(d->handle, &d->fd)) {
                FindClose(d->handle);
                d->handle = INVALID_HANDLE_VALUE;
                d->closed = 1;
                return 0;
            }
        }
        while (d->fd.cFileName[0] == L'.' &&
              (d->fd.cFileName[1] == L'\0' || (d->fd.cFileName[1] == L'.' && d->fd.cFileName[2] == L'\0')))
        {
            if (! FindNextFileW(d->handle, &d->fd)) {
                FindClose(d->handle);
                d->handle = INVALID_HANDLE_VALUE;
                d->closed = 1;
                return 0;
            }
        }
        return push_entry(L, &d->fd, d->details);
    }

    static inline dir_data *filelib_aux_valid_dir(lua_State *L)
    {
        dir_data *d = (dir_data *) lua_touserdata(L, 1);
        if (d && lua_getmetatable(L, 1)) {
            lua_get_metatablelua(dir_handle_instance);
            if (! lua_rawequal(L, -1, -2)) {
               d = NULL;
            }
            lua_pop(L, 2);
        }
        return d;
    }

    static int filelib_aux_dir_iterator(lua_State *L)
    {
        dir_data *d = (dir_data *) lua_touserdata(L, lua_upvalueindex(1));
        return filelib_aux_dir_next(L, d);
    }

    static int filelib_aux_dir_method(lua_State *L)
    {
        dir_data *d = (dir_data *) luaL_checkudata(L, 1, DIR_HANDLE_INSTANCE);
 //     dir_data *d = filelib_aux_valid_dir(L);
        return d ? filelib_aux_dir_next(L, d) : 0;
    }

    static int filelib_aux_dir_close(lua_State *L)
    {
        dir_data *d = (dir_data *) luaL_checkudata(L, 1, DIR_HANDLE_INSTANCE);
 //     dir_data *d = filelib_aux_valid_dir(L);
        if (d && ! d->closed && d->handle != INVALID_HANDLE_VALUE) {
            FindClose(d->handle);
            d->handle = INVALID_HANDLE_VALUE;
        }
        if (d) {
            d->closed = 1;
        }
        return 0;
    }

    static int filelib_dir(lua_State *L)
    {
        const char *path = luaL_checkstring(L, 1);
        int details = (lua_type(L, 2) == LUA_TBOOLEAN) ? lua_toboolean(L, 2) : 1;
        dir_data *d = (dir_data *) lua_newuserdatauv(L, sizeof(dir_data), 0);
        lua_get_metatablelua(dir_handle_instance);
        lua_setmetatable(L, -2);
        d->closed  = 0;
        d->details = details;
        d->handle  = INVALID_HANDLE_VALUE;
        if (!path) {
            path = ".";
        }
        /* Convert path directly to wide string */
        int len = MultiByteToWideChar(CP_UTF8, 0, path, -1, d->pattern, MY_MAXPATHLEN);
        if (len <= 0) {
            luaL_error(L, "path conversion failed or too long: %s", path);
            return 0;
        }
        /* Remove trailing slashes (e.g., "foo/" or "foo\") before appending "\*" */
        while (len > 1 && (d->pattern[len - 2] == L'/' || d->pattern[len - 2] == L'\\')) {
            len--;
        }
        /* Append "\*" for FindFirstFileExW */
        d->pattern[len - 1] = L'\\';
        d->pattern[len]     = L'*';
        d->pattern[len + 1] = L'\0';
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, filelib_aux_dir_iterator, 1);
        lua_insert(L, -2);
        return 2;
    }

# else

    /* POSIX & Linux */
    # include <unistd.h>
    # include <dirent.h>
    # include <fcntl.h>
    # include <sys/types.h>

    typedef struct dir_data {
        DIR *handle;
        int  closed;
        int  details;
        int  dfd;
    } dir_data;

    static int filelib_aux_dir_next(lua_State *L, dir_data *d)
    {
        struct dirent *entry;
        if (! d || d->closed) {
            return 0;
        }
        while ((entry = readdir(d->handle)) != NULL) {
            if (entry->d_name[0] == '.' &&
               (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
                continue;
            }
            break;
        }
        if (! entry) {
            closedir(d->handle);
            d->closed = 1;
            d->handle = NULL;
            d->dfd    = -1;
            return 0;
        }
        lua_pushstring(L, entry->d_name);
        # ifdef _DIRENT_HAVE_D_TYPE
            if (! d->details) {
                if (entry->d_type == DT_DIR) {
                    lua_push_key(directory);
                    return 2;
                } else if (entry->d_type == DT_REG) {
                    lua_push_key(file);
                    return 2;
                }
            }
        # endif
        info_struct info;
        int stat_res = fstatat(d->dfd, entry->d_name, &info, 0);
        if (stat_res != 0) {
            /* target fails (e.g. broken link), inspect link itself */
            stat_res = fstatat(d->dfd, entry->d_name, &info, AT_SYMLINK_NOFOLLOW);
        }
        if (stat_res == 0) {
            if (S_ISDIR(info.st_mode)) {
                lua_push_key(directory);
            } else {
                lua_push_key(file);
            }
            if (d->details) {
                lua_pushinteger(L, (lua_Integer) info.st_size);
                lua_pushinteger(L, (lua_Integer) info.st_mtime);
                return 4;
            }
        } else {
            lua_pushnil(L);
        }
        return 2;
    }

    static inline dir_data *filelib_aux_valid_dir(lua_State *L)
    {
        dir_data *d = (dir_data *) lua_touserdata(L, 1);
        if (d && lua_getmetatable(L, 1)) {
            lua_get_metatablelua(dir_handle_instance);
            if (! lua_rawequal(L, -1, -2)) {
               d = NULL;
            }
            lua_pop(L, 2);
        }
        return d;
    }

    static int filelib_aux_dir_iterator(lua_State *L)
    {
        dir_data *d = (dir_data *) lua_touserdata(L, lua_upvalueindex(1));
        return filelib_aux_dir_next(L, d);
    }

    static int filelib_aux_dir_method(lua_State *L)
    {
        dir_data *d = (dir_data *) luaL_checkudata(L, 1, DIR_HANDLE_INSTANCE);
 //     dir_data *d = filelib_aux_valid_dir(L);
        return d ? filelib_aux_dir_next(L, d) : 0;
    }

    static int filelib_aux_dir_close(lua_State *L)
    {
        dir_data *d = (dir_data *) luaL_checkudata(L, 1, DIR_HANDLE_INSTANCE);
 //     dir_data *d = filelib_aux_valid_dir(L);
        if (d && ! d->closed && d->handle) {
            closedir(d->handle);
        }
        if (d) {
            d->closed = 1;
            d->handle = NULL;
            d->dfd    = -1;
        }
        return 0;
    }

    static int filelib_dir(lua_State *L)
    {
        const char *path = luaL_checkstring(L, 1);
        int details = (lua_type(L, 2) == LUA_TBOOLEAN) ? lua_toboolean(L, 2) : 1;
        dir_data *d = (dir_data *) lua_newuserdatauv(L, sizeof(dir_data), 0);
        lua_get_metatablelua(dir_handle_instance);
        lua_setmetatable(L, -2);
        d->closed  = 0;
        d->details = details;
        d->handle  = opendir(path ? path : ".");
        if (! d->handle) {
            luaL_error(L, "cannot open %s: %s", path, strerror(errno));
        }
        d->dfd = dirfd(d->handle);
        lua_pushvalue(L, -1);
        lua_pushcclosure(L, filelib_aux_dir_iterator, 1);
        lua_insert(L, -2);
        return 2;
    }

# endif

static int dir_create_meta(lua_State *L)
{
    luaL_newmetatable(L, DIR_HANDLE_INSTANCE);
    lua_newtable(L);
    lua_pushcfunction(L, filelib_aux_dir_method);
    lua_setfield(L, -2, "next");
    lua_pushcfunction(L, filelib_aux_dir_close);
    lua_setfield(L, -2, "close");
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, filelib_aux_dir_close);
    lua_setfield(L, -2, "__gc");
    return 1;
}

static inline const char *mode2string(mode_t mode)
{
    if (S_ISREG(mode)) {
        return "file";
    } else if (S_ISDIR(mode)) {
        return "directory";
    } else if (S_ISLNK(mode)) {
        return "link";
    } else {
        return "other";
    }
}

# ifdef _WIN32

    static const char *perm2string(unsigned short mode)
    {
        static char perms[10] = "---------";
        for (int i = 0; i < 9; i++) {
            perms[i] = '-';
        }
        if (mode & _S_IREAD)  { perms[0] = 'r'; perms[3] = 'r'; perms[6] = 'r'; }
        if (mode & _S_IWRITE) { perms[1] = 'w'; perms[4] = 'w'; perms[7] = 'w'; }
        if (mode & _S_IEXEC)  { perms[2] = 'x'; perms[5] = 'x'; perms[8] = 'x'; }
        return perms;
    }

# else

    static const char *perm2string(mode_t mode)
    {
        static char perms[10] = "---------";
        for (int i = 0; i < 9; i++) {
            perms[i] = '-';
        }
        if (mode & S_IRUSR) perms[0] = 'r';
        if (mode & S_IWUSR) perms[1] = 'w';
        if (mode & S_IXUSR) perms[2] = 'x';
        if (mode & S_IRGRP) perms[3] = 'r';
        if (mode & S_IWGRP) perms[4] = 'w';
        if (mode & S_IXGRP) perms[5] = 'x';
        if (mode & S_IROTH) perms[6] = 'r';
        if (mode & S_IWOTH) perms[7] = 'w';
        if (mode & S_IXOTH) perms[8] = 'x';
        return perms;
    }

# endif

/*
    The next one sets access time and modification values for a file:

    utime(filename)                    : current, current
    utime(filename,acess)              : access, access
    utime(filename,acess,modification) : access, modification
*/

static int filelib_touch(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TSTRING) {
        const char *file = luaL_checkstring(L, 1);
        utime_struct utb, *buf;
        if (lua_gettop(L) == 1) {
            buf = NULL;
        } else {
            utb.actime = (time_t) luaL_optinteger(L, 2, 0);
            utb.modtime = (time_t) luaL_optinteger(L, 3, utb.actime);
            buf = &utb;
        }
        lua_pushboolean(L, set_utime(file, buf) != -1);
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

static void push_st_mode (lua_State *L, info_struct *info) { lua_pushstring (L,  mode2string (info->st_mode)); }
static void push_st_size (lua_State *L, info_struct *info) { lua_pushinteger(L, (lua_Integer) info->st_size);  }
static void push_st_mtime(lua_State *L, info_struct *info) { lua_pushinteger(L, (lua_Integer) info->st_mtime); }
static void push_st_atime(lua_State *L, info_struct *info) { lua_pushinteger(L, (lua_Integer) info->st_atime); }
static void push_st_ctime(lua_State *L, info_struct *info) { lua_pushinteger(L, (lua_Integer) info->st_ctime); }
static void push_st_perm (lua_State *L, info_struct *info) { lua_pushstring (L,  perm2string (info->st_mode)); }
static void push_st_nlink(lua_State *L, info_struct *info) { lua_pushinteger(L, (lua_Integer) info->st_nlink); }

typedef void (*push_info_struct_function) (lua_State *L, info_struct *info);

struct file_stat_members {
    const char                *name;
    push_info_struct_function  push;
};

static struct file_stat_members members[] = {
    { "mode",         push_st_mode  },
    { "size",         push_st_size  },
    { "modification", push_st_mtime },
    { "access",       push_st_atime },
    { "change",       push_st_ctime },
    { "permissions",  push_st_perm  },
    { "nlink",        push_st_nlink },
    { NULL,           NULL          },
};

/*
    Get file or symbolic link information. Returns a table or nil.
*/

static int filelib_attributes(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TSTRING) {
        info_struct info;
        const char *file = luaL_checkstring(L, 1);
        if (get_stat(file, &info)) {
            /* bad news */
        } else if (lua_isstring(L, 2)) {
            const char *member = lua_tostring(L, 2);
            for (int i = 0; members[i].name; i++) {
                if (strcmp(members[i].name, member) == 0) {
                    members[i].push(L, &info);
                    return 1;
                }
            }
        } else {
            lua_settop(L, 2);
            if (! lua_istable(L, 2)) {
                lua_createtable(L, 0, 6);
            }
            for (int i = 0; members[i].name; i++) {
                lua_pushstring(L, members[i].name);
                members[i].push(L, &info);
                lua_rawset(L, -3);
            }
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

static inline int filelib_check_path_type(lua_State *L, int access_mode, int check_dir)
{
    if (lua_type(L, 1) == LUA_TSTRING) {
        info_struct info;
        const char *name = lua_tostring(L, 1);
        if (get_stat(name, &info) == 0) {
            int match = check_dir ? S_ISDIR(info.st_mode)
                                  : (S_ISREG(info.st_mode) || S_ISLNK(info.st_mode));
            lua_pushboolean(L, match && (access(name, access_mode) == 0));
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int filelib_isdir          (lua_State *L) { return filelib_check_path_type(L, F_OK, 1); }
static int filelib_isreadabledir  (lua_State *L) { return filelib_check_path_type(L, R_OK, 1); }
static int filelib_iswriteabledir (lua_State *L) { return filelib_check_path_type(L, W_OK, 1); }

static int filelib_isfile         (lua_State *L) { return filelib_check_path_type(L, F_OK, 0); }
static int filelib_isreadablefile (lua_State *L) { return filelib_check_path_type(L, R_OK, 0); }
static int filelib_iswriteablefile(lua_State *L) { return filelib_check_path_type(L, W_OK, 0); }

static int filelib_setexecutable(lua_State *L)
{
    int ok = 0;
    if (lua_type(L, 1) == LUA_TSTRING) {
        info_struct info;
        const char *name = lua_tostring(L, 1);
        if (! get_stat(name, &info) && S_ISREG(info.st_mode)) {
            if (ch_to_exec(name, info.st_mode | exec_mode_flag)) {
                /* the setting failed */
            } else {
                ok = 1;
            }
        } else {
            /* not a valid file */
        }
    }
    lua_pushboolean(L, ok);
    return 1;
}

static int filelib_symlinktarget(lua_State *L)
{
    const char *file = aux_utf8_readlink(luaL_checkstring(L, 1));
    if (file) {
        lua_pushstring(L, file);
    } else { 
        lua_pushnil(L);
    }
    return 1;
}

static int filelib_expandpath(lua_State *L)
{
    const char *file = aux_utf8_expandpath(luaL_checkstring(L, 1));
    if (file) {
        lua_pushstring(L, file);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static int filelib_canonicalize(lua_State *L)
{
    const char *file = aux_utf8_canonicalize(luaL_checkstring(L, 1));
    if (file) {
        lua_pushstring(L, file);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static const struct luaL_Reg filelib_function_list[] = {
    { "attributes",      filelib_attributes        },
    { "chdir",           filelib_chdir             },
    { "currentdir",      filelib_currentdir        },
    { "dir",             filelib_dir               },
    { "mkdir",           filelib_mkdir             },
    { "rmdir",           filelib_rmdir             },
    { "touch",           filelib_touch             },
    { "expandpath",      filelib_expandpath        },
    { "canonicalize",    filelib_canonicalize      },
    /* */
    { "link",            filelib_link              },
    { "symlink",         filelib_symlink           },
    { "setexecutable",   filelib_setexecutable     },
    { "symlinktarget",   filelib_symlinktarget     },
    /* */
    { "isdir",           filelib_isdir             },
    { "isfile",          filelib_isfile            },
    { "iswriteabledir",  filelib_iswriteabledir    },
    { "iswriteablefile", filelib_iswriteablefile   },
    { "isreadabledir",   filelib_isreadabledir     },
    { "isreadablefile",  filelib_isreadablefile    },
    /* */
    { "lessweird",       filelib_lessweird         },
    { "moreweird",       filelib_moreweird         },
    /* */
    { NULL,              NULL                      },
};

int luaopen_filelib(lua_State *L)
{
    dir_create_meta(L);
    luaL_newlib(L, filelib_function_list);
    return 1;
}