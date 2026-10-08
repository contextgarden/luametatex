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

// # ifndef MIN_INTEGER
//     # define MIN_INTEGER -2147483648.0
//     # define MAX_INTEGER  2147483647.0
// # endif
//
// static int strlib_format_f6(lua_State *L)
// {
//     double n = luaL_optnumber(L, 1, 0.0);
//     /* Fast paths for standard constants */
//     if (n == 0.0) {
//      // if (signbit(n)) {
//      //     lua_pushliteral(L, "-0");
//      // } else {
//             lua_pushliteral(L, "0");
//      // }
//         return 1;
//     } else if (n == 1.0) {
//         lua_pushliteral(L, "1");
//         return 1;
//     } else {
//         char s[64];
//         int len;
//         if (fmod(n, 1.0) == 0.0 && n >= MIN_INTEGER && n <= MAX_INTEGER) {
//             len = snprintf(s, sizeof(s), "%ld", (long) n);
//             lua_pushlstring(L, s, (size_t) len);
//             return 1;
//         } else {
//             if (lua_type(L, 2) == LUA_TSTRING) {
//                 const char *f = lua_tostring(L, 2);
//                 len = snprintf(s, sizeof(s), f, n);
//             } else {
//                 len = snprintf(s, sizeof(s), "%.6f", n);
//             }
//             if (len <= 0 || len >= (int) sizeof(s)) {
//                 lua_pushliteral(L, "0");
//                 return 1;
//             }
//             char *dot = strchr(s, '.');
//             if (dot) {
//                 char *ptr = s + len - 1;
//                 while (ptr > dot && *ptr == '0') {
//                     ptr--;
//                 }
//                 if (ptr == dot) {
//                     ptr--;
//                 }
//                 len = (int) (ptr - s + 1);
//             }
//         }
//         lua_pushlstring(L, s, (size_t) len);
//         return 1;
//     }
// }

# include <inttypes.h>

static int strlib_format_f6(lua_State *L)
{
    double n = luaL_optnumber(L, 1, 0.0);
    if (n == 0.0) {
        lua_pushliteral(L, "0");
        return 1;
    } else if (n == 1.0) {
        lua_pushliteral(L, "1");
        return 1;
    } else {
        char s[64];
        int len;
        /* do we have an integer within 53-bit float precision */
        if (fmod(n, 1.0) == 0.0 && n >= -9007199254740992.0 && n <= 9007199254740992.0) {
            len = snprintf(s, sizeof(s), "%" PRId64, (int64_t) n);
            lua_pushlstring(L, s, (size_t) len);
            return 1;
        }
        /* default */
        const char *fmt = "%.6f";
        if (lua_type(L, 2) == LUA_TSTRING) {
            /* maybe check size? */
            fmt = lua_tostring(L, 2);
        }
        /* we fit */
        len = snprintf(s, sizeof(s), fmt, n);
        /* check anyway */
        if (len <= 0 || len >= (int) sizeof(s)) {
            lua_pushliteral(L, "0");
            return 1;
        }
        /* remove trailing zeros after decimal point */
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
        lua_pushlstring(L, s, (size_t) len);
        return 1;
    }
}

static int strlib_format_g6(lua_State *L)
{
    double n = luaL_optnumber(L, 1, 0.0);
    if (n == 0.0) {
        lua_pushliteral(L, "0");
        return 1;
    } else if (n == 1.0) {
        lua_pushliteral(L, "1");
        return 1;
    } else {
        char s[128];
        /* Fast path: 53-bit exact integers */
        if (fmod(n, 1.0) == 0.0 && n >= -9007199254740992.0 && n <= 9007199254740992.0) {
            int len = snprintf(s, sizeof(s), "%" PRId64, (int64_t) n);
            lua_pushlstring(L, s, (size_t) len);
            return 1;
        }
        /* Standard custom double-to-string conversion (handles exponential notation & trimming deterministically) */
        int len = double_to_string_g(n, s, 6);
        lua_pushlstring(L, s, (size_t) len);
        return 1;
    }
}

static int strlib_format_fd(lua_State *L)
{
    double n = luaL_optnumber(L, 1, 0.0);
    int    m = (int) luaL_optinteger(L, 2, 6);
    if (n == 0.0) {
        lua_pushliteral(L, "0");
        return 1;
    } else if (n == 1.0) {
        lua_pushliteral(L, "1");
        return 1;
    } else {
        char str[128];
        int len = double_to_string_f(n, str, m);
        lua_pushlstring(L, str, (size_t) len);
        return 1;
    }
}

static inline int strlib_format_fdn(lua_State *L, int m)
{
    double n = luaL_optnumber(L, 1, 0.0);
    if (n == 0.0) {
        lua_pushliteral(L, "0");
        return 1;
    } else if (n == 1.0) {
        lua_pushliteral(L, "1");
        return 1;
    } else {
        char str[128];
        int len = double_to_string_f(n, str, m);
        lua_pushlstring(L, str, (size_t) len);
        return 1;
    }
}

static int strlib_format_fd3(lua_State *L) { return strlib_format_fdn(L, 3); }
static int strlib_format_fd6(lua_State *L) { return strlib_format_fdn(L, 6); }
static int strlib_format_fd9(lua_State *L) { return strlib_format_fdn(L, 9); }

