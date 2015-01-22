/* tolua: funcitons to convert to C types
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

#include <string.h>
#include <stdlib.h>



/**
 *  数字转换
 *
 *  @param L    状态机
 *  @param narg 栈中位置
 *  @param def  预设值
 *
 *  @return lua中的数字
 */
TOLUA_API lua_Number tolua_tonumber (lua_State* L, int narg, lua_Number def)
{
    /* 会先检查narg是否超出栈的容量 */
    return lua_gettop(L)<abs(narg) ? def : lua_tonumber(L,narg);
}

/**
 *  字符串转换
 *
 *  @param L    状态机
 *  @param narg 栈中位置
 *  @param def  预设值
 *
 *  @return c字符串
 */
TOLUA_API const char* tolua_tostring (lua_State* L, int narg, const char* def)
{
    return lua_gettop(L)<abs(narg) ? def : lua_tostring(L,narg);
}

/**
 *  转换成userdata
 *
 *  @param L    状态机
 *  @param narg 栈中位置
 *  @param def  预设值
 */
TOLUA_API void* tolua_touserdata (lua_State* L, int narg, void* def)
{

    /* return lua_gettop(L)<abs(narg) ? def : lua_touserdata(L,narg); */

    if (lua_gettop(L)<abs(narg)) {
        return def;
    };

    /* 检查是否是lightuserdata */
    if (lua_islightuserdata(L, narg)) {

        return lua_touserdata(L,narg);
    };

    return tolua_tousertype(L, narg, def);
}

extern int push_table_instance(lua_State* L, int lo);

/**
 *
 *
 *  @param L    状态机
 *  @param narg 栈中位置
 *  @param def  预设值
 */
TOLUA_API void* tolua_tousertype (lua_State* L, int narg, void* def)
{
    if (lua_gettop(L)<abs(narg))
        return def;
    else
    {
        void* u;
        if (!lua_isuserdata(L, narg)) {
            if (!push_table_instance(L, narg)) return NULL;
        };
        u = lua_touserdata(L,narg);
        return (u==NULL) ? NULL : *((void**)u); /* nil represents NULL */
    }
}

/**
 *  检查narg是否在栈中，否则返回预设值
 *
 *  @param L    状态机
 *  @param narg 栈中位置
 *  @param def  预设值
 *
 *  @return 位置
 */
TOLUA_API int tolua_tovalue (lua_State* L, int narg, int def)
{
    return lua_gettop(L)<abs(narg) ? def : narg;
}

/**
 *  获得栈顶bool值
 *
 *  @param L    状态机
 *  @param narg 栈中位置
 *  @param def  预设值
 *
 *  @return bool值
 */
TOLUA_API int tolua_toboolean (lua_State* L, int narg, int def)
{
    return lua_gettop(L)<abs(narg) ?  def : lua_toboolean(L,narg);
}

/**
 *  获得某个表的一个数字字段值
 *
 *  即 table[index] 是一个数字
 *
 *  @param L     状态机
 *  @param lo    栈中位置 表的位置
 *  @param index 数字位置 索引位置
 *  @param def   预设值
 *
 *  @return 数值
 */
TOLUA_API lua_Number tolua_tofieldnumber (lua_State* L, int lo, int index, lua_Number def)
{
    double v;
    lua_pushnumber(L,index);
    lua_gettable(L,lo);
    v = lua_isnil(L,-1) ? def : lua_tonumber(L,-1);
    lua_pop(L,1);
    return v;
}

/**
 *  获取某个表的一个字符串的值
 *
 *  即table[index] 是一个字符串
 *
 *  @param L     状态机
 *  @param lo    表的位置
 *  @param index 索引的位置
 *  @param def   预设值
 *
 *  @return 字符串
 */
TOLUA_API const char* tolua_tofieldstring (lua_State* L, int lo, int index, const char* def)
{
    const char* v;
    lua_pushnumber(L,index);
    lua_gettable(L,lo);
    v = lua_isnil(L,-1) ? def : lua_tostring(L,-1);
    lua_pop(L,1);
    return v;
}

/**
 *  获取某个表中的用户数据字段
 *
 *  即 table[index] 是一个用户数据
 *
 *  @param L     状态机
 *  @param lo    表的位置
 *  @param index 索引的位置
 *  @param def   预设值
 *
 *  @return 用户数据 (void*)
 */
TOLUA_API void* tolua_tofielduserdata (lua_State* L, int lo, int index, void* def)
{
    void* v;
    lua_pushnumber(L,index);
    lua_gettable(L,lo);
    v = lua_isnil(L,-1) ? def : lua_touserdata(L,-1);
    lua_pop(L,1);
    return v;
}

/**
 *  获取某个表中的用户类型
 *
 *
 *  @param L     状态机
 *  @param lo    表的位置
 *  @param index 索引的位置
 *  @param def   预设值
 
 *  @return usertype
 */
TOLUA_API void* tolua_tofieldusertype (lua_State* L, int lo, int index, void* def)
{
    void* v;
    lua_pushnumber(L,index);
    lua_gettable(L,lo);
    v = lua_isnil(L,-1) ? def : (*(void **)(lua_touserdata(L, -1))); /* lua_unboxpointer(L,-1); */
    lua_pop(L,1);
    return v;
}

/**
 *  获取某表中的索引值
 *
 *  @param L     状态机
 *  @param lo    表的位置
 *  @param index 索引的位置
 *  @param def   预设值
 *
 *  @return 表的位置
 */
TOLUA_API int tolua_tofieldvalue (lua_State* L, int lo, int index, int def)
{
    int v;
    lua_pushnumber(L,index);
    lua_gettable(L,lo);
    v = lua_isnil(L,-1) ? def : lo;
    lua_pop(L,1);
    return v;
}

/**
 *  获取某表中的布尔字段值
 *
 *  即 table[index] 为布尔类型
 *
 *  @param L     状态机
 *  @param lo    表的位置
 *  @param index 索引的位置
 *  @param def   预设值
 *
 *  @return 布尔值
 */
TOLUA_API int tolua_getfieldboolean (lua_State* L, int lo, int index, int def)
{
    int v;
    lua_pushnumber(L,index);
    lua_gettable(L,lo);
    v = lua_isnil(L,-1) ? 0 : lua_toboolean(L,-1);
    lua_pop(L,1);
    return v;
}
