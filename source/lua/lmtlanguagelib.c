/*
    See license.txt in the root of this project.
*/

/*tex

    This is the interface to everything that relates to hyphenation in the frontend: defining
    a new language, setting properties for hyphenation, loading patterns and exceptions.

*/

# include "luametatex.h"

typedef struct languagelib_language {
    tex_language lang; /* just an indirect pointer */
} languagelib_language;

static int languagelib_new(lua_State *L)
{
    languagelib_language *ulang = lua_newuserdatauv(L, sizeof(languagelib_language), 0);
    if (lua_type(L, 1) == LUA_TNUMBER) {
        halfword lualang = lmt_tohalfword(L, 1);
        ulang->lang = tex_get_language(lualang);
        if lmt_unlikely(! ulang->lang) {
            return luaL_error(L, "undefined language %d", lualang);
        }
    } else {
        ulang->lang = tex_new_language(-1);
        if lmt_unlikely(! ulang->lang) {
            return luaL_error(L, "no room for a new language");
        }
    }
    lua_get_metatablelua(language_instance);
    lua_setmetatable(L, -2);
    return 1;
}

static tex_language languagelib_object(lua_State* L)
{
    tex_language lang = NULL;
    switch (lua_type(L, 1)) {
        case LUA_TNUMBER:
            lang = tex_get_language(lmt_tohalfword(L, 1));
            break;
        case LUA_TUSERDATA:
            {
                languagelib_language *ulang = lua_touserdata(L, 1);
                if (ulang && lua_getmetatable(L, 1)) {
                    lua_get_metatablelua(language_instance);
                    if (lua_rawequal(L, -1, -2)) {
                        lang = ulang->lang;
                    }
                    lua_pop(L, 2);
                }
                break;
            }
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, 1)) {
                lang = tex_get_language(language_par);
            }
            break;
    }
    if lmt_unlikely(! lang) {
        luaL_error(L, "argument should be a valid language id, language object, or true");
    }
    return lang;
}

static int languagelib_id(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if lmt_likely(lang) {
        lua_pushinteger(L, lang->id);
        return 1;
    } else {
        return 0;
    }
}

static int languagelib_patterns(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        if (lua_gettop(L) == 1) {
            if (lang->patterns) {
                lua_pushstring(L, (char *) hnj_dictionary_tostring(lang->patterns));
            } else {
                lua_pushnil(L);
            }
            return 1;
        } else if (lua_type(L, 2) == LUA_TSTRING) {
            tex_load_patterns(lang, (const unsigned char *) lua_tostring(L, 2));
        } else {
            return luaL_error(L, "argument should be a string");
        }
    }
    return 0;
}

static int languagelib_clear_patterns(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        tex_clear_patterns(lang);
    }
    return 0;
}

static int languagelib_hyphenation(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        if (lua_gettop(L) == 1) {
            if (lang->exceptions) {
                luaL_Buffer b;
                luaL_buffinit(L, &b);
                if (lua_rawgeti(L, LUA_REGISTRYINDEX, lang->exceptions) == LUA_TTABLE) {
                    int done = 0;
                    lua_pushnil(L);
                    while (lua_next(L, -2)) {
                        if (done) {
                            luaL_addlstring(&b, " ", 1);
                        } else {
                            done = 1;
                        }
                        luaL_addvalue(&b);
                    }
                }
                luaL_pushresult(&b);
            } else {
                lua_pushnil(L);
            }
            return 1;
        } else if (lua_type(L, 2) == LUA_TSTRING) {
            tex_load_hyphenation(lang, (const unsigned char *) lua_tostring(L, 2));
        } else {
            return luaL_error(L, "argument should be a string");
        }
    }
    return 0;
}

static int languagelib_pre_hyphen_char(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        if (lua_gettop(L) == 1) {
            lua_pushinteger(L, lang->pre_hyphen_char);
            return 1;
        } else if (lua_type(L, 2) == LUA_TNUMBER) {
            lang->pre_hyphen_char = lmt_tohalfword(L, 2);
        } else {
            return luaL_error(L, "argument should be a character number");
        }
    }
    return 0;
}

static int languagelib_post_hyphen_char(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        if (lua_gettop(L) == 1) {
            lua_pushinteger(L, lang->post_hyphen_char);
            return 1;
        } else if (lua_type(L, 2) == LUA_TNUMBER) {
            lang->post_hyphen_char = lmt_tohalfword(L, 2);
        } else {
            return luaL_error(L, "argument should be a character number");
        }
    }
    return 0;
}

static int languagelib_pre_exhyphen_char(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        if (lua_gettop(L) == 1) {
            lua_pushinteger(L, lang->pre_exhyphen_char);
            return 1;
        } else if (lua_type(L, 2) == LUA_TNUMBER) {
            lang->pre_exhyphen_char = lmt_tohalfword(L, 2);
        } else {
            return luaL_error(L, "argument should be a character number");
        }
    }
    return 0;
}

