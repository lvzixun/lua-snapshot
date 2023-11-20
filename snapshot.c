#include <lua.h>
#include <lauxlib.h>
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "lfunc.h"

#include "lgc.h"

#ifdef isshared
	static int
	check_shared(lua_State* L, int idx, int t) {
		switch(t) {
			case LUA_TTABLE:
			case LUA_TSTRING: {
				GCObject* o = (GCObject*)lua_topointer(L, idx);
				if(o) {
					return isshared(o);
				}
			}
			default:
			break;
		}
		return 0;
	}
#endif

struct snapshot_params {
	int max_count;
	int current_mark_count;
};

static void mark_object(lua_State *L, lua_State *dL, const void * parent, const char * desc, struct snapshot_params* args);

#define check_limit(L, args) do { \
	if((args)->max_count > 0) { \
		if(((args)->current_mark_count)++ > (args)->max_count) { \
			lua_pop((L),1); \
			return; \
		} \
	} \
} while(0);

#if LUA_VERSION_NUM == 501

static void
luaL_checkversion(lua_State *L) {
	if (lua_pushthread(L) == 0) {
		luaL_error(L, "Must require in main thread");
	}
	lua_setfield(L, LUA_REGISTRYINDEX, "mainthread");
}

static void
lua_rawsetp(lua_State *L, int idx, const void *p) {
	if (idx < 0) {
		idx += lua_gettop(L) + 1;
	}
	lua_pushlightuserdata(L, (void *)p);
	lua_insert(L, -2);
	lua_rawset(L, idx);
}

static void
lua_rawgetp(lua_State *L, int idx, const void *p) {
	if (idx < 0) {
		idx += lua_gettop(L) + 1;
	}
	lua_pushlightuserdata(L, (void *)p);
	lua_rawget(L, idx);
}

static void
lua_getuservalue(lua_State *L, int idx) {
	lua_getfenv(L, idx);
}

static void
mark_function_env(lua_State *L, lua_State *dL, const void * t, struct snapshot_params* args) {
	lua_getfenv(L,-1);
	if (lua_istable(L,-1)) {
		mark_object(L, dL, t, "[environment]");
	} else {
		lua_pop(L,1);
	}
}

// lua 5.1 has no light c function
#define is_lightcfunction(L, idx) (0)

#else

#define mark_function_env(L,dL,t,args)

static int
is_lightcfunction(lua_State *L, int idx) {
	if (lua_iscfunction(L, idx)) {
		if (lua_getupvalue(L, idx, 1) == NULL) {
			return 1;
		}
		lua_pop(L, 1);
	}
	return 0;
}

#endif

#if LUA_VERSION_NUM == 504
	/*
	** True if value of 'alimit' is equal to the real size of the array
	** part of table 't'. (Otherwise, the array part must be larger than
	** 'alimit'.)
	*/
	#define limitequalsasize(t)	(isrealasize(t) || ispow2((t)->alimit))


	/*
	** Returns the real size of the 'array' array
	*/
	static unsigned int _luaH_realasize (const Table *t) {
	  if (limitequalsasize(t))
	    return t->alimit;  /* this is the size */
	  else {
	    unsigned int size = t->alimit;
	    /* compute the smallest power of 2 not smaller than 'n' */
	    size |= (size >> 1);
	    size |= (size >> 2);
	    size |= (size >> 4);
	    size |= (size >> 8);
	    size |= (size >> 16);
	#if (UINT_MAX >> 30) > 3
	    size |= (size >> 32);  /* unsigned int has more than 32 bits */
	#endif
	    size++;
	    lua_assert(ispow2(size) && size/2 < t->alimit && t->alimit < size);
	    return size;
	  }
	}
#endif

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TABLE 1
#define FUNCTION 2
#define SOURCE 3
#define THREAD 4
#define USERDATA 5
#define STRING 6
#define MARK 7

static bool
ismarked(lua_State *dL, const void *p) {
	lua_rawgetp(dL, MARK, p);
	if (lua_isnil(dL,-1)) {
		lua_pop(dL,1);
		lua_pushboolean(dL,1);
		lua_rawsetp(dL, MARK, p);
		return false;
	}
	lua_pop(dL,1);
	return true;
}

