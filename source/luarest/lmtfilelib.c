/*

    See license.txt in the root of this project.

    This is a replacement for lfs, a file system manipulation library from the Kepler project. I
    started from the lfs.c file from luatex because we need to keep a similar interface. That
    file mentioned:

    Copyright Kepler Project 2003 - 2017 (http://keplerproject.github.io/luafilesystem)

    The original library offers the following functions:

        lfs.attributes(filepath [, attributename | attributetable])
        lfs.chdir(path)
        lfs.currentdir()
        lfs.dir(path)
        lfs.link(old, new[, symlink])
     -- lfs.lock(fh, mode)
     -- lfs.lock_dir(path)
        lfs.mkdir(path)
        lfs.rmdir(path)
     -- lfs.setmode(filepath, mode)
        lfs.symlinkattributes(filepath [, attributename])
        lfs.touch(filepath [, atime [, mtime]])
     -- lfs.unlock(fh)

    We have additional code in other modules and the code was already adapted a little. In the
    meantime the code looks quite different.

    Because \TEX| is multi-platform we try to provide a consistent interface. So, for instance
    block size and inode number are not relevant for us, nor are user and group ids. The lock
    functions have been removed as they serve no purpose in a \TEX\ system and devices make no
    sense either. The iterator could be improved. I also fixed some abnormalities. Permissions are
    not useful either.

*/

# include "../lua/lmtinterface.h"
# include "../utilities/auxmemory.h"
# include "../utilities/auxfile.h"

# ifndef R_OK
# define F_OK 0x0
# define W_OK 0x2
# define R_OK 0x4
# endif

// # define DIR_METATABLE "file.directory"

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

// # ifndef _LARGEFILE64_SOURCE
    # define _LARGEFILE64_SOURCE 1
// # endif

# include <errno.h>
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# include <time.h>
# include <sys/stat.h>

// # ifdef _MSC_VER
//     # ifndef MAX_PATH
//         # define MAX_PATH 256
//     # endif
// # endif

/*tex 
    See |lmtinterface.h| for |DIR_HANDLE_INSTANCE|. here it's not really needed to have a high 
    performance lookup but we are consistent so let's have it. We seldom have more than a few 
    tens of thousands iterations. 
*/

# ifdef _WIN32

    # include <direct.h>
    # include <windows.h>
    # include <io.h>
    # include <fileapi.h>
    # include <sys/locking.h>
    # include <sys/utime.h>
    # include <fcntl.h>

    /* 
        Todo MS Windows: By default, the name is limited to MAX_PATH characters. To extend this 
        limit to 32,767 wide characters, prepend "\\?\" to the path. For more information, see 
        Naming Files, Paths, and Namespaces.
    */

    # ifdef MAX_PATH
        # define MY_MAXPATHLEN MAX_PATH
    # else 
        # define MY_MAXPATHLEN 255
    # endif 

# else

    /* the next one is sensitive for c99 */

    # include <unistd.h>
    # include <dirent.h>
    # include <fcntl.h>
    # include <sys/types.h>
    # include <utime.h>
    # include <sys/param.h>

    # ifdef MAXPATHLEN
        # define MY_MAXPATHLEN MAXPATHLEN
    # else 
        # define MY_MAXPATHLEN 255
    # endif 

# endif

/* This has to go to the h file. See luainit.c where it's also needed. */

