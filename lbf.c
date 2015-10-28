#include <lua.h>
#include <lauxlib.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

// #define ENABLE_LBF_DEBUG

#ifdef ENABLE_LBF_DEBUG
# define DLOG(fmt, ...) fprintf(stderr, "<lbf>" fmt "\n", ##__VA_ARGS__)
#else
# define DLOG(...)
#endif

#if LUA_VERSION_NUM < 502
#  define luaL_newlib(L,l) (lua_newtable(L), luaL_register(L,NULL,l))
#endif

#define META_NAME "lbf{lbf_t}"
#define FN_REF_MAX_SIZE 16
#define DEFAULT_FAKE_RATE 0.01

#define IS_SET(bitarray, i) (bitarray[i/8] & (1 << (i%8)))
#define SET_SET(bitarray, i) bitarray[i/8] |= (1 << (i%8))

typedef struct lbf_s {
	size_t cap;
	size_t alen;
	size_t vsize;
	int fn_size;
	int fn_refs[FN_REF_MAX_SIZE];
	char bitarray[0];
} lbf_t;

static void lbf_init(lbf_t *self, size_t cap, size_t alen)
{
	int i;
	self->vsize = 0;
	self->cap = cap;
	self->alen = alen;
	memset(self->bitarray, 0, sizeof(char) * alen);
	for (i = 0; i < FN_REF_MAX_SIZE; i++) {
		self->bitarray[i] = LUA_NOREF;
	}
}

static int lua__new(lua_State *L)
{
	DLOG("new");
	int cap = luaL_checkinteger(L, 1);
	lua_Number fakerate = luaL_optnumber(L, 2, DEFAULT_FAKE_RATE);
	size_t slots = cap * (log(fakerate)/log(0.6185)); 
	size_t alen = slots / (sizeof(char) * 8) + 1;

	lbf_t *p = lua_newuserdata(L, sizeof(*p) + alen);
	lbf_init(p, cap, alen);
	p->fn_size = -log(fakerate) / log(2);

	luaL_getmetatable(L, META_NAME);
	lua_setmetatable(L, -2);

	lua_newtable(L);
	lua_setfenv(L, -2);
	lua_pushinteger(L, p->fn_size);

	return 2;
}

static int lua__clear(lua_State *L)
{
	lbf_t * p = (lbf_t *)luaL_checkudata(L, 1, META_NAME);
	p->vsize = 0;
	memset(p->bitarray, 0, sizeof(char) * p->alen);
	return 0;
}

static int lua__gc(lua_State *L)
{
	DLOG("gc");
	return 0;
}

static int lua__sethfuncs(lua_State *L)
{
	int i;
	int ref;
	int top = lua_gettop(L);
	int len = 0;
	lbf_t * p;
	if (top != 2) {
		return luaL_error(L, "2 args required in %s", __FUNCTION__);
	}
	p = (lbf_t *)luaL_checkudata(L, 1, META_NAME);
	if (lua_type(L, 2) != LUA_TTABLE) {
		return luaL_error(L, "arg #2 table(function) required");
	}
	len = lua_objlen(L, 2);
	if (len > FN_REF_MAX_SIZE || len < p->fn_size) {
		return luaL_error(L, "%d <= #funs(%d) <= %d in %s", \
				p->fn_size, len,
				FN_REF_MAX_SIZE, __FUNCTION__);
	}
	lua_getfenv(L, 1);
	for (i = 0; i < DEFAULT_FAKE_RATE; i++) {
		ref = p->fn_refs[i];
		if (ref == LUA_NOREF)
			break;
		luaL_unref(L, -1, ref);
	}

	i = 0;
	lua_pushnil(L);  /* first key S: obj, funcs, env, nil */
	while (lua_next(L, -3) != 0) {
		/* first key S: obj, funcs, env, key, value */
		ref = luaL_ref(L, -3);
		p->fn_refs[i++] = ref;
		if (i >= p->fn_size) {
			break;
		}
	}

	return 0;
}

static int lua__size(lua_State *L)
{
	lbf_t * p = (lbf_t *)luaL_checkudata(L, 1, META_NAME);
	lua_pushinteger(L, p->vsize);
	return 1;
}

