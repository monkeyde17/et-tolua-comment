/* tolua: functions to push C values.
** Support code for Lua bindings.
** Written by Waldemar Celes
** TeCGraf/PUC-Rio
** Apr 2003
** $Id: $
*/

/* This code is free software; you can redistribute it and/or modify it.
** The software provided hereunder is on an "as is" basis, and
** the author has no obligation to provide maintenance, support, updates,
** enhancements, or modifications.
*/

#include "tolua++.h"
#include "lauxlib.h"

#include <stdlib.h>

/**
 *
 *
 *  @param L         状态机
 *  @param value     用户数据
 *  @param type      类型
 *  @param addToRoot 是否加入reg.tolua_value_root
 */
void tolua_pushusertype_internal (lua_State* L, void* value, const char* type, int addToRoot)
{
    if (value == NULL)
        lua_pushnil(L);
    else
    {
        /* 查找reg.type */
        luaL_getmetatable(L, type);                                 /* stack: mt */
        if (lua_isnil(L, -1)) { /* NOT FOUND metatable */
            lua_pop(L, 1);
            return;
        }
        /* 查找reg.type.tolua_ubox */
        lua_pushstring(L,"tolua_ubox");
        lua_rawget(L,-2);                                           /* stack: mt ubox */
        if (lua_isnil(L, -1)) { /* 若没有该字段， 则加入全局的reg.tolua_ubox */
            lua_pop(L, 1);
            lua_pushstring(L, "tolua_ubox");
            lua_rawget(L, LUA_REGISTRYINDEX);
        };
        
        /* 将用户数据地址入栈 */
        /* 对象以本身地址为键，不会出现重复的现象 */
        lua_pushlightuserdata(L,value);                             /* stack: mt ubox key<value> */
        
        /* 在ubox中获得数据对象 */
        lua_rawget(L,-2);                                           /* stack: mt ubox ubox[value] */
        
        if (lua_isnil(L,-1)) /* 若为空，需要设置对象，并加入ubox中 */
        {
            /* 先将nil出栈 */
            lua_pop(L,1);                                           /* stack: mt ubox */
            /* 将用户数据地址入栈 */
            lua_pushlightuserdata(L,value);
            /* 新建一块用户数据，并把地址指向value */
            *(void**)lua_newuserdata(L,sizeof(void *)) = value;     /* stack: mt ubox value newud */
            /* 复制用户数据 */
            lua_pushvalue(L,-1);                                    /* stack: mt ubox value newud newud */
            /* 将用户数据移动到-4 */
            lua_insert(L,-4);                                       /* stack: mt newud ubox value newud */
            /* 用户数据地址和用户数据加入ubox */
            lua_rawset(L,-3);                  /* ubox[value] = newud, stack: mt newud ubox */
            /* 将ubox出栈 */
            lua_pop(L,1);                                           /* stack: mt newud */
            /*luaL_getmetatable(L,type);*/
            /* 将用户数据的元表设置成reg.tolua_ubox */
            lua_pushvalue(L, -2);                                   /* stack: mt newud mt */
            lua_setmetatable(L,-2);                      /* update mt, stack: mt newud */
            
#ifdef LUA_VERSION_NUM
            /* 设置用户数据的环境表为registry */
            lua_pushvalue(L, TOLUA_NOPEER);             /* stack: mt newud peer */
            lua_setfenv(L, -2);                         /* stack: mt newud */
#endif
        }
        else /* 若不为空 */
        {
            /* check the need of updating the metatable to a more specialized class */
            
            /* 将ubox删除 */
            lua_insert(L,-2);                                       /* stack: mt ubox[u] ubox */
            lua_pop(L,1);                                           /* stack: mt ubox[u] */
            
            /* 加入全局tolua_super */
            lua_pushstring(L,"tolua_super");
            lua_rawget(L,LUA_REGISTRYINDEX);                        /* stack: mt ubox[u] super */
            
            /* 获得对象的元表 */
            lua_getmetatable(L,-2);                                 /* stack: mt ubox[u] super mt */
            /* 在tolua_super中查找 */
            lua_rawget(L,-2);                                       /* stack: mt ubox[u] super super[mt] */
            
            if (lua_istable(L,-1)) /* 若查找到一个表 */
            {
                /* 在这个表中查询类型 type */
                lua_pushstring(L,type);                             /* stack: mt ubox[u] super super[mt] type */
                lua_rawget(L,-2);                                   /* stack: mt ubox[u] super super[mt] flag */
                
                if (lua_toboolean(L,-1) == 1)                       /* if true */
                {
                    lua_pop(L,3);                                   /* mt ubox[u]*/
                    lua_remove(L, -2);
                    return;
                }
            }
            /* type represents a more specilized type */
            /*luaL_getmetatable(L,type);             // stack: mt ubox[u] super super[mt] flag mt */
            lua_pushvalue(L, -5);                    /* stack: mt ubox[u] super super[mt] flag mt */
            lua_setmetatable(L,-5);                /* stack: mt ubox[u] super super[mt] flag */
            lua_pop(L,3);                          /* stack: mt ubox[u] */
        }
        /* 删除元表 */
        lua_remove(L, -2);    /* stack: ubox[u]*/
        
        if (0 != addToRoot)
        {
            /* 将用户数据加入到reg.tolua_value_root */
            lua_pushvalue(L, -1);
            tolua_add_value_to_root(L, value);
        }
    } 
}

