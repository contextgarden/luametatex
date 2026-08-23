/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"
# include "lmtoptional.h"

# define ZSTD_DEFAULTCLEVEL 3
# define ZSTD_CONTENTSIZE_UNKNOWN ((size_t)-1)
# define ZSTD_CONTENTSIZE_ERROR   ((size_t)-2)

typedef struct zstdlib_state_info {

    int initialized;
    int padding;

    size_t   (*ZSTD_compressBound)       (size_t srcSize);
    size_t   (*ZSTD_getFrameContentSize) (const void *, size_t);
    size_t   (*ZSTD_compress)            (void *dst, size_t dstCapacity, const void *src, size_t srcSize, int compressionLevel);
    size_t   (*ZSTD_decompress)          (void *dst, size_t dstCapacity, const void *src, size_t compressedSize);
    unsigned (*ZSTD_isError)             (size_t code);

} zstdlib_state_info;

static zstdlib_state_info zstdlib_state = {

    .initialized              = 0,
    .padding                  = 0,

    .ZSTD_compressBound       = NULL,
    .ZSTD_getFrameContentSize = NULL,
    .ZSTD_compress            = NULL,
    .ZSTD_decompress          = NULL,
    .ZSTD_isError             = NULL,

};

static int zstdlib_compress(lua_State *L)
{
    if (zstdlib_state.initialized) {
        size_t      sourcesize = 0;
        const char *source     = luaL_checklstring(L, 1, &sourcesize);
        int         level      = lmt_optinteger(L, 2, ZSTD_DEFAULTCLEVEL);
        if (source) {
            size_t targetsize = zstdlib_state.ZSTD_compressBound(sourcesize);
            if (!zstdlib_state.ZSTD_isError(targetsize)) {
                luaL_Buffer buffer;
                char *target = luaL_buffinitsize(L, &buffer, targetsize);
                size_t result = zstdlib_state.ZSTD_compress(target, targetsize, source, sourcesize, level);
                if (!zstdlib_state.ZSTD_isError(result)) {
                    luaL_pushresultsize(&buffer, result);
                    return 1;
                }
            }
        }
    }
    lua_pushnil(L);
    return 1;
}

static int zstdlib_decompress(lua_State *L)
{
    if (zstdlib_state.initialized) {
        size_t      sourcesize = 0;
        const char *source     = luaL_checklstring(L, 1, &sourcesize);
        if (source) {
            size_t targetsize = zstdlib_state.ZSTD_getFrameContentSize(source, sourcesize);
            /* Check that targetsize is valid and not a sentinel error/unknown code */
            if (targetsize != ZSTD_CONTENTSIZE_UNKNOWN &&
                targetsize != ZSTD_CONTENTSIZE_ERROR &&
                ! zstdlib_state.ZSTD_isError(targetsize)) {
                luaL_Buffer buffer;
                char *target = luaL_buffinitsize(L, &buffer, targetsize);
                size_t result = zstdlib_state.ZSTD_decompress(target, targetsize, source, sourcesize);
                if (! zstdlib_state.ZSTD_isError(result)) {
                    luaL_pushresultsize(&buffer, result);
                    return 1;
                }
            }
        }
    }
    lua_pushnil(L);
    return 1;
}

static int zstdlib_initialize(lua_State *L)
{
    if (! zstdlib_state.initialized) {
        const char *filename = lua_tostring(L, 1);
        if (filename) {

            lmt_library lib = lmt_library_load(filename);

            zstdlib_state.ZSTD_compressBound       = lmt_library_find(lib, "ZSTD_compressBound");
            zstdlib_state.ZSTD_getFrameContentSize = lmt_library_find(lib, "ZSTD_getFrameContentSize");
            zstdlib_state.ZSTD_compress            = lmt_library_find(lib, "ZSTD_compress");
            zstdlib_state.ZSTD_decompress          = lmt_library_find(lib, "ZSTD_decompress");
            zstdlib_state.ZSTD_isError             = lmt_library_find(lib, "ZSTD_isError");

            zstdlib_state.initialized = lmt_library_okay(lib);
        }
    }
    lua_pushboolean(L, zstdlib_state.initialized);
    return 1;
}

static struct luaL_Reg zstdlib_function_list[] = {
    { "initialize", zstdlib_initialize },
    { "compress",   zstdlib_compress   },
    { "decompress", zstdlib_decompress },
    { NULL,         NULL               },
};

int luaopen_zstd(lua_State * L)
{
    lmt_library_register(L, "zstd", zstdlib_function_list);
    return 0;
}