static int lua__cap(lua_State *L)
{
	lbf_t * p = (lbf_t *)luaL_checkudata(L, 1, META_NAME);
	lua_pushinteger(L, p->cap);
	return 1;
}

static int lua__set(lua_State *L)
{
	int value;
	int top;
	int ref;
	int i;
	int ret;
	int retarray[FN_REF_MAX_SIZE];


	lbf_t * p = (lbf_t *)luaL_checkudata(L, 1, META_NAME);
	value = luaL_checkinteger(L, 2);
	if (p->fn_refs[0] == LUA_NOREF) {
		return luaL_error(L, "hfuncs not set!");
	}
	lua_getfenv(L, 1);
	top = lua_gettop(L);

	memset(retarray, 0, sizeof(retarray));
	for (i = 0; i < p->fn_size; i++) {
		ref = p->fn_refs[i];
		lua_rawgeti(L, -1, ref);
		if (!lua_isfunction(L, -1)) {
			return luaL_error(L, "ref=%d is not function", ref);
		}
		lua_pushinteger(L, value);
		ret = lua_pcall(L, 1, 1, 0);
		if (ret != 0) {
			return lua_error(L);
		}

		ret = lua_tointeger(L, -1);
		retarray[i] = ret;
		lua_settop(L, top);
	}
	for (i = 0; i < p->fn_size; i++) {
		SET_SET(p->bitarray, retarray[i] % p->alen);
		DLOG("set slots=%zu, ra[%d]=%d, p->alen=%zu",
		     retarray[i] % p->alen, i, retarray[i], p->alen);
	}
	p->vsize++;
	return 0;
}

static int lua__isset(lua_State *L)
{
	int i;
	int ref;
	int top;
	int value;
	int ret;
	lbf_t * p = (lbf_t *)luaL_checkudata(L, 1, META_NAME);
	value = luaL_checkinteger(L, 2);
	lua_getfenv(L, 1);
	top = lua_gettop(L);
	for (i = 0; i < p->fn_size; i++) {
		ref = p->fn_refs[i];
		lua_rawgeti(L, -1, ref);
		if (!lua_isfunction(L, -1)) {
			return luaL_error(L, "ref=%d is not function", ref);
		}
		lua_pushinteger(L, value);
		ret = lua_pcall(L, 1, 1, 0);
		if (ret != 0) {
			return lua_error(L);
		}

		ret = lua_tointeger(L, -1);
		DLOG("get slots, ret=%d, ra[%lu]=%d, p->alen=%zu",
		     ret, ret % p->alen,
		     IS_SET(p->bitarray, ret % p->alen), p->alen);

		if (!IS_SET(p->bitarray, ret % p->alen)) {
			lua_pushboolean(L, 0);
			return 1;
		}

		lua_settop(L, top);
	}
	lua_pushboolean(L, 1);
	return 1;
}


static int lua__get_slots_fn(lua_State *L)
{
	int cap = luaL_checkinteger(L, 1);
	lua_Number fakerate = luaL_optnumber(L, 2, DEFAULT_FAKE_RATE);
	size_t slots = cap * (log(fakerate)/log(0.6185)); 
	int fn_size = -log(fakerate) / log(2);
	lua_pushinteger(L, slots);
	lua_pushinteger(L, fn_size);
	return 2;
}

static int opencls__bf(lua_State *L)
{
	luaL_Reg lmethods[] = {
		{"set_hfuncs", lua__sethfuncs},
		{"set", lua__set},
		{"size", lua__size},
		{"cap", lua__cap},
		{"is_set", lua__isset},
		{"clear", lua__clear},
		{NULL, NULL},
	};
	luaL_newmetatable(L, META_NAME);
	lua_newtable(L);
	luaL_register(L, NULL, lmethods);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction (L, lua__gc);
	lua_setfield (L, -2, "__gc");
	return 1;
}


int luaopen_lbf(lua_State* L)
{
	opencls__bf(L);
	luaL_Reg lfuncs[] = {
		{"new", lua__new},
		{"slots_fn", lua__get_slots_fn},
		{NULL, NULL},
	};
	luaL_newlib(L, lfuncs);
	return 1;
}