# ifdef _WIN32

    # ifndef S_ISDIR
        # define S_ISDIR(mode) (mode & _S_IFDIR)
    # endif

    # ifndef S_ISREG
        # define S_ISREG(mode) (mode & _S_IFREG)
    # endif

    # ifndef S_ISLNK
        # define S_ISLNK(mode) (0)
    # endif

    # ifndef S_ISSUB
        # define S_ISSUB(mode) (file_data.attrib & _A_SUBDIR)
    # endif

    # define info_struct  struct _stati64
    # define utime_struct struct __utimbuf64

    # define exec_mode_flag  _S_IEXEC

    /*
        There is a difference between msvc and mingw wrt the daylight saving time correction being
        applied toy the times. I couldn't figure it out and don't want to waste more time on it.
    */

    /* 
        A windows path should not end with a / so maybe we should check for that and remove it when
        we have one. Even better is to add a period. 

        size_t l = wcslen(w) - 1;
        if (w[l] == L'/') {
            w[l] == L'\0');
        }
    */

    static int get_stat(const char *s, info_struct *i)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wstati64(w, i);
        lmt_memory_free(w);
        return r;
    }

    static int mk_dir(const char *s)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wmkdir(w);
        lmt_memory_free(w);
        return r;
    }

    static int ch_dir(const char *s)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wchdir(w);
        lmt_memory_free(w);
        return r;
    }

    static int rm_dir(const char *s)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wrmdir(w);
        lmt_memory_free(w);
        return r;
    }

 // # if defined(__MINGW64__) || defined(__MINGW32__)
 //     extern int CreateSymbolicLinkW(LPCWSTR lpSymlinkFileName, LPCWSTR lpTargetFileName, DWORD dwFlags);
 // # endif 

    static int mk_symlink(const char *t, const char *f)
    {
        LPWSTR wt = aux_utf8_to_wide(t);
        LPWSTR wf = aux_utf8_to_wide(f);
        int r = CreateSymbolicLinkW((LPCWSTR) t, (LPCWSTR) f, 0x2) != 0;
        lmt_memory_free(wt);
        lmt_memory_free(wf);
        return r;
    }

    static int mk_link(const char *t, const char *f)
    {
        LPWSTR wt = aux_utf8_to_wide(t);
        LPWSTR wf = aux_utf8_to_wide(f);
        int r = CreateSymbolicLinkW((LPCWSTR) t, (LPCWSTR) f, 0x3) != 0;
        lmt_memory_free(wt);
        lmt_memory_free(wf);
        return r;
    }

    static int ch_to_exec(const char *s, int n)
    {
        LPWSTR w = aux_utf8_to_wide(s);
        int r = _wchmod(w, n);
        lmt_memory_free(w);
        return r;
    }

 // # ifdef _MSC_VER
 //
 //     static int set_utime(const char *s, utime_struct *b)
 //     {
 //         LPWSTR w = utf8_to_wide(s);
 //         HANDLE h = CreateFileW(w, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
 //         int r = -1;
 //         lmt_memory_free(w);
 //         if (h != INVALID_HANDLE_VALUE) {
 //             r = SetFileTime(h, (const struct _FILETIME *) b, (const struct _FILETIME *) b, (const struct _FILETIME *) b);
 //             CloseHandle(h);
 //         }
 //         return r;
 //     }
 //
 // # else

        static int set_utime(const char *s, utime_struct *b)
        {
            LPWSTR w = aux_utf8_to_wide(s);
            int r = _wutime64(w, b);
            lmt_memory_free(w);
            return r;
        }

 // # endif

# else

    # define info_struct     struct stat
    # define utime_struct    struct utimbuf

    # define get_stat        stat
    # define mk_dir(p)       (mkdir((p), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH))
    # define ch_dir          chdir
    # define get_cwd         getcwd
    # define rm_dir          rmdir
    # define mk_symlink(f,t) (symlink(f,t) != -1)
    # define mk_link(f,t)    (link(f,t) != -1)
    # define ch_to_exec(f,n) (chmod(f,n))
    # define exec_mode_flag  S_IXUSR | S_IXGRP | S_IXOTH
    # define set_utime(f,b)  utime(f,b)

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
            path = path ? lmt_memory_realloc(path, size) : lmt_memory_malloc(size);
            if (! path) {
                lua_pushboolean(L,0);
                break;
            }
            if (get_cwd(path, size)) {
                lua_pushstring(L, path);
                break;
            }
            if (errno != ERANGE) {
                lua_pushboolean(L,0);
                break;
            }
            size *= 2;
        }
        if (path) {
            lmt_memory_free(path);
        }
        return 1;
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

    The directory iterator returns multiple values:

    for name, mode, size, mtime in dir(path) do ... end

*/