/**
 *  将栈中位置处数值入栈
 *
 *  @param L  状态机
 *  @param lo 栈中位置
 */
TOLUA_API void tolua_pushvalue (lua_State* L, int lo)
{
    lua_pushvalue(L,lo);
}

/**
 *  将栈中位置处布尔值入栈
 *
 *  @param L     状态机
 *  @param value 布尔值
 */
TOLUA_API void tolua_pushboolean (lua_State* L, int value)
{
    lua_pushboolean(L,value);
}

/**
 *  将栈中位置处数字入栈
 *
 *  @param L     状态机
 *  @param value 数字
 */
TOLUA_API void tolua_pushnumber (lua_State* L, lua_Number value)
{
    lua_pushnumber(L,value);
}

/**
 *  将字符串入栈
 *
 *  @param L     状态机
 *  @param value 字符串
 */
TOLUA_API void tolua_pushstring (lua_State* L, const char* value)
{
    if (value == NULL)
        lua_pushnil(L);
    else
        lua_pushstring(L,value);
}

/**
 *  将用户数据地址入栈
 *
 *  @param L     状态机
 *  @param value 用户数据地址
 */
TOLUA_API void tolua_pushuserdata (lua_State* L, void* value)
{
    if (value == NULL)
        lua_pushnil(L);
    else
        lua_pushlightuserdata(L,value);
}

/**
 *  将栈中位置处数值入栈
 *
 *  @param L  状态机
 *  @param lo 栈中位置
 */
TOLUA_API void tolua_pushusertype (lua_State* L, void* value, const char* type)
{
    tolua_pushusertype_internal(L, value, type, 0);
}

TOLUA_API void tolua_pushusertype_and_addtoroot (lua_State* L, void* value, const char* type)
{
    tolua_pushusertype_internal(L, value, type, 1);
}

TOLUA_API void tolua_pushusertype_and_takeownership (lua_State* L, void* value, const char* type)
{
    tolua_pushusertype(L,value,type);
    tolua_register_gc(L,lua_gettop(L));
}

/**
 *  前提：用户数据在栈顶
 *
 *  将用户数据加入到tolua_value_root中，之后将用户数据出栈
 *
 *  @param L   状态机
 *  @param ptr 用户数据地址
 */
TOLUA_API void tolua_add_value_to_root(lua_State* L, void* ptr)
{
    /* 查询reg.tolua_value_root入栈 */
    lua_pushstring(L, TOLUA_VALUE_ROOT);
    lua_rawget(L, LUA_REGISTRYINDEX);                               /* stack: value root */
    
    /* 交换用户数据和reg.tolua_value_root位置 */
    lua_insert(L, -2);                                              /* stack: root value */
    
    /* 用户数据地址入栈 */
    lua_pushlightuserdata(L, ptr);                                  /* stack: root value ptr */
    
    /* 交换用户数据和用户数据地址位置 */
    lua_insert(L, -2);                                              /* stack: root ptr value */
    
    /* 将用户数据地址，用户数据加入到reg.tolua_value_root*/
    lua_rawset(L, -3);                                              /* root[ptr] = value, stack: root */
    
    /* 将reg.tolua_value_root出栈 */
    lua_pop(L, 1);                                                  /* stack: - */
}

