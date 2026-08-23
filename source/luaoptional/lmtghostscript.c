/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"
# include "lmtoptional.h"

# define GS_ARG_ENCODING_UTF8 1

typedef struct gslib_state_info {

    int initialized;
    int padding;

    int (*gsapi_new_instance) (
        void **pinstance,
        void  *caller_handle
    );

    void (*gsapi_delete_instance) (
        void * instance
    );

    int (*gsapi_set_arg_encoding) (
        void *instance,
        int   encoding
    );

    int (*gsapi_init_with_args) (
        void        *instance,
        int          argc,
        const char **argv
    );

    int (*gsapi_set_stdio) (
        void *instance,
        int (*stdin_fn ) (void *caller_handle, char       *buf, int len),
        int (*stdout_fn) (void *caller_handle, const char *str, int len),
        int (*stderr_fn) (void *caller_handle, const char *str, int len)
    );

    int (*gsapi_exit) (
        void *instance
    );

} gslib_state_info;

static gslib_state_info gslib_state = {
    .initialized           = 0,
    .padding               = 0,
    .gsapi_new_instance    = NULL,
    .gsapi_delete_instance = NULL,
    .gsapi_set_arg_encoding= NULL,
    .gsapi_init_with_args  = NULL,
    .gsapi_set_stdio       = NULL,
    .gsapi_exit            = NULL,
};

typedef struct gslib_callback_ctx {
    luaL_Buffer *outbuffer;
    luaL_Buffer *errbuffer;
} gslib_callback_ctx;

static int gslib_initialize(lua_State * L)
{
    if (! gslib_state.initialized) {
        const char *filename = lua_tostring(L, 1);
        if (filename) {
            lmt_library lib = lmt_library_load(filename);

            gslib_state.gsapi_new_instance     = lmt_library_find(lib, "gsapi_new_instance");
            gslib_state.gsapi_delete_instance  = lmt_library_find(lib, "gsapi_delete_instance");
            gslib_state.gsapi_set_arg_encoding = lmt_library_find(lib, "gsapi_set_arg_encoding");
            gslib_state.gsapi_init_with_args   = lmt_library_find(lib, "gsapi_init_with_args");
            gslib_state.gsapi_set_stdio        = lmt_library_find(lib, "gsapi_set_stdio");
            gslib_state.gsapi_exit             = lmt_library_find(lib, "gsapi_exit");

            gslib_state.initialized = lmt_library_okay(lib) && gslib_state.gsapi_exit != NULL;
        }
    }
    lua_pushboolean(L, gslib_state.initialized);
    return 1;
}

static int gslib_stdout(void * caller_handle, const char *str, int len)
{
    gslib_callback_ctx *ctx = (gslib_callback_ctx *) caller_handle;
    if (ctx && ctx->outbuffer) {
        luaL_addlstring(ctx->outbuffer, str, len);
    }
    return len;
}

static int gslib_stderr(void * caller_handle, const char *str, int len)
{
    gslib_callback_ctx *ctx = (gslib_callback_ctx *) caller_handle;
    if (ctx && ctx->errbuffer) {
        luaL_addlstring(ctx->errbuffer, str, len);
    }
    return len;
}

static int gslib_execute(lua_State * L)
{
    if (gslib_state.initialized) {
        if (lua_type(L, 1) == LUA_TTABLE) {
            size_t n = lua_rawlen(L, 1);
            if (n > 0) {
                const char** arguments = malloc((n + 2) * sizeof(char*));
                if (! arguments) {
                    return 0;
                }
                /*tex
                    Collect arguments safely from \LUA\ before initializing buffers.
                */
                int m = 1;
                arguments[0] = "ghostscript";
                for (size_t i = 1; i <= n; i++) {
                    lua_rawgeti(L, 1, i);
                    switch (lua_type(L, -1)) {
                        case LUA_TSTRING:
                        case LUA_TNUMBER:
                        {
                            size_t l = 0;
                            const char *s = lua_tolstring(L, -1, &l);
                            if (l > 0) {
                                arguments[m] = s;
                                m += 1;
                            }
                        }
                        break;
                    }
                    lua_pop(L, 1);
                }
                arguments[m] = NULL;
                /*tex
                    Initialize local buffers & instance.
                */
                luaL_Buffer outbuf, errbuf;
                gslib_callback_ctx ctx = { .outbuffer = &outbuf, .errbuffer = &errbuf };
                void *instance = NULL;
                int result = gslib_state.gsapi_new_instance(&instance, &ctx);
                if (result >= 0) {
                    luaL_buffinit(L, &outbuf);

                    gslib_state.gsapi_set_stdio(instance, NULL, &gslib_stdout, &gslib_stderr);
                    gslib_state.gsapi_set_arg_encoding(instance, GS_ARG_ENCODING_UTF8);

                    result = gslib_state.gsapi_init_with_args(instance, m, arguments);

                    /* ALWAYS exit GS session if init succeeded or attempted */
                    gslib_state.gsapi_exit(instance);
                    gslib_state.gsapi_delete_instance(instance);
                    free((void *) arguments);
                    /*tex
                        Push results to Lua in sequential buffer order.
                    */
                    lua_pushboolean(L, result >= 0);
                    luaL_pushresult(&outbuf);
                    /*tex
                        Initialize and build error buffer string separately to ensure stack isolation.
                    */
                    luaL_buffinit(L, &errbuf);
                    luaL_pushresult(&errbuf);
                    return 3;
                }

                free((void *) arguments);
            }
        }
    }
    return 0;
}

static struct luaL_Reg gslib_function_list[] = {
    { "initialize", gslib_initialize },
    { "execute",    gslib_execute    },
    { NULL,         NULL             },
};

int luaopen_ghostscript(lua_State * L)
{
    lmt_library_register(L, "ghostscript", gslib_function_list);
    return 0;
}
