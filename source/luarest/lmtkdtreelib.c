/*
    See license.txt in the root of this project.
*/

# include <stdio.h>
# include <stdlib.h>
# include <limits.h>

# include "luametatex.h"
# include "utilities/auxkdtree2d.h"
# include "utilities/auxkdtree3d.h"

/*tex

    k dimensional trees aka kd trees

    Solutions for kd trees can be found in the internet but normally are geared to some kind of
    usage, so we do just that here: we will add and adapt as we go. Once we know what we need and
    are stable the more general code will move to its own lib. Okay, we just did that.

    when dimensions is 2 : depth & 1
*/

typedef enum kd_modes {
    kd_default_mode,
    kd_weighted_mode,
} kd_modes;

typedef struct kdtree_data {
    union    {
        void     *root;
        kd2_node  root2d;
        kd3_node  root3d;
    };
    unsigned dimension;
    unsigned size; /* todo */
    unsigned mode;
} kdtree_data;

typedef kdtree_data *kdtree;

/* */

inline static kdtree kdtreelib_aux_check_is_valid(lua_State *L, int n)
{
    kdtree t = (kdtree) lua_touserdata(L, n);
    if (t && lua_getmetatable(L, n)) {
        lua_get_metatablelua(kdtree_instance);
        if (! lua_rawequal(L, -1, -2)) {
            t = NULL;
        }
        lua_pop(L, 2);
        if (t) {
            return t;
        }
    }
    tex_normal_warning("kdtree lib", "lua <kdtree> expected");
    return NULL;
}

static int kdtreelib_new(lua_State *L)
{
    int dimension = lmt_optinteger(L, 1, 3);
    kdtree t = lua_newuserdatauv(L, sizeof(kdtree_data), 0);
    if (t) {
        luaL_setmetatable(L, KDTREE_METATABLE_INSTANCE);
        t->root      = NULL;
        t->dimension = dimension < 2 ? 2 : dimension > 3 ? 3 : dimension;
        t->size      = 0;
        t->mode      = kd_default_mode;
        return 1;
    } else {
        return 0;
    }
}

static int kdtreelib_tostring(lua_State *L)
{
    kdtree t = kdtreelib_aux_check_is_valid(L, 1);
    if (t) {
        if (t->mode == kd_weighted_mode) {
            lua_pushfstring(L, "<kdtree %p : %d %d : weighted>", t, t->dimension, t->size);
        } else {
            lua_pushfstring(L, "<kdtree %p : %d %d>", t, t->dimension, t->size);
        }
        return 1;
    } else {
        return 0;
    }
}

static int kdtreelib_reset(lua_State *L)
{
    kdtree t = kdtreelib_aux_check_is_valid(L, 1);
    if (t && t->root) {
        if (t->dimension == 2) {
            kd2_flush(t->root2d);
        } else {
            kd3_flush(t->root3d);
        }
        t->root = NULL;
        t->size = 0;
    }
    return 0;
}

static int kdtreelib_gc(lua_State *L)
{
    /* currently the same */
    return kdtreelib_reset(L);
}

static int kdtreelib_insert(lua_State *L)
{
    kdtree t = kdtreelib_aux_check_is_valid(L, 1);
    if (t) {
        if (t->dimension == 2) {
            kd2_point_data p = (kd2_point_data) {
                .x     = lua_tonumber(L, 2),
                .y     = lua_tonumber(L, 3),
                .index = ++t->size,
                .axis  = 0,
            };
            t->root2d = kd2_insert(t->root, &p);
        } else {
            kd3_point_data p = (kd3_point_data) {
                .x     = lua_tonumber(L, 2),
                .y     = lua_tonumber(L, 3),
                .z     = lua_tonumber(L, 4),
                .index = ++t->size,
                .axis  = 0,
            };
            t->root3d = kd3_insert(t->root, &p);
        }
        lua_pushinteger(L, t->size);
        return 1;
    } else {
        return 0;
    }
}

