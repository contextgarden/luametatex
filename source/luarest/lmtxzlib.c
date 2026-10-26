/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex
    Because we already have |tar| support in \CONTEXT\ (just \LUA), I decided to also get
    a simple |xz| compressor in the engine because thsi file format is popular in the \TEX\
    ecosystem. A bit of research learned that \quote {xz embedded} was the best candidate.

    We could have an |auxxzembedded.h| file but we need to adapt the |xz_config.h| file
    anyway: |XZ_DEC_CONCATENATED| as well as |XZ_USE_CRC64|.

*/

# include "../libraries/xzembedded/xz_config.h"

/*tex

    We initialize the decoder with dynamic allocation mode and a max dictionary size limit
    (64 MB) to protect against malicious headers.

    This wrapper is simular to for instance miniz but with less options. Here We only
    decompress. We need  |xz_dec_catrun| and not |xz_dec_run| as well as 64 bit checksum
    support. We could go for larger chunks but it looks fast enough.

*/

static int xzlib_decompress(lua_State *L)
{
    size_t      size = 0;
    const char *data = luaL_checklstring(L, 1, &size);

    struct xz_dec *decoder = xz_dec_init(XZ_DYNALLOC, 1 << 26);
    if (! decoder) {
        lua_pushnil(L);
        return 1;
    }

    luaL_Buffer buffer;
    luaL_buffinit(L, &buffer);

    struct xz_buf b;
    b.in      = (const uint8_t *) data;
    b.in_pos  = 0;
    b.in_size = size;

    enum xz_ret ret       = XZ_OK;
    int         success   = 0;
    size_t      chunksize = 16 * 1024;

    while (1) {
        uint8_t *out = (uint8_t *) luaL_prepbuffsize(&buffer, chunksize);
        
        b.out      = out;
        b.out_pos  = 0;
        b.out_size = chunksize;

        ret = xz_dec_catrun(decoder, &b, 1);
        
        if (b.out_pos > 0) {
            luaL_addsize(&buffer, b.out_pos);
        }

        if (ret == XZ_STREAM_END) {
            success = 1;
            break;
        } else if (ret == XZ_OK) {
            continue;
        } else {
            success = 0;
            break;
        }
    }

    xz_dec_end(decoder);

    if (success) {
        luaL_pushresult(&buffer);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

static struct luaL_Reg xzlib_function_list[] = {
    { "decompress", xzlib_decompress },
    { NULL,         NULL             },
};

int luaopen_xz(lua_State *L)
{
    xz_crc32_init();
    xz_crc64_init();
    lua_newtable(L);
    luaL_setfuncs(L, xzlib_function_list, 0);
    return 1;
}