static int strlib_format_gd(lua_State *L)
{
    double n = luaL_optnumber(L, 1, 0.0);
    int    m = (int) luaL_optinteger(L, 2, 6);
    if (n == 0.0) {
        lua_pushliteral(L, "0");
        return 1;
    } else if (n == 1.0) {
        lua_pushliteral(L, "1");
        return 1;
    } else {
        char str[128];
        int len = double_to_string_g(n, str, m);
        lua_pushlstring(L, str, (size_t) len);
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

    Previous (less efficient) implementations can be found in the git history (luametatex and
    context). Here we use a 256-byte lookup table for hex decoding but of course it comes at a
    memory price (we use shorts as suggested by gemini):

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
    lua_pushinteger(L, strtoul(s, NULL, 8));
    return 1; 
}

static int strlib_dectointeger(lua_State *L)
{
    const char *s = lua_tostring(L, 1);
    lua_pushinteger(L, strtoul(s, NULL, 10));
    return 1; 
}

static int strlib_hextointeger(lua_State *L)
{
    const char *s = lua_tostring(L, 1);
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

/*tex

    The table serializer in \CONTEXT\ is quite okay but sometimes we have rather large tables,
    like the character database, fonts and a utility file. So, as a test I played with a C based
    serializer variant. Some initial statistics were promising:

    loading char-def.lua : 0.11100000000000002
    table.serialize      : size 6127996, time: 0.312
    string.serialize     : size 6127997, time: 0.053

    So indeed handy to have it as alternative. For this approach we revive an older idea of
    buffers so eventually we might implement it as described at the bottom of this file.

    Because we know the length of constant strings, we can use the more optimal length aware
    append in most cases. Single characters have their own optimal append. We might add more 
    to this repertoire if we ever add the buffer userdata. 

    We start with string buffers. As mentioned this is oldish code revived. To that we added
    some more, in order to support serializing but after that we also added for instance a 
    format feature because we could share code that way. So it became abit larger effort than 
    initially planned. This kind of code is rather easy in the sense that there are no 
    heuristics or unexpected side effects, contrary to the rest of the engine. This is easy 
    going coding, a distraction from more complex matters, and desired interfaces are known  
    and translations of what we already have in \LUA.

*/

# define lmt_string_buffer_default  (1024 * 1024)

typedef struct lmt_string_buffer {
    char   *buffer;
    size_t  length;
    size_t  size;
    size_t  step;
} lmt_string_buffer;

inline static void lmt_buffer_allocate(lmt_string_buffer *b)
{
    b->size   = lmt_string_buffer_default;
    b->length = 0;
    b->buffer = (char *) lmt_memory_malloc(b->size);
    b->step   = 0; /* maybe, as other memory management in lmtx */
}

inline static void lmt_buffer_dispose(lmt_string_buffer *b)
{
    lmt_memory_free(b->buffer);
}

inline static int lmt_buffer_has_room(lmt_string_buffer *b, size_t needed)
{
    if (b->length + needed >= b->size) {
        if (b->step) {
            while (b->length + needed >= b->size) {
                b->size += b->step;
            }
        } else {
            while (b->length + needed >= b->size) {
                b->size *= 2;
            }
        }
        b->buffer = (char *) lmt_memory_realloc(b->buffer, b->size);
        if lmt_unlikely(! b->buffer) { 
            luaL_error(lmt_lua_state.lua_instance, "not enough memory for string.serialize");
            return 0;
        }
    }
    return 1;
}

inline static void lmt_buffer_add_lstring(lmt_string_buffer *b, const char *str, size_t len)
{
    if (lmt_buffer_has_room(b, len)) {
        memcpy(b->buffer + b->length, str, len);
        b->length += len;
    }
}

inline static void lmt_buffer_add_string(lmt_string_buffer *b, const char *str)
{
    lmt_buffer_add_lstring(b, str, strlen(str));
}

inline static void lmt_buffer_add_char(lmt_string_buffer *b, char chr)
{
    if (lmt_buffer_has_room(b, 1)) {
        b->buffer[b->length] = chr;
        b->length += 1;
    }
}

inline static void lmt_buffer_add_spaces(lmt_string_buffer *b, int n)
{
    if (n > 0 && lmt_buffer_has_room(b, n)) {
        memset(b->buffer + b->length, ' ', n);
        b->length += n;
    }
}

/*tex

    The next (old, moved here) experiment was used to check if using some buffer is more efficient
    than using a table that we concat. It makes no difference. The experiment added the helpers to
    the string library but now that we have the sequencer lib we can best add it there. We could
    gain a little on a bit more efficient |luaL_checkudata| as we use elsewhere because in practice
    (surprise) its overhead makes buffers like this {\em 50 percent} slower than the concatinated
    variant and twice as slow when we reuse a temporary table. It's just better to stay at the
    \LUA\ end. Replacing the userdata test with a dedicated test gives a speed boost but we're
    still some {\em 10 percent} slower. So, as the overhead in the engine is little we just enable
    it, if only for experiments.The archive has the previous code, where it hooks into the string 
    library (that we extend anyway).

    Because I knew that there was room for improvement I chat a bit with gemini and we got to 
    a real low level speedup. 

    Baseline C API            0.381s   1.0×  Full C API stack validation + lua_tolstring   
    Direct Pointer Check      0.095s   4.0×  ttisfulluserdata + getudatamem (bypassed C API stack)
    Direct Buffer Formatting  0.063s   6.0×  serialize_integer (bypassed Lua string creation)  
    Lua table.concat          0.032s  11.9×  Inlined VM bytecode instructions (TSETI)

    Using this feature only makes sense when we have massive amounts and don't want to create 
    intermediate tables. So a 50% performance loss is acceptable. After that I added a addformat 
    variant (okay, asked gemini and then refactored that in what we had as serializers) and after 
    playing around abit we got a 400% percent gain over using |string.format|:

    \starttyping
    local add       = sequencer.addtobuffer
    local addformat = sequencer.addformattobuffer
    local format    = string.format

    local n = 500000

    local c = os.clock()
    local b = sequencer.newbuffer(1024*1024)
    for i=1,n do
        add(b,"foo")
        add(b,i)
        addformat(b,"%.10f",1.2345678900) -- extra test
        add(b,"\n")
    end
    print(sequencer.getbuffersize(b),os.clock()-c)

    local c = os.clock()
    local b = { }
    for i=1,n do
        b[#b+1] = "foo"
        b[#b+1] = i
        b[#b+1] = format("%.10f",1.2345678900) -- extra test
        b[#b+1] = "\n"
    end
    b = table.concat(b)
    print(#,os.clock()-c)
    \stoptyping 

    The buffered size is smaller because we strip redundant zero's. So, after all an interesting 
    experiment.

*/

/*tex See |lmtinterface.h| for |STRING_BUFFER_METATABLE_INSTANCE|. */

 // inline static lmt_string_buffer *strlib_buffer_instance(lua_State *L)
 // {
 //     lmt_string_buffer *b = (lmt_string_buffer *) lua_touserdata(L, 1);
 //     if (b && lua_getmetatable(L, 1)) {
 //      // lua_get_metatablelua(string_buffer_instance);
 //         lua_pushvalue(L, lua_upvalueindex(1));
 //         if (! lua_rawequal(L, -1, -2) || ! b->buffer) {
 //             b = NULL;
 //         }
 //         lua_pop(L, 2);
 //         return b;
 //     }
 //     return NULL;
 // }

 // inline static lmt_string_buffer *strlib_buffer_instance(lua_State *L)
 // {
 //     lmt_string_buffer *b = (lmt_string_buffer *) lua_touserdata(L, 1);
 //     if (b && lua_getmetatable(L, 1)) {
 //         /* Compare metatable (-1) directly against upvalue 1 */
 //         if (! lua_rawequal(L, -1, lua_upvalueindex(1)) || ! b->buffer) {
 //             b = NULL;
 //         }
 //         lua_pop(L, 1); /* Pop only the metatable */
 //         return b;
 //     }
 //     return NULL;
 // }

static inline lmt_string_buffer *strlib_buffer_instance(lua_State *L)
{
    /* Fetch argument 1 directly from CallInfo stack frame */
    TValue *arg1 = s2v(L->ci->func.p + 1);
    /* Check for full userdata */
    if (ttisfulluserdata(arg1)) {
        Udata *u = uvalue(arg1);
        /* Get running C closure to inspect Upvalue 1 (the metatable) */
        CClosure *func = clCvalue(s2v(L->ci->func.p));
        Table *expected_mt = hvalue(&func->upvalue[0]);
        /* Direct pointer comparison on metatable headers */
        if (u->metatable == expected_mt) {
            /* LuaMetaTeX / Lua 5.4 uses getudatamem(u) */
            lmt_string_buffer *b = (lmt_string_buffer *) getudatamem(u);
            if (b && b->buffer) {
                return b;
            }
        }
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

static int strlib_buffer_tostring(lua_State *L)
{
    lmt_string_buffer *b = strlib_buffer_instance(L);
    if (b) {
        lua_pushfstring(L, "<buffer %p>", b);
        return 1;
    } else {
        return 0;
    }
}

# define minimum_step 1024

static int strlib_buffer_new(lua_State *L)
{
    size_t size = lmt_optsizet(L, 1, LUAL_BUFFERSIZE);
    size_t step = lmt_optsizet(L, 2, size);
    lmt_string_buffer *b = (lmt_string_buffer *) lua_newuserdatauv(L, sizeof(lmt_string_buffer), 0);
    /* lmt_buffer_allocate(b); */
    b->size   = size;
    b->length = 0;
    b->buffer = (char *) lmt_memory_malloc(b->size);
    b->step   = (step < minimum_step) ? minimum_step : ((step > size) ? size : step); /* let's be reasonable*/
    lua_get_metatablelua(string_buffer_instance);
    lua_setmetatable(L, -2);
    return 1;
}

 // static int strlib_buffer_add(lua_State *L)
 // {
 //     lmt_string_buffer *b = strlib_buffer_instance(L);
 //     if (b) {
 //         switch (lua_type(L, 2)) {
 //             case LUA_TSTRING:
 //             case LUA_TNUMBER:
 //                 {
 //                     size_t l;
 //                     const char *s = lua_tolstring(L, 2, &l);
 //                     lmt_buffer_add_lstring(b, s, l);
 //                 }
 //                 break;
 //             default:
 //                 break;
 //         }
 //     }
 //     return 0;
 // }

static int strlib_buffer_get_data(lua_State *L)
{
    lmt_string_buffer *b = strlib_buffer_instance(L);
    if (b) {
        lua_pushlstring(L, b->buffer, b->length);
        lua_pushinteger(L, (lua_Integer) b->length);
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

static const luaL_Reg sequencerlib_metatable_list[] =
{
    // { "__newindex", strlib_buffer_setvalue  },
    { "__tostring", strlib_buffer_tostring  },
    { "__gc",       strlib_buffer_gc        },
    { NULL,         NULL                    },
};

/*tex

    Right after we started with \MKIV\ we needed serialization of (often large) tables. Of 
    course we used \LUA. That code could either construct a table with strings (to be concat)
    or write to file. That handle overhead was later avoided by a variant. We also wanted 
    some number values to be hex as it's easier with \UNICODE\ but in the end the overhead 
    made that a seldom used option. We also kept track of circular references but again, that
    was never useful at this level so we dealt with that differently. We also moved on to 
    using formatters, because those are faster than \LUA's formatter. 

    Move some to \CCODE\ was already on the agenda and prototyped in a helperlib that is 
    not part of the regular distribution. We now include an update to this.

*/

/* 
    This is a typical case where one can prompt an llm framework: can you make me a lookup table
    for this. Most languages need something like this when they serialize, so here is what Gemini 
    cooked up (not that much faster but neat anyway). It actually did it wrong but I wanted |\xNN| 
    anyway that got fixed. This replaces a loop with per-character escape checking, pretty much 
    what we do at the \LUA\ end. The |> 127| slots are unchanged because \LUA\ is byte agnostic.  
*/

typedef struct {
    char    bytes[4];
    uint8_t len;
} char_esc_t;

#define ESC1(c)          { { (char) (c), 0, 0, 0 }, 1 }
#define ESC2(a, b)       { { a,          b, 0, 0 }, 2 }
#define ESC3(a, b, c)    { { a,          b, c, 0 }, 3 }
#define ESC4(a, b, c, d) { { a,          b, c, d }, 4 }

static const char_esc_t ESCAPE_LUT[256] = {
    /* 0x00 - 0x07 */ ESC4('\\','x','0','0'), ESC4('\\','x','0','1'), ESC4('\\','x','0','2'), ESC4('\\','x','0','3'), ESC4('\\','x','0','4'), ESC4('\\','x','0','5'), ESC4('\\','x','0','6'), ESC4('\\','x','0','7'),
    /* 0x08 - 0x0F */ ESC4('\\','x','0','8'), ESC2('\\','t'),         ESC2('\\','n'),         ESC4('\\','x','0','B'), ESC4('\\','x','0','C'), ESC2('\\','r'),         ESC4('\\','x','0','E'), ESC4('\\','x','0','F'),
    /* 0x10 - 0x17 */ ESC4('\\','x','1','0'), ESC4('\\','x','1','1'), ESC4('\\','x','1','2'), ESC4('\\','x','1','3'), ESC4('\\','x','1','4'), ESC4('\\','x','1','5'), ESC4('\\','x','1','6'), ESC4('\\','x','1','7'),
    /* 0x18 - 0x1F */ ESC4('\\','x','1','8'), ESC4('\\','x','1','9'), ESC4('\\','x','1','A'), ESC4('\\','x','1','B'), ESC4('\\','x','1','C'), ESC4('\\','x','1','D'), ESC4('\\','x','1','E'), ESC4('\\','x','1','F'),
    /* 0x20 - 0x27 */ ESC1(' '),              ESC1('!'),              ESC2('\\','"'),         ESC1('#'),              ESC1('$'),              ESC1('%'),              ESC1('&'),              ESC1('\''),
    /* 0x28 - 0x2F */ ESC1('('),              ESC1(')'),              ESC1('*'),              ESC1('+'),              ESC1(','),              ESC1('-'),              ESC1('.'),              ESC1('/'),
    /* 0x30 - 0x37 */ ESC1('0'),              ESC1('1'),              ESC1('2'),              ESC1('3'),              ESC1('4'),              ESC1('5'),              ESC1('6'),              ESC1('7'),
    /* 0x38 - 0x3F */ ESC1('8'),              ESC1('9'),              ESC1(':'),              ESC1(';'),              ESC1('<'),              ESC1('='),              ESC1('>'),              ESC1('?'),
    /* 0x40 - 0x47 */ ESC1('@'),              ESC1('A'),              ESC1('B'),              ESC1('C'),              ESC1('D'),              ESC1('E'),              ESC1('F'),              ESC1('G'),
    /* 0x48 - 0x4F */ ESC1('H'),              ESC1('I'),              ESC1('J'),              ESC1('K'),              ESC1('L'),              ESC1('M'),              ESC1('N'),              ESC1('O'),
    /* 0x50 - 0x57 */ ESC1('P'),              ESC1('Q'),              ESC1('R'),              ESC1('S'),              ESC1('T'),              ESC1('U'),              ESC1('V'),              ESC1('W'),
    /* 0x58 - 0x5F */ ESC1('X'),              ESC1('Y'),              ESC1('Z'),              ESC1('['),              ESC2('\\','\\'),        ESC1(']'),              ESC1('^'),              ESC1('_'),
    /* 0x60 - 0x67 */ ESC1('`'),              ESC1('a'),              ESC1('b'),              ESC1('c'),              ESC1('d'),              ESC1('e'),              ESC1('f'),              ESC1('g'),
    /* 0x68 - 0x6F */ ESC1('h'),              ESC1('i'),              ESC1('j'),              ESC1('k'),              ESC1('l'),              ESC1('m'),              ESC1('n'),              ESC1('o'),
    /* 0x70 - 0x77 */ ESC1('p'),              ESC1('q'),              ESC1('r'),              ESC1('s'),              ESC1('t'),              ESC1('u'),              ESC1('v'),              ESC1('w'),
    /* 0x78 - 0x7F */ ESC1('x'),              ESC1('y'),              ESC1('z'),              ESC1('{'),              ESC1('|'),              ESC1('}'),              ESC1('~'),              ESC4('\\','x','7','F'),
    /* 0x80 - 0xFF */ // High bytes (128-255) map to single bytes (UTF-8 compatible)
    ESC1(128), ESC1(129), ESC1(130), ESC1(131), ESC1(132), ESC1(133), ESC1(134), ESC1(135), ESC1(136), ESC1(137), ESC1(138), ESC1(139), ESC1(140), ESC1(141), ESC1(142), ESC1(143),
    ESC1(144), ESC1(145), ESC1(146), ESC1(147), ESC1(148), ESC1(149), ESC1(150), ESC1(151), ESC1(152), ESC1(153), ESC1(154), ESC1(155), ESC1(156), ESC1(157), ESC1(158), ESC1(159),
    ESC1(160), ESC1(161), ESC1(162), ESC1(163), ESC1(164), ESC1(165), ESC1(166), ESC1(167), ESC1(168), ESC1(169), ESC1(170), ESC1(171), ESC1(172), ESC1(173), ESC1(174), ESC1(175),
    ESC1(176), ESC1(177), ESC1(178), ESC1(179), ESC1(180), ESC1(181), ESC1(182), ESC1(183), ESC1(184), ESC1(185), ESC1(186), ESC1(187), ESC1(188), ESC1(189), ESC1(190), ESC1(191),
    ESC1(192), ESC1(193), ESC1(194), ESC1(195), ESC1(196), ESC1(197), ESC1(198), ESC1(199), ESC1(200), ESC1(201), ESC1(202), ESC1(203), ESC1(204), ESC1(205), ESC1(206), ESC1(207),
    ESC1(208), ESC1(209), ESC1(210), ESC1(211), ESC1(212), ESC1(213), ESC1(214), ESC1(215), ESC1(216), ESC1(217), ESC1(218), ESC1(219), ESC1(220), ESC1(221), ESC1(222), ESC1(223),
    ESC1(224), ESC1(225), ESC1(226), ESC1(227), ESC1(228), ESC1(229), ESC1(230), ESC1(231), ESC1(232), ESC1(233), ESC1(234), ESC1(235), ESC1(236), ESC1(237), ESC1(238), ESC1(239),
    ESC1(240), ESC1(241), ESC1(242), ESC1(243), ESC1(244), ESC1(245), ESC1(246), ESC1(247), ESC1(248), ESC1(249), ESC1(250), ESC1(251), ESC1(252), ESC1(253), ESC1(254), ESC1(255)
};

#undef ESC1
#undef ESC2
#undef ESC3
#undef ESC4

static void serialize_string_escaped(lmt_string_buffer *b, const char *s, size_t len)
{
    if (lmt_buffer_has_room(b, len * 4 + 2)) {
        b->buffer[b->length++] = '"';
        for (size_t i = 0; i < len; i++) {
            unsigned char c = (unsigned char)s[i];
            char_esc_t esc = ESCAPE_LUT[c];
            memcpy(b->buffer + b->length, esc.bytes, esc.len);
            b->length += esc.len;
        }
        b->buffer[b->length++] = '"';
    }
}

/*tex

    We can use this: 

    \starttyping
    static void serialize_string(lmt_string_buffer *b, const char *s, size_t len)
    {
        if (lmt_buffer_has_room(b, (len * 2) + 2)) {
            b->buffer[b->length++] = '"';      
            for (size_t i = 0; i < len; i++) {
                char c = s[i];
                if (c == '"' || c == '\\' ) {
                    b->buffer[b->length++] = '\\';
                }           
                b->buffer[b->length++] = c;
            }
            b->buffer[b->length++] = '"';
        }
    }
    \stoptyping

    Instead we use an editor safe variant instead. It adds e.g. 3K to a dejavu stream table of 
    nearly 500K so that's ok. After all we use the bytecode compiler version which is smaller 
    anyway. 

*/

static void serialize_string(lmt_string_buffer *b, const char *s, size_t len)
{
    if (lmt_buffer_has_room(b, (len * 2) + 2)) {
        b->buffer[b->length++] = '"';             
        for (size_t i = 0; i < len; i++) {
            char c = s[i];
            switch (c) {
                /* we keep nul, e.g. scite just shows that well */
                case '"': 
                case '\\':                     
                    b->buffer[b->length++] = '\\';
                    b->buffer[b->length++] = c;
                    break;
                case '\f':
                    b->buffer[b->length++] = '\\';
                    b->buffer[b->length++] = 'f';
                    break;
                case '\n':
                    b->buffer[b->length++] = '\\';
                    b->buffer[b->length++] = 'n';
                    break;
                case '\r':
                    b->buffer[b->length++] = '\\';
                    b->buffer[b->length++] = 'r';
                    break;
                case '\t':
                    b->buffer[b->length++] = '\\';
                    b->buffer[b->length++] = 't';
                    break;
                case '\v':
                    b->buffer[b->length++] = '\\';
                    b->buffer[b->length++] = 'v';
                    break;
                default:
                    b->buffer[b->length++] = c;
                    break;
            }           
        }       
        b->buffer[b->length++] = '"';
    }
}

static void serialize_double(lmt_string_buffer *b, double val) 
{
    /* 26 is enough for both the largest float and integer including a zero terminator */
    if (lmt_buffer_has_room(b, 26)) {
        char *dest = b->buffer + b->length;
        /* prevent undefined behavior on NaN, Infinity, or huge doubles */
        if (val >= -9223372036854775808.0 && val < 9223372036854775808.0) {
            int64_t i = (int64_t) val;           
            /* preserve -0.0 by falling back to %a when i == 0 and signbit is set */
            if ((double) i == val && ! (i == 0 && signbit(val))) {
                b->length += snprintf(dest, 26, "%" PRId64, i);
                return;
            }
        }
        /* float | NaN | Inf | -0.0 */
        b->length += snprintf(dest, 26, "%a", val);
    }
}

static void serialize_integer(lmt_string_buffer *b, lua_Integer val) 
{
    if (lmt_buffer_has_room(b, 21)) {
        char *dest = b->buffer + b->length;
        b->length += snprintf(dest, 21, "%" LUA_INTEGER_FRMLEN "d", val);
    }
}

static int strlib_buffer_add(lua_State *L)
{
    lmt_string_buffer *b = strlib_buffer_instance(L);
    if (b) {
        int top = lua_gettop(L);
        for (int i = 2; i <= top; i++) {
            switch (lua_type(L, i)) {
                case LUA_TSTRING: 
                    {
                        size_t l;
                        const char *s = lua_tolstring(L, i, &l);
                        lmt_buffer_add_lstring(b, s, l);
                        break;
                    }
                case LUA_TNUMBER: 
                    {
                        if (lua_isinteger(L, 2)) {
                            /* Write integer directly into b->buffer (zero Lua string creation) */
                            serialize_integer(b, lua_tointeger(L, i));
                        } else {
                            /* Write double directly into b->buffer using standard formatting */
                            if (lmt_buffer_has_room(b, 64)) {
                                char *dest = b->buffer + b->length;
                                b->length += double_to_string_f(lua_tonumber(L, i), dest, 17);
                            }
                        }
                        break;
                    }
                case LUA_TBOOLEAN: 
                    {
                        if (lua_toboolean(L, i)) {
                            lmt_buffer_add_lstring(b, "true", 4);
                        } else {
                            lmt_buffer_add_lstring(b, "false", 5);
                        }
                        break;
                    }
                default:
                    break;
            }
        }
    }
    return 0;
}


/*tex

    With a bit of gemini help I stepwise added a formatter. Intermediate and final checks 
    stepwise made the buffering faster. A kind of fun excercize as long as one stays in 
    control and does specific suggestions and corrects when needed. Kind of a game and this 
    is straightforward code. 

    \startty    ping 
    local n = 500000

    local format   = string.format 
    local f        = string.formatters["%i %N %N %N %N %N %N cm"]
    local sqformat = sequencer.format
    local s

    local c = os.clock()
    for i=1,n do
        s = format("%i %f %f %f %f %f %f cm",i,1.999,2.999,3.999,4.999,5.999,6.999)
    end
    print("format   ",os.clock()-c,s)

    local c = os.clock()
    for i=1,n do
        s = f(i,1.999,2.999,3.999,4.999,5.999,6.999)
    end
    print("formatter",os.clock()-c,s)

    local c = os.clock()
    for i=1,n do
        local b = newbuffer(64)
        addformat(b,"%i %f %f %f %f %f %f cm",i,1.999,2.999,3.999,4.999,5.999,6.999)
        s = getbuffer(b)
    end
    print("buffer  ",os.clock()-c,s)

    local c = os.clock()
    for i=1,n do
        s = sqformat("%i %N %N %N %N %N %N cm",i,1.999,2.999,3.999,4.999,5.999,6.999)
    end
    print("sqformat",os.clock()-c,s)
    \stoptyping 

    \starttabulate[|l|r|r|r|]
    \HL
    \NC \bf Method
    \NC \bf 6 Floats (\type{%N})
    \NC \bf 1 Int + 6 Floats (\type{%i %N...})
    \NC \bf Delta (Overhead of \type{%i}) \NR
    \HL
    \NC \type{string.format}                \NC 1.036s \NC 1.097s     \NC +0.061s (+5.8\%) \NR
    \NC \type{formatter}                    \NC 0.334s \NC \bf 0.460s \NC \bf +0.126s (+37.7\% penalty) \NR
    \NC \type{buffer} (per-loop allocation) \NC 1.080s \NC 1.128s     \NC +0.048s (+4.4\%) \NR
    \NC \type{sqformat}                     \NC 0.336s \NC \bf 0.354s \NC \bf +0.018s (+5.3\% penalty) \NR
    \HL
    \stoptabulate

    Key Takeaways

    \startitemize
        \startitem 
            formatter (ConTeXt Lua Generator): Performs best when format strings are static and 
            consist purely of fast primitive conversions, but takes a performance hit as format 
            complexity and argument count grow due to Lua string concatenation chain overhead.
        \stopitem 
        \startitem 
            sequencer.format (sqformat): Handles complex, multi-type format strings (integers, 
            floats, hex, strings, booleans) with virtually linear performance scaling.
        \stopitem 
        \startitem 
            sequencer.addformattobuffer (Persistent Buffer): Remaining in C memory across 
            multiple steps without returning an intermediate string to Lua remains the fastest 
            path for bulk output (PDF/SVG streams, file writing, macro expansion).
        \stopitem 
    \stopitemize 

*/

static void strlib_aux_addformat(lua_State *L, lmt_string_buffer *b, int fmt_idx, int arg_idx)
{
    size_t      fmt_len;
    const char *fmt   = luaL_checklstring(L, fmt_idx, &fmt_len);
    size_t      i     = 0;
    size_t      start = 0;

    while (i < fmt_len) {
        if (fmt[i] == '%') {
            /* flush preceding chunk of literal text */
            if (i > start) {
                lmt_buffer_add_lstring(b, fmt + start, i - start);
            }

            /* skip '%' */
            i++;

            int width         = 0;
            int pad_zero      = 0;
            int precision     = 17;
            int has_precision = 0;

            /* parse optional '0' padding flag (e.g. %04d, %04X) */
            if (i < fmt_len && fmt[i] == '0') {
                pad_zero = 1;
                i++;
            }

            /* parse width (e.g. %4X or %04X) */
            while (i < fmt_len && fmt[i] >= '0' && fmt[i] <= '9') {
                width = (width * 10) + (fmt[i] - '0');
                i++;
            }

            /* parse precision specifier (e.g. %.4X or %.3f) */
            if (i < fmt_len && fmt[i] == '.') {
                i++;
                precision     = 0;
                has_precision = 1;
                while (i < fmt_len && fmt[i] >= '0' && fmt[i] <= '9') {
                    precision = (precision * 10) + (fmt[i] - '0');
                    i++;
                }
            }

            /* sanity check */
            if (i >= fmt_len) {
                break;
            }

            char spec = fmt[i++];
            switch (spec) {
                case '%':
                    lmt_buffer_add_char(b, '%');
                    break;
                case 'd':
                case 'i': 
                    { 
                        /* integer with zero-padding support */
                        lua_Integer val = lua_tointeger(L, arg_idx++);
                        if (width > 0) {
                            if (lmt_buffer_has_room(b, width + 32)) {
                                char *dest = b->buffer + b->length;
                                char fmt_str[32];
                                snprintf(fmt_str, sizeof(fmt_str), "%%%s%d" LUA_INTEGER_FRMLEN "d", pad_zero ? "0" : "", width);
                                b->length += snprintf(dest, width + 32, fmt_str, val);
                            }
                        } else {
                            serialize_integer(b, val);
                        }
                        break;
                    }
                case 'f': 
                    { 
                        /* standard C/Lua float (retains fixed trailing zeros) */
                        lua_Number val = lua_tonumber(L, arg_idx++);
                        if (lmt_buffer_has_room(b, 64)) {
                            char *dest = b->buffer + b->length;
                            int prec = has_precision ? precision : 6;
                            b->length += snprintf(dest, 64, "%.*f", prec, val);
                        }
                        break;
                    }
                case 'N':
                case 'n': 
                    {
                        /* stripped Float (ConTeXt formatters drop-in: strips redundant zeros) */                    lua_Number val = lua_tonumber(L, arg_idx++);
                        if (lmt_buffer_has_room(b, 64)) {
                            char *dest = b->buffer + b->length;
                            b->length += double_to_string_f(val, dest, has_precision ? precision : 17);
                        }
                        break;
                    }
                case 'g': 
                    { 
                        /* truncated / Shortest Float */
                        lua_Number val = lua_tonumber(L, arg_idx++);
                        if (lmt_buffer_has_room(b, 64)) {
                            char *dest = b->buffer + b->length;
                            b->length += double_to_string_g(val, dest, has_precision ? precision : 6);
                        }
                        break;
                    }
                case 'x':
                case 'X':
                    { 
                        /* hexadecimal with zero-padding support */
                        lua_Unsigned val = (lua_Unsigned) lua_tointeger(L, arg_idx++);
                        int min_digits = has_precision ? precision : width;
                        if (min_digits <= 0) {
                            min_digits = 1;
                        }
                        char              temp[20];
                        int               len         = 0;
                        static const char hex_upper[] = "0123456789ABCDEF";
                        static const char hex_lower[] = "0123456789abcdef";
                        const char        *digits     = (spec == 'X') ? hex_upper : hex_lower;
                        do {
                            temp[len++] = digits[val & 0x0F];
                            val >>= 4;
                        } while (val > 0);
                        int total_len = (len < min_digits) ? min_digits : len;
                        if (lmt_buffer_has_room(b, total_len)) {
                            for (int z = 0; z < total_len - len; z++) {
                                b->buffer[b->length++] = '0';
                            }
                            while (len > 0) {
                                b->buffer[b->length++] = temp[--len];
                            }
                        }
                        break;
                    }
                case 's': 
                    {
                        size_t slen;
                        const char *s = lua_tolstring(L, arg_idx++, &slen);
                        if (s) {
                            lmt_buffer_add_lstring(b, s, slen);
                        }
                        break;
                    }
                case 'q': 
                    {
                        size_t slen;
                        const char *s = lua_tolstring(L, arg_idx++, &slen);
                        if (s) {
                            serialize_string(b, s, slen);
                        } else {
                            lmt_buffer_add_lstring(b, "\"\"", 2);
                        }
                        break;
                    }
                case 'b': 
                    {
                        int val = lua_toboolean(L, arg_idx++);
                        if (val) {
                            lmt_buffer_add_lstring(b, "true", 4);
                        } else {
                            lmt_buffer_add_lstring(b, "false", 5);
                        }
                        break;
                    }
                default:
                    lmt_buffer_add_char(b, '%');
                    lmt_buffer_add_char(b, spec);
                    break;
            }
            start = i;
        } else {
            i++;
        }
    }

    /* flush remaining literal tail */
    if (i > start) {
        lmt_buffer_add_lstring(b, fmt + start, i - start);
    }
}

/* 
    1  : buffer userdata 
    2  : format string 
    3+ : arguments 
*/

static int strlib_buffer_addformat(lua_State *L)
{
    lmt_string_buffer *b = strlib_buffer_instance(L);
    if (b) {
        strlib_aux_addformat(L, b, 2, 3);
    }
    return 0;
}

/* 
    1  : format string 
    2+ : arguments 
*/

static int strlib_format(lua_State *L)
{
    lmt_string_buffer buffer;
    lmt_buffer_allocate(&buffer);
    strlib_aux_addformat(L, &buffer, 1, 2);
    lua_pushlstring(L, buffer.buffer, buffer.length);
    lmt_buffer_dispose(&buffer);
    return 1;
}

/* 
    An easy one, no escaping etc.: 

    1 : buffer 
    2 : string 
    3 : [offset (1-based)]
    4 : [length]
*/

static int strlib_buffer_addbytes(lua_State *L)
{
    lmt_string_buffer *b = strlib_buffer_instance(L);
    if (b) {
        size_t slen;
        const char *s = lua_tolstring(L, 2, &slen);
        if (s) {
            lua_Integer offset = luaL_optinteger(L, 3, 1) - 1; 
            lua_Integer len    = luaL_optinteger(L, 4, (lua_Integer)(slen - offset));
            if (offset >= 0 && (size_t) offset < slen && len > 0) {
                if ((size_t) (offset + len) > slen) {
                    len = slen - offset;
                }
                lmt_buffer_add_lstring(b, s + offset, (size_t) len);
            }
        }
    }
    return 0;
}

/*
    A bit like lua pack but simple (we have plenty helpers elsewhere): 

    1 : buffer
    2 : format character ('b', 'w', 'l', 'q' or 'W', 'L', 'Q' for Big Endian)
    3 : integer
*/

static int strlib_buffer_addpacking(lua_State *L)
{
    lmt_string_buffer *b = strlib_buffer_instance(L);
    if (b) {
        const char *fmt = lua_tostring(L, 2);
        if (!fmt) return 0;

        lua_Integer val = lua_tointeger(L, 3);

        switch (fmt[0]) {
            case 'b': /* 8-bit uint */
            case 'B':
                if (lmt_buffer_has_room(b, 1)) {
                    b->buffer[b->length++] = (char) (val & 0xFF);
                }
                break;
            case 'w': /* 16-bit Little Endian */ /* wide character ala windows */
                if (lmt_buffer_has_room(b, 2)) {
                    b->buffer[b->length++] = (char)  (val       & 0xFF);
                    b->buffer[b->length++] = (char) ((val >> 8) & 0xFF);
                }
                break;
            case 'W': /* 16-bit Big Endian */ /* wide character ala windows */
                if (lmt_buffer_has_room(b, 2)) {
                    b->buffer[b->length++] = (char) ((val >> 8) & 0xFF);
                    b->buffer[b->length++] = (char)  (val       & 0xFF);
                }
                break;
            case 'l': /* 32-bit Little Endian */
                if (lmt_buffer_has_room(b, 4)) {
                    b->buffer[b->length++] = (char)  (val        & 0xFF);
                    b->buffer[b->length++] = (char) ((val >>  8) & 0xFF);
                    b->buffer[b->length++] = (char) ((val >> 16) & 0xFF);
                    b->buffer[b->length++] = (char) ((val >> 24) & 0xFF);
                }
                break;
            case 'L': /* 32-bit Big Endian */
                if (lmt_buffer_has_room(b, 4)) {
                    b->buffer[b->length++] = (char) ((val >> 24) & 0xFF);
                    b->buffer[b->length++] = (char) ((val >> 16) & 0xFF);
                    b->buffer[b->length++] = (char) ((val >>  8) & 0xFF);
                    b->buffer[b->length++] = (char)  (val        & 0xFF);
                }
                break;
            default:
                break;
        }
    }
    return 0;
}

/*tex
    We now come to the table serializers. We need to retain the same sorting as the original \LUA\
    variant. This evolved a bit.
*/

# define lmt_key_default 64

/*tex 

    We don't need (have) long strings, so a 32 bit integer is okay for the length. The sort type 
    also dictates the  order. It's hard to measure gain but on various large tables we're about 
    5 times as as efficient as the original \LUA\ variant that we had around for about two 
    decades. I might (as experiment bring back hex indexes but then we also need control over 
    specific fields so plenty of extra overhead). 

    There is quite a bit of repetition in the code below but so is in the \LUA\ originals. When 
    implementing this kind of code one has to test compatibility and I think we're fine. The 
    integer check when we have a double is actually better so we acoid hex floats when possible. 

    For sure this will evolve a bit more. I also need to check this with the key sorters elsewhere
    as they were the starting point but became a bit redundant now. But one step at a time (after 
    all we've been fine for quite a while now.)

    Capitalized like LUA_T....:
*/ 

typedef enum lmt_sort_type {
    LMT_TYPE_NIL = 0,  // serialized tables normally have sane keys 
    LMT_TYPE_FALSE,    // 0 integer 
    LMT_TYPE_TRUE,     // 1 integer 
    LMT_TYPE_INTEGER,  // NN
    LMT_TYPE_DOUBLE,   // NN.MM after NN 
    LMT_TYPE_STRING,
    /* not used as key in a serialize */
    LMT_TYPE_TABLE,
    LMT_TYPE_FUNCTION,
    LMT_TYPE_USERDATA,
} lmt_sort_type;

typedef struct lmt_serialize_sort_data {
    uint32_t        type;          /* 4 bytes */
    uint32_t        string_length; /* 4 bytes */
    union {                        /* 8 bytes */
        lua_Integer integer_value;
        double      double_value;
        const char *string_value;
    };
} lmt_serialize_sort_data;

static int lmt_serialize_compare(const void *a, const void *b)
{
    const lmt_serialize_sort_data *ka = (const lmt_serialize_sort_data *) a;
    const lmt_serialize_sort_data *kb = (const lmt_serialize_sort_data *) b;
    /* Both elements have the exact same type tag. */
    if (ka->type == kb->type) {
        switch (ka->type) {
            case LMT_TYPE_INTEGER:
                return (ka->integer_value > kb->integer_value) 
                     - (ka->integer_value < kb->integer_value);
            case LMT_TYPE_DOUBLE:
                return (ka->double_value > kb->double_value) 
                     - (ka->double_value < kb->double_value);
            case LMT_TYPE_STRING: {
                uint32_t min_len = ka->string_length < kb->string_length 
                                 ? ka->string_length : kb->string_length;
                int cmp = memcmp(ka->string_value, kb->string_value, min_len);
                if (cmp != 0) {
                    return cmp;
                } else {
                    return (ka->string_length > kb->string_length) 
                         - (ka->string_length < kb->string_length);
                }
            }
            default:
                /* NIL, FALSE, and TRUE are identical to themselves. */
                return 0;
        }
    }
    /* Mixed number comparison (e.g. INT vs FLOAT: 1 < 1.5 < 2). */
    if ((ka->type == LMT_TYPE_INTEGER || ka->type == LMT_TYPE_DOUBLE) &&
        (kb->type == LMT_TYPE_INTEGER || kb->type == LMT_TYPE_DOUBLE)) 
    {
        double da = (ka->type == LMT_TYPE_INTEGER) ? (double) ka->integer_value : ka->double_value;
        double db = (kb->type == LMT_TYPE_INTEGER) ? (double) kb->integer_value : kb->double_value;
        return (da > db) - (da < db);
    }
    /* Different primary types (ordered by enum: NIL < FALSE < TRUE < INT/FLOAT < STRING) */
    return (ka->type < kb->type) ? -1 : 1;
}

static void strlib_aux_serialize(lua_State *L, int index, lmt_string_buffer *b, int depth, int escaped)
{
    index = lua_absindex(L, index);

    int maxkeys = lmt_key_default;
    int nofkeys = 0;

    lmt_serialize_sort_data *keys = (lmt_serialize_sort_data *) lmt_memory_malloc(maxkeys * sizeof(lmt_serialize_sort_data));

    /* First we collect keys. */

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (nofkeys >= maxkeys) {
            maxkeys *= 2;
            keys = (lmt_serialize_sort_data *) lmt_memory_realloc(keys, maxkeys * sizeof(lmt_serialize_sort_data));
        }

        lmt_serialize_sort_data *sk = &keys[nofkeys];

        switch (lua_type(L, -2)) {
            case LUA_TNUMBER:
                if (lua_isinteger(L, -2)) {
                    sk->type          = LMT_TYPE_INTEGER;
                    sk->integer_value = lua_tointeger(L, -2);
                } else {
                    sk->type         = LMT_TYPE_DOUBLE;
                    sk->double_value = lua_tonumber(L, -2);
                }
                nofkeys++;
                break;
            case LUA_TBOOLEAN:
                sk->type = lua_toboolean(L, -2) ? LMT_TYPE_TRUE : LMT_TYPE_FALSE;
                nofkeys++;
                break;
            case LUA_TSTRING: {
                size_t len;
                sk->type          = LMT_TYPE_STRING;
                sk->string_value  = lua_tolstring(L, -2, &len);
                sk->string_length = (uint32_t) len;
                nofkeys++;
                break;
            }
            default:
                /* Ignore unsupported key types (tables, functions, userdata) */
                break;
        }
        lua_pop(L, 1);
    }

    /*tex We quit when we have an empty table or one with weird keys. */

    if (nofkeys == 0) {
        lmt_buffer_add_lstring(b, "{}", 2);
        lmt_memory_free(keys);
        return;
    }

    /*tex We sort as we do in the \LUA\ original. */

    qsort(keys, nofkeys, sizeof(lmt_serialize_sort_data), lmt_serialize_compare);

    /*tex We want simple array-only tables as one-liners. */

    int is_sequence = 1;
    for (int i = 0; i < nofkeys; i++) {
        lmt_serialize_sort_data *sk = &keys[i];

        if (sk->type != LMT_TYPE_INTEGER || sk->integer_value != (i + 1)) {
            is_sequence = 0;
            break;
        }
        lua_pushinteger(L, sk->integer_value);
        lua_gettable(L, index);
        if (lua_istable(L, -1)) {
            is_sequence = 0;
        }
        lua_pop(L, 1);
        if (! is_sequence) {
            break;
        }
    }

    /* Now we can start serializing the (sub)table. */

    lmt_buffer_add_lstring(b, is_sequence ? "{ " : "{\n", 2);

    /* We're serialize the entries. */

    lua_Integer expected_index = 1;

    for (int i = 0; i < nofkeys; i++) {
        lmt_serialize_sort_data *sk = &keys[i];
        /* We push the key back onto the stack. */
        switch (sk->type) {
            case LMT_TYPE_INTEGER : lua_pushinteger(L, sk->integer_value); break;
            case LMT_TYPE_DOUBLE  : lua_pushnumber (L, sk->double_value); break;
            case LMT_TYPE_FALSE   : lua_pushboolean(L, 0); break;
            case LMT_TYPE_TRUE    : lua_pushboolean(L, 1); break;
            case LMT_TYPE_STRING  : lua_pushlstring(L, sk->string_value, sk->string_length); break;
            default               : lua_pushnil    (L); break;
        }

        lua_pushvalue(L, -1);
        lua_gettable(L, index);

        int v_index = lua_gettop(L);

        /* Serialize the key but only when we have no sequence. */

        if (! is_sequence) {
            lmt_buffer_add_spaces(b, depth);
            int is_sequence_key = 0;
            switch (sk->type) {
                case LMT_TYPE_INTEGER:
                    if (sk->integer_value == expected_index) {
                        is_sequence_key = 1;
                        expected_index++;
                    }
                    if (! is_sequence_key) {
                        lmt_buffer_add_char(b, '[');
                        serialize_integer(b, sk->integer_value);
                        lmt_buffer_add_lstring(b, "]=", 2);
                    }
                    break;
                case LMT_TYPE_DOUBLE:
                    lmt_buffer_add_char(b, '[');
                    serialize_double(b, sk->double_value);
                    lmt_buffer_add_lstring(b, "]=", 2);
                    break;
                case LMT_TYPE_STRING:
                    lmt_buffer_add_char(b, '[');
                    if lmt_unlikely(escaped) { 
                        serialize_string_escaped(b, sk->string_value, sk->string_length);
                    } else {
                        serialize_string(b, sk->string_value, sk->string_length);
                    }
                    lmt_buffer_add_lstring(b, "]=", 2);
                    break;
                case LMT_TYPE_FALSE:
                    lmt_buffer_add_lstring(b, "[false]=", 8);
                    break;
                case LMT_TYPE_TRUE:
                    lmt_buffer_add_lstring(b, "[true]=", 7);
                    break;
                default:
                    break;
            }
        }

        /* Serialize the value. */

        switch (lua_type(L, v_index)) {
            case LUA_TNUMBER:
                if (lua_isinteger(L, v_index)) { 
                    serialize_integer(b, lua_tointeger(L, v_index));
                } else {
                    serialize_double(b, lua_tonumber(L, v_index));
                }
                break;
            case LUA_TSTRING: {
                size_t len;
                const char *v_str = lua_tolstring(L, v_index, &len);
                serialize_string(b, v_str, len);
                break;
            }
            case LUA_TBOOLEAN:
                if (lua_toboolean(L, v_index)) {
                    lmt_buffer_add_lstring(b, "true", 4);
                } else {
                    lmt_buffer_add_lstring(b, "false", 5);
                }
                break;
            case LUA_TTABLE:
                strlib_aux_serialize(L, v_index, b, depth + 1, escaped);
                break;
            default:
                lmt_buffer_add_lstring(b, "nil", 3);
                break;
        }

        /* Add comma's and newlines. */

        if (! is_sequence) {
            lmt_buffer_add_lstring(b, ",\n", 2);
        } else if (i < nofkeys - 1) {
            lmt_buffer_add_lstring(b, ", ", 2);
        }

        lua_pop(L, 2);
    }

     /* We can end the table and of course we need to handle spaces. */

    if (is_sequence) {
        lmt_buffer_add_lstring(b, " }", 2);
    } else {
        if (depth > 1) {
            lmt_buffer_add_spaces(b, depth - 1);
        }
        lmt_buffer_add_char(b, '}');
    }

    lmt_memory_free(keys);
}

static int strlib_serialize(lua_State *L)
{
    lmt_string_buffer buffer;
    lmt_buffer_allocate(&buffer);
    switch (lua_type(L, 2)) {
        case LUA_TSTRING:
            {
                const char *name = lua_tostring(L, 2);
                if (strcmp(name, "return") == 0) {
                    lmt_buffer_add_lstring(&buffer, "return ", 7);
                } else {
                    lmt_buffer_add_string(&buffer, name);
                    lmt_buffer_add_char(&buffer, '=');
                }
            }
            break;
        case LUA_TNUMBER:
            {
                size_t len;
                const char *num_str = lua_tolstring(L, 2, &len);
                lmt_buffer_add_char(&buffer, '[');
                lmt_buffer_add_lstring(&buffer, num_str, len);
                lmt_buffer_add_lstring(&buffer, "]=", 2);
            }
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, 2)) {
                lmt_buffer_add_lstring(&buffer, "return ", 7);
                break;
            }
            FALLTHROUGH
        default:
            lmt_buffer_add_lstring(&buffer, "t=", 2);
            break;
    }
    if (! lua_istable(L, 1)) {
        lmt_buffer_add_lstring(&buffer, "{}", 2);
    } else {
        strlib_aux_serialize(L, 1, &buffer, 1, lua_type(L, 3) == LUA_TBOOLEAN ? lua_toboolean(L, 3) : 0);
    }
    lua_pushlstring(L, buffer.buffer, buffer.length);
    lmt_buffer_dispose(&buffer);
    return 1;
}

static void serialize_value(lua_State *L, int index, lmt_string_buffer *b)
{
    switch (lua_type(L, index)) {
        case LUA_TNUMBER:
            if (lua_isinteger(L, index)) {
                serialize_integer(b, lua_tointeger(L, index));
            } else {
                serialize_double(b, lua_tonumber(L, index));
            }
            break;
        case LUA_TSTRING: {
            size_t len;
            const char *s = lua_tolstring(L, index, &len);
            serialize_string(b, s, len);
            break;
        }
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, index)) {
                lmt_buffer_add_lstring(b, "true", 4);
            } else {
                lmt_buffer_add_lstring(b, "false", 5);
            }
            break;
        default:
            lmt_buffer_add_lstring(b, "nil", 3);
            break;
    }
}

static void strlib_aux_fastserialize(lua_State *L, int index, lmt_string_buffer *b, int outer)
{
    index = lua_absindex(L, index);

    lmt_buffer_add_char(b, '{');

    lua_Unsigned n = lua_rawlen(L, index);

    if (n > 0) {

        /* We handle index 0 special. */

        lua_rawgeti(L, index, 0);
        if (! lua_isnil(L, -1)) {
            lmt_buffer_add_lstring(b, "[0]=", 4);
            if (lua_istable(L, -1)) {
                strlib_aux_fastserialize(L, lua_gettop(L), b, 0);
            } else {
                serialize_value(L, -1, b);
                lmt_buffer_add_char(b, ',');
            }
        }
        lua_pop(L, 1);

        /* The array part, if present, comes first. */

        for (lua_Unsigned i = 1; i <= n; i++) {
            lua_rawgeti(L, index, (lua_Integer)i);
            lmt_buffer_add_char(b, '[');
            serialize_integer(b, (lua_Integer)i);
            lmt_buffer_add_lstring(b, "]=", 2);
            if (lua_istable(L, -1)) {
                strlib_aux_fastserialize(L, lua_gettop(L), b, 0);
            } else {
                serialize_value(L, -1, b);
                lmt_buffer_add_char(b, ',');
            }
            lua_pop(L, 1);
        }
    }

    /* The hash part comes next, but of course we need to skip the array entries. */

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        int process = 1;
        if (lua_type(L, -2) == LUA_TNUMBER) {
            lua_Number k = lua_tonumber(L, -2);
            if (k <= (lua_Number) n && k >= 0) {
                process = 0; 
            }
        }
        if (process) {
            lmt_buffer_add_char(b, '[');
            serialize_value(L, -2, b);
            lmt_buffer_add_lstring(b, "]=", 2);
            if (lua_istable(L, -1)) {
                strlib_aux_fastserialize(L, lua_gettop(L), b, 0);
            } else {
                serialize_value(L, -1, b);
                lmt_buffer_add_char(b, ',');
            }
        }
        /* pop value, retain key for next iteration */
        lua_pop(L, 1);
    }

    if (outer) {
        lmt_buffer_add_char(b, '}');
    } else {
        lmt_buffer_add_lstring(b, "},", 2);
    }
}