static int kdtreelib_delete(lua_State *L)
{
    kdtree t = kdtreelib_aux_check_is_valid(L, 1);
    if (t && t->root) {
        if (t->dimension == 2) {
            kd2_point_data p = (kd2_point_data) {
                .x = lua_tonumber(L, 2),
                .y = lua_tonumber(L, 3),
            };
            t->root2d = kd2_delete(t->root, &p);
        } else {
            kd3_point_data p = (kd3_point_data) {
                .x = lua_tonumber(L, 2),
                .y = lua_tonumber(L, 3),
                .z = lua_tonumber(L, 4),
            };
            t->root3d = kd3_delete(t->root, &p);
        }
    }
    return 0;
}

static int kdtreelib_found(lua_State *L)
{
    kdtree t = kdtreelib_aux_check_is_valid(L, 1);
    if (t && t->root) {
        int found = 0;
        if (t->dimension == 2) {
            kd2_point_data p = (kd2_point_data) {
                 .x = lua_tonumber(L, 2),
                 .y = lua_tonumber(L, 3),
            };
            found = kd2_found(t->root, &p) != NULL;
        } else {
            kd3_point_data p = (kd3_point_data) {
                 .x = lua_tonumber(L, 2),
                 .y = lua_tonumber(L, 3),
                 .z = lua_tonumber(L, 4),
            };
            found = kd3_found(t->root, &p) != NULL;
        }
        lua_pushboolean(L, found);
        return 1;
    } else {
        return 0;
    }
}

static int kdtreelib_nearest(lua_State *L)
{
    kdtree t = kdtreelib_aux_check_is_valid(L, 1);
    if (t && t->root) {
        if (t->dimension == 2) {
            kd2_point_data p = (kd2_point_data) {
                .x = lua_tonumber(L, 2),
                .y = lua_tonumber(L, 3),
            };
            kd2_node nearest = kd2_nearest(t->root, &p);
            if (nearest) {
                lua_pushnumber(L, nearest->point.x);
                lua_pushnumber(L, nearest->point.y);
                return 2;
            }
        } else {
            kd3_point_data p = (kd3_point_data) {
                .x = lua_tonumber(L, 2),
                .y = lua_tonumber(L, 3),
                .z = lua_tonumber(L, 4),
            };
            kd3_node nearest = kd3_nearest(t->root, &p);
            if (nearest) {
                lua_pushnumber(L, nearest->point.x);
                lua_pushnumber(L, nearest->point.y);
                lua_pushnumber(L, nearest->point.z);
                return 3;
            }
        }
    }
    return 0;
}

static int kdtreelib_nearestindex(lua_State *L)
{
    kdtree t = kdtreelib_aux_check_is_valid(L, 1);
    if (t && t->root && t->mode == kd_weighted_mode) {
        if (t->dimension == 2) {
            kd2_point_data p = (kd2_point_data) {
                .x = lua_tonumber(L, 2),
                .y = lua_tonumber(L, 3),
            };
            kd2_node nearest = kd2_nearestindex(t->root, &p);
            if (nearest) {
                lua_pushinteger(L, nearest->point.index);
                lua_pushnumber(L, nearest->point.x);
                lua_pushnumber(L, nearest->point.y);
                return 3;
            }
        } else {
            kd3_point_data p = (kd3_point_data) {
                .x = lua_tonumber(L, 2),
                .y = lua_tonumber(L, 3),
                .z = lua_tonumber(L, 4),
            };
            kd3_node nearest = kd3_nearestindex(t->root, &p);
            if (nearest) {
                lua_pushinteger(L, nearest->point.index);
                lua_pushnumber(L, nearest->point.x);
                lua_pushnumber(L, nearest->point.y);
                lua_pushnumber(L, nearest->point.z);
                return 4;
            }
        }
    }
    return 0;
}

/*
    Traversing: we need to backtrack so that gets delegated to specialized code that we put in the
    kd2 and kd3 namespace. It could go in the modules but is only used here.

*/

# define iterator_stack_size 128  /* plenty unless we have a real bad tree */

typedef struct kd2_iterator {
    kd2_node stack[iterator_stack_size];
    int      top;
    kd2_node current;
} kd2_iterator;

typedef struct kd3_iterator {
    kd3_node stack[iterator_stack_size];
    int      top;
    kd3_node current;
} kd3_iterator;

static void kd2_iterator_initialize(kd2_iterator *iterator, kd2_node root)
{
    iterator->top = -1;
    iterator->current = root;
    while (iterator->current) {
        iterator->stack[++iterator->top] = iterator->current;
        iterator->current = iterator->current->left;
    }
}