static const void *
readobject(lua_State *L, lua_State *dL, const void *parent, const char *desc) {
	int t = lua_type(L, -1);
#ifdef isshared
	if(check_shared(L, -1, t)) {
		lua_pop(L, 1);
		return NULL;
	}
#endif
	int tidx = 0;
	switch (t) {
	case LUA_TTABLE:
		tidx = TABLE;
		break;
	case LUA_TFUNCTION:
		if (is_lightcfunction(L, -1)) {
			lua_pop(L, 1);
			return NULL;
		}
		tidx = FUNCTION;
		break;
	case LUA_TTHREAD:
		tidx = THREAD;
		break;
	case LUA_TUSERDATA:
		tidx = USERDATA;
		break;
#ifdef DUMP_STRING
	case LUA_TSTRING:
		tidx = STRING;
		break;
#endif
	default:
		lua_pop(L, 1);
		return NULL;
	}

	const void * p = NULL;
	if(t == LUA_TUSERDATA) {
		const TValue *o = NULL;
		#if LUA_VERSION_NUM == 504
			o = s2v(L->top - 1);
		#else
			o = L->top - 1;
		#endif
		// const TValue *o = index2value(L, -1);
		p = (const void*)uvalue(o);
	} else if (t == LUA_TSTRING) {
		p = lua_tostring(L, -1);
	}
	else {
		p = (const void*)lua_topointer(L, -1);
	}
	if (ismarked(dL, p)) {
		lua_rawgetp(dL, tidx, p);
		if (!lua_isnil(dL,-1)) {
			lua_pushstring(dL,desc);
			lua_rawsetp(dL, -2, parent);
		}
		lua_pop(dL,1);
		lua_pop(L,1);
		return NULL;
	}

	lua_newtable(dL);
	lua_pushstring(dL,desc);
	lua_rawsetp(dL, -2, parent);
	lua_rawsetp(dL, tidx, p);

	return p;
}

static const char *
keystring(lua_State *L, int index, char * buffer) {
	int t = lua_type(L,index);
	switch (t) {
	case LUA_TSTRING:
		return lua_tostring(L,index);
	case LUA_TNUMBER:
		sprintf(buffer,"[%lg]",lua_tonumber(L,index));
		break;
	case LUA_TBOOLEAN:
		sprintf(buffer,"[%s]",lua_toboolean(L,index) ? "true" : "false");
		break;
	case LUA_TNIL:
		sprintf(buffer,"[nil]");
		break;
	default:
		sprintf(buffer,"[%s:%p]",lua_typename(L,t),lua_topointer(L,index));
		break;
	}
	return buffer;
}

static void
mark_table(lua_State *L, lua_State *dL, const void * parent, const char * desc, struct snapshot_params* args) {
	const void * t = readobject(L, dL, parent, desc);
	if (t == NULL)
		return;

	check_limit(L, args);

	bool weakk = false;
	bool weakv = false;
	if (lua_getmetatable(L, -1)) {
		lua_pushliteral(L, "__mode");
		lua_rawget(L, -2);
		if (lua_isstring(L,-1)) {
			const char *mode = lua_tostring(L, -1);
			if (strchr(mode, 'k')) {
				weakk = true;
			}
			if (strchr(mode, 'v')) {
				weakv = true;
			}
		}
		lua_pop(L,1);

		luaL_checkstack(L, LUA_MINSTACK, NULL);
		mark_table(L, dL, t, "[metatable]", args);
	}

	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		if (weakv) {
			lua_pop(L,1);
		} else {
			char temp[32];
			const char * desc = keystring(L, -2, temp);
			mark_object(L, dL, t , desc, args);
		}
		if (!weakk) {
			lua_pushvalue(L,-1);
			mark_object(L, dL, t , "[key]", args);
		}
	}

	lua_pop(L,1);
}

static void
mark_string(lua_State *L, lua_State *dL, const void * parent, const char *desc) {
	const void* t = readobject(L, dL, parent, desc);
	if(t == NULL)
		return;
	lua_pop(L,1);
}

