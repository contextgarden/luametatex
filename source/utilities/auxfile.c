/*
    See license.txt in the root of this project.
*/

# include <stdio.h>
# include <sys/stat.h>
# include <stdbool.h>

# include "auxfile.h"
# include "auxmemory.h"

# ifndef PATH_MAX
#     ifdef MAX_PATH
#         define PATH_MAX MAX_PATH
#     else
#         define PATH_MAX 260
#     endif
# endif

# ifdef _WIN32

    # include <windows.h>
    # include <ctype.h>
    # include <io.h>
    # include <shellapi.h>
    # include <fileapi.h>

    static void aux_utf8_finalize_path_slashes(char *path)
    {
        if (! path) {
            return;
        }
        bool is_unc = (path[0] == '\\' || path[0] == '/') && (path[1] == '\\' || path[1] == '/');
        if (is_unc) {
            for (char *p = path; *p != '\0'; p++) {
                if (*p == '/') *p = '\\';
            }
        } else {
            for (char *p = path; *p != '\0'; p++) {
                if (*p == '\\') *p = '/';
            }
        }
    }

    LPWSTR aux_utf8_to_wide(const char *utf8str)
    {
        if (utf8str) {
            int    length = MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, NULL, 0); /* preroll */
            LPWSTR wide   = (LPWSTR) lmt_memory_malloc(sizeof(WCHAR) * length);
            MultiByteToWideChar(CP_UTF8, 0, utf8str, -1, wide, length);
            return wide;
        } else {
            return NULL;
        }
    }

    char *aux_utf8_from_wide(LPWSTR widestr)
    {
        if (widestr) {
            int   length  = WideCharToMultiByte(CP_UTF8, 0, widestr, -1, NULL, 0, NULL, NULL);
            char *utf8str = (char *) lmt_memory_malloc(sizeof(char) * length);
            WideCharToMultiByte(CP_UTF8, 0, widestr, -1, utf8str, length, NULL, NULL);
            return (char *) utf8str;
        } else {
            return NULL;
        }
    }

    FILE *aux_utf8_fopen(const char *path, const char *mode)
    {
        if (path && mode) {
            LPWSTR  wpath = aux_utf8_to_wide(path);
            LPWSTR  wmode = aux_utf8_to_wide(mode);
            FILE   *f     = _wfopen(wpath,wmode);
            lmt_memory_free(wpath);
            lmt_memory_free(wmode);
            return f;
        } else {
            return NULL;
        }
    }

    FILE *aux_utf8_popen(const char *path, const char *mode)
    {
        if (path && mode) {
            LPWSTR  wpath = aux_utf8_to_wide(path);
            LPWSTR  wmode = aux_utf8_to_wide(mode);
            FILE   *f     = _wpopen(wpath,wmode);
            lmt_memory_free(wpath);
            lmt_memory_free(wmode);
            return f;
        } else {
            return NULL;
        }
    }

    int aux_utf8_system(const char *cmd)
    {
        LPWSTR wcmd   = aux_utf8_to_wide(cmd);
        int    result = _wsystem(wcmd);
        lmt_memory_free(wcmd);
        return result;
    }

    int aux_utf8_remove(const char *name)
    {
        LPWSTR wname  = aux_utf8_to_wide(name);
        int    result = _wremove(wname);
        lmt_memory_free(wname);
        return result;
    }

    int aux_utf8_rename(const char *oldname, const char *newname)
    {
        LPWSTR woldname = aux_utf8_to_wide(oldname);
        LPWSTR wnewname = aux_utf8_to_wide(newname);
        int    result   = _wrename(woldname, wnewname);
        lmt_memory_free(woldname);
        lmt_memory_free(wnewname);
        return result;
    }

    int aux_utf8_setargv(char * **av, char **argv, int argc)
    {
        *av = NULL;
        if (argv) {
            int c = 0;
            LPWSTR *l = CommandLineToArgvW(GetCommandLineW(), &c);
            if (l != NULL) {
                char **v = lmt_memory_malloc(sizeof(char *) * c);
                if (v != NULL) {
                    for (int i = 0; i < c; i++) {
                        v[i] = aux_utf8_from_wide(l[i]);
                    }
                    *av = v;
                    /*tex Let's be nice with path names: |c:\\foo\\etc| */
                    if (c > 1) {
                        if ((strlen(v[c-1]) > 2) && isalpha(v[c-1][0]) && (v[c-1][1] == ':') && (v[c-1][2] == '\\')) {
                            for (char *p = v[c-1]+2; *p; p++) {
                                if (*p == '\\') {
                                    *p = '/';
                                }
                            }
                        }
                    }
                }
                LocalFree(l);
            }
            return c;
        } else {
            return argc;
        }
    }

    char *aux_utf8_getownpath(const char *file)
    {
        if (file) {
            char *path = NULL;
            char  buffer[MAX_PATH];
            GetModuleFileName(NULL, buffer, sizeof(buffer));
            path = lmt_memory_strdup(buffer);
            if (path && strlen(path) > 0) {
                for (size_t i = 0; i < strlen(path); i++) {
                    if (path[i] == '\\') {
                        path[i] = '/';
                    }
                }
                return path;
            }
        }
        return lmt_memory_strdup(".");
    }

    /*tex We always return a copy so that we're consistent with windows/unix. */

 // # if defined(__MINGW64__) || defined(__MINGW32__)
 //     extern DWORD GetFinalPathNameByHandleW(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags);
 // # endif

    char *aux_utf8_readlink(const char *file)
    {
        LPWSTR wide   = aux_utf8_to_wide(file);
        HANDLE handle = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        char *link = NULL;
        if (handle != INVALID_HANDLE_VALUE) {
            LPWSTR path = (LPWSTR) lmt_memory_malloc((MAX_PATH+1) * sizeof(WCHAR));
            DWORD  size = GetFinalPathNameByHandleW(handle, path, MAX_PATH, VOLUME_NAME_NT);
            if (size > 0 && size < MAX_PATH) {
                path[size] = '\0';
                link = aux_utf8_from_wide(path);
            }
            lmt_memory_free(path);
            CloseHandle(handle);
        }
        lmt_memory_free(wide);
        return link ? link : lmt_memory_strdup(file);
    }

    /* Derived from a Gemini query: */

    char *aux_utf8_expandpath(const char *file)
    {
        if (! file) {
            return NULL;
        }
        LPWSTR wide = aux_utf8_to_wide(file);
        if (! wide) {
            return NULL;
        }
        char *result = NULL;
        DWORD required_size = GetFullPathNameW(wide, 0, NULL, NULL);
        if (required_size > 0) {
            LPWSTR path = (LPWSTR) lmt_memory_malloc((required_size + 1) * sizeof(WCHAR));
            DWORD  size = GetFullPathNameW(wide, required_size, path, NULL);
            if (size > 0 && size < required_size) {
                path[size] = L'\0';
                result = aux_utf8_from_wide(path);
            }
            lmt_memory_free(path);
        }
        lmt_memory_free(wide);
        if (! result) {
            result = lmt_memory_strdup(file);
        }
        aux_utf8_finalize_path_slashes(result);
        return result;
    }

    /* Derived from a Gemini query: */

    char *aux_utf8_canonicalize(const char *file)
    {
        if (! file) {
            return NULL;
        }
        LPWSTR wide = aux_utf8_to_wide(file);
        if (! wide) {
            return NULL;
        }
        HANDLE handle = CreateFileW(
            wide,
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            NULL
        );
        char *result = NULL;
        if (handle != INVALID_HANDLE_VALUE) {
            DWORD required_size = GetFinalPathNameByHandleW(handle, NULL, 0, VOLUME_NAME_DOS);
            if (required_size > 0) {
                LPWSTR path = (LPWSTR) lmt_memory_malloc((required_size + 1) * sizeof(WCHAR));
                DWORD  size = GetFinalPathNameByHandleW(handle, path, required_size, VOLUME_NAME_DOS);
                if (size > 0 && size < required_size) {
                    path[size] = L'\0';
                    LPWSTR clean_path = path;
                    if (wcsncmp(clean_path, L"\\\\?\\", 4) == 0) {
                        clean_path += 4;
                        if (wcsncmp(clean_path, L"UNC\\", 4) == 0) {
                            clean_path += 2;
                            clean_path[0] = L'\\';
                        }
                    }
                    result = aux_utf8_from_wide(clean_path);
                }
                lmt_memory_free(path);
            }
            CloseHandle(handle);
        }
        lmt_memory_free(wide);
        if (! result) {
            result = aux_utf8_expandpath(file);
        } else {
            aux_utf8_finalize_path_slashes(result);
        }
        return result;
    }

