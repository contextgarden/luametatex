/*
    See license.txt in the root of this project.
*/

/* For now just a simple conversion, like in the example module. */

# include "luametatex.h"
# include "lmtoptional.h"

typedef enum gmlib_NoiseType {
    UniformNoise,
    GaussianNoise,
    MultiplicativeGaussianNoise,
    ImpulseNoise,
    LaplacianNoise,
    PoissonNoise,
    RandomNoise,
    UndefinedNoise
} gmlib_NoiseType;

typedef struct gmlib_state_info {

    int initialized;
    int padding;

    void (*gm_InitializeMagick) (
        void *path
    );

    void (*gm_DestroyMagick) (
        void
    );

    void * (*gm_NewMagickWand) (
        void
    );

    void (*gm_DestroyMagickWand) (
        void *wand
    );

    int (*gm_MagickReadImage) (
        void       *wand,
        const char *name
    );

    int (*gm_MagickWriteImage) (
        void       *wand,
        const char *name
    );

    int (*gm_MagickBlurImage) (
        void         *wand,
        const double  radius,
        const double  sigma
    );

    int (*gm_MagickAddNoiseImage) (
        void  *wand,
        const  gmlib_NoiseType noise_type
    );

} gmlib_state_info;

static gmlib_state_info gmlib_state = {
    .initialized            = 0,
    .padding                = 0,
    .gm_InitializeMagick    = NULL,
    .gm_DestroyMagick       = NULL,
    .gm_NewMagickWand       = NULL,
    .gm_DestroyMagickWand   = NULL,
    .gm_MagickReadImage     = NULL,
    .gm_MagickWriteImage    = NULL,
    .gm_MagickBlurImage     = NULL,
    .gm_MagickAddNoiseImage = NULL,
};

static int gmlib_initialize(lua_State * L)
{
    if (! gmlib_state.initialized) {
        const char *filename1 = lua_tostring(L, 1);
        const char *filename2 = lua_tostring(L, 2);
        if (filename1) {
            lmt_library lib = lmt_library_load(filename1);

            gmlib_state.gm_InitializeMagick = lmt_library_find(lib, "InitializeMagick");
            gmlib_state.gm_DestroyMagick    = lmt_library_find(lib, "DestroyMagick");

            gmlib_state.initialized = lmt_library_okay(lib);
        }
        if (gmlib_state.initialized && filename2) {
            lmt_library lib = lmt_library_load(filename2);

            gmlib_state.gm_NewMagickWand       = lmt_library_find(lib, "NewMagickWand");
            gmlib_state.gm_DestroyMagickWand   = lmt_library_find(lib, "DestroyMagickWand");
            gmlib_state.gm_MagickReadImage     = lmt_library_find(lib, "MagickReadImage");
            gmlib_state.gm_MagickWriteImage    = lmt_library_find(lib, "MagickWriteImage");
            gmlib_state.gm_MagickBlurImage     = lmt_library_find(lib, "MagickBlurImage");
            gmlib_state.gm_MagickAddNoiseImage = lmt_library_find(lib, "MagickAddNoiseImage");

            gmlib_state.initialized = lmt_library_okay(lib);
        }
    }
    lua_pushboolean(L, gmlib_state.initialized);
    return 1;
}

/* We could have a callback for stdout and error. */

/* Somehow not in gm: (void) MagickImageCommand(image_info, arg_count, args, NULL, exception); */

static int gmlib_execute(lua_State * L)
{
    if (gmlib_state.initialized) {
        if (gmlib_state.initialized == 1) {
            /* Once per run. */
            gmlib_state.gm_InitializeMagick(NULL);
            gmlib_state.initialized = 2;
        }
        if (lua_type(L, 1) == LUA_TTABLE) {
            const char *inpname = NULL;
            const char *outname = NULL;
            /*tex
                Pin options table index to 1 (absolute index).
            */
            lua_getfield(L, 1, "inputfile");
            inpname = luaL_optstring(L, -1, NULL);
            lua_pop(L, 1);
            lua_getfield(L, 1, "outputfile");
            outname = luaL_optstring(L, -1, NULL);
            lua_pop(L, 1);
            if (!inpname || !outname) {
                lua_pushboolean(L, 0);
                lua_pushliteral(L, "missing inputfile or outputfile");
                return 2;
            }
            void *wand = gmlib_state.gm_NewMagickWand();
            if (wand) {
                int state = gmlib_state.gm_MagickReadImage(wand, inpname);
                if (state) {
                    /* Processing blur */
                    if (lua_getfield(L, 1, "blur") == LUA_TTABLE) {
                        int blur_tbl = lua_gettop(L);
                        lua_getfield(L, blur_tbl, "radius");
                        lua_getfield(L, blur_tbl, "sigma");
                        double radius = lua_tonumber(L, -2);
                        double sigma  = lua_tonumber(L, -1);
                        gmlib_state.gm_MagickBlurImage(wand, radius, sigma);
                        lua_pop(L, 3); /* pops sigma, radius, and blur table */
                    } else {
                        lua_pop(L, 1);
                    }
                    /* Processing noise */
                    if (lua_getfield(L, 1, "noise") == LUA_TTABLE) {
                        int noise_tbl = lua_gettop(L);
                        lua_getfield(L, noise_tbl, "type");
                        gmlib_NoiseType ntype = (gmlib_NoiseType) lmt_tointeger(L, -1);
                        gmlib_state.gm_MagickAddNoiseImage(wand, ntype);
                        lua_pop(L, 2); /* pops type and noise table */
                    } else {
                        lua_pop(L, 1);
                    }
                    state = gmlib_state.gm_MagickWriteImage(wand, outname);
                    gmlib_state.gm_DestroyMagickWand(wand);
                    if (state) {
                        lua_pushboolean(L, 1);
                        return 1;
                    } else {
                        lua_pushboolean(L, 0);
                        lua_pushliteral(L, "possible write error");
                        return 2;
                    }
                } else {
                    gmlib_state.gm_DestroyMagickWand(wand);
                    lua_pushboolean(L, 0);
                    lua_pushliteral(L, "possible read error");
                    return 2;
                }
            } else {
                lua_pushboolean(L, 0);
                lua_pushliteral(L, "possible memory issue");
                return 2;
            }
        } else {
            lua_pushboolean(L, 0);
            lua_pushliteral(L, "invalid specification");
            return 2;
        }
    }
    lua_pushboolean(L, 0);
    lua_pushliteral(L, "not initialized");
    return 2;
}

static int gmlib_noisetypes(lua_State * L)
{
    lua_createtable(L, 6, 2);
    lua_set_string_by_index(L, UniformNoise,                "uniform");
    lua_set_string_by_index(L, GaussianNoise,               "gaussian");
    lua_set_string_by_index(L, MultiplicativeGaussianNoise, "multiplicative");
    lua_set_string_by_index(L, ImpulseNoise,                "impulse");
    lua_set_string_by_index(L, LaplacianNoise,              "laplacian");
    lua_set_string_by_index(L, PoissonNoise,                "poisson");
    lua_set_string_by_index(L, RandomNoise,                 "random");
    lua_set_string_by_index(L, UndefinedNoise,              "undefined");
    return 1;
}

static struct luaL_Reg gmlib_function_list[] = {
    { "initialize",    gmlib_initialize },
    { "execute",       gmlib_execute    },
    { "getnoisetypes", gmlib_noisetypes },
    { NULL,            NULL             },
};

int luaopen_graphicsmagick(lua_State * L)
{
    lmt_library_register(L, "graphicsmagick", gmlib_function_list);
    return 0;
}