static int strlib_fastserialize(lua_State *L)
{
    lmt_string_buffer buffer;
    lmt_buffer_allocate(&buffer);

    if (lua_type(L, 2) == LUA_TSTRING) {
        size_t len;
        const char *prefix = lua_tolstring(L, 2, &len);
        lmt_buffer_add_lstring(&buffer, prefix, len);
    } else {
        lmt_buffer_add_lstring(&buffer, "return", 6);
    }

    if (lua_istable(L, 1)) {
        strlib_aux_fastserialize(L, 1, &buffer, 1);
    } else {
        lmt_buffer_add_lstring(&buffer, "{}", 2);
    }

    lua_pushlstring(L, buffer.buffer, buffer.length);
    lmt_buffer_dispose(&buffer);

    return 1;
}

/*tex

    Once we had the above in place, and given the buffer infrastructure, the next bunch 
    was easy: auto converted and cleaned up cq. optimized afterwards. 

*/ 

/* Helper structure for zero-copy entry sorting */

typedef struct tabstr_entry {
    const char *str;
    uint32_t    len;
} tabstr_entry;

typedef struct tabstr_item {
    uint32_t offset;
    uint32_t len;
} tabstr_item;

static int tabstr_entry_compare(const void *a, const void *b)
{
    const tabstr_entry *ea = (const tabstr_entry *)a;
    const tabstr_entry *eb = (const tabstr_entry *)b;
    uint32_t minlen = ea->len < eb->len ? ea->len : eb->len;
    int cmp = memcmp(ea->str, eb->str, minlen);
    if (cmp != 0) {
        return cmp;
    } else {
        return (ea->len > eb->len) - (ea->len < eb->len);
    }
}