/**
 *  将用户数据从tolua_value_root中删除
 *
 *  @param L   状态机
 *  @param ptr 用户数据
 */
TOLUA_API void tolua_remove_value_from_root (lua_State* L, void* ptr)
{
    /* 将reg.tolua_value_root入栈 */
    lua_pushstring(L, TOLUA_VALUE_ROOT);
    lua_rawget(L, LUA_REGISTRYINDEX);                               /* stack: root */
    
    /* 将用户数据地址入栈 */
    lua_pushlightuserdata(L, ptr);                                  /* stack: root ptr */
    /* 入栈nil，作为用户数据地址的在reg.tolua_value_root中的值 */
    lua_pushnil(L);                                                 /* stack: root ptr nil */
    
    /* 将键值对加入tolua_value_root中 */
    lua_rawset(L, -3);                                              /* root[ptr] = nil, stack: root */
    
    /* 将tolua_value_root出栈 */
    lua_pop(L, 1);
}

/**
 *  向表中添加以数字为键，lua值为值字段
 *
 *  @param L     状态机
 *  @param lo    栈中位置
 *  @param index 索引
 *  @param v     值在栈中位置
 */
TOLUA_API void tolua_pushfieldvalue (lua_State* L, int lo, int index, int v)
{
    lua_pushnumber(L,index);
    lua_pushvalue(L,v);
    lua_settable(L,lo);
}

/**
 *  向表中添加以数字为键，布尔为值的字段
 *
 *  @param L     状态机
 *  @param lo    栈中位置
 *  @param index 索引
 *  @param v     值在栈中位置
 */
TOLUA_API void tolua_pushfieldboolean (lua_State* L, int lo, int index, int v)
{
    lua_pushnumber(L,index);
    lua_pushboolean(L,v);
    lua_settable(L,lo);
}


/**
 *  向表中添加以数字为键，数字为值的字段
 *
 *  @param L     状态机
 *  @param lo    栈中位置
 *  @param index 索引
 *  @param v     数字
 */
TOLUA_API void tolua_pushfieldnumber (lua_State* L, int lo, int index, lua_Number v)
{
    lua_pushnumber(L,index);
    tolua_pushnumber(L,v);
    lua_settable(L,lo);
}

/**
 *  向表中添加以数字为键，字符串为值的字段
 *
 *  @param L     状态机
 *  @param lo    栈中位置
 *  @param index 索引
 *  @param v     字符串
 */
TOLUA_API void tolua_pushfieldstring (lua_State* L, int lo, int index, const char* v)
{
    lua_pushnumber(L,index);
    tolua_pushstring(L,v);
    lua_settable(L,lo);
}
/**
 *  向表中添加以数字为键，用户数据为值的字段
 *
 *  @param L     状态机
 *  @param lo    栈中位置
 *  @param index 索引
 *  @param v     用户数据
 */
TOLUA_API void tolua_pushfielduserdata (lua_State* L, int lo, int index, void* v)
{
    lua_pushnumber(L,index);
    tolua_pushuserdata(L,v);
    lua_settable(L,lo);
}

/**
 *  向表中添加以数字为键，用户类型为值的字段
 *
 *  @param L     状态机
 *  @param lo    栈中位置
 *  @param index 索引
 *  @param v     用户类型
 */
TOLUA_API void tolua_pushfieldusertype (lua_State* L, int lo, int index, void* v, const char* type)
{
    lua_pushnumber(L,index);
    tolua_pushusertype(L,v,type);
    lua_settable(L,lo);
}

/**
 *  向表中添加以数字为键，用户类型串为值的字段
 *
 *  多了tolua_register_gc
 *
 *  @param L     状态机
 *  @param lo    栈中位置
 *  @param index 索引
 *  @param v     用户类型
 */
TOLUA_API void tolua_pushfieldusertype_and_takeownership (lua_State* L, int lo, int index, void* v, const char* type)
{
    lua_pushnumber(L,index);
    tolua_pushusertype(L,v,type);
    tolua_register_gc(L,lua_gettop(L));
    lua_settable(L,lo);
}

