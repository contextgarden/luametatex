/*
    See license.txt in the root of this project.
*/

/* todo: byteconcat and utf concat (no separator) */

/*tex

    The code below evolved over time. For instance the loop over \UTF\ came from the original
    \LUATEX\ code base. There are various ways to deal with \UTF\ and claims with respect to
    performance often don't really show off, also because over time compilers became very clever
    in optimizing. Take multiplication versus shifting ... it depends. Anyway, we occasionally
    come back to the code and improve it. Actually this is one of the areas where an \LLM\ comes
    in handy: identify a potential (!) bottleneck and come up with a commonly known variant that
    one can then try out. Because these are well defines small use cases, it might work out in
    out favor.

*/

# include "luametatex.h"

/*tex Helpers */

static inline int strlib_aux_tounicode(const char *str, size_t len, size_t *pos)
{
    const unsigned char *s = (const unsigned char *) str;
    size_t p = *pos;
    if (p < len) {
        unsigned char i = s[p++];
        if (i < 0x80) {
            *pos = p;
            return i;
        } else {
            /*tex We only test redundantly when we have a wrong one. */
            if (i >= 0xF0) {
                if (p + 2 < len) {
                    unsigned char j = s[p];
                    unsigned char k = s[p + 1];
                    unsigned char l = s[p + 2];
                    if ((j & 0xC0) == 0x80 && (k & 0xC0) == 0x80 && (l & 0xC0) == 0x80) {
                        *pos = p + 3;
                        return ((i & 0x07) << 18) | ((j & 0x3F) << 12) | ((k & 0x3F) << 6) | (l & 0x3F);
                    }
                }
            } else if (i >= 0xE0) {
                if (p + 1 < len) {
                    unsigned char j = s[p];
                    unsigned char k = s[p + 1];
                    if ((j & 0xC0) == 0x80 && (k & 0xC0) == 0x80) {
                        *pos = p + 2;
                        return ((i & 0x0F) << 12) | ((j & 0x3F) << 6) | (k & 0x3F);
                    }
                }
            } else if (i >= 0xC0) {
                if (p < len) {
                    unsigned char j = s[p];
                    if ((j & 0xC0) == 0x80) {
                        *pos = p + 1;
                        return ((i & 0x1F) << 6) | (j & 0x3F);
                    }
                }
            }
            *pos = p;
        }
    }
    /* invalid sequence or incomplete trailing bytes */
    return 0xFFFD;
}

static inline int strlib_aux_tounichar(const char *str, size_t len, size_t pos)
{
    if (pos < len) {
        const unsigned char *s = (const unsigned char *) str;
        unsigned char i = s[pos++];
        if (i < 0x80) {
            return 1;
        } else if (i >= 0xF0) {
            if ((pos + 2) < len) {
                unsigned char j = s[pos];
                unsigned char k = s[pos + 1];
                unsigned char l = s[pos + 2];
                if ((j & 0xC0) == 0x80 && (k & 0xC0) == 0x80 && (l & 0xC0) == 0x80) {
                    return 4;
                }
            }
        } else if (i >= 0xE0) {
            if ((pos + 1) < len) {
                unsigned char j = s[pos];
                unsigned char k = s[pos + 1];
                if ((j & 0xC0) == 0x80 && (k & 0xC0) == 0x80) {
                    return 3;
                }
            }
        } else if (i >= 0xC0) {
            if (pos < len) {
                unsigned char j = s[pos];
                if ((j & 0xC0) == 0x80) {
                    return 2;
                }
            }
        }
    }
    /* invalid sequence or incomplete bytes */
    return 0;
}

static inline size_t strlib_aux_toline(const char *s, size_t l, size_t p, size_t *b)
{
    size_t i = p;
    *b = 0;
    while (i < l) {
        char c = s[i];
        if (c == '\r') {
            if (i + 1 < l && s[i + 1] == '\n') {
                *b = 2; /* cr lf */
            } else {
                *b = 1; /* cr */
            }
            return i - p;
        } else if (c == '\n') {
            *b = 1;     /* lf */
            return i - p;
        }
        i++;
    }
    /* end of buffer */
    return i - p;
}

/*tex End of helpers. */

static int strlib_aux_bytepairs(lua_State *L)
{
    size_t ls = 0;
    const char *s = lua_tolstring(L, lua_upvalueindex(1), &ls);
    size_t ind = lmt_tointeger(L, lua_upvalueindex(2));
    if (ind < ls) {
        unsigned char i;
        /*tex iterator */
        if (ind + 1 < ls) {
            lua_pushinteger(L, ind + 2);
        } else {
            lua_pushinteger(L, ind + 1);
        }
        lua_replace(L, lua_upvalueindex(2));
        i = (unsigned char)*(s + ind);
        /*tex byte one */
        lua_pushinteger(L, i);
        if (ind + 1 < ls) {
            /*tex byte two */
            i = (unsigned char)*(s + ind + 1);
            lua_pushinteger(L, i);
        } else {
            /*tex odd string length */
            lua_pushnil(L);
        }
        return 2;
    } else {
        return 0;
    }
}

static int strlib_bytepairs(lua_State *L)
{
    luaL_checkstring(L, 1);
    lua_settop(L, 1);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, strlib_aux_bytepairs, 2);
    return 1;
}

static int strlib_aux_bytes(lua_State *L)
{
    size_t ls = 0;
    const char *s = lua_tolstring(L, lua_upvalueindex(1), &ls);
    size_t ind = lmt_tointeger(L, lua_upvalueindex(2));
    if (ind < ls) {
        /*tex iterator */
        lua_pushinteger(L, ind + 1);
        lua_replace(L, lua_upvalueindex(2));
        /*tex byte */
        lua_pushinteger(L, (unsigned char)*(s + ind));
        return 1;
    } else {
        return 0;
    }
}

static int strlib_bytes(lua_State *L)
{
    luaL_checkstring(L, 1);
    lua_settop(L, 1);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, strlib_aux_bytes, 2);
    return 1;
}

static int strlib_aux_utf_failed(lua_State *L, int new_ind)
{
    lua_pushinteger(L, new_ind);
    lua_replace(L, lua_upvalueindex(2));
    lua_pushliteral(L, utf_fffd_string);
    return 1;
}

/* kind of complex ... these masks */

// static int strlib_aux_utfcharacters(lua_State *L)
// {
//     static const unsigned char mask[4] = { 0x80, 0xE0, 0xF0, 0xF8 };
//     static const unsigned char mequ[4] = { 0x00, 0xC0, 0xE0, 0xF0 };
//     size_t ls = 0;
//     const char *s = lua_tolstring(L, lua_upvalueindex(1), &ls);
//     size_t ind = lmt_tointeger(L, lua_upvalueindex(2));
//     size_t l = ls;
//     if (ind >= l) {
//         return 0;
//     } else {
//         unsigned char c = (unsigned char) s[ind];
//         for (size_t j = 0; j < 4; j++) {
//             if ((c & mask[j]) == mequ[j]) {
//                 if (ind + 1 + j > l) {
//                     /*tex The result will not fit. */
//                     return strlib_aux_utf_failed(L, (int) l);
//                 }
//                 for (size_t k = 1; k <= j; k++) {
//                     c = (unsigned char) s[ind + k];
//                     if ((c & 0xC0) != 0x80) {
//                         /*tex We have a bad follow byte. */
//                         return strlib_aux_utf_failed(L, (int) (ind + k));
//                     }
//                 }
//                 /*tex The iterator. */
//                 lua_pushinteger(L, ind + j + 1);
//                 lua_replace(L, lua_upvalueindex(2));
//                 lua_pushlstring(L, ind + s, j + 1);
//                 return 1;
//             }
//         }
//         return strlib_aux_utf_failed(L, (int) (ind + 1)); /* we found a follow byte! */
//     }
// }

