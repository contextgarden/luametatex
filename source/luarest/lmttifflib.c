/*
    See license.txt in the root of this project.
*/

/*tex 

    Here we implement a TIFF decoder that supports uncompressed, CCITT G4, LZW, Deflate, 
    and PackBits (RL). It returns raw bytes and dimensions to \LUA. originally I only 
    wanted G4 for which we can use the single file |tiff_g4| library but because we have 
    decompression code elsewhere we can also suport some more. 

    It took some iterations to get here because in the end I also wanted it to be kind of 
    similar to nanojpeg (interface wise) but here we need to pass quite some parameters. Of 
    course the first test I did was with a sliced and indexed color so we had to handle 
    that at the \LUA\ end (collect slides and color map). The color map could be emulated
    in \LUA\ with a gsub but doing it here makes more sense. As usual one can find details 
    on the web and nowadays also check with e.g. Gemini, especially the specific tags but 
    in practice one can cook up thsi code relatively fast because graphic formats share 
    their concepts. 
    
    Keep in mind that we only want to deal with simple images: b/w, gray scale and rgb, no
    cmyk because there one needs color profiles: just embed a pdf. The main reason for also 
    supporting rgb and gray scale is that we then can load them in a bytemap and mess with 
    them in \LUAMETAFUN.

*/

# include "luametatex.h"
# include "libraries/tiff_g4/tiff_g4.h"
# include "utilities/auxzlib.h"
# include <utiliof.h>
# include <utilbasexx.h>
# include <utillzw.h>

/*tex 
    The initial interfacing code to tiff_g4 was assembled by querying Gemini, but later
    we moved to handling more (see above). The predictor code also comes from that end.
*/

typedef struct tifflib_raw_context {
    unsigned char *buffer;
    int            nx;
    int            bits_per_sample;
    int            nz;
} tifflib_raw_context;

static void tifflib_decode_g4_callback(TIFFDRAW *pDraw) 
{
    tifflib_raw_context *ctx = (tifflib_raw_context *) pDraw->pUser;
    int nx = pDraw->iWidth;
    int y  = pDraw->y;
    if (ctx->bits_per_sample == 1 && ctx->nz == 1) {
        unsigned char *row_dest = ctx->buffer + (y * nx);
        for (int x = 0; x < nx; x++) {
            uint8_t byte = pDraw->pPixels[x >> 3];
            uint8_t bit  = (byte >> (7 - (x & 7))) & 1;
            row_dest[x]  = bit ? 0 : 255;
        }
    } else {
        int row_stride = (nx + 7) / 8;
        memcpy(ctx->buffer + (y * row_stride), pDraw->pPixels, row_stride);
    }
}

static void tifflib_undo_predictor2(unsigned char *buffer, int nx, int ny, int nz, int bits_per_sample)
{
    if (bits_per_sample == 8) {
        size_t row_bytes = (size_t) nx * nz;
        for (int y = 0; y < ny; y++) {
            unsigned char *row = buffer + (y * row_bytes);
            for (size_t i = nz; i < row_bytes; i++) {
                row[i] = (unsigned char) (row[i] + row[i - nz]);
            }
        }
    } else if (bits_per_sample == 16) {
        size_t row_samples = (size_t) nx * nz;
        for (int y = 0; y < ny; y++) {
            uint16_t *row = (uint16_t *) (buffer + (y * row_samples * 2));
            for (size_t i = nz; i < row_samples; i++) {
                row[i] = (uint16_t) (row[i] + row[i - nz]);
            }
        }
    }
}

/*tex 
    Here the starting point was what we already had elsewhere (initially we constructed 
    a bytemap here but later decided to do that elsewhere.
*/