# else

    # include <string.h>
    # include <stdlib.h>
    # include <unistd.h>
    # include <limits.h>

    int aux_utf8_setargv(char * **av, char **argv, int argc)
    {
        *av = argv;
        return argc;
    }

    char *aux_utf8_getownpath(const char *file)
    {
        if (strchr(file, '/')) {
            return lmt_memory_strdup(file);
        } else {
            char       *searchpath = lmt_memory_strdup(getenv("PATH"));
            const char *index      = searchpath;
            char       *path       = NULL;
            if (index) {
                const char *esp;
                size_t totallen  = 0;
                size_t filelen   = strlen(file);
                size_t prefixlen = 0;
                do {
                    esp = strchr(index, ':');
                    if (esp) {
                        prefixlen = (size_t) (esp - index);
                    } else {
                        prefixlen = strlen(index);
                    }
                    if (prefixlen == 0 || index[prefixlen - 1] == '/') {
                        totallen = prefixlen + filelen;
# ifdef PATH_MAX
                        if (totallen >= PATH_MAX) {
                            continue;
                        }
# endif
                        path = lmt_memory_malloc(totallen + 1);
                        if (path) {
                            memcpy(path, index, prefixlen);
                            memcpy(path + prefixlen, file, filelen);
                        } else {
                            /*tex This is an error, unlikely, but checking makes compilers happy. */
                            goto OEPS;
                        }
                    } else {
                        totallen = prefixlen + filelen + 1;
# ifdef PATH_MAX
                        if (totallen >= PATH_MAX) {
                            continue;
                        }
# endif
                        path = lmt_memory_malloc(totallen + 1);
                        if (path) {
                            memcpy(path, index, prefixlen);
                            path[prefixlen] = '/';
                            memcpy(path + prefixlen + 1, file, filelen);
                        } else {
                            /*tex This is an error, unlikely, but checking makes compilers happy. */
                            goto OEPS;
                        }
                    }
                    path[totallen] = '\0';
                    if (access(path, X_OK) >= 0) {
                        break;
                    }
                    lmt_memory_free(path);
                    path = NULL;
                    index = esp + 1;
                } while (esp);
            }
            lmt_memory_free(searchpath);
            if (path) {
                return path;
            } else {
              OEPS:
                lmt_memory_free(searchpath);
                return lmt_memory_strdup("."); /* ok? */
            }
        }
    }

    /*tex We always return a copy so that we're consistent with windows/unix. */

    char *aux_utf8_readlink(const char *file)
    {
        int size = 256;
        while (1) {
            char *target = lmt_memory_malloc(size);
            if (! target) {
                break;
            } else {
                ssize_t tsize = readlink(file, target, size);
                if (tsize <= 0) {
                    lmt_memory_free(target);
                    break;
                } else if (tsize < size) {
                    target[tsize] = '\0';
                    return target;
                } else {
                    size *= 2;
                }
            }
        }
        return lmt_memory_strdup(file);
    }

    /* Derived from a Gemini query: */

    static void aux_utf8_normalize_posix_path(char *path)
    {
        if (! path || path[0] == '\0') {
            return;
        }
        char *src = path;
        char *dst = path;
        bool last_was_slash = false;
        while (*src != '\0') {
            if (*src == '/') {
                if (!last_was_slash) {
                    *dst++ = '/';
                    last_was_slash = true;
                }
            } else {
                *dst++ = *src;
                last_was_slash = false;
            }
            src++;
        }
        *dst = '\0';
        char *segments[1024];
        int   seg_count   = 0;
        bool  is_absolute = (path[0] == '/');
        char *token       = is_absolute ? path + 1 : path;
        while (*token != '\0') {
            char *slash = strchr(token, '/');
            if (slash) {
                *slash = '\0';
            }
            if (strcmp(token, ".") == 0 || strcmp(token, "") == 0) {
                /* Skip */
            } else if (strcmp(token, "..") == 0) {
                if (seg_count > 0) {
                    seg_count--;
                }
            } else {
                if (seg_count < 1024) {
                    segments[seg_count++] = token;
                }
            }
            if (! slash) {
                break;
            }
            token = slash + 1;
        }
        dst = path;
        if (is_absolute) {
            *dst++ = '/';
        }
        for (int i = 0; i < seg_count; i++) {
            if (i > 0 && ! (is_absolute && i == 0)) {
                if (dst > path && dst[-1] != '/') {
                    *dst++ = '/';
                }
            }
            size_t len = strlen(segments[i]);
            memcpy(dst, segments[i], len);
            dst += len;
        }
        if (dst == path) {
            *dst++ = is_absolute ? '/' : '.';
        }
        *dst = '\0';
    }

    /* Derived from a Gemini query: */

    char *aux_utf8_expandpath(const char *file)
    {
        if (! file) {
            return NULL;
        }
        char *result = NULL;
        if (file[0] == '/') {
            result = lmt_memory_strdup(file);
        } else {
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                size_t len = strlen(cwd) + 1 + strlen(file) + 1;
                result = (char *) lmt_memory_malloc(len);
                if (result) {
                    snprintf(result, len, "%s/%s", cwd, file);
                }
            }
        }
        if (! result) {
            result = lmt_memory_strdup(file);
        }
        aux_utf8_normalize_posix_path(result);
        return result;
    }

    /* Derived from a Gemini query: */

    char *aux_utf8_canonicalize(const char *file)
    {
        if (! file) {
            return NULL;
        } else {
            char resolved[PATH_MAX];
            if (realpath(file, resolved) != NULL) {
                return lmt_memory_strdup(resolved);
            } else {
                return aux_utf8_expandpath(file);
            }
        }
    }