/*tex
    Based on the above the gemini-in-browser \LLM\ suggested the following variant, but here we
    have a branche instead.
*/

// static int strlib_aux_utfcharacters(lua_State *L)
// {
//     size_t ls = 0;
//     const char *s = lua_tolstring(L, lua_upvalueindex(1), &ls);
//     size_t ind = lmt_tointeger(L, lua_upvalueindex(2));
//     if (ind >= ls) {
//         return 0;
//     } else {
//         unsigned char c = (unsigned char) s[ind];
//         size_t len;
//         if (c < 0x80) {
//             len = 1;
//         } else if (c >= 0xC0 && c < 0xE0) {
//             len = 2;
//         } else if (c >= 0xE0 && c < 0xF0) {
//             len = 3;
//         } else if (c >= 0xF0 && c < 0xF8) {
//             len = 4;
//         } else {
//             return strlib_aux_utf_failed(L, (int)(ind + 1));
//         }
//         if (ind + len > ls) {
//             /*tex The result will not fit. */
//             return strlib_aux_utf_failed(L, (int)ls);
//         }
//         for (size_t k = 1; k < len; k++) {
//             if (((unsigned char) s[ind + k] & 0xC0) != 0x80) {
//                 /*tex We have a bad follow byte. */
//                 return strlib_aux_utf_failed(L, (int) (ind + k));
//             }
//         }
//         lua_pushinteger(L, ind + len);
//         lua_replace(L, lua_upvalueindex(2));
//         lua_pushlstring(L, s + ind, len);
//         return 1;
//     }
// }

/*tex
    So, when pressed a bit for avoiding this like the original, it cooked up the following, which
    probably is the way to go on modern \CPU's. It is a nice limited case so relatively easy to
    check (which is really needed!). The basic setup is still the same as we had.
*/

/*tex
    And when asked about the if's it then went (slightly adapted afterwards). But we might as well
    consult the competition as this seems to be a common approach.
*/

static const unsigned char utf8_lengths[16] = {
    1, 1, 1, 1, 1, 1, 1, 1, /* 0x0 - 0x7 : ASCII (1 byte) */
    0, 0, 0, 0,             /* 0x8 - 0xB : continuation bytes (invalid lead) */
    2, 2,                   /* 0xC - 0xD : 2-byte sequence */
    3,                      /* 0xE       : 3-byte sequence */
    4                       /* 0xF       : 4-byte sequence (or invalid if > 0xF4) */
};

static int strlib_aux_utfcharacters(lua_State *L)
{
    size_t ls = 0;
    const char *s = lua_tolstring(L, lua_upvalueindex(1), &ls);
    size_t ind = (size_t) lmt_tointeger(L, lua_upvalueindex(2));
    if (ind >= ls) {
        return 0;
    } else {
        unsigned char c = (unsigned char) s[ind];
        if (c < 0x80) {
            lua_pushinteger(L, ind + 1);
            lua_replace(L, lua_upvalueindex(2));
            lua_pushlstring(L, s + ind, 1);
            return 1;
        } else {
            size_t len = utf8_lengths[c >> 4];
            if (len == 0 || c > 0xF4) {
                return strlib_aux_utf_failed(L, (int) (ind + 1));
            } else if (ind + len > ls) {
                /*tex The result will not fit. */
                return strlib_aux_utf_failed(L, (int) ls);
            } else {
                for (size_t k = 1; k < len; k++) {
                    if (((unsigned char) s[ind + k] & 0xC0) != 0x80) {
                        /*tex We have a bad follow byte. */
                        return strlib_aux_utf_failed(L, (int) (ind + k));
                    }
                }
                lua_pushinteger(L, ind + len);
                lua_replace(L, lua_upvalueindex(2));
                lua_pushlstring(L, s + ind, len);
                return 1;
            }
        }
    }
}

static int strlib_utfcharacters(lua_State *L)
{
    luaL_checkstring(L, 1);
    lua_settop(L, 1);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, strlib_aux_utfcharacters, 2);
    return 1;
}

static int strlib_aux_utfvalues(lua_State *L)
{
    size_t l = 0;
    const char *s = lua_tolstring(L, lua_upvalueindex(1), &l);
    size_t ind = lmt_tointeger(L, lua_upvalueindex(2));
    if (ind < l) {
        int v = strlib_aux_tounicode(s, l, &ind);
        lua_pushinteger(L, ind);
        lua_replace(L, lua_upvalueindex(2));
        lua_pushinteger(L, v);
        return 1;
    } else {
        return 0;
    }
}

static int strlib_utfvalues(lua_State *L)
{
    luaL_checkstring(L, 1);
    lua_settop(L, 1);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, strlib_aux_utfvalues, 2);
    return 1;
}

static int strlib_aux_characterpairs(lua_State *L)
{
    size_t ls = 0;
    const char *s = lua_tolstring(L, lua_upvalueindex(1), &ls);
    size_t ind = lmt_tointeger(L, lua_upvalueindex(2));
    if (ind < ls) {
        char b[1];
        lua_pushinteger(L, ind + 2); /*tex So we can overshoot ls here. */
        lua_replace(L, lua_upvalueindex(2));
        b[0] = s[ind];
        lua_pushlstring(L, b, 1);
        if ((ind + 1) < ls) {
            b[0] = s[ind + 1];
            lua_pushlstring(L, b, 1);
        } else {
            lua_pushliteral(L, "");
        }
        return 2;
    } else {
        return 0;  /* string ended */
    }
}

static int strlib_characterpairs(lua_State *L)
{
    luaL_checkstring(L, 1);
    lua_settop(L, 1);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, strlib_aux_characterpairs, 2);
    return 1;
}

static int strlib_aux_characters(lua_State *L)
{
    size_t ls = 0;
    const char *s = lua_tolstring(L, lua_upvalueindex(1), &ls);
    size_t ind = lmt_tointeger(L, lua_upvalueindex(2));
    if (ind < ls) {
        char b[1];
        lua_pushinteger(L, ind + 1); /* iterator */
        lua_replace(L, lua_upvalueindex(2));
        b[0] = *(s + ind);
        lua_pushlstring(L, b, 1);
        return 1;
    } else {
        return 0;  /* string ended */
    }
}

static int strlib_characters(lua_State *L)
{
    luaL_checkstring(L, 1);
    lua_settop(L, 1);
    lua_pushinteger(L, 0);
    lua_pushcclosure(L, strlib_aux_characters, 2);
    return 1;
}

