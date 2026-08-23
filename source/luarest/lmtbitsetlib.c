/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"

/*tex

    This started out as a \LUA\ variant of the smaller code base we used for z-buffering and
    when documenting the interfaces I decided to also add bitwise operators and offer both
    zero and one based sets. This is typical code that you can feed into an llm and ask for
    checks and such, because it is easy to make mistakes with juggling bytes and bits in
    bytes. So in the end we got some more than setters, getters and an iterator.

    The length operator is interesting because we can use of as weight in an ordered set of
    options, locations, complexity etc. We'll see where it gets applied in \CONTEXT.

*/

typedef struct bitset {
    int           max;
    int           zero_based; // 0 for 1-based (Lua default), 1 for 0-based
    unsigned char set[1];
} bitset;

/* Helper macros to convert user index into byte offset and bit mask */

# define BITSET_BYTE(b, i)       (((i) - ((b)->zero_based ? 0 : 1)) / 8)
# define BITSET_MASK(b, i) (1 << (((i) - ((b)->zero_based ? 0 : 1)) % 8))

# define BITSET_MIN_INDEX(b)             ((b)->zero_based ? 0 : 1)
# define BITSET_MAX_INDEX(b) ((b)->max - ((b)->zero_based ? 1 : 0))

static bitset *bitsetlib_aux_check_is_valid(lua_State *L, int n)
{
    bitset *b = (bitset *) lua_touserdata(L, n);
    if (b && lua_getmetatable(L, n)) {
        lua_get_metatablelua(bitset_instance);
        if (! lua_rawequal(L, -1, -2)) {
            b = NULL;
        }
        lua_pop(L, 2);
        if (b) {
            return b;
        }
    }
    tex_normal_warning("bitset lib", "lua <bitset> expected");
    return NULL;
}

static int bitsetlib_aux_new(lua_State *L, int zero_based)
{
    int     max   = lmt_optinteger(L, 1, 64);
    int     dflt  = lua_toboolean(L, 2);
    size_t  bytes = (max + 7) / 8;
    bitset *b     = lua_newuserdatauv(L, sizeof(bitset) + bytes, 0);
    if (b) {
        luaL_setmetatable(L, BITSET_METATABLE_INSTANCE);
        b->max        = max;
        b->zero_based = zero_based;
        memset(&(b->set[0]), dflt ? 0xFF : 0, bytes);
        return 1;
    } else {
        return 0;
    }
}

static int bitsetlib_new(lua_State *L)
{
    return bitsetlib_aux_new(L, 0); /* Default: 1-based */
}

static int bitsetlib_newonebased(lua_State *L)
{
    return bitsetlib_aux_new(L, 0);
}

static int bitsetlib_newzerobased(lua_State *L)
{
    return bitsetlib_aux_new(L, 1);
}

static int bitsetlib_tostring(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        lua_pushfstring(L, "<bitset %p : %d (%s-based)>", b->set, b->max, b->zero_based ? "0" : "1");
        return 1;
    } else {
        return 0;
    }
}

static int bitsetlib_set(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        int i = lmt_tointeger(L, 2);
        if (i >= BITSET_MIN_INDEX(b) && i <= BITSET_MAX_INDEX(b)) {
            b->set[BITSET_BYTE(b, i)] |= BITSET_MASK(b, i);
        }
    }
    return 0;
}

static int bitsetlib_reset(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        int i = lmt_tointeger(L, 2);
        if (i >= BITSET_MIN_INDEX(b) && i <= BITSET_MAX_INDEX(b)) {
            b->set[BITSET_BYTE(b, i)] &= ~BITSET_MASK(b, i);
        }
    }
    return 0;
}

static int bitsetlib_assign(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        int i   = lmt_tointeger(L, 2);
        int val = lua_toboolean(L, 3) ? 1 : 0; /* normalize to strictly 0 or 1 */
        if (i >= BITSET_MIN_INDEX(b) && i <= BITSET_MAX_INDEX(b)) {
            int byte_idx = BITSET_BYTE(b, i);
            int bit_idx  = ((i) - ((b)->zero_based ? 0 : 1)) % 8;
            int mask     = 1 << bit_idx;
            /* single branchless update: fastest according to gemini */
            b->set[byte_idx] = (b->set[byte_idx] & ~mask) | (val << bit_idx);
        }
    }
    return 0;
}