static void
mark_userdata(lua_State *L, lua_State *dL, const void * parent, const char *desc, struct snapshot_params* args) {
	const void * t = readobject(L, dL, parent, desc);
	if (t == NULL)
		return;

	check_limit(L, args);

	if (lua_getmetatable(L, -1)) {
		mark_table(L, dL, t, "[metatable]", args);
	}

	lua_getuservalue(L,-1);
	if (lua_isnil(L,-1)) {
		lua_pop(L,2);
	} else {
		mark_object(L, dL, t, "[uservalue]", args);
		lua_pop(L,1);
	}
}

static void
mark_function(lua_State *L, lua_State *dL, const void * parent, const char *desc, struct snapshot_params* args) {
	const void * t = readobject(L, dL, parent, desc);
	if (t == NULL)
		return;

	check_limit(L, args);

	mark_function_env(L,dL,t, args);
	int i;
	for (i=1;;i++) {
		const char *name = lua_getupvalue(L,-1,i);
		if (name == NULL)
			break;
		mark_object(L, dL, t, name[0] ? name : "[upvalue]", args);
	}
	if (lua_iscfunction(L,-1)) {
		lua_pop(L,1);
	} else {
		lua_Debug ar;
		lua_getinfo(L, ">S", &ar);
		luaL_Buffer b;
		luaL_buffinit(dL, &b);
		luaL_addstring(&b, ar.short_src);
		char tmp[16];
		sprintf(tmp,":%d",ar.linedefined);
		luaL_addstring(&b, tmp);
		luaL_pushresult(&b);
		lua_rawsetp(dL, SOURCE, t);
	}
}

static void
mark_thread(lua_State *L, lua_State *dL, const void * parent, const char *desc, struct snapshot_params* args) {
	const void * t = readobject(L, dL, parent, desc);
	if (t == NULL)
		return;

	check_limit(L, args);

	int level = 0;
	lua_State *cL = lua_tothread(L,-1);
	if (cL == L) {
		level = 1;
	} else {
		// mark stack
		int top = lua_gettop(cL);
		luaL_checkstack(cL, 1, NULL);
		int i;
		char tmp[16];
		for (i=0;i<top;i++) {
			lua_pushvalue(cL, i+1);
			sprintf(tmp, "[%d]", i+1);
			mark_object(cL, dL, cL, tmp, args);
		}
	}
	lua_Debug ar;
	luaL_Buffer b;
	luaL_buffinit(dL, &b);
	while (lua_getstack(cL, level, &ar)) {
		char tmp[128];
		lua_getinfo(cL, "Sl", &ar);
		luaL_addstring(&b, ar.short_src);
		if (ar.currentline >=0) {
			char tmp[16];
			sprintf(tmp,":%d ",ar.currentline);
			luaL_addstring(&b, tmp);
		}

		int i,j;
		for (j=1;j>-1;j-=2) {
			for (i=j;;i+=j) {
				const char * name = lua_getlocal(cL, &ar, i);
				if (name == NULL)
					break;
				snprintf(tmp, sizeof(tmp), "%s{#%s:%d}",name,ar.short_src,ar.linedefined);
				mark_object(cL, dL, t, tmp, args);
			}
		}

		++level;
	}
	luaL_pushresult(&b);
	lua_rawsetp(dL, SOURCE, t);
	lua_pop(L,1);
}

static void 
mark_object(lua_State *L, lua_State *dL, const void * parent, const char *desc, struct snapshot_params* args) {
	luaL_checkstack(L, LUA_MINSTACK, NULL);
	int t = lua_type(L, -1);
#ifdef isshared
	if(check_shared(L, -1, t)) {
		lua_pop(L, 1);
		return;
	}
#endif
	switch (t) {
	case LUA_TTABLE:
		mark_table(L, dL, parent, desc, args);
		break;
	case LUA_TUSERDATA:
		mark_userdata(L, dL, parent, desc, args);
		break;
	case LUA_TFUNCTION:
		mark_function(L, dL, parent, desc, args);
		break;
	case LUA_TTHREAD:
		mark_thread(L, dL, parent, desc, args);
		break;
	case LUA_TSTRING:
		mark_string(L, dL, parent, desc);
		break;
	default:
		lua_pop(L,1);
		break;
	}
}