/* We push nuts! */

int lmt_handle_word(
    tex_language  language,
    const char   *original,
    const char   *word,
    int           length,
    halfword      first,
    halfword      last,
    char        **replacement,
    int          *size
)
{
    if (language->wordhandler && word && first && last) {
        lua_State *L = lmt_lua_state.lua_instance;
        int stacktop = lua_gettop(L);
        int result = 0;
        int res;
        *replacement = NULL;
        lua_pushcfunction(L, lmt_traceback); /* goes before function */
        lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_language_state.handler_table_id);
        lua_rawgeti(L, -1, language->id);
        lua_pushinteger(L, language->id);
        lua_pushstring(L, original);
        lua_pushstring(L, word);
        lua_pushinteger(L, length);
        lua_pushinteger(L, first);
        lua_pushinteger(L, last);
        res = lua_pcall(L, 6, 1, 0);
        if lmt_unlikely(res) {
            lua_remove(L, stacktop + 1); /* really ? */
            lmt_error(L, "function call", -1, res == LUA_ERRRUN ? 0 : 1);
        }
        ++lmt_language_state.handler_count;
        switch (lua_type(L, -1)) {
            case LUA_TSTRING:
                {
                    size_t l = 0;
                    const char *s = lua_tolstring(L, -1, &l);
                    *size = (int) l;
                    *replacement = (char *) lmt_memory_strdup(s);
                    break;
                }
            case LUA_TNUMBER:
                {
                    result = lmt_tointeger(L, -1);
                    break;
                }
            default:
                break;
        }
        lua_settop(L, stacktop);
        return result;
    }
    return 0;
}

void lmt_initialize_languages(void)
{
     lua_State *L = lmt_lua_state.lua_instance;
     lua_newtable(L);
     lmt_language_state.handler_table_id = luaL_ref(L, LUA_REGISTRYINDEX);
     lua_pushstring(L, LANGUAGE_FUNCTIONS_REGISTRY);
     lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_language_state.handler_table_id);
     lua_settable(L, LUA_REGISTRYINDEX);
}

static int languagelib_setwordhandler(lua_State* L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        switch (lua_type(L, 2)) {
            case LUA_TBOOLEAN:
                if (lua_toboolean(L, 2)) {
                    goto DEFAULT;
                }
                FALLTHROUGH
            case LUA_TNIL:
                if (lang->wordhandler) {
                    lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_language_state.handler_table_id);
                    lua_pushnil(L);
                    lua_rawseti(L, -2, lang->id);
                    lang->wordhandler = 0;
                }
                break;
            case LUA_TFUNCTION:
                lua_rawgeti(L, LUA_REGISTRYINDEX, lmt_language_state.handler_table_id);
                lua_pushvalue(L, 2);
                lua_rawseti(L, -2, lang->id);
                lang->wordhandler = 1;
                break;
            default:
              DEFAULT:
                return luaL_error(L, "argument should be a function, false or nil");
        }
    }
    return 0;
}

static int languagelib_sethjcode(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        if lmt_likely(lua_type(L, 2) == LUA_TNUMBER) {
            int i = lmt_checkinteger(L, 2);
            if lmt_unlikely(! character_in_range(i)) {
                return luaL_error(L, "argument should be a character number in range 0.." LMT_TOSTRING(max_character_code));
            }
            if (lua_type(L, 3) == LUA_TNUMBER) {
                int v = lmt_checkinteger(L, 3);
                if lmt_unlikely(! character_in_range(v)) {
                    return luaL_error(L, "mapped value should be a character number in range 0.." LMT_TOSTRING(max_character_code));
                } else {
                    tex_set_hj_code(lang->id, i, v, -1);
                }
            } else {
                tex_set_hj_code(lang->id, i, i, -1);
            }
        } else {
            return luaL_error(L, "argument should be a character number");
        }
    }
    return 0;
}

static int languagelib_gethjcode(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        if lmt_likely(lua_type(L, 2) == LUA_TNUMBER) {
            int i = lmt_checkinteger(L, 2);
            if lmt_unlikely(! character_in_range(i)) {
                return luaL_error(L, "argument should be a character number in range 0.." LMT_TOSTRING(max_character_code));
            } else {
                lua_pushinteger(L, tex_get_hj_code(lang->id, i));
            }
            return 1;
        } else {
            return luaL_error(L, "argument should be a character number");
        }
    }
    return 0;
}

static int languagelib_post_exhyphen_char(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        if (lua_gettop(L) == 1) {
            lua_pushinteger(L, lang->post_exhyphen_char);
            return 1;
        } else if (lua_type(L, 2) == LUA_TNUMBER) {
            lang->post_exhyphen_char = lmt_tohalfword(L, 2);
            return 0;
        } else {
            return luaL_error(L, "argument should be a character number");
        }
    }
    return 0;
}