/* Helper to append key/value strings safely without mutating Lua keys during lua_next */

static inline void tabstr_add_value_string(lua_State *L, int idx, lmt_string_buffer *b)
{
    size_t len;
    /* 
        luaL_tolstring pushes the string representation onto the top of stack
        without mutating the value at 'idx' (critical for lua_next key safety). 
    */
    const char *s = luaL_tolstring(L, idx, &len);
    lmt_buffer_add_lstring(b, s, len);
    lua_pop(L, 1);
}

/* tabstr_normal */

static void strlib_aux_tabstr_normal(lua_State *L, int index, lmt_string_buffer *out)
{
    index = lua_absindex(L, index);

    int maxentries = 16;
    int nofentries = 0;
    tabstr_item *entries = (tabstr_item *) lmt_memory_malloc(maxentries * sizeof(tabstr_item));

    lmt_string_buffer text_buf;
    lmt_buffer_allocate(&text_buf);

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (nofentries >= maxentries) {
            maxentries *= 2;
            entries = (tabstr_item *) lmt_memory_realloc(entries, maxentries * sizeof(tabstr_item));
        }

        size_t start_off = text_buf.length;

        /* Append Key */
        tabstr_add_value_string(L, -2, &text_buf);

        /* Value rules:
           - table:  k .. ">" .. tabstr_normal(v)
           - true:   k .. "+"
           - truthy: k .. "=" .. v
           - falsy:  k .. "-"
        */
        if (lua_istable(L, -1)) {
            lmt_buffer_add_char(&text_buf, '>');
            strlib_aux_tabstr_normal(L, lua_gettop(L), &text_buf);
        } else if (lua_isboolean(L, -1) && lua_toboolean(L, -1)) {
            lmt_buffer_add_char(&text_buf, '+');
        } else if (lua_toboolean(L, -1)) { /* truthy (non-boolean, non-nil, non-table) */
            lmt_buffer_add_char(&text_buf, '=');
            tabstr_add_value_string(L, -1, &text_buf);
        } else { /* falsy (false or nil) */
            lmt_buffer_add_char(&text_buf, '-');
        }

        entries[nofentries].offset = (uint32_t) start_off;
        entries[nofentries].len    = (uint32_t) (text_buf.length - start_off);
        nofentries++;

        lua_pop(L, 1);
    }

    if (nofentries == 1) {
        lmt_buffer_add_lstring(out, text_buf.buffer + entries[0].offset, entries[0].len);
    } else if (nofentries > 1) {
        tabstr_entry *sort_list = (tabstr_entry *) lmt_memory_malloc(nofentries * sizeof(tabstr_entry));
        for (int i = 0; i < nofentries; i++) {
            sort_list[i].str = text_buf.buffer + entries[i].offset;
            sort_list[i].len = entries[i].len;
        }
        qsort(sort_list, nofentries, sizeof(tabstr_entry), tabstr_entry_compare);

        for (int i = 0; i < nofentries; i++) {
            if (i > 0) {
                lmt_buffer_add_char(out, ',');
            }
            lmt_buffer_add_lstring(out, sort_list[i].str, sort_list[i].len);
        }
        lmt_memory_free(sort_list);
    }

    lmt_memory_free(entries);
    lmt_buffer_dispose(&text_buf);
}

