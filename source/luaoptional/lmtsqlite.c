/*
    See license.txt in the root of this project.
*/

# include "luametatex.h"
# include "lmtoptional.h"

typedef struct sqlite3_instance sqlite3_instance;

# define SQLITELIB_METATABLE  "luatex.sqlitelib"

typedef struct sqlitelib_data {
    sqlite3_instance *db;
} sqlitelib_data;

typedef struct sqlitelib_state_info {
    int initialized;
    int padding;

    int (*sqlite3_initialize) (
        void
    );

    int (*sqlite3_open) (
        const char        *filename,
        sqlite3_instance **ppDb
    );

    int (*sqlite3_close) (
        sqlite3_instance *
    );

    int (*sqlite3_exec) (
        sqlite3_instance *,
        const char       *sql,
        int             (*callback)(void*, int, char**, char**),
        void             *,
        char            **errmsg
    );

    const char * (*sqlite3_errmsg) (
        sqlite3_instance *
    );

} sqlitelib_state_info;

static sqlitelib_state_info sqlitelib_state = {
    .initialized        = 0,
    .padding            = 0,
    .sqlite3_initialize = NULL,
    .sqlite3_open       = NULL,
    .sqlite3_close      = NULL,
    .sqlite3_exec       = NULL,
    .sqlite3_errmsg     = NULL,
};

typedef struct callback_context {
    lua_State *L;
    int        row_count;
} callback_context;

static int sqlitelib_initialize(lua_State * L)
{
    if (! sqlitelib_state.initialized) {
        const char *filename = lua_tostring(L, 1);
        if (filename) {
            lmt_library lib = lmt_library_load(filename);

            sqlitelib_state.sqlite3_initialize = lmt_library_find(lib, "sqlite3_initialize");
            sqlitelib_state.sqlite3_open       = lmt_library_find(lib, "sqlite3_open");
            sqlitelib_state.sqlite3_close      = lmt_library_find(lib, "sqlite3_close");
            sqlitelib_state.sqlite3_exec       = lmt_library_find(lib, "sqlite3_exec");
            sqlitelib_state.sqlite3_errmsg     = lmt_library_find(lib, "sqlite3_errmsg");

            sqlitelib_state.initialized = lmt_library_okay(lib);
        }
        if (sqlitelib_state.initialized) {
            sqlitelib_state.sqlite3_initialize();
        }
    }
    lua_pushboolean(L, sqlitelib_state.initialized);
    return 1;
}

static int sqlitelib_open(lua_State * L)
{
    if (sqlitelib_state.initialized) {
        const char *filename = lua_tostring(L, 1);
        if (filename != NULL) {
            sqlitelib_data *data = lua_newuserdatauv(L, sizeof(*data), 0);
            if (! sqlitelib_state.sqlite3_open(filename, &(data->db))) {
                luaL_getmetatable(L, SQLITELIB_METATABLE);
                lua_setmetatable(L, -2);
                return 1;
            }
        }
    }
    return 0;
}

static int sqlitelib_close(lua_State * L)
{
    if (sqlitelib_state.initialized) {
        sqlitelib_data * data = luaL_checkudata(L, 1, SQLITELIB_METATABLE);
        if (data != NULL && data->db != NULL) {
            sqlitelib_state.sqlite3_close(data->db);
            data->db = NULL;
        }
    }
    return 0;
}

static int sqlitelib_callback(void *ud, int nofcolumns, char **values, char **fields)
{
    callback_context *ctx = (callback_context *) ud;
    lua_State        *L   = ctx->L;
    /* Duplicate the function sitting at index 3 */
    lua_pushvalue(L, 3);
    if (nofcolumns > 0 && values != NULL) {
        /* Values table */
        lua_createtable(L, nofcolumns, 0);
        for (int i = 0; i < nofcolumns; i++) {
            lua_pushstring(L, values[i]);
            lua_rawseti(L, -2, (lua_Integer)i + 1);
        }
        /* Send header fields only on the first row */
        if (ctx->row_count == 0 && fields != NULL) {
            lua_createtable(L, nofcolumns, 0);
            for (int i = 0; i < nofcolumns; i++) {
                lua_pushstring(L, fields[i]);
                lua_rawseti(L, -2, (lua_Integer)i + 1);
            }
            lua_call(L, 2, 0); /* callback(values, fields) */
        } else {
            lua_call(L, 1, 0); /* callback(values) */
        }
    } else {
        lua_call(L, 0, 0);
    }
    ctx->row_count++;
    return 0;
}

/* execute(database, querystring, callback) */

static int sqlitelib_execute(lua_State * L)
{
    if (sqlitelib_state.initialized) {
        sqlitelib_data * data = luaL_checkudata(L, 1, SQLITELIB_METATABLE);
        if (data != NULL && data->db != NULL) {
            const char *query = lua_tostring(L, 2);
            if (query != NULL) {
                int result = 0;
                if (lua_isfunction(L, 3)) {
                    callback_context ctx = { .L = L, .row_count = 0 };
                    result = sqlitelib_state.sqlite3_exec(data->db, query, &sqlitelib_callback, &ctx, NULL);
                } else {
                    result = sqlitelib_state.sqlite3_exec(data->db, query, NULL, NULL, NULL);
                }
                lua_pushboolean(L, result == 0);
                return 1;
            }
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

static int sqlitelib_getmessage(lua_State * L)
{
    if (sqlitelib_state.initialized) {
        sqlitelib_data * data = luaL_checkudata(L, 1, SQLITELIB_METATABLE);
        if (data != NULL && data->db != NULL) {
            lua_pushstring(L, sqlitelib_state.sqlite3_errmsg(data->db));
            return 1;
        }
    }
    return 0;
}

static int sqlitelib_free(lua_State * L)
{
    return sqlitelib_close(L);
}

static int sqlitelib_tostring(lua_State * L)
{
    if (sqlitelib_state.initialized) {
        sqlitelib_data * data = luaL_checkudata(L, 1, SQLITELIB_METATABLE);
        if (data != NULL) {
            (void) lua_pushfstring(L, "<sqlitelib-instance %p>", data->db);
        } else {
            lua_pushnil(L);
        }
        return 1;
    } else {
        return 0;
    }
}

static const struct luaL_Reg sqlitelib_metatable[] = {
    { "__tostring", sqlitelib_tostring },
    { "__gc",       sqlitelib_free     },
    { NULL,         NULL               },
};

static struct luaL_Reg sqlitelib_function_list[] = {
    { "initialize", sqlitelib_initialize },
    { "open",       sqlitelib_open       },
    { "close",      sqlitelib_close      },
    { "execute",    sqlitelib_execute    },
    { "getmessage", sqlitelib_getmessage },
    { NULL,         NULL                 },
};

int luaopen_sqlite(lua_State * L)
{
    luaL_newmetatable(L, SQLITELIB_METATABLE);
    luaL_setfuncs(L, sqlitelib_metatable, 0);
    lmt_library_register(L, "sqlite", sqlitelib_function_list);
    return 0;
}