/*tex

    On unix we cannot get the size and time in one go without interference. Also, not all file
    systems return this field. So eventually we might not do this on unix and revert to the
    slower method at the lua end when DT_DIR is undefined. After a report from the mailing
    list about symbolic link issues this is what Taco and I came up with. The |_less| variant
    is mainly there because in \UNIX\ we then can avoid a costly |stat| when we don't need the
    details (only a symlink demands such a |stat|).

*/

/*tex

    For quite a while we used the first variant but when I asked Gemini it became clear that
    we can assume the more efficient accessors to be available. Most linux file systems have
    fast stat methods and in windows we can get the utf name more efficient and can avoid 8.3
    handling. So, per 2026-08 we upgraded the iterator.

    Another (small) optimization is to not pass |.| and |..| to the scanner because these make
    no sense. It saves a stat and some allocations.
*/

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

*/

# if 0

  # ifdef _WIN32

    typedef struct dir_data {
        intptr_t handle;
        int      closed;
        int      details;
        char     pattern[MY_MAXPATHLEN+5];
    } dir_data;

    static inline int push_entry(lua_State *L, struct _wfinddata_t file_data, int details)
    {
        char *s = aux_utf8_from_wide(file_data.name);
        lua_pushstring(L, s);
        lmt_memory_free(s);
        if (S_ISSUB(file_data.attrib)) {
            lua_push_key(directory);
        } else {
            lua_push_key(file);
        }
        if (details) {
            lua_pushinteger(L, file_data.size);
            lua_pushinteger(L, file_data.time_write);
            return 4;
        } else {
            return 2;
        }
    }

    static int filelib_aux_dir_iterator(lua_State *L)
    {
        struct _wfinddata_t file_data;
        int details;
     // dir_data *d = (dir_data *) luaL_checkudata(L, 1, DIR_METATABLE);
        dir_data *d = (dir_data *) lua_touserdata(L, 1);
        if (d && lua_getmetatable(L, 1)) {
            lua_get_metatablelua(dir_handle_instance);
            if (! lua_rawequal(L, -1, -2)) {
               d = NULL;
            }
            lua_pop(L, 2);
        }
        if (! d) { 
            /* some fatal error */
            return 0;
        }
        details = d->details;
        luaL_argcheck(L, d->closed == 0, 1, "closed directory");
        if (d->handle == 0L) {
            /* first entry */
            LPWSTR s = aux_utf8_to_wide(d->pattern);
            if ((d->handle = _wfindfirst(s, &file_data)) == -1L) {
                d->closed = 1;
                lmt_memory_free(s);
                return 0;
            } else {
                lmt_memory_free(s);
                return push_entry(L, file_data, details);
            }
        } else if (_wfindnext(d->handle, &file_data) == -1L) {
            /* no more entries */
            /* lmt_memory_free(d->handle); */ /* is done for us */
            _findclose(d->handle);
            d->closed = 1;
            return 0;
        } else {
            /* successive entries */
            return push_entry(L, file_data, details);
        }
    }

    static int filelib_aux_dir_close(lua_State *L)
    {
        dir_data *d = (dir_data *) lua_touserdata(L, 1);
        if (! d->closed && d->handle) {
            _findclose(d->handle);
        }
        d->closed = 1;
        return 0;
    }

    static int filelib_dir(lua_State *L)
    {
        const char *path = luaL_checkstring(L, 1);
        int details = lua_type(L, 2) == LUA_TBOOLEAN ? lua_toboolean(L, 2) : 1;
        dir_data *d;
        lua_pushcfunction(L, filelib_aux_dir_iterator);
        d = (dir_data *) lua_newuserdatauv(L, sizeof(dir_data), 0);
        lua_get_metatablelua(dir_handle_instance);
        lua_setmetatable(L, -2);
        d->closed  = 0;
        d->details = details;
        d->handle  = 0L;
        if (strlen(path) > MY_MAXPATHLEN-2) {
            luaL_error(L, "path too long: %s", path);
        } else {
            sprintf(d->pattern, "%s/*", path); /* brrr */
        }
        return 2;
    }

  # else

    typedef struct dir_data {
        DIR  *handle;
        int   closed;
        int   details;
        char  pattern[MY_MAXPATHLEN+1];
    } dir_data;

    static int filelib_aux_dir_iterator(lua_State *L)
    {
        struct dirent *entry;
        dir_data *d;
        int details;
    //  d = (dir_data *) luaL_checkudata(L, 1, DIR_HANDLE_INSTANCE);
        d = (dir_data *) lua_touserdata(L, 1);
        if (d && lua_getmetatable(L, 1)) {
            lua_get_metatablelua(dir_handle_instance);
            if (! lua_rawequal(L, -1, -2)) {
               d = NULL;
            }
            lua_pop(L, 2);
        }
        if (! d) { 
            /* some fatal error */
            return 0;
        }
        details = d->details;
        luaL_argcheck(L, d->closed == 0, 1, "closed directory");
        entry = readdir (d->handle);
        if (entry) {
            lua_pushstring(L, entry->d_name);
            # ifdef _DIRENT_HAVE_D_TYPE
                if (! details) {
                    if (entry->d_type == DT_DIR) {
                        lua_push_key(directory);
                        return 2;
                    } else if (entry->d_type == DT_REG) {
                        lua_push_key(file);
                        return 2;
                    }
                }
            # endif
            /*tex We can have a symlink and/or we need the details an dfor both we need to |get_stat|. */
            {
                info_struct info;
                char file_path[2*MY_MAXPATHLEN];
                snprintf(file_path, 2*MY_MAXPATHLEN, "%s/%s", d->pattern, entry->d_name);
                if (! get_stat(file_path, &info)) {
                    if (S_ISDIR(info.st_mode)) {
                        lua_push_key(directory);
                    } else if (S_ISREG(info.st_mode) || S_ISLNK(info.st_mode)) {
                        lua_push_key(file);
                    } else {
                        lua_pushnil(L);
                        return 2;
                    }
                    if (details) {
                        lua_pushinteger(L, info.st_size);
                        lua_pushinteger(L, info.st_mtime);
                        return 4;
                    }
                } else {
                    lua_pushnil(L);
                }
                return 2;
            }
        } else {
            closedir(d->handle);
            d->closed = 1;
            return 0;
        }
    }

    static int filelib_aux_dir_close(lua_State *L)
    {
        dir_data *d = (dir_data *) lua_touserdata(L, 1);
        if (!d->closed && d->handle) {
            closedir(d->handle);
        }
        d->closed = 1;
        return 0;
    }

    static int filelib_dir(lua_State *L)
    {
        const char *path = luaL_checkstring(L, 1);
        int details = lua_type(L, 2) == LUA_TBOOLEAN ? lua_toboolean(L, 2) : 1;
        dir_data *d;
        lua_pushcfunction(L, filelib_aux_dir_iterator);
        d = (dir_data *) lua_newuserdatauv(L, sizeof(dir_data), 0);
        lua_get_metatablelua(dir_handle_instance);
        lua_setmetatable(L, -2);
        d->closed = 0;
        d->details = details;
        d->handle = opendir(path);
        if (! d->handle) {
            luaL_error(L, "cannot open %s: %s", path, strerror(errno));
        }
        snprintf(d->pattern, MY_MAXPATHLEN, "%s", path);
        return 2;
    }

  # endif