static int bitsetlib_get(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        int i = lmt_tointeger(L, 2);
        if (i >= BITSET_MIN_INDEX(b) && i <= BITSET_MAX_INDEX(b)) {
            lua_pushboolean(L, (b->set[BITSET_BYTE(b, i)] & BITSET_MASK(b, i)) != 0);
            return 1;
        }
    }
    return 0;
}

static int bitsetlib_wipe(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        memset(&(b->set[0]), 0, (b->max + 7) / 8);
    }
    return 0;
}

static int bitsetlib_aux_bitwise_op(lua_State *L, char op)
{
    bitset *b1 = bitsetlib_aux_check_is_valid(L, 1);
    bitset *b2 = bitsetlib_aux_check_is_valid(L, 2);
    if (b1 && b2) {
        if (b1->max != b2->max || b1->zero_based != b2->zero_based) {
            tex_normal_warning("bitset lib", "bitsets must have equal size and indexing base for binary operations");
            return 0;
        } else {
            size_t  bytes = (b1->max + 7) / 8;
            bitset *res   = lua_newuserdatauv(L, sizeof(bitset) + bytes, 0);
            if (res) {
                luaL_setmetatable(L, BITSET_METATABLE_INSTANCE);
                res->max        = b1->max;
                res->zero_based = b1->zero_based;
                for (size_t i = 0; i < bytes; i++) {
                    switch (op) {
                        case '&': res->set[i] = b1->set[i] & b2->set[i]; break;
                        case '|': res->set[i] = b1->set[i] | b2->set[i]; break;
                        case '^': res->set[i] = b1->set[i] ^ b2->set[i]; break;
                    }
                }
                return 1;
            }
        }
    }
    return 0;
}

static int bitsetlib_band(lua_State *L)
{
    return bitsetlib_aux_bitwise_op(L, '&');
}

static int bitsetlib_bor(lua_State *L)
{
    return bitsetlib_aux_bitwise_op(L, '|');
}

static int bitsetlib_bxor(lua_State *L)
{
    return bitsetlib_aux_bitwise_op(L, '^');
}

static int bitsetlib_bnot(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        size_t  bytes = (b->max + 7) / 8;
        bitset *res   = lua_newuserdatauv(L, sizeof(bitset) + bytes, 0);
        if (res) {
            luaL_setmetatable(L, BITSET_METATABLE_INSTANCE);
            res->max        = b->max;
            res->zero_based = b->zero_based;
            for (size_t i = 0; i < bytes; i++) {
                res->set[i] = ~b->set[i];
            }
            return 1;
        }
    }
    return 0;
}

/*tex
    A pre-computed population counts for all 8-bit values (0-255). A first implementation
    was way less efficient. Thanks to Gemini we now have a cache friendly alternative that
    doesn't depend on compiler specific popcount support.
*/

static const unsigned char popcount_lookup[256] = {
    0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
    4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8
};

static int bitsetlib_len(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        size_t bytes = (b->max + 7) / 8;
        int    count = 0;
        for (size_t i = 0; i < bytes; i++) {
            count += popcount_lookup[b->set[i]];
        }
        lua_pushinteger(L, count);
        return 1;
    } else {
        return 0;
    }
}

/*tex
    Here comes the usual iterator trio, with some upvalues to control which is the three
    variants we use: (bs), (bs,true), (bs,false).
*/

static int bitsetlib_aux_nil(lua_State *L)
{
    lua_pushnil(L);
    return 1;
}

