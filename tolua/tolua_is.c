/* tolua: functions to check types.
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
#include <string.h>

/**
 *  a fast check if a is b, without parameter validation
 *  i.e. if b is equal to a or a superclass of a.
 *
 *  比较两个类是否是同个类
 *
 *  @param L           状态机
 *  @param mt_indexa   表a位置
 *  @param mt_indexb   表b位置
 *  @param super_index 父类表位置
 *
 *  @return 是否表a和表相等
 *  @return 1 : 相等
 *  @return 0 : 不相等
 */
TOLUA_API int tolua_fast_isa(lua_State *L, int mt_indexa, int mt_indexb, int super_index)
{
    int result;
    if (lua_rawequal(L,mt_indexa,mt_indexb))
        result = 1;
    else
    {
        if (super_index) {
            lua_pushvalue(L, super_index);
        } else {
            lua_pushliteral(L,"tolua_super");
            lua_rawget(L,LUA_REGISTRYINDEX);  /* stack: super */
        };
        /* 在父类中查找类a对应的表 */
        lua_pushvalue(L,mt_indexa);       /* stack: super mta */
        lua_rawget(L,-2);                 /* stack: super super[mta] */
        
        /* 在reg中查找类b对应的名字 */
        lua_pushvalue(L,mt_indexb);       /* stack: super super[mta] mtb */
        lua_rawget(L,LUA_REGISTRYINDEX);  /* stack: super super[mta] typenameB */
        
        /* 在类a的元表中查找类b的名字 */
        lua_rawget(L,-2);                 /* stack: super super[mta] bool */
        
        /* 检查返回值 */
        result = lua_toboolean(L,-1);
        lua_pop(L,3);
    }
    return result;
}

/**
 *  Push and returns the corresponding object typename
 *
 *  获取指定位置类型
 *
 *  @param L  状态机
 *  @param lo 栈中位置
 *
 *  @return 类型
 */
TOLUA_API const char* tolua_typename (lua_State* L, int lo)
{
    int tag = lua_type(L,lo);
    if (tag == LUA_TNONE)
        lua_pushstring(L,"[no object]");
    else if (tag != LUA_TUSERDATA && tag != LUA_TTABLE)
        lua_pushstring(L,lua_typename(L,tag));
    else if (tag == LUA_TUSERDATA)
    {
        /* 对于用户数据 */
        if (!lua_getmetatable(L,lo))    /* 若没有元表 */
            lua_pushstring(L,lua_typename(L,tag));
        else                            /* 若有元表 */
        {
            /* 查询在register中对应的值 */
            lua_rawget(L,LUA_REGISTRYINDEX);
            if (!lua_isstring(L,-1))
            {

                lua_pop(L,1);
                lua_pushstring(L,"[undefined]");
            }
        }
    }
    else  /* is table */            /* 若是表 */
    {
        /* 在register中查找 */
        lua_pushvalue(L,lo);
        lua_rawget(L,LUA_REGISTRYINDEX);
        if (!lua_isstring(L,-1))
        {
            lua_pop(L,1);
            lua_pushstring(L,"table");
        }
        else
        {
            lua_pushstring(L,"class ");
            lua_insert(L,-2);
            lua_concat(L,2);
        }
    }
    return lua_tostring(L,-1);
}

/**
 *  tolua错误处理函数
 *
 *  @param L   状态机
 *  @param msg 错误消息
 *  @param err 错误描述
 */
TOLUA_API void tolua_error (lua_State* L, const char* msg, tolua_Error* err)
{
    if (msg[0] == '#')
    {
        const char* expected = err->type;
        const char* provided = tolua_typename(L,err->index);
        if (msg[1]=='f')
        {
            int narg = err->index;
            if (err->array)
                luaL_error(L,"%s\n     argument #%d is array of '%s'; array of '%s' expected.\n",
                           msg+2,narg,provided,expected);
            else
                luaL_error(L,"%s\n     argument #%d is '%s'; '%s' expected.\n",
                           msg+2,narg,provided,expected);
        }
        else if (msg[1]=='v')
        {
            if (err->array)
                luaL_error(L,"%s\n     value is array of '%s'; array of '%s' expected.\n",
                           msg+2,provided,expected);
            else
                luaL_error(L,"%s\n     value is '%s'; '%s' expected.\n",
                           msg+2,provided,expected);
        }
    }
    else
        luaL_error(L,msg);
}

/**
 *  the equivalent of lua_is* for usertable
 *
 *  成功返回，type中含有对应字符串
 *
 *  @param L    状态机
 *  @param lo   栈中位置
 *  @param type 类型描述
 *
 *  @return 是否成功获得类型
 */