static int strlib_bytetable(lua_State *L)
{
    size_t l;
    const char *s = luaL_checklstring(L, 1, &l);
    lua_createtable(L, (int) l, 0);
    for (size_t i = 0; i < l; i++) {
        lua_pushinteger(L, (unsigned char)*(s + i));
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int strlib_utfvaluetable(lua_State *L)
{
    size_t n = 1;
    size_t l = 0;
    size_t p = 0;
    const char *s = luaL_checklstring(L, 1, &l);
    lua_createtable(L, (int) l, 0);
    while (p < l) {
        lua_pushinteger(L, strlib_aux_tounicode(s, l, &p));
        lua_rawseti(L, -2, n++);
    }
    return 1;
}

static int strlib_utfcharactertable(lua_State *L)
{
    size_t n = 1;
    size_t l = 0;
    size_t p = 0;
    const char *s = luaL_checklstring(L, 1, &l);
    lua_createtable(L, (int) l, 0);
    while (p < l) {
        int b = strlib_aux_tounichar(s, l, p);
        if (b) {
            lua_pushlstring(L, s + p, b);
            p += b;
        } else {
            lua_pushliteral(L, utf_fffd_string);
            p += 1;
        }
        lua_rawseti(L, -2, n++);
    }
    return 1;
}

static int strlib_linetable(lua_State *L)
{
    size_t n = 1;
    size_t l = 0;
    size_t p = 0;
    const char *s = luaL_checklstring(L, 1, &l);
    lua_createtable(L, (int) l, 0);
    while (p < l) {
        size_t b = 0;
        size_t m = strlib_aux_toline(s, l, p, &b);
        if (m) {
            lua_pushlstring(L, s + p, m);
        } else {
            lua_pushliteral(L, "");
        }
        p += m + b;
        lua_rawseti(L, -2, n++);
    }
    return 1;
}

/*tex

    In \LUATEX\ we have \type {slunicode} but in \CONTEXT\ we rather early decided not to use
    it because we needed more detail. There all happened in a dedicated \type {utf8} module that
    has a rather good performance anyway. In \LUAMETATEX\ we introduced some helpers; there
    actually is all kind of \UTF\ code spread over the code base, depending on needs. Keep in
    mind that in the end little \UTF\ is needed because when for instance we resolve \type
    {\csname}'s string mostly contain \ASCII\ as do macro files, \LUA\ files and \METAPOST.

*/

# define MAXUNICODE 0x10FFFF

/*tex
    This is a quick and dirty, no checking done variant. We use a \LUA\ buffer which comes at
    a price.
*/

static inline void strlib_aux_add_utfchar(luaL_Buffer *b, unsigned u)
{
    if (u <= MAXUNICODE) {
        if (0x80 > u) {
            luaL_addchar(b, (unsigned char) u);
        } else {
            if (0x800 > u)
                luaL_addchar(b, (unsigned char) (0xC0 | (u >> 6)));
            else {
                if (0x10000 > u)
                    luaL_addchar(b, (unsigned char) (0xE0 | (u >> 12)));
                else {
                    luaL_addchar(b, (unsigned char) (0xF0 | (u >> 18)));
                    luaL_addchar(b, (unsigned char) (0x80 | (0x3F & (u >> 12))));
                }
                luaL_addchar(b, 0x80 | (0x3F & (u >> 6)));
            }
            luaL_addchar(b, 0x80 | (0x3F & u));
        }
    }
}

static inline void strlib_aux_add_utfnumber(lua_State *L, luaL_Buffer *b, int index)
{
    strlib_aux_add_utfchar(b, (unsigned) lmt_tounsigned(L, index));
}

static inline void strlib_aux_add_utfstring(lua_State *L, luaL_Buffer *b, int index)
{
    size_t ls = 0;
    const char *s = lua_tolstring(L, index, &ls);
    luaL_addlstring(b, s, ls);
}

static inline void strlib_aux_add_utftable(lua_State *L, luaL_Buffer *b, int index)
{
    lua_Unsigned n = lua_rawlen(L, index);
    if (n > 0) { 
        for (lua_Unsigned i = 1; i <= n; i++) {
            lua_rawgeti(L, index, i);
            switch (lua_type(L, -1)) { 
                case LUA_TNUMBER: 
                    strlib_aux_add_utfnumber(L, b, -1);
                    break;
                case LUA_TTABLE:
                    strlib_aux_add_utftable(L, b, -1);
                    break;
                case LUA_TSTRING: 
                    strlib_aux_add_utfstring(L, b, -1);
                    break;
                default: 
                    /* we could just quit */
                    break;
            }
            lua_pop(L, 1);
        }
    }
}

static int strlib_utfcharacter(lua_State *L)
{
    int n = lua_gettop(L);
    if (n == 1 && lua_type(L, 1) == LUA_TNUMBER) {
        char u[6];
        char *c = aux_uni2string(&u[0], (unsigned) lua_tointeger(L, 1));
        *c = '\0';
        lua_pushstring(L, u);
        return 1;
    } else {
        luaL_Buffer b;
        luaL_buffinitsize(L, &b, (size_t) n * 4); 
        for (int i = 1; i <= n; i++) {
            switch (lua_type(L, i)) {
                case LUA_TNUMBER: strlib_aux_add_utfnumber(L, &b, i); break;
                case LUA_TTABLE : strlib_aux_add_utftable (L, &b, i); break;
                case LUA_TSTRING: strlib_aux_add_utfstring(L, &b, i); break;
            }
        }
        luaL_pushresult(&b);
        return 1;
    }
}

/*tex

    The \UTF8 code point function takes two arguments, being positions in the string, while slunicode
    byte takes two arguments representing the number of \UTF\ characters. The variant below always
    returns all code points.

*/

static int strlib_utfvalue(lua_State *L)
{
    size_t l = 0;
    size_t p = 0;
    int i = 0;
    const char *s = luaL_checklstring(L, 1, &l);
    while (p < l) {
        lua_pushinteger(L, strlib_aux_tounicode(s, l, &p));
        i++;
    }
    return i;
}

/*tex This is a simplified version of utf8.len but without range. */

// static int strlib_utflength(lua_State *L)
// {
//     size_t ls = 0;
//     size_t ind = 0;
//     size_t n = 0;
//     const char *s = lua_tolstring(L, 1, &ls);
//     while (ind < ls) {
//         unsigned char i = (unsigned char) *(s + ind);
//         if (i < 0x80) {
//             ind += 1;
//         } else if (i >= 0xF0) {
//             ind += 4;
//         } else if (i >= 0xE0) {
//             ind += 3;
//         } else if (i >= 0xC0) {
//             ind += 2;
//         } else {
//             /*tex bad news, stupid recovery */
//             ind += 1;
//         }
//         n++;
//     }
//     lua_pushinteger(L, n);
//     return 1;
// }

/*tex
    This one uses the same shift based few \CPU\ cycle length trick as above. It does
    a bit more checking so that likely kills the gain.
*/

static int strlib_utflength(lua_State *L)
{
    size_t ls = 0;
    const char *s = lua_tolstring(L, 1, &ls);
    size_t ind = 0;
    size_t n = 0;
    while (ind < ls) {
        unsigned char i = (unsigned char) s[ind];
        if (i < 0x80) {
            ind++;
            n++;
            continue;
        }
        /* O(1) length lookup via high nibble */
        size_t len = utf8_lengths[i >> 4];
        /*
           If we have an invalid lead byte (len == 0, continuation byte 0x80..0xBF, etc.)
           or an overlong lead byte (> 0xF4), we fall back to 1 byte.
        */
        if (len == 0 || i > 0xF4) {
            len = 1;
        }
        if (ind + len > ls) {
            len = ls - ind;
        }
        ind += len;
        n++;
    }
    lua_pushinteger(L, (lua_Integer) n);
    return 1;
}

/*tex
    This one is uses quite a bit in \CONTEXT\ and it formats a float where it strips trailing zeros;
    it also checks for zeros and ones. It started out as \LUA\ code but we wanted a bit more
    performance. We mostly use it with six digits precision but occasionally we go for nine
    digits.
*/

// static int strlib_format_f6(lua_State *L)
// {
//     double n = luaL_optnumber(L, 1, 0.0);
//     if (n == 0.0) {
//         lua_pushliteral(L, "0");
//     } else if (n == 1.0) {
//         lua_pushliteral(L, "1");
//  // } else if (n == 10.0) {
//  //     /* does happen in our use case but not worth it */
//  //     lua_pushliteral(L, "10");
//     } else {
//         char s[128];
//         int i;
//         /* this is really needed in order to get integers in pdf  */
//         if (fmod(n, 1) == 0 && n >= min_integer && n <= max_integer) {
//            i = snprintf(s, 128, "%i", (int) n);
//         } else {
//             int l;
//             if (lua_type(L, 2) == LUA_TSTRING) {
//                 const char *f = lua_tostring(L, 2);
//                 i = snprintf(s, 128, f, n);
//             } else {
//                 i = snprintf(s, 128, "%0.6f", n) ;
//             }
//             l = i - 1;
//             while (l > 1) {
//                 if (s[l - 1] == '.') {
//                     break;
//                 } else if (s[l] == '0') {
//                  // s[l] = '\0'; /* redundant */
//                     --i;
//                 } else {
//                     break;
//                 }
//                 l--;
//             }
//         }
//         lua_pushlstring(L, s, i);
//     }
//     return 1;
// }

# ifndef MIN_INTEGER
    # define MIN_INTEGER -2147483648.0
    # define MAX_INTEGER  2147483647.0
# endif

static int strlib_format_f6(lua_State *L)
{
    double n = luaL_optnumber(L, 1, 0.0);
    /* Fast paths for standard constants */
    if (n == 0.0) {
     // if (signbit(n)) {
     //     lua_pushliteral(L, "-0");
     // } else {
            lua_pushliteral(L, "0");
     // }
        return 1;
    } else if (n == 1.0) {
        lua_pushliteral(L, "1");
        return 1;
    } else {
        char s[64];
        int len;
        if (fmod(n, 1.0) == 0.0 && n >= MIN_INTEGER && n <= MAX_INTEGER) {
            len = snprintf(s, sizeof(s), "%ld", (long) n);
            lua_pushlstring(L, s, (size_t) len);
            return 1;
        } else {
            if (lua_type(L, 2) == LUA_TSTRING) {
                const char *f = lua_tostring(L, 2);
                len = snprintf(s, sizeof(s), f, n);
            } else {
                len = snprintf(s, sizeof(s), "%.6f", n);
            }
            if (len <= 0 || len >= (int) sizeof(s)) {
                lua_pushliteral(L, "0");
                return 1;
            }
            char *dot = strchr(s, '.');
            if (dot) {
                char *ptr = s + len - 1;
                while (ptr > dot && *ptr == '0') {
                    ptr--;
                }
                if (ptr == dot) {
                    ptr--;
                }
                len = (int) (ptr - s + 1);
            }
        }
        lua_pushlstring(L, s, (size_t) len);
        return 1;
    }
}

/*tex
    The next one is mostly provided as check because doing it in pure \LUA\ is not slower and it's
    not a bottleneck anyway. There are some subtle side effects when we don't check for these ranges,
    especially the trigger bytes (|0xD7FF| etc.) because we can get negative numbers which means
    wrapping around and such.
*/

# if 1

    /* We don't want these in tounicode vectors: */

    static inline int invalid_unicode(lua_Integer u)
    {
        return
           (u >= 0x00E000 && u <= 0x00F8FF)
        || (u >= 0x0F0000 && u <= 0x0FFFFF)
        || (u >= 0x100000 && u <= 0x10FFFF)
        || (u >= 0x00D800 && u <= 0x00DFFF)
        || (u >= 0x00D7FF && u <= 0x00DFFF)
        || (u >  0x10FFFF);
    }

# else

    /* But officially it is: surrogates, non-characters, out-of-bounds */

    static inline int invalid_unicode(lua_Integer u)
    {
        return
            (u >= 0x00E000 && u <= 0x00F8FF)
         || (u >= 0x00D800 && u <= 0x00DFFF)
         || (u >  0x10FFFF);
    }

# endif

// static inline unsigned char strlib_aux_hexdigit(unsigned char n)
// {
//     return (n < 10 ? '0' : 'A' - 10) + n;
// }
//
// static int strlib_format_tounicode16(lua_State *L)
// {
//     lua_Integer u = lua_tointeger(L, 1);
//     if (invalid_unicode(u)) {
//         /* privates are valid but we don't want them */
//         lua_pushliteral(L, "FFFD");
//     } else if (u < 0xD7FF || (u >= 0xDFFF && u <= 0xFFFF)) {
//         /* basic multilingual plane: single 16-bit word */
//         char s[4] ;
//         s[3] = strlib_aux_hexdigit((unsigned char) ((u & 0x000F) >>  0));
//         s[2] = strlib_aux_hexdigit((unsigned char) ((u & 0x00F0) >>  4));
//         s[1] = strlib_aux_hexdigit((unsigned char) ((u & 0x0F00) >>  8));
//         s[0] = strlib_aux_hexdigit((unsigned char) ((u & 0xF000) >> 12));
//         lua_pushlstring(L, s, 4);
//     } else {
//         /* supplementary planes (U+10000 .. U+10FFFF): UTF-16 surrogate pair */
//         unsigned u1, u2;
//         char     s[8] ;
//         u = u - 0x10000; /* negative when invalid range */
//         u1 = (unsigned) (u >> 10)   + 0xD800; /* high surrogate */
//         u2 = (unsigned) (u % 0x400) + 0xDC00; /* low  surrogate */
//         s[3] = strlib_aux_hexdigit((unsigned char) ((u1 & 0x000F) >>  0));
//         s[2] = strlib_aux_hexdigit((unsigned char) ((u1 & 0x00F0) >>  4));
//         s[1] = strlib_aux_hexdigit((unsigned char) ((u1 & 0x0F00) >>  8));
//         s[0] = strlib_aux_hexdigit((unsigned char) ((u1 & 0xF000) >> 12));
//         s[7] = strlib_aux_hexdigit((unsigned char) ((u2 & 0x000F) >>  0));
//         s[6] = strlib_aux_hexdigit((unsigned char) ((u2 & 0x00F0) >>  4));
//         s[5] = strlib_aux_hexdigit((unsigned char) ((u2 & 0x0F00) >>  8));
//         s[4] = strlib_aux_hexdigit((unsigned char) ((u2 & 0xF000) >> 12));
//         lua_pushlstring(L, s, 8);
//     }
//     return 1;
// }

static const char hex_digits[] = "0123456789ABCDEF";

static inline void write_hex16(char *s, unsigned int val)
{
    s[0] = hex_digits[(val >> 12) & 0xF];
    s[1] = hex_digits[(val >> 8)  & 0xF];
    s[2] = hex_digits[(val >> 4)  & 0xF];
    s[3] = hex_digits[ val        & 0xF];
}

static int strlib_format_tounicode16(lua_State *L)
{
    lua_Integer u = lua_tointeger(L, 1);
    if (invalid_unicode(u)) {
        lua_pushliteral(L, "FFFD");
    } else if (u <= 0xFFFF) {
        /* basic multilingual plane: single 16-bit word */
        char s[4];
        write_hex16(s, (unsigned int) u);
        lua_pushlstring(L, s, 4);
    } else {
        /* supplementary planes (U+10000 .. U+10FFFF): UTF-16 surrogate pair */
        char s[8];
        unsigned int v = (unsigned int) (u - 0x10000);
        unsigned int u1 = (v >> 10)   + 0xD800; /* high surrogate */
        unsigned int u2 = (v & 0x3FF) + 0xDC00; /* low  surrogate */
        write_hex16(s, u1);
        write_hex16(s + 4, u2);
        lua_pushlstring(L, s, 8);
    }
    return 1;
}

static int strlib_format_toutf8(lua_State *L) /* could be integrated into utfcharacter */
{
    if (lua_type(L, 1) == LUA_TTABLE) {
        lua_Integer n = lua_rawlen(L, 1);
        if (n > 0) {
            luaL_Buffer b;
            luaL_buffinitsize(L, &b, (n + 1) * 4);
            for (lua_Integer i = 0; i <= n; i++) {
                /* there should be one operation for getting a number from a table */
                if (lua_rawgeti(L, 1, i) == LUA_TNUMBER) {
                    unsigned u = (unsigned) lua_tointeger(L, -1);
                    if (0x80 > u) {
                        luaL_addchar(&b, (unsigned char) u);
                    } else if (invalid_unicode(u)) {
                        luaL_addchar(&b, 0xFF);
                        luaL_addchar(&b, 0xFD);
                    } else {
                        if (0x800 > u)
                            luaL_addchar(&b, (unsigned char) (0xC0 | (u >> 6)));
                        else {
                            if (0x10000 > u)
                                luaL_addchar(&b, (unsigned char) (0xE0 | (u >> 12)));
                            else {
                                luaL_addchar(&b, (unsigned char) (0xF0 | (u >>18)));
                                luaL_addchar(&b, (unsigned char) (0x80 | (0x3F & (u >> 12))));
                            }
                            luaL_addchar(&b, 0x80 | (0x3F & (u >> 6)));
                        }
                        luaL_addchar(&b, 0x80 | (0x3F & u));
                    }
                }
                lua_pop(L, 1);
            }
            luaL_pushresult(&b);
        } else {
            lua_pushliteral(L, "");
        }
        return 1;
    }
    return 0;
}

static int strlib_format_toutf16(lua_State* L) {
    if (lua_type(L, 1) == LUA_TTABLE) {
        lua_Integer n = lua_rawlen(L, 1);
        if (n > 0) {
            int addzero = lua_toboolean(L, 2);
            luaL_Buffer b;
            luaL_buffinitsize(L, &b, (n + 2) * 4);
            for (lua_Integer i = 0; i <= n; i++) {
                if (lua_rawgeti(L, 1, i) == LUA_TNUMBER) {
                    unsigned u = (unsigned) lua_tointeger(L, -1);
                    if (invalid_unicode(u)) {
                        luaL_addchar(&b, 0xFF);
                        luaL_addchar(&b, 0xFD);
                    } else if (u < 0x10000) {
                        luaL_addchar(&b, (unsigned char) ((u & 0x00FF)     ));
                        luaL_addchar(&b, (unsigned char) ((u & 0xFF00) >> 8));
                    } else {
                        u = u - 0x10000;
                        luaL_addchar(&b, (unsigned char) ((((u >> 10) + 0xD800) & 0x00FF)     ));
                        luaL_addchar(&b, (unsigned char) ((((u >> 10) + 0xD800) & 0xFF00) >> 8));
                        luaL_addchar(&b, (unsigned char) (( (u % 1024 + 0xDC00) & 0x00FF)     ));
                        luaL_addchar(&b, (unsigned char) (( (u % 1024 + 0xDC00) & 0xFF00) >> 8));
                    }
                }
                lua_pop(L, 1);
            }
            if (addzero) { 
                luaL_addchar(&b, 0);
                luaL_addchar(&b, 0);
            }
            luaL_pushresult(&b);
        } else {
            lua_pushliteral(L, "");
        }
        return 1;
    }
    return 0;
}

static int strlib_format_toutf32(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TTABLE) {
        lua_Integer n = lua_rawlen(L, 1);
        if (n > 0) {
            int addzero = lua_toboolean(L, 2);
            luaL_Buffer b;
            luaL_buffinitsize(L, &b, (n + 2) * 4);
            for (lua_Integer i = 0; i <= n; i++) {
                /* there should be one operation for getting a number from a table */
                if (lua_rawgeti(L, 1, i) == LUA_TNUMBER) {
                    unsigned u = (unsigned) lua_tointeger(L, -1);
                    if (invalid_unicode(u)) {
                        luaL_addchar(&b, 0x00);
                        luaL_addchar(&b, 0x00);
                        luaL_addchar(&b, 0xFF);
                        luaL_addchar(&b, 0xFD);
                    } else {
                        luaL_addchar(&b, (unsigned char) ((u & 0x000000FF)      ));
                        luaL_addchar(&b, (unsigned char) ((u & 0x0000FF00) >>  8));
                        luaL_addchar(&b, (unsigned char) ((u & 0x00FF0000) >> 16));
                        luaL_addchar(&b, (unsigned char) ((u & 0xFF000000) >> 24));
                    }
                }
                lua_pop(L, 1);
            }
            if (addzero) { 
                for (int i = 0; i <= 3; i++) {
                    luaL_addchar(&b, 0);
                }
            }
            luaL_pushresult(&b);
        } else {
            lua_pushliteral(L, "");
        }
        return 1;
    }
    return 0;
}

/* 
    str, true       : big endian
    str, false      : little endian
    str, nil, true  : check bom, default to big endian 
    str, nil, false : check bom, default to little endian 
    str, nil, nil   : check bom, default to little endian 
*/

// static int strlib_utf16toutf8(lua_State *L)
// {
//     size_t ls = 0;
//     const char *s = lua_tolstring(L, 1, &ls);
//     if (ls % 2) {
//         --ls;
//     }
//     if (ls) {
//         luaL_Buffer b;
//         int more = 0;
//         int be = 1;
//         size_t i = 0;
//         luaL_buffinitsize(L, &b, ls * 2); /* unlikely to be larger if we have latin */
//         if (lua_type(L, 2) == LUA_TBOOLEAN) {
//             be = lua_toboolean(L, 2);
//         } else if (s[0] == '\xFE' && s[1] == '\xFF') {
//             be = 1;
//             i += 2;
//         } else if (s[0] == '\xFF' && s[1] == '\xFE') {
//             be = 0;
//            i += 2;
//        } else {
//            be = lua_toboolean(L, 3);
//        }
//        while (i < ls) {
//            unsigned char l = (unsigned char) s[i++];
//            unsigned char r = (unsigned char) s[i++];
//            unsigned now = be ? 256 * l + r : l + 256 * r;
//            if (more) {
//                now = (more - 0xD800) * 0x400 + (now - 0xDC00) + 0x10000;
//                more = 0;
//                strlib_aux_add_utfchar(&b, now);
//            } else if (now >= 0xD800 && now <= 0xDBFF) {
//                more = now;
//            } else {
//                strlib_aux_add_utfchar(&b, now);
//            }
//        }
//        luaL_pushresult(&b);
//    } else {
//         lua_pushliteral(L, "");
//     }
//     return 1;
// }

static int strlib_utf16toutf8(lua_State *L)
{
    size_t ls = 0;
    const char *s = lua_tolstring(L, 1, &ls);
    /* ignore trailing odd byte if string length isn't even */
    ls &= ~(size_t) 1;
    if (!ls) {
        lua_pushliteral(L, "");
    } else {
        int be = 1;
        size_t i = 0;
        if (lua_type(L, 2) == LUA_TBOOLEAN) {
            be = lua_toboolean(L, 2);
        } else if (ls >= 2 && (unsigned char) s[0] == 0xFE && (unsigned char) s[1] == 0xFF) {
            be = 1;
            i += 2;
        } else if (ls >= 2 && (unsigned char) s[0] == 0xFF && (unsigned char) s[1] == 0xFE) {
            be = 0;
            i += 2;
        } else {
            be = lua_toboolean(L, 3);
        }
        luaL_Buffer b;
        luaL_buffinitsize(L, &b, (ls - i) * 3 / 2 + 1);
        unsigned int high_surrogate = 0;
        while (i < ls) {
            unsigned char c1 = (unsigned char) s[i++];
            unsigned char c2 = (unsigned char) s[i++];
            unsigned int word = be ? ((unsigned int) c1 << 8) | c2
                                   : ((unsigned int) c2 << 8) | c1;
            if (high_surrogate) {
                if (word >= 0xDC00 && word <= 0xDFFF) {
                    /* valid surrogate pair: decode codepoint */
                    unsigned int codepoint = (((high_surrogate & 0x3FF) << 10) | (word & 0x3FF)) + 0x10000;
                    strlib_aux_add_utfchar(&b, codepoint);
                    high_surrogate = 0;
                    continue;
                } else {
                    /* previous high surrogate was orphan/unpaired */
                    strlib_aux_add_utfchar(&b, 0xFFFD);
                    high_surrogate = 0;
                }
            }
            if (word >= 0xD800 && word <= 0xDBFF) {
                /* high surrogate for next word */
                high_surrogate = word;
            } else if (word >= 0xDC00 && word <= 0xDFFF) {
                /* lone low surrogate */
                strlib_aux_add_utfchar(&b, 0xFFFD);
            } else {
                /* normal BMP character */
                strlib_aux_add_utfchar(&b, word);
            }
        }
        /* trailing unpaired high surrogate at end-of-string */
        if (high_surrogate) {
            strlib_aux_add_utfchar(&b, 0xFFFD);
        }
        luaL_pushresult(&b);
    }
    return 1;
}

static int strlib_pack_rows_columns(lua_State* L)
{
    if (lua_type(L, 1) == LUA_TTABLE) {
        lua_Integer rows = lua_rawlen(L, 1);
        if (lua_rawgeti(L, 1, 1) == LUA_TTABLE) {
            lua_Integer columns = lua_rawlen(L, -1);
            switch (lua_rawgeti(L, -1, 1)) {
                case LUA_TNUMBER:
                    {
                        size_t size = rows * columns;
                        unsigned char *result = lmt_memory_malloc(size);
                        lua_pop(L, 2); /* row and cell */
                        if (result) {
                            unsigned char *first = result;
                            for (lua_Integer r = 1; r <= rows; r++) {
                                if (lua_rawgeti(L, -1, r) == LUA_TTABLE) {
                                    for (lua_Integer c = 1; c <= columns; c++) {
                                        if (lua_rawgeti(L, -1, c) == LUA_TNUMBER) {
                                             lua_Integer v = lua_tointeger(L, -1);
                                            *result++ = v < 0 ? 0 : v > 255 ? 255 : (unsigned char) v;
                                        } else { 
                                            *result++ = 0;
                                        }
                                        lua_pop(L, 1);
                                    }
                                }
                                lua_pop(L, 1);
                            }
                            lua_pushlstring(L, (char *) first, result - first);
                            return 1;
                        }
                    }
                case LUA_TTABLE:
                    {
                        int mode = (int) lua_rawlen(L, -1);
                        size_t size = rows * columns * mode;
                        unsigned char *result = lmt_memory_malloc(size);
                        lua_pop(L, 2); /* row and cell */
                        if (result) {
                            unsigned char *first = result;
                            for (lua_Integer r = 1; r <= rows; r++) {
                                if (lua_rawgeti(L, -1, r) == LUA_TTABLE) {
                                    for (lua_Integer c = 1; c <= columns; c++) {
                                        if (lua_rawgeti(L, -1, c) == LUA_TTABLE) {
                                            for (int i = 1; i <= mode; i++) {
                                                    if (lua_rawgeti(L, -1, i) == LUA_TNUMBER) {
                                                    lua_Integer v = lua_tointeger(L, -1);
                                                    *result++ = v < 0 ? 0 : v > 255 ? 255 : (unsigned char) v;
                                                } else { 
                                                    *result++ = 0;
                                                }
                                                lua_pop(L, 1);
                                            }
                                        }
                                        lua_pop(L, 1);
                                    }
                                }
                                lua_pop(L, 1);
                            }
                            lua_pushlstring(L, (char *) first, result - first);
                            return 1;
                        }
                    }
            }
        }
    }
    lua_pushnil(L);
    return 1;
}

/*tex 
    This converts a hex string to characters. Spacing is ignored and invalid characters result in 
    a false result. Empty strings are okay. Originally we assumed \type {XX XX XX} and just skipped
    single ones but we might as well also handle \type {X X X}. This code is not that critical and
    was introduced when we wanted flexible bitmap definition in \METAPOST\ and \LUA\ as part of the
    \type {potrace} experiments.
*/

// static int strlib_hextocharacters(lua_State *L)
// {
//     size_t ls = 0;
//     const char *s = lua_tolstring(L, 1, &ls);
//     if (ls > 0) {
//         luaL_Buffer b;
//         luaL_buffinitsize(L, &b, ls/2);
//         while (1) {
//             unsigned char first = *s++;
//             switch (first) {
//                 case ' ': case '\n': case '\r': case '\t':
//                     continue;
//                 case '\0':
//                     goto DONE;
//                 default:
//                     {
//                         unsigned char second = *s++;
//                         switch (second) {
//                             case ' ': case '\n': case '\r': case '\t':
//                                 continue;
//                             case '\0':
//                                 goto BAD;
//                             default:
//                                {
//                                    unsigned char chr;
//                                    if (first >= '0' && first <= '9') {
//                                        chr = 16 * (first - '0');
//                                    } else if (first >= 'A' && first <= 'F') {
//                                        chr = 16 * (first - 'A' + 10);
//                                    } else if (first >= 'a' && first <= 'f') {
//                                        chr = 16 * (first - 'a' + 10);
//                                    } else {
//                                        goto BAD;
//                                    }
//                                    if (second >= '0' && second <= '9') {
//                                        chr += second - '0';
//                                    } else if (second >= 'A' && second <= 'F') {
//                                        chr += second - 'A' + 10;
//                                    } else if (second >= 'a' && second <= 'f') {
//                                        chr += second - 'a' + 10;
//                                    } else {
//                                        goto BAD;
//                                    }
//                                    luaL_addchar(&b, chr);
//                                    break;
//                                }
//                        }
//                        break;
//                    }
//            }
//        }
//      DONE:
//        luaL_pushresult(&b);
//        return 1;
//      BAD:
//        lua_pushboolean(L, 0);
//        return 1;
//    } else {
//        lua_pushliteral(L, "");
//         return 1;
//     }
// }

// static int strlib_hextocharacters(lua_State *L)
// {
//     size_t ls = 0;
//     const char *s = lua_tolstring(L, 1, &ls);
//     if (ls == 0) {
//         lua_pushliteral(L, "");
//         return 1;
//     } else {
//         const char *end = s + ls;
//         luaL_Buffer b;
//         luaL_buffinitsize(L, &b, ls / 2);
//         while (s < end) {
//             unsigned char first;
//             do {
//                 if (s >= end) {
//                     goto DONE;
//                 } else {
//                     first = (unsigned char) *s++;
//                 }
//             } while (first == ' ' || first == '\n' || first == '\r' || first == '\t');
//             unsigned char second;
//             do {
//                 if (s >= end) {
//                     goto BAD;
//                 } else {
//                     second = (unsigned char) *s++;
//                 }
//             } while (second == ' ' || second == '\n' || second == '\r' || second == '\t');
//             unsigned char high, low;
//                  if (first >= '0' && first <= '9') high = first - '0';
//             else if (first >= 'A' && first <= 'F') high = first - 'A' + 10;
//             else if (first >= 'a' && first <= 'f') high = first - 'a' + 10;
//             else goto BAD;
//                  if (second >= '0' && second <= '9') low = second - '0';
//             else if (second >= 'A' && second <= 'F') low = second - 'A' + 10;
//             else if (second >= 'a' && second <= 'f') low = second - 'a' + 10;
//             else goto BAD;
//             luaL_addchar(&b, (high << 4) | low);
//         }
//       DONE:
//         luaL_pushresult(&b);
//         return 1;
//       BAD:
//         lua_pushboolean(L, 0);
//         return 1;
//     }
// }

/*
    Here we use a 256-byte lookup table for hex decoding but of course it comes
    at a memory price (as we use shorts as suggested by gemini):

    - 0 .. 15 : valid hex nibbles
    - 256     : whitespace (skip)
    - 257     : invalid character / failure

    It's anyway a nice example of a variant, given the usual reformatting and a bit of
    cleaning up.
*/

static const unsigned short strlib_hex_table[256] = {
    /* 0x00 - 0x1F: control characters \t \n \r are valid spacing */
    257,257,257,257,257,257,257,257,257,256,256,257,257,256,257,257,
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    /* 0x20 - 0x2F: ' ' is valid spacing */
    256,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    /* 0x30 - 0x39: digits '0'-'9' (0 .. 9) */
    0,    1,  2,  3,  4,  5,  6,  7,  8,  9,257,257,257,257,257,257,
    /* 0x40 - 0x4F: uppercase 'A'-'F' (10 .. 15) */
    257, 10, 11, 12, 13, 14, 15,257,257,257,257,257,257,257,257,257,
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    /* 0x60 - 0x6F: lowercase 'a'-'f' (10 .. 15) */
    257, 10, 11, 12, 13, 14, 15,257,257,257,257,257,257,257,257,257,
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    /* 0x80 - 0xFF: high ASCII / non ASCII (all invalid) */
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
    257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,257,
};

static int strlib_hextocharacters(lua_State *L)
{
    size_t ls = 0;
    const char *s = lua_tolstring(L, 1, &ls);
    if (ls == 0) {
        lua_pushliteral(L, "");
        return 1;
    } else {
        const char *end = s + ls;
        luaL_Buffer b;
        luaL_buffinitsize(L, &b, ls / 2);
        /* states :: -1 : looking for 1st digit, >= 0 : holds 1st digit */
        int high_nibble = -1;
        while (s < end) {
            unsigned char c = (unsigned char) *s++;
            unsigned short val = strlib_hex_table[c];
            if (val < 16) {
                if (high_nibble < 0) {
                    /* first digit of pair */
                    high_nibble = val;
                } else {
                    /* second digit: assemble byte and add it to the buffer */
                    luaL_addchar(&b, (char) ((high_nibble << 4) | val));
                    high_nibble = -1;
                }
            } else if (val == 256) {
                /* skip white spacing safely */
                continue;
            } else {
                /* invalid character found */
                lua_pushboolean(L, 0);
                return 1;
            }
        }
        /* a trailing unpaired hex digit is invalid */
        if (high_nibble >= 0) {
            lua_pushboolean(L, 0);
            return 1;
        } else {
            luaL_pushresult(&b);
            return 1;
        }
    }
}

static int strlib_octtointeger(lua_State *L)
{
    const char *s = lua_tostring(L, 1);
 // lua_Integer n = 0;
 // int negate = *s == '-';
 // if (negate) {
 //     s++;
 // }
 // while (*s && n < 0xFFFFFFFF) { /* large enough */
 //     if (*s >= '0' && *s <= '7') {
 //         n = n * 8 + *s - '0';
 //     } else {
 //         break;
 //     }
 //     s++;
 // }
 // lua_pushinteger(L, negate ? -n : n);
    lua_pushinteger(L, strtoul(s, NULL, 8));
    return 1; 
}

static int strlib_dectointeger(lua_State *L)
{
    const char *s = lua_tostring(L, 1);
 // lua_Integer n = 0;
 // int negate = *s == '-';
 // if (negate) {
 //     s++;
 // }
 // while (*s && n < 0xFFFFFFFF) { /* large enough */
 //     if (*s >= '0' && *s <= '9') {
 //         n = n * 10 + *s - '0';
 //     } else {
 //         break;
 //     }
 //     s++;
 // }
 // lua_pushinteger(L, negate ? -n : n);
 // lua_pushinteger(L, atol(s));
    lua_pushinteger(L, strtoul(s, NULL, 10));
    return 1; 
}

static int strlib_hextointeger(lua_State *L)
{
    const char *s = lua_tostring(L, 1);
 // lua_Integer n = 0;
 // int negate = *s == '-';
 // if (negate) {
 //     s++;
 // }
 // while (*s && n < 0xFFFFFFFF) { /* large enough */
 //     if (*s >= '0' && *s <= '9') {
 //         n = n * 16 + *s - '0';
 //     } else if (*s >= 'A' && *s <= 'F') {
 //         n = n * 16 + *s - 'A' + 10;
 //     } else if (*s >= 'a' && *s <= 'f') {
 //         n = n * 16 + *s - 'a' + 10;
 //     } else {
 //        break;
 //     }
 //     s++;
 // }
 // lua_pushinteger(L, negate ? -n : n);
    lua_pushinteger(L, strtoul(s, NULL, 16));
    return 1; 
}

static int strlib_chrtointeger(lua_State *L)
{
    size_t l = 0;
    const char *s = lua_tolstring(L, 1, &l);
    if (l == 0) {
        lua_pushinteger(L, 0);
        return 1;
    } else if (l > sizeof(lua_Integer)) {
        lua_pushboolean(L, 0);
        return 1;
    } else {
        lua_Unsigned n = 0;
        for (size_t p = 0; p < l; p++) {
            n = (n << 8) | (unsigned char)s[p];
        }
        lua_pushinteger(L, (lua_Integer)n);
        return 1;
    }
}

/*tex 
    I considered a version where we |break| on a non-number but in that case we normally know where
    the last useful slot is anyway so I removed that variant. 
*/

static int strlib_utftabletostring(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TTABLE) {
        lua_Integer n = lua_rawlen(L, 1);
        if (n > 0) {
            lua_Integer f = lmt_optinteger(L, 2, 1);
            lua_Integer l = lmt_optinteger(L, 3, n);
            if (f < 1) { f = 1; }
            if (l > n) { l = n; }
            if (l >= f) {
                luaL_Buffer b;
                luaL_buffinitsize(L, &b, (size_t) (l-f+1) * 4); 
                for (lua_Integer i = f; i <= l; i++) {
                    if (lua_rawgeti(L, 1, i) == LUA_TNUMBER) {
                        strlib_aux_add_utfnumber(L, &b, -1);
                        lua_pop(L, 1);
                    }
                }
                luaL_pushresult(&b);
                return 1;
            }
        }
    }
    lua_pushliteral(L, "");
    return 1;
}