static int
count_table(lua_State *L, int idx) {
	int n = 0;
	lua_pushnil(L);
	while (lua_next(L, idx) != 0) {
		++n;
		lua_pop(L,1);
	}
	return n;
}

static void
gen_table_desc(lua_State *dL, luaL_Buffer *b, const void * parent, const char *desc) {
	char tmp[32];
	size_t l = sprintf(tmp,"%p : ",parent);
	luaL_addlstring(b, tmp, l);
	luaL_addstring(b, desc);
	luaL_addchar(b, '\n');
}

static size_t 
_table_size(Table* p) {
	size_t size = sizeof(*p);
	size_t tl = (p->lastfree == NULL)?(0):(sizenode(p));
	size += tl*sizeof(Node);
	#if LUA_VERSION_NUM == 504
		size += _luaH_realasize(p)*sizeof(TValue);
	#else
		size += p->sizearray*sizeof(TValue);
	#endif
	return size;
}

static size_t
_thread_size(struct lua_State* p) {
	size_t size = sizeof(*p);
  	size += p->nci*sizeof(CallInfo);
	#ifdef stacksize
  		size += stacksize(p)*sizeof(*p->stack);
  	#else
  		size += p->stacksize*sizeof(*p->stack);
  	#endif
  	return size;
}


static size_t
_userdata_size(Udata *p) {
	#if LUA_VERSION_NUM == 504
		int size = sizeudata(p->nuvalue, p->len);
		return size;
	#else
		int l = sizeudata(p);
		size_t size = sizeludata(l);
    	return size;
	#endif
}

static size_t
_cfunc_size(CClosure* p) {
	int n = (int)(p->nupvalues);
    size_t size = sizeCclosure(n);
    return size;
}


static size_t
_lfunc_size(LClosure *p) {
	int n = (int)(p->nupvalues);
    size_t size = sizeLclosure(n);
    return size;
}

static void
pdesc(lua_State *L, lua_State *dL, int idx, const char * typename) {
	lua_pushnil(dL);
	size_t size = 0;
	char buff_sz[128] = {0};
	while (lua_next(dL, idx) != 0) {
		luaL_Buffer b;
		luaL_buffinit(L, &b);
		const void * key = lua_touserdata(dL, -2);
		switch(idx) {
			case FUNCTION: {
				lua_rawgetp(dL, SOURCE, key);
				if (lua_isnil(dL, -1)) {
					size = _cfunc_size((CClosure*)key);
					snprintf(buff_sz, sizeof(buff_sz), "{%zd}", size);
					luaL_addstring(&b,"cfunction");
					luaL_addchar(&b,' ');
					luaL_addstring(&b, buff_sz);
					luaL_addchar(&b,'\n');
				} else {
					size_t l = 0;
					size = _lfunc_size((LClosure*)key);
					snprintf(buff_sz, sizeof(buff_sz), "{%zd}", size);
					const char * s = lua_tolstring(dL, -1, &l);
					if(l==0) {
						s = "?";
						l = 1;
					}
					luaL_addlstring(&b,s,l);
					luaL_addchar(&b,' ');
					luaL_addstring(&b, buff_sz);
					luaL_addchar(&b,'\n');
				}
				lua_pop(dL, 1);
			}break;

			case THREAD: {
				lua_rawgetp(dL, SOURCE, key);
				size_t l = 0;
				size = _thread_size((struct lua_State*)key);
				snprintf(buff_sz, sizeof(buff_sz), "{%zd}", size);
				const char * s = lua_tolstring(dL, -1, &l);
				luaL_addstring(&b,"thread ");
				luaL_addlstring(&b,s,l);
				luaL_addchar(&b,' ');
				luaL_addstring(&b, buff_sz);
				luaL_addchar(&b,'\n');
				lua_pop(dL, 1);
			}break;

			case TABLE: {
				size = _table_size((Table*)key);
				snprintf(buff_sz, sizeof(buff_sz), "{%zd}", size);
				luaL_addstring(&b, typename);
				luaL_addchar(&b,' ');
				luaL_addstring(&b, buff_sz);
				luaL_addchar(&b,'\n');
			}break;

			case USERDATA: {
				Udata* p = (Udata*)key;
				size = _userdata_size(p);
				snprintf(buff_sz, sizeof(buff_sz), "{%zd}", size);
				luaL_addstring(&b, typename);
				luaL_addchar(&b,' ');
				luaL_addstring(&b, buff_sz);
				luaL_addchar(&b,'\n');
			}break;

			case STRING: {
				#if LUA_VERSION_NUM == 504
				TString* ts = (TString*)(((char*)key) - offsetof(TString, contents));
				#else
				TString* ts = (TString*)(((char*)key) - sizeof(UTString));
				#endif
				size = tsslen(ts);
				snprintf(buff_sz, sizeof(buff_sz), "{%zd}", size);
				luaL_addstring(&b, typename);
				luaL_addchar(&b,' ');
				luaL_addstring(&b, buff_sz);
				luaL_addchar(&b,'\n');
			}break;

			default: {
				snprintf(buff_sz, sizeof(buff_sz), "{0}");
				luaL_addstring(&b, typename);
				luaL_addchar(&b,' ');
				luaL_addstring(&b, buff_sz);
				luaL_addchar(&b,'\n');
			}break;
		}
		lua_pushnil(dL);
		while (lua_next(dL, -2) != 0) {
			const void * parent = lua_touserdata(dL,-2);
			const char * desc = luaL_checkstring(dL,-1);
			gen_table_desc(dL, &b, parent, desc);
			lua_pop(dL,1);
		}
		luaL_pushresult(&b);
		lua_rawsetp(L, -2, key);
		lua_pop(dL,1);
	}
}