static int strlib_tabstr_normal(lua_State *L)
{
    if (! lua_istable(L, 1)) {
        lua_pushliteral(L, "");
        return 1;
    }
    lmt_string_buffer buffer;
    lmt_buffer_allocate(&buffer);
    strlib_aux_tabstr_normal(L, 1, &buffer);
    lua_pushlstring(L, buffer.buffer, buffer.length);
    lmt_buffer_dispose(&buffer);
    return 1;
}

/* tabstr_flat */

static int strlib_tabstr_flat(lua_State *L)
{
    if (! lua_istable(L, 1)) {
        lua_pushliteral(L, "");
        return 1;
    }

    int maxentries = 16;
    int nofentries = 0;
    tabstr_item *entries = (tabstr_item *) lmt_memory_malloc(maxentries * sizeof(tabstr_item));

    lmt_string_buffer text_buf;
    lmt_buffer_allocate(&text_buf);

    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        if (nofentries >= maxentries) {
            maxentries *= 2;
            entries = (tabstr_item *) lmt_memory_realloc(entries, maxentries * sizeof(tabstr_item));
        }

        size_t start_off = text_buf.length;
        tabstr_add_value_string(L, -2, &text_buf);
        lmt_buffer_add_char(&text_buf, '=');
        tabstr_add_value_string(L, -1, &text_buf);

        entries[nofentries].offset = (uint32_t) start_off;
        entries[nofentries].len    = (uint32_t) (text_buf.length - start_off);
        nofentries++;

        lua_pop(L, 1);
    }

    lmt_string_buffer out;
    lmt_buffer_allocate(&out);

    if (nofentries == 1) {
        lmt_buffer_add_lstring(&out, text_buf.buffer + entries[0].offset, entries[0].len);
    } else if (nofentries > 1) {
        tabstr_entry *sort_list = (tabstr_entry *) lmt_memory_malloc(nofentries * sizeof(tabstr_entry));
        for (int i = 0; i < nofentries; i++) {
            sort_list[i].str = text_buf.buffer + entries[i].offset;
            sort_list[i].len = entries[i].len;
        }
        qsort(sort_list, nofentries, sizeof(tabstr_entry), tabstr_entry_compare);

        for (int i = 0; i < nofentries; i++) {
            if (i > 0) {
                lmt_buffer_add_char(&out, ',');
            }
            lmt_buffer_add_lstring(&out, sort_list[i].str, sort_list[i].len);
        }
        lmt_memory_free(sort_list);
    }

    lua_pushlstring(L, out.buffer, out.length);
    lmt_memory_free(entries);
    lmt_buffer_dispose(&text_buf);
    lmt_buffer_dispose(&out);
    return 1;
}