static int strlib_splitintolines(lua_State *L)
{
    size_t l = 0;
    const char *s = lua_tolstring(L, 1, &l);
    lua_newtable(L);
    if (l > 0) {
        size_t n = 0;
        lua_Integer i = 0;
        const char *f = s;
        while (*s) {
            if (*s == 13) { 
                /* cr */
                lua_pushlstring(L, f, n);
                lua_rawseti(L, -2, ++i);
                f = ++s; 
                if (*f == 10) { 
                    f = ++s; 
                }
                if (! *f) {
                    return 1; 
                }
                n = 0;
            } else if (*s == 10) { 
                /* lf */
                lua_pushlstring(L, f, n);
                lua_rawseti(L, -2, ++i);
                f = ++s; 
                if (! *f) {
                    return 1; 
                }
                n = 0;
            } else {
                ++n;
                ++s;
            }
        }
        if (f) {
            lua_pushlstring(L, f, n);
            lua_rawseti(L, -2, ++i);
        }
    }
    return 1; 
}

static const luaL_Reg strlib_function_list[] = {
    { "characters",        strlib_characters         },
    { "characterpairs",    strlib_characterpairs     },
    { "bytes",             strlib_bytes              },
    { "bytepairs",         strlib_bytepairs          },
    { "bytetable",         strlib_bytetable          },
    { "linetable",         strlib_linetable          },
    { "utfvalues",         strlib_utfvalues          },
    { "utfcharacters",     strlib_utfcharacters      },
    { "utfcharacter",      strlib_utfcharacter       },
    { "utfvalue",          strlib_utfvalue           },
    { "utflength",         strlib_utflength          },
    { "utfvaluetable",     strlib_utfvaluetable      },
    { "utfcharactertable", strlib_utfcharactertable  },
    { "utftabletostring",  strlib_utftabletostring   },
    { "f6",                strlib_format_f6          },
    { "tounicode16",       strlib_format_tounicode16 },
    { "toutf8",            strlib_format_toutf8      },
    { "toutf16",           strlib_format_toutf16     }, /* untested */
    { "toutf32",           strlib_format_toutf32     },
    { "utf16toutf8",       strlib_utf16toutf8        },
    { "packrowscolumns",   strlib_pack_rows_columns  },
    { "hextocharacters",   strlib_hextocharacters    },
    { "octtointeger",      strlib_octtointeger       },
    { "dectointeger",      strlib_dectointeger       },
    { "hextointeger",      strlib_hextointeger       },
    { "chrtointeger",      strlib_chrtointeger       },
    { "splitintolines",    strlib_splitintolines     },
    /* */
    { NULL,                NULL                      },
};