static int tifflib_decode(lua_State *L) 
{
    /*tex One can always wrap a string in a single slot array. */

    luaL_checktype(L, 1, LUA_TTABLE);

    int nx              = lmt_checkinteger(L, 2);
    int ny              = lmt_checkinteger(L, 3);
    int nz              = lmt_checkinteger(L, 4); /* samples_per_pixel */
    int compression     = lmt_checkinteger(L, 5);
    int bits_per_sample = lmt_optinteger(L, 6, nz == 1 ? 1 : 8);
    int fill_order      = lmt_optinteger(L, 7, BITDIR_MSB_FIRST);
    int rows_per_strip  = lmt_optinteger(L, 8, ny);
    int predictor       = lmt_optinteger(L, 9, 1);
    int has_colormap    = ! lua_isnoneornil(L, 10) && lua_istable(L, 10); 

    if (nx <= 0 || ny <= 0 || (nz != 1 && nz != 3)) {
        return luaL_error(L, "invalid dimensions or color channels (%d,%d,%d)", nx, ny, nz);
    }

    int    is_1bpp_gray = (bits_per_sample == 1 && nz == 1);
    size_t row_bytes    = is_1bpp_gray
                        ? (size_t) nx
                        : (size_t) nx * nz * (bits_per_sample / 8 > 0 ? bits_per_sample / 8 : 1);
    size_t total_bytes  = row_bytes * (size_t) ny;
    int    n_strips     = (int) lua_rawlen(L, 1);

    /*tex 
        As usual we try to use \LUA\ buffers as that saves us a copy later on because we already
        have a string on the stack. 
    */

    luaL_Buffer b;
    luaL_buffinit(L, &b);
    char *out_buffer = luaL_prepbuffsize(&b, total_bytes);
    
    /*tex 
        This should not be needed but better be white than random in case of some mismatch and
        one can replace 0 by 128 to identify issues (which I did when figuring out the color 
        mapping).
    */
    memset(out_buffer, 0, total_bytes);

    size_t write_offset = 0;

    for (int s = 1; s <= n_strips && write_offset < total_bytes; s++) {
        size_t      source_size = 0;
        const char *source_data = NULL;

        lua_rawgeti(L, 1, s);
        source_data = luaL_checklstring(L, -1, &source_size);

        int rows_done = (int) (write_offset / row_bytes);
        int strip_ny  = ny - rows_done;
        if (rows_per_strip > 0 && strip_ny > rows_per_strip) {
            strip_ny = rows_per_strip;
        }

        if (compression == 4) {
            /* This is actually why we made this module and use the one-file |tiff_G4|. */
            TIFFIMAGE tiff;
            if (! TIFF_openRAW(&tiff, nx, strip_ny, fill_order, (uint8_t *) (uintptr_t) source_data, (int) source_size, tifflib_decode_g4_callback)) {
                lua_pop(L, 1);
                return luaL_error(L, "failed to initialize G4 raw decoder");
            }
            tifflib_raw_context ctx = { (unsigned char *) (out_buffer + write_offset), nx, bits_per_sample, nz };
            tiff.pUser = &ctx;
            int result = TIFF_decode(&tiff);
            if (result != TIFF_SUCCESS) {
                int err = TIFF_getLastError(&tiff);
                lua_pop(L, 1);
                return luaL_error(L, "CCITT G4 decoding failed with error code %d", err);
            }
            size_t strip_out_bytes = is_1bpp_gray
                                   ? (size_t) strip_ny * nx
                                   : (size_t) strip_ny * ((nx + 7) / 8);
            write_offset += strip_out_bytes;
            lua_pop(L, 1);
            continue;
        }

        const unsigned char *bytes       = NULL;
        unsigned char       *buffer      = NULL;
        size_t               accumulated = 0;
        size_t               remaining   = total_bytes - write_offset;

        if (compression == 1) { 
            bytes       = (const unsigned char *) source_data;
            accumulated = source_size;
        } else if (compression == 5) { 
            /* Here we use pplib helpers. */
            iof *inp = iof_filter_string_reader((const unsigned char *) source_data, source_size);
            iof *out = iof_filter_buffer_writer(remaining * 2);
            lzw_decode(inp, out, LZW_DECODER_DEFAULTS);
            bytes       = out->buf;
            accumulated = out ? iof_size(out) : 0;
            if (inp) { iof_close(inp); }
            if (out) { iof_close(out); }
        } else if (compression == 8) { 
            /* This is basically what we have in |lmtziplib|, copied and adapted. */
            unsigned long target_size = (unsigned long) remaining * 2;
            buffer = (unsigned char *) lmt_memory_malloc(target_size);
            if (! buffer) {
                lua_pop(L, 1);
                return luaL_error(L, "failed to allocate memory for zip decompression");
            }
            z_stream strm;
            strm.zalloc    = lmt_zlib_alloc;
            strm.zfree     = lmt_zlib_free;
            strm.opaque    = Z_NULL;
            strm.next_in   = (unsigned char *) (uintptr_t) source_data;
            strm.avail_in  = (unsigned int) source_size;
            strm.next_out  = buffer;
            strm.avail_out = (unsigned int) target_size;
            if (inflateInit2(&strm, 15) != Z_OK) {
                lmt_memory_free(buffer);
                lua_pop(L, 1);
                return luaL_error(L, "failed to initialize zip inflate");
            }
            int status = inflate(&strm, Z_FINISH);
            if (status != Z_STREAM_END && status != Z_OK) {
                inflateEnd(&strm);
                lmt_memory_free(buffer);
                lua_pop(L, 1);
                return luaL_error(L, "zip inflate failed with status %d", status);
            }
            inflateEnd(&strm);
            bytes       = buffer;
            accumulated = (size_t) strm.total_out;
        } else if (compression == 32773) { 
            /* Here we use pplib helpers. */
            iof *inp = iof_filter_string_reader((const unsigned char *) source_data, source_size);
            iof *out = iof_filter_buffer_writer(remaining * 2);
            runlength_decode(inp, out);
            bytes       = out->buf;
            accumulated = out ? iof_size(out) : 0;
            if (inp) { iof_close(inp); }
            if (out) { iof_close(out); }
        } else {
            lua_pop(L, 1);
            return luaL_error(L, "unsupported TIFF compression type: %d", compression);
        }

        if (bytes && accumulated > 0) {
            if (is_1bpp_gray) {
                /* Not my speciality: */
                int row_stride = (nx + 7) / 8;
                int strip_rows = (int) (accumulated / row_stride);
                int max_rows   = (int) (remaining   / nx);
                if (strip_rows > max_rows) {
                    strip_rows = max_rows;
                }
                unsigned char *dest_base = (unsigned char *) out_buffer + write_offset;
                for (int y = 0; y < strip_rows; y++) {
                    const unsigned char *row_src  = bytes + (y * row_stride);
                    unsigned char       *row_dest = dest_base + (y * nx);
                    for (int x = 0; x < nx; x++) {
                        int bit = (row_src[x / 8] >> (7 - (x % 8))) & 1;
                        row_dest[x] = bit ? 0 : 255;
                    }
                }
                write_offset += (size_t) strip_rows * nx;
            } else {
                if (accumulated > remaining) {
                    accumulated = remaining;
                }
                memcpy((unsigned char *) out_buffer + write_offset, bytes, accumulated);
                write_offset += accumulated;
            }
        }

        if (buffer) { 
            lmt_memory_free(buffer); 
        }
        lua_pop(L, 1);
    }

    if (predictor == 2) {
        tifflib_undo_predictor2((unsigned char *) out_buffer, nx, ny, nz, bits_per_sample);
    }

    if (has_colormap && nz == 1 && bits_per_sample == 8) {
        size_t         out_bytes  = (size_t) nx * ny;
        size_t         rgb_bytes  = out_bytes * 3;
        unsigned char *rgb_buffer = (unsigned char *) lmt_memory_malloc(rgb_bytes);
        if (! rgb_buffer) {
            return luaL_error(L, "failed to allocate memory for colormap expansion");
        }

        unsigned char map_r[256], map_g[256], map_b[256]; /* efficient lookup tables */
        for (int i = 0; i < 256; i++) {
            lua_rawgeti(L, 10,       i + 1); map_r[i] = (unsigned char) (lua_tointeger(L, -1) >> 8); lua_pop(L, 1);
            lua_rawgeti(L, 10, 256 + i + 1); map_g[i] = (unsigned char) (lua_tointeger(L, -1) >> 8); lua_pop(L, 1);
            lua_rawgeti(L, 10, 512 + i + 1); map_b[i] = (unsigned char) (lua_tointeger(L, -1) >> 8); lua_pop(L, 1);
        }

        unsigned char *out = (unsigned char *) out_buffer;
        for (size_t i = 0; i < out_bytes; i++) {
            unsigned char idx = out[i];
            rgb_buffer[i * 3 + 0] = map_r[idx];
            rgb_buffer[i * 3 + 1] = map_g[idx];
            rgb_buffer[i * 3 + 2] = map_b[idx];
        }

        lua_pushinteger(L, nx);
        lua_pushinteger(L, ny);
        lua_pushinteger(L, 3);
        lua_pushlstring(L, (const char *) rgb_buffer, rgb_bytes);
        lmt_memory_free(rgb_buffer);
        return 4;
    }

    luaL_addsize(&b, total_bytes);
    lua_pushinteger(L, nx);
    lua_pushinteger(L, ny);
    lua_pushinteger(L, nz);
    luaL_pushresult(&b);
    return 4;
}

static const struct luaL_Reg tifflib_function_list[] = 
{
    { "decode", tifflib_decode },
    { NULL,     NULL           }
};
 
int luaopen_tinytiff(lua_State *L) 
{
    luaL_newlib(L, tifflib_function_list);
    return 1;
}