static void kd3_iterator_initialize(kd3_iterator *iterator, kd3_node root)
{
    iterator->top = -1;
    iterator->current = root;
    while (iterator->current) {
        iterator->stack[++iterator->top] = iterator->current;
        iterator->current = iterator->current->left;
    }
}

static kd2_node kd2_iterator_next(kd2_iterator *iterator)
{
    if (iterator->top < 0) {
        return NULL;
    } else {
        kd2_node node = iterator->stack[iterator->top--];
        kd2_node temp = node->right;
        while (temp) {
            iterator->stack[++iterator->top] = temp;
            temp = temp->left;
        }
        return node;
    }
}

static kd3_node kd3_iterator_next(kd3_iterator *iterator)
{
    if (iterator->top < 0) {
        return NULL;
    } else {
        kd3_node node = iterator->stack[iterator->top--];
        kd3_node temp = node->right;
        while (temp) {
            iterator->stack[++iterator->top] = temp;
            temp = temp->left;
        }
        return node;
    }
}

static int kdtreelib2_next(lua_State *L)
{
    kd2_iterator *iterator = (kd2_iterator *) lua_touserdata(L, 1);
    kd2_node      node     = kd2_iterator_next(iterator);
    if (node) {
        lua_pushinteger(L, node->point.index);
        lua_pushinteger(L, node->point.x);
        lua_pushinteger(L, node->point.y);
        if (node->point.axis) {
            lua_pushinteger(L, node->point.axis);
            return 4;
        } else {
            return 3;
        }
    } else {
        return 0;
    }
}

static int kdtreelib3_next(lua_State *L)
{
    kd3_iterator *iterator = (kd3_iterator *) lua_touserdata(L, 1);
    kd3_node      node     = kd3_iterator_next(iterator);
    if (node) {
        lua_pushinteger(L, node->point.index);
        lua_pushinteger(L, node->point.x);
        lua_pushinteger(L, node->point.y);
        lua_pushinteger(L, node->point.z);
        if (node->point.axis) {
            lua_pushinteger(L, node->point.axis);
            return 5;
        } else {
            return 4;
        }
    } else {
        return 0;
    }
}

static int kdtreelib_traverse(lua_State *L)
{
    kdtree t = kdtreelib_aux_check_is_valid(L, 1);
    if (t) {
        if (t->dimension == 2) {
            kd2_iterator *iterator = (kd2_iterator *) lua_newuserdatauv(L, sizeof(kd2_iterator), 0);
            kd2_iterator_initialize(iterator, (kd2_node) t->root);
         // luaL_setmetatable(L, KDTREEITERATE2D_METATABLE_INSTANCE);
            lua_pushcfunction(L, kdtreelib2_next);
        } else {
            kd3_iterator *iterator = (kd3_iterator *) lua_newuserdatauv(L, sizeof(kd3_iterator), 0);
            kd3_iterator_initialize(iterator, (kd3_node) t->root);
         // luaL_setmetatable(L, KDTREEITERATE3D_METATABLE_INSTANCE);
            lua_pushcfunction(L, kdtreelib3_next);
        }
        lua_pushvalue(L, -2);
        return 2;
    }
    return 0;
}

static int kdtreelib2d_aux_totable(lua_State *L, kdtree t, kd2_node root, int n)
{
    int index = 0;
    if (root->left) {
        n = kdtreelib2d_aux_totable(L, t, root->left, n);
    }
    lua_createtable(L, t->mode == kd_weighted_mode ? 3 : 2, 0);
    lua_pushnumber(L, root->point.x); lua_rawseti(L, -2, ++index);
    lua_pushnumber(L, root->point.y); lua_rawseti(L, -2, ++index);
    if (t->mode == kd_weighted_mode) {
        lua_pushinteger(L, root->point.axis); lua_rawseti(L, -2, ++index);
        lua_rawseti(L, -2, root->point.index);
    } else {
        lua_rawseti(L, -2, n++);
    }
    if (root->right) {
        n = kdtreelib2d_aux_totable(L, t, root->right, n);
    }
    return n;
}