# else

  # ifdef _WIN32

    # include <windows.h>
    # include <fileapi.h>

    # ifdef MAX_PATH
        # define MY_MAXPATHLEN MAX_PATH
    # else
        # define MY_MAXPATHLEN 255
    # endif

    typedef struct dir_data {
        HANDLE           handle;
        int              closed;
        int              details;
        WIN32_FIND_DATAW fd;
        wchar_t          pattern[MY_MAXPATHLEN + 5];
    } dir_data;

    /* helper to convert wide string to UTF-8 using stack allocation */

    static inline void push_utf8_filename(lua_State *L, const wchar_t *wstr) {
        char utf8buf[MY_MAXPATHLEN * 4];
        int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8buf, sizeof(utf8buf), NULL, NULL);
        if (len > 0) {
            lua_pushlstring(L, utf8buf, len - 1);
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
            lua_Integer mtime = (lua_Integer) ((ull.QuadPart - 116444736000000000ULL) / 10000000ULL);
            /* combine high/low size bytes */
            lua_Integer size = ((lua_Integer)fd->nFileSizeHigh << 32) | fd->nFileSizeLow;
            lua_pushinteger(L, size);
            lua_pushinteger(L, mtime);
            return 4;
        } else {
            return 2;
        }
    }

    static int filelib_aux_dir_iterator(lua_State *L)
    {
        dir_data *d = (dir_data *) lua_touserdata(L, 1);
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
                0
            );
            if (d->handle == INVALID_HANDLE_VALUE) {
                d->closed = 1;
                return 0;
            }
        } else {
            /* next entries */
            if (! FindNextFileW(d->handle, &d->fd)) {
                FindClose(d->handle);
                d->handle = INVALID_HANDLE_VALUE;
                d->closed = 1;
                return 0;
            }
        }
        /* Fast check to filter out "." and ".." */
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

    static int filelib_aux_dir_close(lua_State *L)
    {
        dir_data *d = (dir_data *) lua_touserdata(L, 1);
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
        lua_pushcfunction(L, filelib_aux_dir_iterator);
        dir_data *d = (dir_data *) lua_newuserdatauv(L, sizeof(dir_data), 0);
        lua_get_metatablelua(dir_handle_instance);
        lua_setmetatable(L, -2);
        d->closed  = 0;
        d->details = details;
        d->handle  = INVALID_HANDLE_VALUE;
        char pattern_utf8[MY_MAXPATHLEN + 5];
        if (path && strlen(path) > MY_MAXPATHLEN - 3) {
            luaL_error(L, "path too long: %s", path);
        } else {
            snprintf(pattern_utf8, sizeof(pattern_utf8), "%s/*", path ? path : ".");
            MultiByteToWideChar(CP_UTF8, 0, pattern_utf8, -1, d->pattern, MY_MAXPATHLEN + 5);
        }
        return 2;
    }

  # else

    /* POSIX & Linux */

    # include <unistd.h>
    # include <dirent.h>
    # include <fcntl.h>
    # include <sys/types.h>

    # ifdef MAXPATHLEN
        # define MY_MAXPATHLEN MAXPATHLEN
    # else
        # define MY_MAXPATHLEN 255
    # endif

    # define info_struct struct stat

    typedef struct dir_data {
        DIR *handle;
        int  closed;
        int  details;
        int  dfd;     /* file descriptor for fstatat */
    } dir_data;

    static int filelib_aux_dir_iterator(lua_State *L)
    {
        struct dirent *entry;
        dir_data *d = (dir_data *) lua_touserdata(L, 1);
        if (! d || d->closed) {
            return 0;
        }
        /* Loop until we find an entry that is NOT "." or ".." */
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
            return 0;
        }
        lua_pushstring(L, entry->d_name);
        # ifdef _DIRENT_HAVE_D_TYPE
            /* fast: avoid stat if we don't need details and type is known */
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
        /* slower: fstatat relative lookup (avoids snprintf overhead) */
        info_struct info;
        if (fstatat(d->dfd, entry->d_name, &info, 0) == 0) {
            if (S_ISDIR(info.st_mode)) {
                lua_push_key(directory);
            } else if (S_ISREG(info.st_mode) || S_ISLNK(info.st_mode)) {
                lua_push_key(file);
            } else {
                lua_pushnil(L);
                return 2;
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

    static int filelib_aux_dir_close(lua_State *L)
    {
        dir_data *d = (dir_data *) lua_touserdata(L, 1);
        if (d && ! d->closed && d->handle) {\
            /* closedir also closes internal fd */
            closedir(d->handle);
            d->handle = NULL;
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
        lua_pushcfunction(L, filelib_aux_dir_iterator);
        dir_data *d = (dir_data *) lua_newuserdatauv(L, sizeof(dir_data), 0);
        lua_get_metatablelua(dir_handle_instance);
        lua_setmetatable(L, -2);
        d->closed  = 0;
        d->details = details;
        d->handle  = opendir(path ? path : ".");
        if (! d->handle) {
            luaL_error(L, "cannot open %s: %s", path, strerror(errno));
        }
        /* obtain file descriptor for lightweight fstatat operations */
        d->dfd = dirfd(d->handle);
        return 2;
    }

  # endif

# endif

static int dir_create_meta(lua_State *L)
{
    luaL_newmetatable(L, DIR_HANDLE_INSTANCE);
    lua_newtable(L);
    lua_pushcfunction(L, filelib_aux_dir_iterator);
    lua_setfield(L, -2, "next");
    lua_pushcfunction(L, filelib_aux_dir_close);
    lua_setfield(L, -2, "close");
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, filelib_aux_dir_close);
    lua_setfield(L, -2, "__gc");
    return 1;
}

# define mode2string(mode) \
    ((S_ISREG(mode)) ? "file" : ((S_ISDIR(mode)) ? "directory" : ((S_ISLNK(mode)) ? "link" : "other")))

/* We keep this for a while: will change to { r, w, x hash }  */

# ifdef _WIN32

    static const char *perm2string(unsigned short mode)
    {
        static char perms[10] = "---------";
        /* persistent change hence the for loop */
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
        /* persistent change hence the for loop */
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

static void push_st_mode (lua_State *L, info_struct *info) { lua_pushstring (L,  mode2string (info->st_mode)); } /* inode protection mode */
static void push_st_size (lua_State *L, info_struct *info) { lua_pushinteger(L, (lua_Integer) info->st_size);  } /* file size, in bytes */
static void push_st_mtime(lua_State *L, info_struct *info) { lua_pushinteger(L, (lua_Integer) info->st_mtime); } /* time of last data modification */
static void push_st_atime(lua_State *L, info_struct *info) { lua_pushinteger(L, (lua_Integer) info->st_atime); } /* time of last access */
static void push_st_ctime(lua_State *L, info_struct *info) { lua_pushinteger(L, (lua_Integer) info->st_ctime); } /* time of last file status change */
static void push_st_perm (lua_State *L, info_struct *info) { lua_pushstring (L,  perm2string (info->st_mode)); } /* permissions string */
static void push_st_nlink(lua_State *L, info_struct *info) { lua_pushinteger(L, (lua_Integer) info->st_nlink); } /* number of hard links to the file */

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

# define is_whatever(L,IS_OK,okay) do { \
    if (lua_type(L, 1) == LUA_TSTRING) { \
        info_struct info; \
        const char *name = lua_tostring(L, 1); \
        if (get_stat(name, &info)) { \
            lua_pushboolean(L, 0); \
        } else { \
            lua_pushboolean(L, okay && ! access(name, IS_OK)); \
        } \
    } else { \
        lua_pushboolean(L, 0); \
    } \
    return 1; \
} while(1)

static int filelib_isdir          (lua_State *L) { is_whatever(L, F_OK, (S_ISDIR(info.st_mode))); }
static int filelib_isreadabledir  (lua_State *L) { is_whatever(L, R_OK, (S_ISDIR(info.st_mode))); }
static int filelib_iswriteabledir (lua_State *L) { is_whatever(L, W_OK, (S_ISDIR(info.st_mode))); }

static int filelib_isfile         (lua_State *L) { is_whatever(L, F_OK, (S_ISREG(info.st_mode) || S_ISLNK(info.st_mode))); }
static int filelib_isreadablefile (lua_State *L) { is_whatever(L, R_OK, (S_ISREG(info.st_mode) || S_ISLNK(info.st_mode))); }
static int filelib_iswriteablefile(lua_State *L) { is_whatever(L, W_OK, (S_ISREG(info.st_mode) || S_ISLNK(info.st_mode))); }

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

/*
    Push the symlink target to the top of the stack. Assumes the file name is at position 1 of the
    stack. Returns 1 if successful (with the target on top of the stack), 0 on failure (with stack
    unchanged, and errno set).

    link("name")          : table
    link("name","target") : targetname
*/

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