static int bitsetlib_aux_next(lua_State *L)
{
    bitset *b       = (bitset *) lua_touserdata(L, lua_upvalueindex(1));
    int     how     = lmt_tointeger(L, lua_upvalueindex(2));
    int     idx     = lmt_tointeger(L, lua_upvalueindex(3));
    int     max_idx = BITSET_MAX_INDEX(b);
    while (idx <= max_idx) {
        int val = (b->set[BITSET_BYTE(b, idx)] & BITSET_MASK(b, idx)) != 0;
        switch (how) {
            case 2:
                if (val) {
                    idx += 1;
                    continue;
                }
                break;
            case 1:
                if (! val) {
                    idx += 1;
                    continue;
                }
                break;
        }
        lua_pushinteger(L, (lua_Integer) idx + 1);
        lua_replace(L, lua_upvalueindex(3));
        lua_pushinteger(L, idx);
        if (how) {
            return 1;
        } else {
            lua_pushboolean(L, val);
            return 2;
        }
    }
    return 0;
}

static int bitsetlib_traverse(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        int how = lua_type(L, 2) == LUA_TBOOLEAN ? (lua_toboolean(L, 2) ? 1 : 2) : 0;
        lua_settop(L, 1);
        lua_pushinteger(L, how);
        lua_pushinteger(L, BITSET_MIN_INDEX(b));
        lua_pushcclosure(L, bitsetlib_aux_next, 3);
    } else {
        lua_pushcclosure(L, bitsetlib_aux_nil, 0);
    }
    return 1;
}

static int bitsetlib_asstring(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        luaL_Buffer buffer;
        int step    = lmt_optinteger(L, 2, b->max + 1);
        int n       = 0;
        int min_idx = BITSET_MIN_INDEX(b);
        int max_idx = BITSET_MAX_INDEX(b);
        luaL_buffinitsize(L, &buffer, b->max + b->max / step);
        for (int i = min_idx; i <= max_idx; i++) {
            int val = (b->set[BITSET_BYTE(b, i)] & BITSET_MASK(b, i)) != 0;
            if (n >= step) {
                luaL_addchar(&buffer, ' ');
                n = 1;
            } else {
                n++;
            }
            luaL_addchar(&buffer, val ? '1' : '0');
        }
        luaL_pushresult(&buffer);
        return 1;
    } else {
        return 0;
    }
}

static int bitsetlib_totable(lua_State *L)
{
    bitset *b = bitsetlib_aux_check_is_valid(L, 1);
    if (b) {
        int min_idx = BITSET_MIN_INDEX(b);
        int max_idx = BITSET_MAX_INDEX(b);
        lua_createtable(L, b->max, 0);
        for (int i = min_idx; i <= max_idx; i++) {
            lua_pushboolean(L, (b->set[BITSET_BYTE(b, i)] & BITSET_MASK(b, i)) != 0);
            lua_rawseti(L, -2, b->zero_based ? i + 1 : i);
        }
        return 1;
    } else {
        return 0;
    }
}

/*tex
    This module started out simple but when documenting in I checked with Gemini about shifting
    and this is what it cooked up, using the interfaces that we already had. Juggling.
*/

/* Helper: create an empty destination bitset matching the source layout */

static bitset *bitsetlib_aux_clone_empty(lua_State *L, bitset *src)
{
    size_t  bytes = (src->max + 7) / 8;
    bitset *res   = lua_newuserdatauv(L, sizeof(bitset) + bytes, 0);
    if (res) {
        luaL_setmetatable(L, BITSET_METATABLE_INSTANCE);
        res->max        = src->max;
        res->zero_based = src->zero_based;
        memset(&(res->set[0]), 0, bytes);
    }
    return res;
}

static int bitsetlib_shl(lua_State *L)
{
    bitset *b     = bitsetlib_aux_check_is_valid(L, 1);
    int     shift = lmt_tointeger(L, 2);
    if (! b) {
        return 0;
    }
    bitset *res = bitsetlib_aux_clone_empty(L, b);
    if (! res) {
        return 0;
    }
    if (shift <= 0) {
        /* no-op shift or negative shift -> copy original */
        size_t bytes = (b->max + 7) / 8;
        if (shift == 0) {
            memcpy(res->set, b->set, bytes);
        }
        return 1;
    }
    if (shift >= b->max) {
        /* shifted past total capacity -> remains all 0s */
        return 1;
    }
    size_t total_bytes = (b->max + 7) / 8;
    int    byte_shift  = shift / 8;
    int    bit_shift   = shift % 8;
    for (size_t i = total_bytes; i-- > 0; ) {
        if ((int) i >= byte_shift) {
            size_t src_i = i - byte_shift;
            unsigned char val = b->set[src_i] << bit_shift;
            if (bit_shift > 0 && src_i > 0) {
                val |= (b->set[src_i - 1] >> (8 - bit_shift));
            }
            res->set[i] = val;
        }
    }
    /* Mask out any extra overflow bits in the highest allocated byte */
    int tail_bits = b->max % 8;
    if (tail_bits != 0) {
        res->set[total_bytes - 1] &= (1 << tail_bits) - 1;
    }
    return 1;
}

