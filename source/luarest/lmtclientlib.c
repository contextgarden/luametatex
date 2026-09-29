/*
    See license.txt in the root of this project.
*/

# include <luametatex.h>

/*tex

    When you search a bit you find plenty templates for doing this. In fact, we already have the
    \CURL\ method as optional library. It looks like we can just assume that curl is available on
    \LINUX\ and \OSX\ and that the generic \MSWINDOWS\ interface will be around as long as this
    operating system is there. Although \CURL\ is nowadays installed on \MSWINDOWS\ we cannot
    assume the library to be there.

    So, after investigating this a bit I settled for two code paths, something we need to have
    anyway due to the wide filenames. We have helpers fot that so we can save some code. We share
    the response struct. All these flags \unknown\ not something you like to figure out.

    This module very simple because we only need to fetch and delegate all other logic to the
    \LUA\ end where we already have all in place anyway. The curl code path is a variant of what
    we already had in the optional |lmtcurl| file but limited to |http(s)| usage.

    After we had all in place a bit of checking with gemini about a potential speedup by writing
    directly to the lua buffer (no intermediate buffer in the windows code path and larger socket
    buffers) as well as checking the content length and preallocating the buffer at the start
    gave a better saturation of the (1GB) connection I tested with. Before that, the socket
    library performed better, after that code below won.

*/

# if _WIN32

    # include <stdint.h>
    # include <windows.h>
    # include <winhttp.h>

    # define LMT_HTTP_CHUNK_SIZE (64 * 1024)

    static int clientlib_aux_http_request(
        lua_State   *L,
        luaL_Buffer *b,
        const char  *method,
        const char  *url,
        const char **headers,
        const char  *body,
        size_t       length,
        int         *status_code,
        char       **errormessage
    )
    {
        LPWSTR         wurl    = aux_utf8_to_wide(url);
        LPWSTR         wmethod = aux_utf8_to_wide(method);
        URL_COMPONENTS components;

        memset(&components, 0, sizeof(components));

        components.dwStructSize      = sizeof(components);
        components.dwHostNameLength  = (DWORD) -1;
        components.dwUrlPathLength   = (DWORD) -1;
        components.dwExtraInfoLength = (DWORD) -1;
        components.dwSchemeLength    = (DWORD) -1;

        if (! WinHttpCrackUrl(wurl, 0, 0, &components)) {
            *errormessage = lmt_memory_strdup("invalid url");
            lmt_memory_free(wurl);
            lmt_memory_free(wmethod);
            return 0;
        }

        WCHAR host[256] = { 0 };
        DWORD host_len = components.dwHostNameLength;
        if (host_len >= 256) {
            host_len = 255;
        }
        wcsncpy(host, components.lpszHostName, host_len);
        host[host_len] = L'\0';

        WCHAR path[2048] = { 0 };
        DWORD path_len = components.dwUrlPathLength + components.dwExtraInfoLength;
        if (path_len >= 2048) {
            path_len = 2047;
        }
        if (components.lpszUrlPath && path_len > 0) {
            wcsncpy(path, components.lpszUrlPath, path_len);
            path[path_len] = L'\0';
        }

        HINTERNET session    = WinHttpOpen(L"LMTHTTP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        HINTERNET connection = session ? WinHttpConnect(session, host, components.nPort, 0) : NULL;

        DWORD     flags      = (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET request    = connection ? WinHttpOpenRequest(connection, wmethod, path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags) : NULL;

        if (! request) {
            *errormessage = lmt_memory_strdup("HTTP connection failed");
            if (connection) {
                WinHttpCloseHandle(connection);
            }
            if (session) {
                WinHttpCloseHandle(session);
            }
            lmt_memory_free(wurl);
            lmt_memory_free(wmethod);
            return 0;
        }

        if (headers) {
            for (int i = 0; headers[i] != NULL; i++) {
                LPWSTR wheader = aux_utf8_to_wide(headers[i]);
                WinHttpAddRequestHeaders(request, wheader, -1L, WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
                lmt_memory_free(wheader);
            }
        }

        int success = 0;

        if (WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID) (uintptr_t) body, (DWORD) length, (DWORD) length, 0)) {
            DWORD status     = 0;
            DWORD statussize = sizeof(status);

            WinHttpReceiveResponse(request, NULL);
            WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status,
                &statussize,
                WINHTTP_NO_HEADER_INDEX
            );

            *status_code = (int) status;

            # if 0

                char chunk[8192*2]; /* 8192 is kind of slow on a 260 MB zip */
                while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
                    DWORD to_read = available > sizeof(chunk) ? (DWORD) sizeof(chunk) : available;
                    if (WinHttpReadData(request, chunk, to_read, &downloaded) && downloaded > 0) {
                        luaL_addlstring(b, chunk, downloaded);
                    } else {
                        break;
                    }
                }

            # else

                DWORD content_length = 0;
                DWORD cl_size        = sizeof(content_length);
                if (WinHttpQueryHeaders(
                        request,
                        WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &content_length,
                        &cl_size,
                        WINHTTP_NO_HEADER_INDEX)
                ) {
                    if (content_length > 0) {
                        luaL_prepbuffsize(b, content_length);
                    }
                }

                DWORD available  = 0;
                DWORD downloaded = 0;
                while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
                    char *dest = luaL_prepbuffsize(b, available);
                    if (WinHttpReadData(request, dest, available, &downloaded) && downloaded > 0) {
                        luaL_addsize(b, downloaded);
                    } else {
                        break;
                    }
                }

            # endif

            success = 1;
        } else {
            *errormessage = lmt_memory_strdup("HTTP request failed");
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        lmt_memory_free(wurl);
        lmt_memory_free(wmethod);

        return success;
    }

    static int clientlib_aux_supported(void)
    {
        return 1;
    }