/*
    The next (old, moved here) experiment was used to check if using some buffer is more efficient
    than using a table that we concat. It makes no difference. If we ever use this, the initializer
    |luaextend_string_buffer| will be merged into |luaextend_string|. We could gain a little on a
    bit more efficient |luaL_checkudata| as we use elsewhere because in practice (surprise) its
    overhead makes buffers like this {\em 50 percent} slower than the concatinated variant and
    twice as slow when we reuse a temporary table. It's just better to stay at the \LUA\ end.

    Replacing the userdata test with a dedicated test gives a speed boost but we're still some 
    {\em 10 percent} slower. So, for now we comment this feature. 
*/

# if (0) 

    /*tex See |lmtinterface.h| for |STRING_BUFFER_METATABLE_INSTANCE|. */

    typedef struct lmt_string_buffer {
        char   *buffer;
        size_t  length;
        size_t  size;
        size_t  step;
    } lmt_string_buffer;

    static lmt_string_buffer *strlib_buffer_instance(lua_State *L)
    {
        lmt_string_buffer *b = (lmt_string_buffer *) lua_touserdata(L, 1);
        if (b && lua_getmetatable(L, 1)) {
            lua_get_metatablelua(string_buffer_instance);
            if (! lua_rawequal(L, -1, -2)) {
                b = NULL;
            } else if (! b->buffer) {
                b = NULL;
            }
            lua_pop(L, 2);
            return b;
        }
        return NULL;
    }

    static int strlib_buffer_gc(lua_State *L)
    {
        lmt_string_buffer *b = strlib_buffer_instance(L);
        if (b) {
            lmt_memory_free(b->buffer);
        }
        return 0;
    }

    static int strlib_buffer_new(lua_State *L)
    {
        size_t size = lmt_optsizet(L, 1, LUAL_BUFFERSIZE);
        size_t step = lmt_optsizet(L, 2, size);
        lmt_string_buffer *b = (lmt_string_buffer *) lua_newuserdatauv(L, sizeof(lmt_string_buffer), 0);
        b->buffer = lmt_memory_malloc(size);
        b->size   = size;
        b->step   = step;
        b->length = 0;
        lua_get_metatablelua(string_buffer_instance);
        lua_setmetatable(L, -2);
        return 1;
    }

    static int strlib_buffer_add(lua_State *L)
    {
        lmt_string_buffer *b = strlib_buffer_instance(L);
        if (b) {
            switch (lua_type(L, 2)) {
                case LUA_TSTRING:
                case LUA_TNUMBER:
                    {
                        size_t l;
                        const char *s = lua_tolstring(L, 2, &l);
                        size_t length = b->length + l;
                        if (length >= b->size) {
                            while (length >= b->size) {
                                 b->size += b->step;
                            }
                            b->buffer = lmt_memory_realloc(b->buffer, b->size);
                        }
                        memcpy(&b->buffer[b->length], s, l);
                        b->length = length;
                    }
                    break;
                default:
                    break;
            }
        }
        return 0;
    }

    static int strlib_buffer_get_data(lua_State *L)
    {
        lmt_string_buffer *b = strlib_buffer_instance(L);
        if (b) {
            lua_pushlstring(L, b->buffer, b->length);
            lua_pushinteger(L, (int) b->length);
            return 2;
        } else {
            lua_pushnil(L);
            return 1;
        }
    }

    static int strlib_buffer_get_size(lua_State *L)
    {
        lmt_string_buffer *b = strlib_buffer_instance(L);
        lua_pushinteger(L, b ? b->length : 0);
        return 1;
    }

    static const luaL_Reg strlib_function_list_buffer[] = {
        { "newbuffer",     strlib_buffer_new      },
        { "addtobuffer",   strlib_buffer_add      },
        { "getbufferdata", strlib_buffer_get_data },
        { "getbuffersize", strlib_buffer_get_size },
        { NULL,            NULL                   },
    };

    static int luaextend_string_buffer(lua_State *L)
    {
        lua_getglobal(L, "string");
        for (const luaL_Reg *lib = strlib_function_list_buffer; lib->name; lib++) {
            lua_pushcfunction(L, lib->func);
            lua_setfield(L, -2, lib->name);
        }
        lua_pop(L, 1);
        luaL_newmetatable(L, STRING_BUFFER_METATABLE_INSTANCE);
        lua_pushcfunction(L, strlib_buffer_gc);
        lua_setfield(L, -2, "__gc");
        lua_pop(L, 1);
        return 1;
    }

