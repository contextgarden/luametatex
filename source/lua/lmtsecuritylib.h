/*
    See license.txt in the root of this project.
*/

# ifndef LSECURITYLIB_H
# define LSECURITYLIB_H

# include <luametatex.h>

extern bool lmt_valid_target  (lua_State *L, int target, const char *str, int action);
extern void lmt_disable_debug (lua_State *L, int complete);
extern int  luaopen_security  (lua_State *L);

# endif