/* tabstr_mixed (array indexed 1..n, no sorting) */

static int strlib_tabstr_mixed(lua_State *L)
{
    if (! lua_istable(L, 1)) {
        lua_pushliteral(L, "");
        return 1;
    }

    lua_Unsigned n = lua_rawlen(L, 1);
    if (n == 0) {
        lua_pushliteral(L, "");
        return 1;
    }

    lmt_string_buffer b;
    lmt_buffer_allocate(&b);

    for (lua_Unsigned i = 1; i <= n; i++) {
        if (i > 1) {
            lmt_buffer_add_char(&b, ',');
        }
        lua_rawgeti(L, 1, (lua_Integer)i);

        if (lua_isboolean(L, -1)) {
            if (lua_toboolean(L, -1)) {
                lmt_buffer_add_lstring(&b, "++", 2);
            } else {
                lmt_buffer_add_lstring(&b, "--", 2);
            }
        } else {
            tabstr_add_value_string(L, -1, &b);
        }
        lua_pop(L, 1);
    }

    lua_pushlstring(L, b.buffer, b.length);
    lmt_buffer_dispose(&b);
    return 1;
}

/* tabstr_boolean */

static int strlib_tabstr_boolean(lua_State *L)
{
    if (! lua_istable(L, 1)) {
        lua_pushliteral(L, "");
        return 1;
    }

    int maxentries = 16;
    int nofentries = 0;
    tabstr_item *entries = (tabstr_item *) lmt_memory_malloc(maxentries * sizeof(tabstr_item));

    lmt_string_buffer text_buf;
    lmt_buffer_allocate(&text_buf);

    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        if (nofentries >= maxentries) {
            maxentries *= 2;
            entries = (tabstr_item *) lmt_memory_realloc(entries, maxentries * sizeof(tabstr_item));
        }

        size_t start_off = text_buf.length;
        tabstr_add_value_string(L, -2, &text_buf);

        if (lua_toboolean(L, -1)) {
            lmt_buffer_add_char(&text_buf, '+');
        } else {
            lmt_buffer_add_char(&text_buf, '-');
        }

        entries[nofentries].offset = (uint32_t) start_off;
        entries[nofentries].len    = (uint32_t) (text_buf.length - start_off);
        nofentries++;

        lua_pop(L, 1);
    }

    lmt_string_buffer out;
    lmt_buffer_allocate(&out);

    if (nofentries == 1) {
        lmt_buffer_add_lstring(&out, text_buf.buffer + entries[0].offset, entries[0].len);
    } else if (nofentries > 1) {
        tabstr_entry *sort_list = (tabstr_entry *) lmt_memory_malloc(nofentries * sizeof(tabstr_entry));
        for (int i = 0; i < nofentries; i++) {
            sort_list[i].str = text_buf.buffer + entries[i].offset;
            sort_list[i].len = entries[i].len;
        }
        qsort(sort_list, nofentries, sizeof(tabstr_entry), tabstr_entry_compare);

        for (int i = 0; i < nofentries; i++) {
            if (i > 0) {
                lmt_buffer_add_char(&out, ',');
            }
            lmt_buffer_add_lstring(&out, sort_list[i].str, sort_list[i].len);
        }
        lmt_memory_free(sort_list);
    }

    lua_pushlstring(L, out.buffer, out.length);
    lmt_memory_free(entries);
    lmt_buffer_dispose(&text_buf);
    lmt_buffer_dispose(&out);
    return 1;
}

