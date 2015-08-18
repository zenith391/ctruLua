#include <lua.h>
#include <lauxlib.h>

int load_gfx_lib(lua_State *L);
int load_os_lib(lua_State *L);
int load_news_lib(lua_State *L);
int load_ptm_lib(lua_State *L);

static const struct luaL_Reg ctr_lib[] = {
	{ NULL, NULL }
};

struct { char *name; int (*load)(lua_State *L); } ctr_libs[] = {
	{ "gfx",  load_gfx_lib },
	{ "news", load_news_lib },
	{ "ptm", load_ptm_lib }
	{ NULL, NULL },
};

int luaopen_ctr_lib(lua_State *L) {
	luaL_newlib(L, ctr_lib);

	for (int i = 0; ctr_libs[i].name; i++) {
		ctr_libs[i].load(L);
		lua_setfield(L, -2, ctr_libs[i].name);
	}

	return 1;
}

void load_ctr_lib(lua_State *L) {
	load_os_lib(L);
	luaL_requiref(L, "ctr", luaopen_ctr_lib, 0);
}