static int kdtreelib3d_aux_totable(lua_State *L, kdtree t, kd3_node root, int n)
{
    int index = 0;
    if (root->left) {
        n = kdtreelib3d_aux_totable(L, t, root->left, n);
    }
    lua_createtable(L, t->mode == kd_weighted_mode ? 4 : 3, 0);
    lua_pushnumber(L, root->point.x); lua_rawseti(L, -2, ++index);
    lua_pushnumber(L, root->point.y); lua_rawseti(L, -2, ++index);
    lua_pushnumber(L, root->point.z); lua_rawseti(L, -2, ++index);
    if (t->mode == kd_weighted_mode) {
        lua_pushinteger(L, root->point.axis); lua_rawseti(L, -2, ++index);
        lua_rawseti(L, -2, root->point.index);
    } else {
        lua_rawseti(L, -2, n++);
    }
    if (root->right) {
        n = kdtreelib3d_aux_totable(L, t, root->right, n);
    }
    return n;
}

static int kdtreelib_totable(lua_State *L)
{
    kdtree t = kdtreelib_aux_check_is_valid(L, 1);
    if (t) {
        lua_createtable(L, t->size, 0);
        if (t->root) {
            if (t->dimension == 2) {
                kdtreelib2d_aux_totable(L, t, t->root, 1);
            } else {
                kdtreelib3d_aux_totable(L, t, t->root, 1);
            }
        }
        return 1;
    } else {
        return 0;
    }
}

/*

    We could use lua tables but settled for a dedicated compact arrays. We also want to play nice
    with points, sometimes too large (overkill) but useful for us.

    We might drop the lua table points when we're stable.

*/

static lua_State *sort_lua    = NULL;
static int        sort_index  = 0;
static points     sort_points = NULL;

static int compare_points_x(const void *a, const void *b)
{
    int ai = *(const int *) a;
    int bi = *(const int *) b;
    double ad;
    double bd;
    if (sort_points) {
        ad = sort_points->data[ai-1].x;
        bd = sort_points->data[bi-1].x;
    } else {
        lua_rawgeti(sort_lua, sort_index, ai); lua_rawgeti(sort_lua, -1, 1); ad = lua_tonumber(sort_lua, -1); lua_pop(sort_lua, 2);
        lua_rawgeti(sort_lua, sort_index, bi); lua_rawgeti(sort_lua, -1, 1); bd = lua_tonumber(sort_lua, -1); lua_pop(sort_lua, 2);
    }
 // return (ad > bd) - (ad < bd);
    if (ad < bd) {
        return -1;
    } else if (ad > bd) {
        return 1;
    } else {
        return 0;
    }
}

static int compare_points_y(const void *a, const void *b)
{
    int ai = *(const int *) a;
    int bi = *(const int *) b;
    double ad;
    double bd;
    if (sort_points) {
        ad = sort_points->data[ai-1].y;
        bd = sort_points->data[bi-1].y;
    } else {
        lua_rawgeti(sort_lua, sort_index, ai); lua_rawgeti(sort_lua, -1, 2); ad = lua_tonumber(sort_lua, -1); lua_pop(sort_lua, 2);
        lua_rawgeti(sort_lua, sort_index, bi); lua_rawgeti(sort_lua, -1, 2); bd = lua_tonumber(sort_lua, -1); lua_pop(sort_lua, 2);
    }
    if (ad < bd) {
        return -1;
    } else if (ad > bd) {
        return 1;
    } else {
        return 0;
    }
}