# else

    # include <dlfcn.h>

    static size_t write_callback(
        void   *buffer,
        size_t  size,
        size_t  nitems,
        void   *userdata // luaL_Buffer pointer
    )
    {
        size_t total = size * nitems;
        luaL_Buffer *b = (luaL_Buffer*) userdata;
        luaL_addlstring(b, (const char*) buffer, total);
        return total;
    }

    static size_t header_callback(
        char   *buffer,
        size_t  size,
        size_t  nitems,
        void   *userdata // luaL_Buffer pointer
    )
    {
        size_t total = size * nitems;
        /* Check for Content-Length (case-insensitive) */
        if (total > 15 && strncasecmp(buffer, "Content-Length:", 15) == 0) {
            size_t len = 0;
            if (sscanf(buffer + 15, " %zu", &len) == 1 && len > 0) {
                luaL_Buffer *b = (luaL_Buffer*) userdata;
                luaL_prepbuffsize(b, len);
            }
        }
        return total;
    }

    typedef void*       (*fn_curl_easy_init)      (void);
    typedef void        (*fn_curl_easy_cleanup)   (void*);
    typedef int         (*fn_curl_easy_perform)   (void*);
    typedef int         (*fn_curl_easy_setopt)    (void*, int, ...);
    typedef int         (*fn_curl_easy_getinfo)   (void*, int, ...);
    typedef const char* (*fn_curl_easy_strerror)  (int);
    typedef void*       (*fn_curl_slist_append)   (void*, const char*);
    typedef void        (*fn_curl_slist_free_all) (void*);

    static void                  *curl_handle         = NULL;
    static int                    curl_loaded         = -1; /* not yet checked */

    static fn_curl_easy_init      curl_easy_init      = NULL;
    static fn_curl_easy_cleanup   curl_easy_cleanup   = NULL;
    static fn_curl_easy_perform   curl_easy_perform   = NULL;
    static fn_curl_easy_setopt    curl_easy_setopt    = NULL;
    static fn_curl_easy_getinfo   curl_easy_getinfo   = NULL;
    static fn_curl_easy_strerror  curl_easy_strerror  = NULL;
    static fn_curl_slist_append   curl_slist_append   = NULL;
    static fn_curl_slist_free_all curl_slist_free_all = NULL;

    /*tex The complete list is in the |lmtcurl.c| file. */

    # define curl_option_writedata            1
    # define curl_option_url                  2
    # define curl_option_readdata             9
    # define curl_option_writefunction       11
    # define curl_option_postfields          15
    # define curl_option_headerfunction      20
    # define curl_option_httpheader          23
    # define curl_option_headerdata          29
    # define curl_option_customrequest       36
    # define curl_option_postfieldsize       60
    # define curl_option_buffersize          98
    # define curl_option_nosignal            99

    # define curl_integer_base                0 /* long */
    # define curl_string_base             10000
    # define curl_object_base             10000
    # define curl_function_base           20000
    # define curl_offset_base             30000
    # define curl_blob_base               40000

    # define curl_info_long            0x200000
    # define curl_info_double          0x300000

    # define curl_responsecode                2
    # define curl_content_length_download    15

    /*tex Because we know we're on \UNIX\ we can do this: */

    static void *clientlib_aux_load_curl_library(void)
    {
        static const char *names[] = {
            "libcurl.so.4",
            "libcurl.so",
            "libcurl.4.dylib",
            "libcurl.dylib",
            NULL
        };
        for (int i = 0; names[i]; i++) {
            void *handle = dlopen(names[i], RTLD_LAZY | RTLD_GLOBAL);
            if (handle) {
                return handle;
            }
        }
        return NULL;
    }

    static int clientlib_aux_curl_loaded(void)
    {
        if (curl_loaded >= 0) {
            return curl_loaded;
        }
        curl_handle = clientlib_aux_load_curl_library();
        if (curl_handle) {
            curl_easy_init      = (fn_curl_easy_init)      dlsym(curl_handle, "curl_easy_init");
            curl_easy_cleanup   = (fn_curl_easy_cleanup)   dlsym(curl_handle, "curl_easy_cleanup");
            curl_easy_perform   = (fn_curl_easy_perform)   dlsym(curl_handle, "curl_easy_perform");
            curl_easy_setopt    = (fn_curl_easy_setopt)    dlsym(curl_handle, "curl_easy_setopt");
            curl_easy_getinfo   = (fn_curl_easy_getinfo)   dlsym(curl_handle, "curl_easy_getinfo");
            curl_easy_strerror  = (fn_curl_easy_strerror)  dlsym(curl_handle, "curl_easy_strerror");
            curl_slist_append   = (fn_curl_slist_append)   dlsym(curl_handle, "curl_slist_append");
            curl_slist_free_all = (fn_curl_slist_free_all) dlsym(curl_handle, "curl_slist_free_all");
            if (   ! curl_easy_init    || ! curl_easy_cleanup || ! curl_easy_setopt
                || ! curl_easy_perform || ! curl_easy_getinfo || ! curl_easy_strerror
                || ! curl_slist_append || ! curl_slist_free_all) {
                dlclose(curl_handle);
                curl_handle = NULL;
            }
        }
        curl_loaded = curl_handle != NULL;
        return curl_loaded;
    }

    static int clientlib_aux_http_request(
        lua_State   *L,
        luaL_Buffer *buffer,
        const char  *method,
        const char  *url,
        const char **headers,
        const char  *body,
        size_t       length,
        int         *status_code,
        char       **errormessage
    )
    {
        if (! clientlib_aux_curl_loaded()) {
            *errormessage = lmt_memory_strdup("libcurl is not available on this system");
            return 0;
        }

        void *curl = curl_easy_init();
        if (! curl) {
            *errormessage = lmt_memory_strdup("initializing dynamic curl failed");
            return 0;
        }

        void *slist = NULL;
        if (headers) {
            for (int i = 0; headers[i] != NULL; i++) {
                slist = curl_slist_append(slist, headers[i]);
            }
        }
        curl_easy_setopt(curl, curl_integer_base + curl_option_nosignal, 1L);
        curl_easy_setopt(curl, curl_string_base + curl_option_url, (const void*) url);
        curl_easy_setopt(curl, curl_string_base + curl_option_customrequest, (const void*) method);

        if (slist) {
            curl_easy_setopt(curl, curl_object_base + curl_option_httpheader, (const void*) slist);
        }

        if (body && length > 0) {
            curl_easy_setopt(curl, curl_string_base + curl_option_postfields, (const void*) body);
            curl_easy_setopt(curl, curl_integer_base + curl_option_postfieldsize, (const void*) (uintptr_t) length);
        }

        curl_easy_setopt(curl, curl_function_base + curl_option_headerfunction, (const void*) &header_callback);
        curl_easy_setopt(curl, curl_object_base + curl_option_headerdata, (const void*) buffer);

        curl_easy_setopt(curl, curl_function_base + curl_option_writefunction, (const void*) &write_callback);
        curl_easy_setopt(curl, curl_object_base + curl_option_writedata, (const void*) buffer);

        curl_easy_setopt(curl, curl_integer_base + curl_option_buffersize, 1024L * 1024L);

        int code = curl_easy_perform(curl);
        if (code == 0) {
            long status = 0;
            curl_easy_getinfo(curl, curl_info_long + curl_responsecode, (void*) &status);
            *status_code = (int) status;
        } else {
            *errormessage = lmt_memory_strdup(curl_easy_strerror(code));
        }

        if (slist) {
            curl_slist_free_all(slist);
        }
        curl_easy_cleanup(curl);
        return (code == 0);
    }

    static int clientlib_aux_supported(void)
    {
        return clientlib_aux_curl_loaded();
    }