/* 
    From this it made sense to also provide a native loop over a sorted hash, another 
    oldie we use a lot. We're constent in the sorting. 
*/

static int strlib_aux_sorted_next(lua_State *L)
{
    lua_Integer  idx = lua_tointeger(L, lua_upvalueindex(3));
    lua_Unsigned len = lua_rawlen(L, lua_upvalueindex(2));

    if ((lua_Unsigned) idx > len) {
        return 0; /* Iteration complete */
    }

    /* Advance iterator index upvalue */

    lua_pushinteger(L, idx + 1);
    lua_replace(L, lua_upvalueindex(3));

    /* Push key from sorted keys array (Upvalue 2) */

    lua_rawgeti(L, lua_upvalueindex(2), idx);

    /* Push value from target table (Upvalue 1) */

    lua_pushvalue(L, -1);
    lua_gettable(L, lua_upvalueindex(1));

    return 2; /* Yields key, value */
}

static int strlib_sortedhash(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    int index   = lua_absindex(L, 1);
    int maxkeys = lmt_key_default;
    int nofkeys = 0;

    lmt_serialize_sort_data *keys = (lmt_serialize_sort_data *) lmt_memory_malloc(maxkeys * sizeof(lmt_serialize_sort_data));

    /* Collect all sortable keys. */

    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (nofkeys >= maxkeys) {
            maxkeys *= 2;
            keys = (lmt_serialize_sort_data *) lmt_memory_realloc(keys, maxkeys * sizeof(lmt_serialize_sort_data));
        }
        lmt_serialize_sort_data *sk = &keys[nofkeys];

        switch (lua_type(L, -2)) {
            case LUA_TNUMBER:
                if (lua_isinteger(L, -2)) {
                    sk->type          = LMT_TYPE_INTEGER;
                    sk->integer_value = lua_tointeger(L, -2);
                } else {
                    sk->type         = LMT_TYPE_DOUBLE;
                    sk->double_value = lua_tonumber(L, -2);
                }
                nofkeys++;
                break;
            case LUA_TBOOLEAN:
                sk->type = lua_toboolean(L, -2) ? LMT_TYPE_TRUE : LMT_TYPE_FALSE;
                nofkeys++;
                break;
            case LUA_TSTRING: {
                size_t len;
                sk->type          = LMT_TYPE_STRING;
                sk->string_value  = lua_tolstring(L, -2, &len);
                sk->string_length = (uint32_t) len;
                nofkeys++;
                break;
            }
            default:
                break;
        }
        lua_pop(L, 1);
    }

    /* Sort these keys using the already existing comparator. */

    if (nofkeys > 1) {
        qsort(keys, nofkeys, sizeof(lmt_serialize_sort_data), lmt_serialize_compare);
    }

    /* Construct an array of sorted keys. */

    lua_createtable(L, nofkeys, 0);
    int keys_table_idx = lua_gettop(L);

    for (int i = 0; i < nofkeys; i++) {
        lmt_serialize_sort_data *sk = &keys[i];
        switch (sk->type) {
            case LMT_TYPE_INTEGER:
                lua_pushinteger(L, sk->integer_value);
                break;
            case LMT_TYPE_DOUBLE:
                lua_pushnumber(L, sk->double_value);
                break;
            case LMT_TYPE_FALSE:
                lua_pushboolean(L, 0);
                break;
            case LMT_TYPE_TRUE:
                lua_pushboolean(L, 1);
                break;
            case LMT_TYPE_STRING:
                lua_pushlstring(L, sk->string_value, sk->string_length);
                break;
            default:
                lua_pushnil(L);
                break;
        }
        lua_rawseti(L, keys_table_idx, i + 1);
    }

    lmt_memory_free(keys);

    /* Return a C closure with upvalues (the modern approach):

       1: target table
       2: sorted keys array table
       3: first index counter

    */

    lua_pushvalue(L, 1);
    lua_pushvalue(L, keys_table_idx);
    lua_pushinteger(L, 1);

    /* And off we go! */

    lua_pushcclosure(L, strlib_aux_sorted_next, 3);
    return 1;
}