# endif

# ifndef S_ISREG
    # define S_ISREG(mode) (mode & _S_IFREG)
# endif

# ifdef _WIN32

    char *aux_basename(const char *name) {
        char base[PATH_MAX + 1];
        char suff[PATH_MAX + 1];
        _splitpath(name,NULL,NULL,base,suff);
        {
            size_t b = strlen((const char*)base);
            size_t s = strlen((const char*)suff);
            char *result = (char *) lmt_memory_malloc(sizeof(char) * (b+s+1));
            if (result) {
                memcpy(&result[0], &base[0], b);
                memcpy(&result[b], &suff[0], s);
                result[b + s] = '\0';
            }
            return result;
        }
    }

    char *aux_dirname(const char *name) {
        char driv[PATH_MAX + 1];
        char path[PATH_MAX + 1];
        _splitpath(name,driv,path,NULL,NULL);
        {
            size_t d = strlen((const char*)driv);
            size_t p = strlen((const char*)path);
            char *result = (char *) lmt_memory_malloc(sizeof(char) * (d+p+1));
            if (result) {
                if (p > 0 && (path[p - 1] == '/' || path[p - 1] == '\\')) {
                    --p;
                }
                memcpy(&result[0], &driv[0], d);
                memcpy(&result[d], &path[0], p);
                result[d + p] = '\0';
            }
            return result;
        }
    }

    int aux_is_readable(const char *filename)
    {
        struct  _stati64 info;
        LPWSTR  w = aux_utf8_to_wide(filename);
        int     r = _wstati64(w, &info);
        FILE   *f;
        lmt_memory_free(w);
        return (r == 0)
            && (S_ISREG(info.st_mode))
            && ((f = aux_utf8_fopen(filename, "r")) != NULL)
            && ! fclose(f);
    }

# else

    # include <libgen.h>

    int aux_is_readable(const char *filename)
    {
        struct stat  finfo;
        FILE        *f;
        return (stat(filename, &finfo) == 0)
            && S_ISREG(finfo.st_mode)
            && ((f = fopen(filename, "r")) != NULL)
            && ! fclose(f);
    }

# endif
