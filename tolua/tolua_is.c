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
 *  @param super_index 超类表位置
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
            lua_rawget(L,LUA_REGISTRYINDEX);    /* stack: super:=tolua_super */
        };
        /* 在父类中查找类a对应的表 */
        lua_pushvalue(L,mt_indexa);             /* stack: super mta */
        lua_rawget(L,-2);                       /* stack: super sta:=super[mta] */
        
        /* 在reg中查找类b对应的名字 */
        lua_pushvalue(L,mt_indexb);             /* stack: super sta mtb */
        lua_rawget(L,LUA_REGISTRYINDEX);        /* stack: super sta nameb:=reg.mtb */
        
        /* 在类a的元表中查找类b的名字 */
        lua_rawget(L,-2);                       /* stack: super sta bool:=sta.nameb */
        
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
    else if (tag != LUA_TUSERDATA && tag != LUA_TTABLE) /* 除了用户数据和表之外 */
        lua_pushstring(L,lua_typename(L,tag));
    else if (tag == LUA_TUSERDATA)                      /* 对于用户数据 */
    {
        if (!lua_getmetatable(L,lo))                    /* 若没有元表 */
            lua_pushstring(L,lua_typename(L,tag));
        else                                            /* 若有元表 */
        {
            /* 查询在reg中对应的值 -- 类名 */
            lua_rawget(L,LUA_REGISTRYINDEX);
            if (!lua_isstring(L,-1))
            {
                lua_pop(L,1);
                lua_pushstring(L,"[undefined]");
            }
        }
    }
    else                                                /* 若是表 */
    {
        /* 在reg中查找 */
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
            lua_concat(L,2);                            /* 即 "class xxxx" */
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
 *  即检查栈中lo处对象是否含有一个.c_instance字段的用户数据
 *  
 *  有则将其入栈，否则什么都不做。
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
 *  检查是否为type类型的用户数据
 *
 *  @param L    状态机
 *  @param lo   栈中位置
 *  @param type 类型名称
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
int lua_isusertype (lua_State* L, int lo, const char* type)
{
    if (!lua_isuserdata(L,lo)) { /* 若不是用户数据 */
        if (!push_table_instance(L, lo)) { /* 若是table查询是否有c_instance字段 */
            return 0;
        };
    };
    {
        /* check if it is of the same type */
        int r;
        const char *tn;
        /* 获得lo处的元表 */
        if (lua_getmetatable(L,lo))        /* if metatable? */
        {
            /* 在registry中查询名字 */
            lua_rawget(L,LUA_REGISTRYINDEX);  /* get registry[mt] */
            /* 转成c字符串 */
            tn = lua_tostring(L,-1);
            /* 比较类型 */
            r = tn && (strcmp(tn,type) == 0);
            lua_pop(L, 1);
            
            if (r) /* 相同 */
                return 1;
            else   /* 不同 */
            {
                /* check if it is a specialized class */
                
                /* 检查元表在全局tolua_super中的值 */
                lua_pushstring(L,"tolua_super");
                lua_rawget(L,LUA_REGISTRYINDEX); /* get super */
                lua_getmetatable(L,lo);
                lua_rawget(L,-2);                /* get super[mt] */
                if (lua_istable(L,-1)) /* 若是表 */
                {
                    int b;
                    /* 检查是否有如此类型 */
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
 *  栈中位置是否不是对象
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

/**
 *  栈中位置是否为布尔
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  栈中位置是否是数字
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  栈中位置是否是字符串
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  栈中位置是否为表
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  战中位置是否是用户数据
 *
 *  @param L    状态机
 *  @param lo   栈中位置
 *  @param type 类型
 *  @param def  预设值
 *  @param err  错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  栈中位置是否为用户数据
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  栈中位置是否为nil
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  栈中位置是否为lua值
 *  
 *  只需满足lo在栈中即可
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
TOLUA_API int tolua_isvalue (lua_State* L, int lo, int def, tolua_Error* err)
{
    if (def || abs(lo)<=lua_gettop(L))  /* any valid index */
        return 1;
    err->index = lo;
    err->array = 0;
    err->type = "value";
    return 0;
}

/**
 *  栈中位置是否为用户数据
 *
 *  @param L    状态机
 *  @param lo   栈中位置
 *  @param type 类型
 *  @param def  预设值
 *  @param err  错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  是否为数值数组
 *
 *  即检查是否为table
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param dim 数组数量
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
TOLUA_API int tolua_isvaluearray
(lua_State* L, int lo, int dim, int def, tolua_Error* err)
{
    if (!tolua_istable(L,lo,def,err))
        return 0;
    else
        return 1;
}

/**
 *  是否为布尔数组
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param dim 数组数量
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  是否为数字数组
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param dim 数组数量
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  检查是否为字符串数组
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param dim 数组数量
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  是否为表数组
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param dim 数组数量
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  是否用户数据数组
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param dim 数组数量
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  是否为用户类型数组
 *
 *  @param L    状态机
 *  @param type 类型
 *  @param lo   栈中位置
 *  @param dim  数组数量
 *  @param def  预设值
 *  @param err  错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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
/**
 *  检查表中位置是否为布尔类型
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param i   表中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  检查表中位置是否为数字类型
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param i   表中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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
/**
 *  检查表中位置是否为布尔类型
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param i   表中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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
/**
 *  检查表中位置是否为布尔类型
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param i   表中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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
/**
 *  检查表中位置是否为布尔类型
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param i   表中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  检查表中位置是否为用户数据类型
 *
 *  @param L   状态机
 *  @param lo  栈中位置
 *  @param i   表中位置
 *  @param def 预设值
 *  @param err 错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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

/**
 *  检查表中位置是否为用户类型
 *
 *  @param L    状态机
 *  @param lo   栈中位置
 *  @param type 类型
 *  @param i    表中位置
 *  @param def  预设值
 *  @param err  错误描述
 *
 *  @return 1 : 是
 *  @return 0 : 否
 */
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