/*tex

    These are (for instance) part of a hash, not that critical in terms of performance but it fits 
    the repertoire. 

*/

static void strlib_aux_sequenced(
    lua_State         *L, 
    int                index, 
    lmt_string_buffer *buffer,
    int                sep_is_true, 
    const char        *sep_str, 
    size_t             sep_len, 
    int                simple
)
{
    index = lua_absindex(L, index);

    if (sep_is_true) {
        lmt_buffer_add_lstring(buffer, "{ ", 2);
    }

    lua_Unsigned n = lua_rawlen(L, index);

    if (n > 0) {
        /* array part */
        for (lua_Unsigned i = 1; i <= n; i++) {
            if (i > 1) {
                if (sep_is_true) {
                    lmt_buffer_add_lstring(buffer, ", ", 2);
                } else {
                    lmt_buffer_add_lstring(buffer, sep_str, sep_len);
                }
            }
            lua_rawgeti(L, index, (lua_Integer)i);
            if (lua_istable(L, -1)) {
                lmt_buffer_add_char(buffer, '{');
                strlib_aux_sequenced(L, lua_gettop(L), buffer, sep_is_true, sep_str, sep_len, simple);
                lmt_buffer_add_char(buffer, '}');
            } else {
                tabstr_add_value_string(L, -1, buffer);
            }
            lua_pop(L, 1);
        }
    } else {
        /* hashed part (sorted keys) */
        int maxkeys = lmt_key_default;
        int nofkeys = 0;
        lmt_serialize_sort_data *keys = (lmt_serialize_sort_data *) lmt_memory_malloc(maxkeys * sizeof(lmt_serialize_sort_data));

        lua_pushnil(L);
        while (lua_next(L, index) != 0) {

            if (nofkeys >= maxkeys) {
                maxkeys *= 2;
                keys = (lmt_serialize_sort_data *) lmt_memory_realloc(keys, maxkeys * sizeof(lmt_serialize_sort_data));
            }

            lmt_serialize_sort_data *sk = &keys[nofkeys];

            switch (lua_type(L, -2)) {
                case LUA_TNUMBER:
                    if (lua_isinteger(L, -2)) {
                        sk->type          = LMT_TYPE_INTEGER;
                        sk->integer_value = lua_tointeger(L, -2);
                    } else {
                        sk->type         = LMT_TYPE_DOUBLE;
                        sk->double_value = lua_tonumber(L, -2);
                    }
                    nofkeys++;
                    break;
                case LUA_TBOOLEAN:
                    sk->type = lua_toboolean(L, -2) ? LMT_TYPE_TRUE : LMT_TYPE_FALSE;
                    nofkeys++;
                    break;
                case LUA_TSTRING: {
                    size_t len;
                    sk->type          = LMT_TYPE_STRING;
                    sk->string_value  = lua_tolstring(L, -2, &len);
                    sk->string_length = (uint32_t) len;
                    nofkeys++;
                    break;
                }
                default:
                    break;
            }
            lua_pop(L, 1);
        }

        if (nofkeys > 1) {
            qsort(keys, nofkeys, sizeof(lmt_serialize_sort_data), lmt_serialize_compare);
        }

        int entries_added = 0;

        for (int i = 0; i < nofkeys; i++) {
            lmt_serialize_sort_data *sk = &keys[i];

            /* key */
            switch (sk->type) {
                case LMT_TYPE_INTEGER: lua_pushinteger(L, sk->integer_value); break;
                case LMT_TYPE_DOUBLE : lua_pushnumber (L, sk->double_value); break;
                case LMT_TYPE_FALSE  : lua_pushboolean(L, 0); break;
                case LMT_TYPE_TRUE   : lua_pushboolean(L, 1); break;
                case LMT_TYPE_STRING : lua_pushlstring(L, sk->string_value, sk->string_length); break;
                default:               lua_pushnil    (L); break;
            }

            /* value */
            lua_pushvalue(L, -1);
            lua_gettable(L, index);

            int v_type  = lua_type(L, -1);
            int process = 0;
            int is_true = 0;

            if (simple) {
                if (v_type == LUA_TBOOLEAN && lua_toboolean(L, -1)) {
                    process = 1;
                    is_true = 1;
                } else if (v_type != LUA_TNIL && !(v_type == LUA_TBOOLEAN && ! lua_toboolean(L, -1))) {
                    if (v_type == LUA_TSTRING) {
                        size_t slen;
                        lua_tolstring(L, -1, &slen);
                        if (slen > 0) {
                            process = 1;
                        }
                    } else {
                        process = 1;
                    }
                }
            } else {
                process = 1;
            }

            if (process) {
                if (entries_added > 0) {
                    if (sep_is_true) {
                        lmt_buffer_add_lstring(buffer, ", ", 2);
                    } else {
                        lmt_buffer_add_lstring(buffer, sep_str, sep_len);
                    }
                }
                tabstr_add_value_string(L, -2, buffer);  /* key */
                if (! is_true) {
                    lmt_buffer_add_char(buffer, '=');
                    if (v_type == LUA_TTABLE) {
                        lmt_buffer_add_char(buffer, '{');
                        strlib_aux_sequenced(L, lua_gettop(L), buffer, sep_is_true, sep_str, sep_len, simple);
                        lmt_buffer_add_char(buffer, '}');
                    } else {
                        tabstr_add_value_string(L, -1, buffer);
                    }
                }
                entries_added++;
            }

            lua_pop(L, 2); /* pop key and value */
        }

        lmt_memory_free(keys);
    }

    if (sep_is_true) {
        lmt_buffer_add_lstring(buffer, " }", 2);
    }
}

static int strlib_sequenced(lua_State *L)
{
    if (lua_isnoneornil(L, 1) || (lua_isboolean(L, 1) && !lua_toboolean(L, 1))) {
        lua_pushliteral(L, "");
        return 1;
    }

    if (! lua_istable(L, 1)) {
        size_t len;
        const char *s = luaL_tolstring(L, 1, &len);
        lua_pushlstring(L, s, len);
        return 1;
    }

    /* parse 'sep' parameter */

    int         sep_is_true = 0;
    const char *sep_str     = " | ";
    size_t      sep_len     = 3;

    if (lua_isboolean(L, 2)) {
        if (lua_toboolean(L, 2)) {
            sep_is_true = 1;
        }
    } else if (lua_type(L, 2) == LUA_TSTRING) {
        sep_str = lua_tolstring(L, 2, &sep_len);
    }

    /* parse 'simple' parameter */

    int simple = lua_toboolean(L, 3);

    lmt_string_buffer buffer;
    lmt_buffer_allocate(&buffer);

    strlib_aux_sequenced(L, 1, &buffer, sep_is_true, sep_str, sep_len, simple);

    lua_pushlstring(L, buffer.buffer, buffer.length);
    lmt_buffer_dispose(&buffer);
    return 1;
}

/*tex

    We define these function in the |strlib| namespace but eventually register them in their own
    library |sequencer|. They could have gone into |table| but they are rather specific and target
    our usage, although serialize is kind of generic. 

*/

static const luaL_Reg sequencerlib_function_list[] = {
    { "serialize",          strlib_serialize          },
    { "fastserialize",      strlib_fastserialize      },
    { "tabstr_normal",      strlib_tabstr_normal      },
    { "tabstr_flat",        strlib_tabstr_flat        },
    { "tabstr_mixed",       strlib_tabstr_mixed       },
    { "tabstr_boolean",     strlib_tabstr_boolean     },
    { "sortedhash",         strlib_sortedhash         },
    { "sequenced",          strlib_sequenced          },
    { "format",             strlib_format             },
    /* */
    { "newbuffer",          strlib_buffer_new         },
    { "addtobuffer",        strlib_buffer_add         },
    { "addformattobuffer",  strlib_buffer_addformat   },
    { "addbytestobuffer",   strlib_buffer_addbytes    },
    { "addpackingtobuffer", strlib_buffer_addpacking  },
    { "getbufferdata",      strlib_buffer_get_data    },
    { "getbuffersize",      strlib_buffer_get_size    },
    /* */
    { NULL,                NULL                       },
};

int luaopen_sequencer(lua_State *L)
{
    luaL_newmetatable(L, STRING_BUFFER_METATABLE_INSTANCE); /* metatable */
    luaL_setfuncs(L, sequencerlib_metatable_list, 0);
    lua_newtable(L);                                        /* metatable library */
    lua_pushvalue(L, -2);                                   /* metatable library metatable */
    luaL_setfuncs(L, sequencerlib_function_list, 1);        /* metatable library */ /* upvalue 1: metatable */
    lua_remove(L, -2);                                      /* library */
    return 1;
}

/* */

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
    { "g6",                strlib_format_g6          },
    { "fd",                strlib_format_fd          },
    { "fd3",               strlib_format_fd3         }, /* no argument needed */
    { "fd6",               strlib_format_fd6         }, /* idem */
    { "fd9",               strlib_format_fd9         }, /* idem */
    { "gd",                strlib_format_gd          }, /* seldom needed btu cheap to provide */
    { "tounicode16",       strlib_format_tounicode16 },
    { "toutf8",            strlib_format_toutf8      },
    { "toutf16",           strlib_format_toutf16     }, /* this is kind of untested */
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

int luaextend_string(lua_State * L)
{
    lua_getglobal(L, "string");
    for (const luaL_Reg *lib = strlib_function_list; lib->name; lib++) {
        lua_pushcfunction(L, lib->func);
        lua_setfield(L, -2, lib->name);
    }
    lua_pop(L, 1);
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