# else 

    static int luaextend_string_buffer(lua_State *L)
    {
        (void) L;
        return 0;
    }

# endif 

int luaextend_string(lua_State * L)
{
    lua_getglobal(L, "string");
    for (const luaL_Reg *lib = strlib_function_list; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 1);
    luaextend_string_buffer(L);
    return 1;
}

/*

LUAMOD_API int luaopen_strlib(lua_State *L) {
    // 1. Create the library table (or use luaL_newlib for other functions)
    lua_newtable(L);

    // 2. Push the upvalues onto the stack BEFORE creating the closure
    lua_pushliteral(L, "0"); // Upvalue index 1
    lua_pushliteral(L, "1"); // Upvalue index 2

    // 3. Create the closure with 2 upvalues
    // (This pops the 2 strings off the stack and attaches them to the function)
    lua_pushcclosure(L, strlib_format_f6, 2);

    // 4. Set the function in your library table ( equivalent to: lib["f6"] = strlib_format_f6 )
    lua_setfield(L, -2, "f6");

    return 1; // Return the library table
}

static int strlib_format_f6(lua_State *L)
{
    double n = luaL_optnumber(L, 1, 0.0);

    if (n == 0.0) {
        // Upvalue 1: "0"
        lua_pushvalue(L, lua_upvalueindex(1));
        return 1;
    } else if (n == 1.0) {
        // Upvalue 2: "1"
        lua_pushvalue(L, lua_upvalueindex(2));
        return 1;
    }

    // ...
}

*/