# endif

/*
    method  : string
    url     : string
    headers : table
    body    : string

    status  : integer | nil
    result  : string  | nil
    error   : nil     | string

    result  : integer + string
    error   : false   + string

*/

static int clientlib_httprequest(lua_State *L)
{
    const char  *url     = luaL_checkstring(L, 1);
    const char  *method  = luaL_optstring(L, 2, "GET");
    const char **headers = NULL;

    if (lua_istable(L, 3)) {
        size_t len = lua_rawlen(L, 3);
        headers = (const char**) lmt_memory_calloc(len + 1, sizeof(char*));
        for (size_t i = 1; i <= len; i++) {
            lua_rawgeti(L, 3, i);
            headers[i - 1] = lua_tostring(L, -1);
            lua_pop(L, 1);
        }
    } else {
        /* we could handle this */
    }

    size_t      length = 0;
    const char *body   = lua_tolstring(L, 4, &length);

    luaL_Buffer buffer;
    luaL_buffinit(L, &buffer);

    int   status       = 0;
    char *errormessage = NULL;

    int success = clientlib_aux_http_request(L,
        &buffer,
        method, url, headers, body, length,
        &status, &errormessage
    );

    lmt_memory_free(headers);

    if (! success || errormessage) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, errormessage ? errormessage : "the HTTP(S) request failed");
        lmt_memory_free(errormessage);
    } else {
        /*tex
            We need to finish the buffer first, and as that can involve pushing stuff on the stack
            we need to keep the order right. Which in turn means pushing the status after that is
            done and then move it up.
        */
        luaL_pushresult(&buffer);
        lua_pushinteger(L, status);
        lua_insert(L, -2);
    }
    return 2;
}

static int clientlib_supported(lua_State *L)
{
    lua_pushboolean(L, clientlib_aux_supported());
    return 1;
}

static const struct luaL_Reg clientlib_function_list[] = {
    { "httprequest", clientlib_httprequest },
    { "supported",   clientlib_supported   },
    { NULL,          NULL                  },
};

int luaopen_client(lua_State *L)
{
    luaL_newlib(L, clientlib_function_list);
    return 1;
}