static  int lua_isusertable (lua_State* L, int lo, const char* type)
{
    int r = 0;
    if (lo < 0) lo = lua_gettop(L)+lo+1;
    lua_pushvalue(L,lo);
    lua_rawget(L,LUA_REGISTRYINDEX);  /* get registry[t] */
    if (lua_isstring(L,-1))
    {
        r = strcmp(lua_tostring(L,-1),type)==0;
        if (!r)
        {
            /* try const */
            lua_pushstring(L,"const ");
            lua_insert(L,-2);
            lua_concat(L,2);
            r = lua_isstring(L,-1) && strcmp(lua_tostring(L,-1),type)==0;
        }
    }
    lua_pop(L, 1);
    return r;
}

/**
 *  检查相应位置是否为table实例
 *
 *  @param L  状态机
 *  @param lo 栈中位置
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
int push_table_instance(lua_State* L, int lo)
{
    if (lua_istable(L, lo)) {

        /* 检查 ".c_instance" */
        lua_pushstring(L, ".c_instance");
        lua_gettable(L, lo);
        
        if (lua_isuserdata(L, -1)) { /* 若是用户数据 */

            lua_replace(L, lo); /* 将用户数据替换成lo位置的表 */
            return 1;
        } else {

            lua_pop(L, 1);
            return 0;
        };
    } else {
        return 0;
    };

    return 0;
};

/**
 *  the equivalent of lua_is* for usertype
 *
 *
 *  @param L    状态机
 *  @param lo   栈中位置
 *  @param type 类型名称
 *
 *  @return 1 :
 *  @return 0 :
 */
int lua_isusertype (lua_State* L, int lo, const char* type)
{
    if (!lua_isuserdata(L,lo)) {
        if (!push_table_instance(L, lo)) {
            return 0;
        };
    };
    {
        /* check if it is of the same type */
        int r;
        const char *tn;
        if (lua_getmetatable(L,lo))        /* if metatable? */
        {
            lua_rawget(L,LUA_REGISTRYINDEX);  /* get registry[mt] */
            tn = lua_tostring(L,-1);
            r = tn && (strcmp(tn,type) == 0);
            lua_pop(L, 1);
            if (r)
                return 1;
            else
            {
                /* check if it is a specialized class */
                lua_pushstring(L,"tolua_super");
                lua_rawget(L,LUA_REGISTRYINDEX); /* get super */
                lua_getmetatable(L,lo);
                lua_rawget(L,-2);                /* get super[mt] */
                if (lua_istable(L,-1))
                {
                    int b;
                    lua_pushstring(L,type);
                    lua_rawget(L,-2);                /* get super[mt][type] */
                    b = lua_toboolean(L,-1);
                    lua_pop(L,3);
                    if (b)
                        return 1;
                }
            }
        }
    }
    return 0;
}

/**
 *  栈顶元素是否不是对象
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param err 错误描述
 *
 *  @return 1 :
 *  @return 0 :
 */
TOLUA_API int tolua_isnoobj (lua_State* L, int lo, tolua_Error* err)
{
    if (lua_gettop(L)<abs(lo))
        return 1;
    err->index = lo;
    err->array = 0;
    err->type = "[no object]";
    return 0;
}

TOLUA_API int tolua_isboolean (lua_State* L, int lo, int def, tolua_Error* err)
{
    if (def && lua_gettop(L)<abs(lo))
        return 1;
    if (lua_isnil(L,lo) || lua_isboolean(L,lo))
        return 1;
    err->index = lo;
    err->array = 0;
    err->type = "boolean";
    return 0;
}

TOLUA_API int tolua_isnumber (lua_State* L, int lo, int def, tolua_Error* err)
{
    if (def && lua_gettop(L)<abs(lo))
        return 1;
    if (lua_isnumber(L,lo))
        return 1;
    err->index = lo;
    err->array = 0;
    err->type = "number";
    return 0;
}

TOLUA_API int tolua_isstring (lua_State* L, int lo, int def, tolua_Error* err)
{
    if (def && lua_gettop(L)<abs(lo))
        return 1;
    if (lua_isnil(L,lo) || lua_isstring(L,lo))
        return 1;
    err->index = lo;
    err->array = 0;
    err->type = "string";
    return 0;
}

TOLUA_API int tolua_istable (lua_State* L, int lo, int def, tolua_Error* err)
{
    if (def && lua_gettop(L)<abs(lo))
        return 1;
    if (lua_istable(L,lo))
        return 1;
    err->index = lo;
    err->array = 0;
    err->type = "table";
    return 0;
}