static int bitsetlib_shr(lua_State *L)
{
    bitset *b     = bitsetlib_aux_check_is_valid(L, 1);
    int     shift = lmt_tointeger(L, 2);
    if (! b) {
        return 0;
    }
    bitset *res = bitsetlib_aux_clone_empty(L, b);
    if (! res) {
        return 0;
    }
    /* here we go */
    if (shift <= 0) {
        size_t bytes = (b->max + 7) / 8;
        if (shift == 0) {
            memcpy(res->set, b->set, bytes);
        }
    } else if (shift >= b->max) {
        /* we're done */
    } else {
        size_t total_bytes = (b->max + 7) / 8;
        int    byte_shift  = shift / 8;
        int    bit_shift   = shift % 8;
        for (size_t i = 0; i < total_bytes; i++) {
            size_t src_i = i + byte_shift;
            if (src_i < total_bytes) {
                unsigned char val = b->set[src_i] >> bit_shift;
                if (bit_shift > 0 && (src_i + 1) < total_bytes) {
                    val |= (b->set[src_i + 1] << (8 - bit_shift));
                }
                res->set[i] = val;
            }
        }
    }
    return 1;
}

static const struct luaL_Reg bitsetlib_instance[] = {
    { "__tostring", bitsetlib_tostring },
    { "__index",    bitsetlib_get      },
    { "__newindex", bitsetlib_set      },
    { "__band",     bitsetlib_band     }, /* b1 & b2  */
    { "__bor",      bitsetlib_bor      }, /* b1 | b2  */
    { "__bxor",     bitsetlib_bxor     }, /* b1 ~ b2  */
    { "__bnot",     bitsetlib_bnot     }, /* ~b       */
    { "__shl",      bitsetlib_shl      }, /* b << k   */
    { "__shr",      bitsetlib_shr      }, /* b >> k   */
    { "__len",      bitsetlib_len      }, /* #b       */
    { NULL,         NULL               },
};

static const luaL_Reg bitsetlib_function_list[] = {
    { "new",          bitsetlib_new          },
    { "newonebased",  bitsetlib_newonebased  },
    { "newzerobased", bitsetlib_newzerobased },
    { "set",          bitsetlib_set          },
    { "get",          bitsetlib_get          },
    { "assign",       bitsetlib_assign       },
    { "reset",        bitsetlib_reset        },
    { "wipe",         bitsetlib_wipe         },
    { "asstring",     bitsetlib_asstring     },
    { "tostring",     bitsetlib_tostring     },
    { "totable",      bitsetlib_totable      },
    { "traverse",     bitsetlib_traverse     },
    { "band",         bitsetlib_band         }, /* b1 & b2  */
    { "bor",          bitsetlib_bor          }, /* b1 | b2  */
    { "bxor",         bitsetlib_bxor         }, /* b1 ~ b2  */
    { "bnot",         bitsetlib_bnot         }, /* ~b       */
    { "shl",          bitsetlib_shl          }, /* b << k   */
    { "shr",          bitsetlib_shr          }, /* b >> k   */
    { "len",          bitsetlib_len          }, /* #b       */
    { NULL,           NULL                   },
};

int luaopen_bitset(lua_State *L)
{
    luaL_newmetatable(L, BITSET_METATABLE_INSTANCE);
    luaL_setfuncs(L, bitsetlib_instance, 0);
    lua_newtable(L);
    luaL_setfuncs(L, bitsetlib_function_list, 0);
    return 1;
}