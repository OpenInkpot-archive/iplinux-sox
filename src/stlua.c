/*
 * luast - miscellaneous Lua support functions.
 *
 * Copyright 2006-2007 Reuben Thomas
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, write to the Free Software
 * Foundation, Fifth Floor, 51 Franklin Street, Boston, MA 02111-1301,
 * USA.  */

#include "st_i.h"

#include <assert.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* st_sample_t arrays */

static const char *handle = "st_sample_t array";

int st_lua_pusharray(lua_State *L, st_sample_t_array_t arr)
{
  lua_newuserdata(L, sizeof(st_sample_t_array_t));
  *(st_sample_t_array_t *)lua_touserdata(L, -1) = arr;
  luaL_getmetatable(L, handle);
  lua_setmetatable(L, -2);
  return 1;
}

/* array, key -> value */
static int arr_index(lua_State *L)
{
  st_sample_t_array_t *p = luaL_checkudata(L, 1, handle);
  lua_Integer k = luaL_checkinteger(L, 2);

  if ((st_size_t)k == 0 || (st_size_t)k > p->size)
    lua_pushnil(L);
  else
    lua_pushinteger(L, (lua_Integer)p->data[k - 1]);
    
  return 1;
}

/* array, key, value -> */
static int arr_newindex(lua_State *L)
{
  st_sample_t_array_t *p = luaL_checkudata(L, 1, handle);
  lua_Integer k = luaL_checkinteger(L, 2);
  lua_Integer v = luaL_checkinteger(L, 3);

  /* FIXME: Have some indication for out of range */
  if ((st_size_t)k > 0 && (st_size_t)k <= p->size)
    p->data[k - 1] = v;
    
  return 0;
}

/* array -> #array */
static int arr_len(lua_State *L)
{
  st_sample_t_array_t *p;
  p = luaL_checkudata(L, 1, handle);
  lua_pushinteger(L, (lua_Integer)p->size);
  return 1;
}

static int arr_tostring(lua_State *L)
{
  char buf[256];
  void *udata = luaL_checkudata(L, 1, handle);
  if(udata) {
    sprintf(buf, "%s (%p)", handle, udata);
    lua_pushstring(L, buf);
  }
  else {
    sprintf(buf, "must be userdata of type '%s'", handle);
    luaL_argerror(L, 1, buf);
  }
  return 1;
}

/* Metatable */
static const luaL_reg meta[] = {
  {"__index", arr_index},
  {"__newindex", arr_newindex},
  {"__len", arr_len},
  {"__tostring", arr_tostring},
  {NULL, NULL}
};

/* Allocator function for use by Lua */
static void *lua_alloc(void *ud UNUSED, void *ptr, size_t osize UNUSED, size_t nsize)
{
  if (nsize == 0) {
    free(ptr);
    return NULL;
  } else
    return xrealloc(ptr, nsize);
}

/* Create a Lua state and set it up for libst use */
lua_State *st_lua_new(void)
{
  lua_State *L;

  /* Since the allocator quits if it fails, this should always
     succeed if it returns. */
  assert((L = lua_newstate(lua_alloc, NULL)));

  /* TODO: If concerned about security, lock down here: in particular,
     don't open the io library. */
  luaL_openlibs(L);
  lua_cpcall(L, luaopen_int, NULL);

  /* Create st_sample_t array userdata type */
  createmeta(L, handle);
  luaL_register(L, NULL, meta);
  lua_pop(L, 1);

  /* Stop file handles being GCed */
  luaL_getmetatable(L, LUA_FILEHANDLE);
  lua_pushnil(L);
  lua_setfield(L, -2, "__gc");
  lua_pop(L, 1);

  return L;
}

/* Push a FILE * as a Lua file handle */
void st_lua_pushfile(lua_State *L, FILE *fp)
{
  FILE **pf = lua_newuserdata(L, sizeof *pf);
  *pf = NULL; /* Avoid potential GC before we're fully initialised */
  /* This step is currently unnecessary as we've disabled the __gc method,
     but if sox.c were rewritten in Lua, that wouldn't be needed, and
     this step would be. */
  luaL_getmetatable(L, LUA_FILEHANDLE);
  lua_setmetatable(L, -2);
  *pf = fp;
}