TOLUA_API int tolua_isusertable (lua_State* L, int lo, const char* type, int def, tolua_Error* err)
{
    if (def && lua_gettop(L)<abs(lo))
        return 1;
    if (lua_isusertable(L,lo,type))
        return 1;
    err->index = lo;
    err->array = 0;
    err->type = type;
    return 0;
}


TOLUA_API int tolua_isuserdata (lua_State* L, int lo, int def, tolua_Error* err)
{
    if (def && lua_gettop(L)<abs(lo))
        return 1;
    if (lua_isnil(L,lo) || lua_isuserdata(L,lo))
        return 1;
    err->index = lo;
    err->array = 0;
    err->type = "userdata";
    return 0;
}

TOLUA_API int tolua_isvaluenil (lua_State* L, int lo, tolua_Error* err) {

    if (lua_gettop(L)<abs(lo))
        return 0; /* somebody else should chack this */
    if (!lua_isnil(L, lo))
        return 0;

    err->index = lo;
    err->array = 0;
    err->type = "value";
    return 1;
};

TOLUA_API int tolua_isvalue (lua_State* L, int lo, int def, tolua_Error* err)
{
    if (def || abs(lo)<=lua_gettop(L))  /* any valid index */
        return 1;
    err->index = lo;
    err->array = 0;
    err->type = "value";
    return 0;
}

TOLUA_API int tolua_isusertype (lua_State* L, int lo, const char* type, int def, tolua_Error* err)
{
    if (def && lua_gettop(L)<abs(lo))
        return 1;
    if (lua_isnil(L,lo) || lua_isusertype(L,lo,type))
        return 1;
    err->index = lo;
    err->array = 0;
    err->type = type;
    return 0;
}

TOLUA_API int tolua_isvaluearray
(lua_State* L, int lo, int dim, int def, tolua_Error* err)
{
    if (!tolua_istable(L,lo,def,err))
        return 0;
    else
        return 1;
}

TOLUA_API int tolua_isbooleanarray
(lua_State* L, int lo, int dim, int def, tolua_Error* err)
{
    if (!tolua_istable(L,lo,def,err))
        return 0;
    else
    {
        int i;
        for (i=1; i<=dim; ++i)
        {
            lua_pushnumber(L,i);
            lua_gettable(L,lo);
            if (!(lua_isnil(L,-1) || lua_isboolean(L,-1)) &&
                    !(def && lua_isnil(L,-1))
               )
            {
                err->index = lo;
                err->array = 1;
                err->type = "boolean";
                return 0;
            }
            lua_pop(L,1);
        }
    }
    return 1;
}

TOLUA_API int tolua_isnumberarray
(lua_State* L, int lo, int dim, int def, tolua_Error* err)
{
    if (!tolua_istable(L,lo,def,err))
        return 0;
    else
    {
        int i;
        for (i=1; i<=dim; ++i)
        {
            lua_pushnumber(L,i);
            lua_gettable(L,lo);
            if (!lua_isnumber(L,-1) &&
                    !(def && lua_isnil(L,-1))
               )
            {
                err->index = lo;
                err->array = 1;
                err->type = "number";
                return 0;
            }
            lua_pop(L,1);
        }
    }
    return 1;
}

TOLUA_API int tolua_isstringarray
(lua_State* L, int lo, int dim, int def, tolua_Error* err)
{
    if (!tolua_istable(L,lo,def,err))
        return 0;
    else
    {
        int i;
        for (i=1; i<=dim; ++i)
        {
            lua_pushnumber(L,i);
            lua_gettable(L,lo);
            if (!(lua_isnil(L,-1) || lua_isstring(L,-1)) &&
                    !(def && lua_isnil(L,-1))
               )
            {
                err->index = lo;
                err->array = 1;
                err->type = "string";
                return 0;
            }
            lua_pop(L,1);
        }
    }
    return 1;
}

TOLUA_API int tolua_istablearray
(lua_State* L, int lo, int dim, int def, tolua_Error* err)
{
    if (!tolua_istable(L,lo,def,err))
        return 0;
    else
    {
        int i;
        for (i=1; i<=dim; ++i)
        {
            lua_pushnumber(L,i);
            lua_gettable(L,lo);
            if (! lua_istable(L,-1) &&
                    !(def && lua_isnil(L,-1))
               )
            {
                err->index = lo;
                err->array = 1;
                err->type = "table";
                return 0;
            }
            lua_pop(L,1);
        }
    }
    return 1;
}