static void
gen_result(lua_State *L, lua_State *dL) {
	int count = 0;
	count += count_table(dL, TABLE);
	count += count_table(dL, FUNCTION);
	count += count_table(dL, USERDATA);
	count += count_table(dL, THREAD);
	count += count_table(dL, STRING);
	lua_createtable(L, 0, count);
	pdesc(L, dL, TABLE, "table");
	pdesc(L, dL, USERDATA, "userdata");
	pdesc(L, dL, FUNCTION, "function");
	pdesc(L, dL, THREAD, "thread");
	pdesc(L, dL, STRING, "string");
}

static int
snapshot(lua_State *L) {
	int i;
	struct snapshot_params args = {0};
	args.max_count = luaL_optinteger(L, 1, 0);
	lua_State *dL = luaL_newstate();
	for (i=0;i<MARK;i++) {
		lua_newtable(dL);
	}
	lua_pushvalue(L, LUA_REGISTRYINDEX);
	mark_table(L, dL, NULL, "[registry]", &args);
	gen_result(L, dL);
	lua_close(dL);
	return 1;
}

static int
l_str2lightuserdata(lua_State* L) {
	const char* s = luaL_checklstring(L, 1, NULL);
	void* p = NULL;
	sscanf(s, "%p", &p);
	lua_pushlightuserdata(L, p);
	return 1;
}

static int
l_lightuserdata2str(lua_State* L) {
	void* p = lua_touserdata(L, 1);
	char buff[128];
	snprintf(buff, sizeof(buff), "%p", p);
	lua_pushstring(L, buff);
	return 1;
}

static int
l_obj2addr(lua_State* L) {
	int t = lua_type(L, 1);
	void * p = NULL;
	if(t == LUA_TUSERDATA) {
		const TValue *o = NULL;
		#if LUA_VERSION_NUM == 504
			o = s2v(L->top - 1);
		#else
			o = L->top - 1;
		#endif
		// const TValue *o = index2value(L, -1);
		p = (void*)uvalue(o);
	} else if (t == LUA_TSTRING) {
		p = (void*)lua_tostring(L, -1);
	}
	else {
		p = (void*)lua_topointer(L, -1);
	}
	lua_pushlightuserdata(L, p);
	return 1;
}

int
luaopen_snapshot(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{"snapshot", snapshot},
		{"str2ud", l_str2lightuserdata},
		{"ud2str", l_lightuserdata2str},
		{"obj2addr", l_obj2addr},
		{NULL, NULL},
	};
	// lua_pushcfunction(L, snapshot);
	luaL_newlib(L, l);
	return 1;
}