static int languagelib_hyphenation_min(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        if (lua_gettop(L) == 1) {
            lua_pushinteger(L, lang->hyphenation_min);
            return 1;
        } else if (lua_type(L, 2) == LUA_TNUMBER) {
            lang->hyphenation_min = lmt_tohalfword(L, 2);
        } else {
            return luaL_error(L, "argument should be a number");
        }
    }
    return 0;
}

static int languagelib_clear_hyphenation(lua_State *L)
{
    tex_language lang = languagelib_object(L);
    if (lang) {
        tex_clear_hyphenation(lang);
    }
    return 0;
}

static int languagelib_clean(lua_State *L)
{
    char *cleaned = NULL;
    if (lua_type(L, 1) == LUA_TSTRING) {
        tex_clean_hyphenation(cur_lang_par, lua_tostring(L, 1), &cleaned);
    } else {
        tex_language lang = languagelib_object(L);
        if lmt_likely(lang) {
            if lmt_likely(lua_type(L, 2) == LUA_TSTRING) {
                tex_clean_hyphenation(lang->id, lua_tostring(L, 2), &cleaned);
            } else {
                return luaL_error(L, "second argument should be a string");
            }
        } else {
            return luaL_error(L, "first argument should be a string or language");
        }
    }
    lua_pushstring(L, cleaned);
    lmt_memory_free(cleaned);
    return 1;
}

static int languagelib_hyphenate(lua_State *L)
{
    halfword head = lmt_check_isnode(L, 1);
    halfword tail = null;
    if (lua_isuserdata(L, 2)) {
        tail = lmt_check_isnode(L, 2);
    }
    if (! tail) {
        tail = tex_tail_of_node_list(head);
    }
    tex_hyphenate_list(head, tail);
    lmt_push_node_fast(L, head);
    lmt_push_node_fast(L, tail);
    lua_pushboolean(L, 1);
    return 3;
}

static int languagelib_current(lua_State *L)
{
    lua_pushinteger(L, language_par);
    return 1;
}

static int languagelib_haslanguage(lua_State *L)
{
    halfword head = lmt_check_isnode(L, 1);
    while (head) {
        if (node_type(head) == glyph_node && get_glyph_language(head) > 0) {
            lua_pushboolean(L, 1);
            return 1;
        } else {
            head = node_next(head);
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static const struct luaL_Reg languagelib_metatable[] = {
    { "clearpatterns",     languagelib_clear_patterns     },
    { "clearhyphenation",  languagelib_clear_hyphenation  },
    { "patterns",          languagelib_patterns           },
    { "hyphenation",       languagelib_hyphenation        },
    { "prehyphenchar",     languagelib_pre_hyphen_char    },
    { "posthyphenchar",    languagelib_post_hyphen_char   },
    { "preexhyphenchar",   languagelib_pre_exhyphen_char  },
    { "postexhyphenchar",  languagelib_post_exhyphen_char },
    { "hyphenationmin",    languagelib_hyphenation_min    },
    { "sethjcode",         languagelib_sethjcode          },
    { "gethjcode",         languagelib_gethjcode          },
    { "setwordhandler",    languagelib_setwordhandler     },
    { "id",                languagelib_id                 },
    { NULL,                NULL                           },
};

static const struct luaL_Reg languagelib_function_list[] = {
    { "clearpatterns",     languagelib_clear_patterns     },
    { "clearhyphenation",  languagelib_clear_hyphenation  },
    { "patterns",          languagelib_patterns           },
    { "hyphenation",       languagelib_hyphenation        },
    { "prehyphenchar",     languagelib_pre_hyphen_char    },
    { "posthyphenchar",    languagelib_post_hyphen_char   },
    { "preexhyphenchar",   languagelib_pre_exhyphen_char  },
    { "postexhyphenchar",  languagelib_post_exhyphen_char },
    { "hyphenationmin",    languagelib_hyphenation_min    },
    { "sethjcode",         languagelib_sethjcode          },
    { "gethjcode",         languagelib_gethjcode          },
    { "setwordhandler",    languagelib_setwordhandler     },
    { "id",                languagelib_id                 },
    { "clean",             languagelib_clean              }, /* maybe obsolete */
    { "haslanguage",       languagelib_haslanguage        },
    { "hyphenate",         languagelib_hyphenate          },
    { "current",           languagelib_current            },
    { "new",               languagelib_new                },
    { NULL,                NULL                           },
};

int luaopen_language(lua_State *L)
{
    luaL_newmetatable(L, LANGUAGE_METATABLE_INSTANCE);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, languagelib_metatable, 0);
    lua_newtable(L);
    luaL_setfuncs(L, languagelib_function_list, 0);
    return 1;
}