TOLUA_API int tolua_isuserdataarray
(lua_State* L, int lo, int dim, int def, tolua_Error* err)
{
    if (!tolua_istable(L,lo,def,err))
        return 0;
    else
    {
        int i;
        for (i=1; i<=dim; ++i)
        {
            lua_pushnumber(L,i);
            lua_gettable(L,lo);
            if (!(lua_isnil(L,-1) || lua_isuserdata(L,-1)) &&
                    !(def && lua_isnil(L,-1))
               )
            {
                err->index = lo;
                err->array = 1;
                err->type = "userdata";
                return 0;
            }
            lua_pop(L,1);
        }
    }
    return 1;
}

TOLUA_API int tolua_isusertypearray
(lua_State* L, int lo, const char* type, int dim, int def, tolua_Error* err)
{
    if (!tolua_istable(L,lo,def,err))
        return 0;
    else
    {
        int i;
        for (i=1; i<=dim; ++i)
        {
            lua_pushnumber(L,i);
            lua_gettable(L,lo);
            if (!(lua_isnil(L,-1) || lua_isuserdata(L,-1)) &&
                    !(def && lua_isnil(L,-1))
               )
            {
                err->index = lo;
                err->type = type;
                err->array = 1;
                return 0;
            }
            lua_pop(L,1);
        }
    }
    return 1;
}

#if 0
int tolua_isbooleanfield
(lua_State* L, int lo, int i, int def, tolua_Error* err)
{
    lua_pushnumber(L,i);
    lua_gettable(L,lo);
    if (!(lua_isnil(L,-1) || lua_isboolean(L,-1)) &&
            !(def && lua_isnil(L,-1))
       )
    {
        err->index = lo;
        err->array = 1;
        err->type = "boolean";
        return 0;
    }
    lua_pop(L,1);
    return 1;
}

int tolua_isnumberfield
(lua_State* L, int lo, int i, int def, tolua_Error* err)
{
    lua_pushnumber(L,i);
    lua_gettable(L,lo);
    if (!lua_isnumber(L,-1) &&
            !(def && lua_isnil(L,-1))
       )
    {
        err->index = lo;
        err->array = 1;
        err->type = "number";
        return 0;
    }
    lua_pop(L,1);
    return 1;
}

int tolua_isstringfield
(lua_State* L, int lo, int i, int def, tolua_Error* err)
{
    lua_pushnumber(L,i);
    lua_gettable(L,lo);
    if (!(lua_isnil(L,-1) || lua_isstring(L,-1)) &&
            !(def && lua_isnil(L,-1))
       )
    {
        err->index = lo;
        err->array = 1;
        err->type = "string";
        return 0;
    }
    lua_pop(L,1);
    return 1;
}

int tolua_istablefield
(lua_State* L, int lo, int i, int def, tolua_Error* err)
{
    lua_pushnumber(L,i+1);
    lua_gettable(L,lo);
    if (! lua_istable(L,-1) &&
            !(def && lua_isnil(L,-1))
       )
    {
        err->index = lo;
        err->array = 1;
        err->type = "table";
        return 0;
    }
    lua_pop(L,1);
}

int tolua_isusertablefield
(lua_State* L, int lo, const char* type, int i, int def, tolua_Error* err)
{
    lua_pushnumber(L,i);
    lua_gettable(L,lo);
    if (! lua_isusertable(L,-1,type) &&
            !(def && lua_isnil(L,-1))
       )
    {
        err->index = lo;
        err->array = 1;
        err->type = type;
        return 0;
    }
    lua_pop(L,1);
    return 1;
}

int tolua_isuserdatafield
(lua_State* L, int lo, int i, int def, tolua_Error* err)
{
    lua_pushnumber(L,i);
    lua_gettable(L,lo);
    if (!(lua_isnil(L,-1) || lua_isuserdata(L,-1)) &&
            !(def && lua_isnil(L,-1))
       )
    {
        err->index = lo;
        err->array = 1;
        err->type = "userdata";
        return 0;
    }
    lua_pop(L,1);
    return 1;
}

int tolua_isusertypefield
(lua_State* L, int lo, const char* type, int i, int def, tolua_Error* err)
{
    lua_pushnumber(L,i);
    lua_gettable(L,lo);
    if (!(lua_isnil(L,-1) || lua_isusertype(L,-1,type)) &&
            !(def && lua_isnil(L,-1))
       )
    {
        err->index = lo;
        err->type = type;
        err->array = 1;
        return 0;
    }
    lua_pop(L,1);
    return 1;
}

#endif