static kd2_node kdtreelib_aux_weighted(int * list, int size, int depth)
{
    int axis   = (depth % 2) + 1;
    int *left  = lmt_memory_malloc(size * sizeof(int));
    int *right = lmt_memory_malloc(size * sizeof(int));
    int m      = floor((size + 1) / 2);
 // int m      = (size + 1) / 2;
    int l      = 0;
    int r      = 0;
    if (axis == 1) {
        qsort(list, size, sizeof(int), compare_points_x);
    } else {
        qsort(list, size, sizeof(int), compare_points_y);
    }
    for (int i = 1; i <= m - 1; i++) {
        left[l++] = list[i-1];
    }
    for (int i = m + 1; i <= size; i++) {
        right[r++] = list[i-1];
    }
    /* the compiler will likely do this:
        memcpy(&left [l], &list[0], (m    - 1) * sizeof(list[0])); l += m    - 1;
        memcpy(&right[r], &list[m], (size - m) * sizeof(list[0])); r += size - m;
    */
    kd2_node temp = (kd2_node) lmt_memory_malloc(sizeof(kd2_node_data)); /* maybe union index/axis */
    temp->point.index = list[m-1];
    temp->point.axis  = axis;
    temp->left  = l > 0 ? kdtreelib_aux_weighted(left,  l, depth + 1) : NULL;
    temp->right = r > 0 ? kdtreelib_aux_weighted(right, r, depth + 1) : NULL;
    if (sort_points) {
        temp->point.x = sort_points->data[temp->point.index-1].x;
        temp->point.y = sort_points->data[temp->point.index-1].y;
    } else {
        if (lua_rawgeti(sort_lua, sort_index, temp->point.index) == LUA_TTABLE) {
            lua_rawgeti(sort_lua, -1, 1); temp->point.x = lua_tonumber(sort_lua, -1); lua_pop(sort_lua, 1);
            lua_rawgeti(sort_lua, -1, 2); temp->point.y = lua_tonumber(sort_lua, -1); lua_pop(sort_lua, 1);
        }
        lua_pop(sort_lua, 1);
    }
    lmt_memory_free(left);
    lmt_memory_free(right);
    return temp;
}

static int kdtreelib_newweighted(lua_State *L)
{
    kdtree t = lua_newuserdatauv(L, sizeof(kdtree_data), 0);
    if (t) {
        luaL_setmetatable(L, KDTREE_METATABLE_INSTANCE);
        t->root      = NULL;
        t->dimension = 2;
        t->size      = 0;
        t->mode      = kd_default_mode;
        sort_lua     = NULL;
        sort_index   = 0;
        sort_points  = NULL;
        switch (lua_type(L, 1)) {
            case LUA_TUSERDATA:
                {
                    sort_points = vectorlib_points_aux_get(L, 1);
                    if (sort_points && sort_points->size > 0) {
                        int size  = sort_points->size;
                        int *list = lmt_memory_malloc(size * sizeof(int));
                        for (int i = 0; i < size; i++) {
                            list[i] = i + 1;
                        }
                        kd2_flush(t->root);
                        t->root2d = kdtreelib_aux_weighted(list, size, 0);
                        t->size   = size;
                        t->mode   = kd_weighted_mode;
                        lmt_memory_free(list);
                        return 1;
                    } else {
                        return 0;
                    }
                }
            case LUA_TTABLE:
                {
                    int size  = (int) lua_rawlen(L, 1);
                    int *list = lmt_memory_malloc(size * sizeof(int));
                    for (int i = 0; i < size; i++) {
                        list[i] = i + 1;
                    }
                    kd2_flush(t->root);
                    sort_lua   = L;
                    sort_index = 1;
                    t->root2d = kdtreelib_aux_weighted(list, size, 0);
                    t->size   = size;
                    t->mode   = kd_weighted_mode;
                    lmt_memory_free(list);
                    return 1;
                }
            default:
                return 0;
        }
    }
    return 0;
}

/* */

static const struct luaL_Reg kdtreelib_instance[] = {
    { "__tostring", kdtreelib_tostring },
    { "__gc",       kdtreelib_gc       },
    { "__call",     kdtreelib_insert   },
    { NULL,         NULL               },
};

static const luaL_Reg kdtreelib_function_list[] = {
    { "new",          kdtreelib_new          },
    { "newweighted",  kdtreelib_newweighted  },
    { "reset",        kdtreelib_reset        },
    { "insert",       kdtreelib_insert       },
    { "delete",       kdtreelib_delete       },
    { "found",        kdtreelib_found        },
    { "nearest",      kdtreelib_nearest      },
    { "nearestindex", kdtreelib_nearestindex },
    { "traverse",     kdtreelib_traverse     },
    { "totable",      kdtreelib_totable      },
    /* */
    { NULL,           NULL                   },
};

int luaopen_kdtree(lua_State *L)
{
    luaL_newmetatable(L, KDTREE_METATABLE_INSTANCE);
    luaL_setfuncs(L, kdtreelib_instance, 0);
    lua_newtable(L);
    luaL_setfuncs(L, kdtreelib_function_list, 0);
    return 1;
}
