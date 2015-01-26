/* tolua: functions to map features
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
#include "tolua_event.h"
#include "lauxlib.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


/**
 *  Create metatable
 *
 *  Create and register new metatable
 *
 *  在全局注册表(register)中创建一个新的表，并把各个元方法注册到register中
 *
 *  @param L    状态机
 *  @param name 注册名称
 *
 *  @return 1 : 成功
 *  @return 0 : 失败
 */
static int tolua_newmetatable (lua_State* L, const char* name)
{
    /* 创建一个新的元表 */
    /* 将元表添加到全局注册表中 */
    /* register.name = {} */
    int r = luaL_newmetatable(L,name);

#ifdef LUA_VERSION_NUM /* only lua 5.1 */
    if (r) {
        lua_pushvalue(L, -1);
        lua_pushstring(L, name);
        
        /* lua5.1需要重新查询一次，将元表放到栈顶 */
        /* 是将返回的 表r 作为键 name 作为值 */
        /* register.r = name */
        lua_settable(L, LUA_REGISTRYINDEX); /* reg[mt] = type_name */
    };
#endif

    /* 重新绑定注册各个元方法 */
    if (r)
        tolua_classevents(L); /* set meta events */
    
    /* 将这个表出栈 */
    lua_pop(L,1);
    return r;
}

/**
 * Map super classes
 *
 * It sets 'name' as being also a 'base', mapping all super classes of 'base' in 'name'
 *
 *
 *  @param L    状态机
 *  @param name 子类
 *  @param base 基类
 */
static void mapsuper (lua_State* L, const char* name, const char* base)
{
    /* push registry.super */
    /* 获得全局注册表中的tolua_super，并入栈 */
    lua_pushstring(L,"tolua_super");
    lua_rawget(L,LUA_REGISTRYINDEX);    /* stack: super */
    
    /* 将registry[name]表入栈 */
    luaL_getmetatable(L,name);          /* stack: super mt */
    /* 查询registry[name]表在tolua_super中的值 */
    lua_rawget(L,-2);                   /* stack: super table */
    
    /* 若为空的话，则新建一个表，并加入tolua_super表中 */
    if (lua_isnil(L,-1))
    {
        /* create table */
        /* 先将栈顶的nil出栈 */
        lua_pop(L,1);
        
        /* 新建一个表并入栈 */
        lua_newtable(L);                    /* stack: super table */
        /* 获得栈顶表的元表并入栈 */
        luaL_getmetatable(L,name);          /* stack: super table mt */
        /* 复制表到栈顶 */
        lua_pushvalue(L,-2);                /* stack: super table mt table */
        /* 加入tolua_super表中 */
        /* tolua_super[元表mt] = 表t */
        lua_rawset(L,-4);                   /* stack: super table */
    }

    /* set base as super class */
    /* 将 基类base 加入tolua_super中 */
    lua_pushstring(L,base);
    lua_pushboolean(L,1);
    /* tolua_super[基类base] = true */
    lua_rawset(L,-3);                    /* stack: super table */

    /* set all super class of base as super class of name */
    /* 获得 基类base 的元表 */
    luaL_getmetatable(L,base);          /* stack: super table base_mt */
    /* 查询 基类base 在tolua_super中的值 */
    /* tolua_super[基类base] */
    lua_rawget(L,-3);                   /* stack: super table base_table */
    /* 如果tolua_super[基类base]是一个表的话 -- 基类base对应表 */
    if (lua_istable(L,-1))
    {
        /* traverse base table */
        /* 遍历整个 base对应表  */
        /* 将所有的键值对加入到 tolua_super 表中 */
        
        /* 先将nil入栈作为第一个查询的键 */
        lua_pushnil(L);  /* first key */
        /* lua_next会在栈上弹出一个键，将下一个键值入栈，如果没有下个键则不入栈任何元素 */
        while (lua_next(L,-2) != 0)
        {
            /* stack: ... base_table key value */
            /* 将 base对应表 复制到栈顶 */
            lua_pushvalue(L,-2);    /* stack: ... base_table key value key */
            /* 将栈顶的 base对应表 插入到 -2 这个位置 */
            /* 其实就是交换栈顶两个元素的位置，即交换键值对的位置 */
            lua_insert(L,-2);       /* stack: ... base_table key key value */
            
            /* 将键值对加入到tolua_super中 */
            lua_rawset(L,-5);       /* stack: ... base_table key */
        }
    }
    /* 将剩余元素出栈 */
    lua_pop(L,3);                       /* stack: <empty> */
}

/* creates a 'tolua_ubox' table for base clases, and
// expects the metatable and base metatable on the stack */
/**
 *  期望：
 *
 *  @param L 状态机
 */
static void set_ubox(lua_State* L) {

    /* mt basemt */
    if (!lua_isnil(L, -1)) {
        lua_pushstring(L, "tolua_ubox");
        lua_rawget(L,-2);
    } else {
        lua_pushnil(L);
    };
    /* mt basemt base_ubox */
    if (!lua_isnil(L,-1)) {
        lua_pushstring(L, "tolua_ubox");
        lua_insert(L, -2);
        /* mt basemt key ubox */
        lua_rawset(L,-4);
        /* (mt with ubox) basemt */
    } else {
        /* mt basemt nil */
        lua_pop(L, 1);
        lua_pushstring(L,"tolua_ubox");
        lua_newtable(L);
        /* make weak value metatable for ubox table to allow userdata to be
        garbage-collected */
        lua_newtable(L);
        lua_pushliteral(L, "__mode");
        lua_pushliteral(L, "v");
        lua_rawset(L, -3);               /* stack: string ubox mt */
        lua_setmetatable(L, -2);  /* stack:mt basemt string ubox */
        lua_rawset(L,-4);
    };

};

/* Map inheritance
    * It sets 'name' as derived from 'base' by setting 'base' as metatable of 'name'
*/
static void mapinheritance (lua_State* L, const char* name, const char* base)
{
    /* set metatable inheritance */
    luaL_getmetatable(L,name);

    if (base && *base)
        luaL_getmetatable(L,base);
    else {

        if (lua_getmetatable(L, -1)) { /* already has a mt, we don't overwrite it */
            lua_pop(L, 2);
            return;
        };
        luaL_getmetatable(L,"tolua_commonclass");
    };

    set_ubox(L);

    lua_setmetatable(L,-2);
    lua_pop(L,1);
}

/* Object type
*/
static int tolua_bnd_type (lua_State* L)
{
    tolua_typename(L,lua_gettop(L));
    return 1;
}

/* Take ownership
*/
static int tolua_bnd_takeownership (lua_State* L)
{
    int success = 0;
    if (lua_isuserdata(L,1))
    {
        if (lua_getmetatable(L,1))        /* if metatable? */
        {
            lua_pop(L,1);             /* clear metatable off stack */
            /* force garbage collection to avoid C to reuse a to-be-collected address */
#ifdef LUA_VERSION_NUM
            lua_gc(L, LUA_GCCOLLECT, 0);
#else
            lua_setgcthreshold(L,0);
#endif

            success = tolua_register_gc(L,1);
        }
    }
    lua_pushboolean(L,success!=0);
    return 1;
}

/* Release ownership
*/
static int tolua_bnd_releaseownership (lua_State* L)
{
    int done = 0;
    if (lua_isuserdata(L,1))
    {
        void* u = *((void**)lua_touserdata(L,1));
        /* force garbage collection to avoid releasing a to-be-collected address */
#ifdef LUA_VERSION_NUM
        lua_gc(L, LUA_GCCOLLECT, 0);
#else
        lua_setgcthreshold(L,0);
#endif
        lua_pushstring(L,"tolua_gc");
        lua_rawget(L,LUA_REGISTRYINDEX);
        lua_pushlightuserdata(L,u);
        lua_rawget(L,-2);
        lua_getmetatable(L,1);
        if (lua_rawequal(L,-1,-2))  /* check that we are releasing the correct type */
        {
            lua_pushlightuserdata(L,u);
            lua_pushnil(L);
            lua_rawset(L,-5);
            done = 1;
        }
    }
    lua_pushboolean(L,done!=0);
    return 1;
}

/* Type casting
*/
int tolua_bnd_cast (lua_State* L)
{

    /* // old code
            void* v = tolua_tousertype(L,1,NULL);
            const char* s = tolua_tostring(L,2,NULL);
            if (v && s)
             tolua_pushusertype(L,v,s);
            else
             lua_pushnil(L);
            return 1;
    */

    void* v;
    const char* s;
    if (lua_islightuserdata(L, 1)) {
        v = tolua_touserdata(L, 1, NULL);
    } else {
        v = tolua_tousertype(L, 1, 0);
    };

    s = tolua_tostring(L,2,NULL);
    if (v && s)
        tolua_pushusertype(L,v,s);
    else
        lua_pushnil(L);
    return 1;
}

/* Test userdata is null
*/
static int tolua_bnd_isnulluserdata (lua_State* L) {
    void **ud = (void**)lua_touserdata(L, -1);
    tolua_pushboolean(L, ud == NULL || *ud == NULL);
    return 1;
}

/* Inheritance
*/
static int tolua_bnd_inherit (lua_State* L) {

    /* stack: lua object, c object */
    lua_pushstring(L, ".c_instance");
    lua_pushvalue(L, -2);
    lua_rawset(L, -4);
    /* l_obj[".c_instance"] = c_obj */

    return 0;
};

#ifdef LUA_VERSION_NUM /* lua 5.1 */
static int tolua_bnd_setpeer(lua_State* L) {

    /* stack: userdata, table */
    if (!lua_isuserdata(L, -2)) {
        lua_pushstring(L, "Invalid argument #1 to setpeer: userdata expected.");
        lua_error(L);
    };

    if (lua_isnil(L, -1)) {

        lua_pop(L, 1);
        lua_pushvalue(L, TOLUA_NOPEER);
    };
    lua_setfenv(L, -2);

    return 0;
};

static int tolua_bnd_getpeer(lua_State* L) {

    /* stack: userdata */
    lua_getfenv(L, -1);
    if (lua_rawequal(L, -1, TOLUA_NOPEER)) {
        lua_pop(L, 1);
        lua_pushnil(L);
    };
    return 1;
};
#endif

/* static int class_gc_event (lua_State* L); */

/**
 *
 *  主要在 register 中做了以下事情：
 *
 *      1. register.tolua_opened = true
 *
 *      2. register.tolua_value_root = {} -- TOLUA_VALUE_ROOT
 *
 *      3. register.tolua_peers = {__mode = "k"} -- for lua 5.1
 *
 *      4. register.tolua_ubox = {__mode = "v"}
 *
 *      5. register.tolua_super = {}
 *
 *      6. register.tolua_gc = {}
 *
 *      7. register.tolua_gc_event = cfunc_calss_gc_event(tolua_gc, tolua_super) ... end
 *
 *      8. register.tolua_commonclass = {
 *              __index     = class_index_event,
 *              __newindex  = class_newindex_event,
 *              __add       = class_add_event,
 *              __sub       = class_sub_event,
 *              __mul       = class_mul_event,
 *              __div       = class_div_event,
 *              __lt        = class_lt_event,
 *              __le        = class_le_event,
 *              __eq        = class_eq_event,
 *              __call      = class_call_event,
 *              __gc        = register.tolua_gc_event,
 *      }
 *
 *  还有在 global 中做了以下事情：这些在lua中可以访问
 *
 *      1. global.tolua.type = cfunc
 *
 *      2. global.tolua.takeownership = cfunc
 *
 *      3. global.tolua.cast = cfunc
 *
 *      4. global.tolua.isnull = cfunc
 *
 *      5. global.tolua.inherit = cfunc
 *
 *      6. global.tolua.getpeer = cfunc
 *
 *      7. global.tolua.setpeer = cfunc
 *
 *  @param L 状态机
 */
TOLUA_API void tolua_open (lua_State* L)
{
    int top = lua_gettop(L);
    lua_pushstring(L,"tolua_opened");
    lua_rawget(L,LUA_REGISTRYINDEX);
    /* 检查是否已经打开过 */
    if (!lua_isboolean(L,-1))
    {
        /* 将tolua_opened设置为true */
        lua_pushstring(L,"tolua_opened");
        lua_pushboolean(L,1);
        lua_rawset(L,LUA_REGISTRYINDEX);
        
        // create value root table
        /* 创建一个表，全局注册表索引为 TOLUA_VALUE_ROOT */
        lua_pushstring(L, TOLUA_VALUE_ROOT);
        /* 创建一个table，入栈 */
        lua_newtable(L);
        lua_rawset(L, LUA_REGISTRYINDEX);

#ifndef LUA_VERSION_NUM /* only prior to lua 5.1 */
        /* create peer object table */
        /* 创建tolua_peers映射 */
        /* 只对lua5.1有效 */
        lua_pushstring(L, "tolua_peers");
        /* 创建一个 表t */
        lua_newtable(L);
        /* make weak key metatable for peers indexed by userdata object */
        /* 创建 t表 的元表 -- 表mt */
        lua_newtable(L);
            /* 将 表mt 的__mode字段设置成"k" */
            /* 表示 表mt 的键是若引用的 */
            lua_pushliteral(L, "__mode");
            lua_pushliteral(L, "k");
            lua_rawset(L, -3);                /* stack: string peers mt */
        /* 将表t的元表设置成表mt */
        lua_setmetatable(L, -2);   /* stack: string peers */
    
        /* 注册到全局注册表中 */
        lua_rawset(L,LUA_REGISTRYINDEX);
#endif

        /* create object ptr -> udata mapping table */
        /* 创建tolua_ubox映射 */
        lua_pushstring(L,"tolua_ubox");
        lua_newtable(L);
        /* make weak value metatable for ubox table to allow userdata to be
           garbage-collected */
        lua_newtable(L);
            /* 将 表mt 的__mode字段设置成"v" */
            /* 表示 表mt 的值是弱引用的 */
            lua_pushliteral(L, "__mode");
            lua_pushliteral(L, "v");
            lua_rawset(L, -3);               /* stack: string ubox mt */
        lua_setmetatable(L, -2);  /* stack: string ubox */
        lua_rawset(L,LUA_REGISTRYINDEX);
        
//        /* create object ptr -> class type mapping table */
//        lua_pushstring(L, "tolua_ptr2type");
//        lua_newtable(L);
//        lua_rawset(L, LUA_REGISTRYINDEX);

        /* 注册super表 */
        lua_pushstring(L,"tolua_super");
        lua_newtable(L);
        lua_rawset(L,LUA_REGISTRYINDEX);
        
        /* 注册gc表 */
        lua_pushstring(L,"tolua_gc");
        lua_newtable(L);
        lua_rawset(L,LUA_REGISTRYINDEX);

        /* create gc_event closure */
        /* 创建一个闭包 tolua_gc_event */
        lua_pushstring(L, "tolua_gc_event");
            /* 先将闭包的upvalue入栈 */
            lua_pushstring(L, "tolua_gc");
            lua_rawget(L, LUA_REGISTRYINDEX);
            lua_pushstring(L, "tolua_super");
            lua_rawget(L, LUA_REGISTRYINDEX);
        /* 注册闭包到全局注册表 */
        lua_pushcclosure(L, class_gc_event, 2);
        lua_rawset(L, LUA_REGISTRYINDEX);

        /* 新建一个表，并重新注册各种元方法 */
        tolua_newmetatable(L,"tolua_commonclass");

        /* 貌似什么都没做，将全局表入栈，再出栈 */
        tolua_module(L,NULL,0);
        
        /* 其实就是将全局表(global)入栈 */
        tolua_beginmodule(L,NULL); /* stack : global */
        
            /* global.tolua = {} */
            /* 查询名字空间，查到就返回对应表 */
            /* 否则就新建一个表 */
            tolua_module(L,"tolua",0);
        
            /* 将tolua入栈 */
            tolua_beginmodule(L,"tolua"); /* stack : global tolua */
                /* 即tolua.type = tolua_bnd_type 下同*/
                tolua_function(L,"type",tolua_bnd_type);
                tolua_function(L,"takeownership",tolua_bnd_takeownership);
                tolua_function(L,"releaseownership",tolua_bnd_releaseownership);
                tolua_function(L,"cast",tolua_bnd_cast);
                tolua_function(L,"isnull",tolua_bnd_isnulluserdata);
                tolua_function(L,"inherit", tolua_bnd_inherit);
#ifdef LUA_VERSION_NUM /* lua 5.1 */
                tolua_function(L, "setpeer", tolua_bnd_setpeer);
                tolua_function(L, "getpeer", tolua_bnd_getpeer);
#endif
        
            tolua_endmodule(L); /* stack : global */
        tolua_endmodule(L); /* stack : <empty> */
    }
    /* 恢复栈顶设置 */
    lua_settop(L,top);
}

/* Copy a C object
*/
TOLUA_API void* tolua_copy (lua_State* L, void* value, unsigned int size)
{
    void* clone = (void*)malloc(size);
    if (clone)
        memcpy(clone,value,size);
    else
        tolua_error(L,"insuficient memory",NULL);
    return clone;
}

/* Default collect function
*/
TOLUA_API int tolua_default_collect (lua_State* tolua_S)
{
    void* self = tolua_tousertype(tolua_S,1,0);
    free(self);
    return 0;
}

/* Do clone
*/
TOLUA_API int tolua_register_gc (lua_State* L, int lo)
{
    int success = 1;
    void *value = *(void **)lua_touserdata(L,lo);
    lua_pushstring(L,"tolua_gc");
    lua_rawget(L,LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L,value);
    lua_rawget(L,-2);
    if (!lua_isnil(L,-1)) /* make sure that object is not already owned */
        success = 0;
    else
    {
        lua_pushlightuserdata(L,value);
        lua_getmetatable(L,lo);
        lua_rawset(L,-4);
    }
    lua_pop(L,2);
    return success;
}

/* Register a usertype
    * It creates the correspoding metatable in the registry, for both 'type' and 'const type'.
    * It maps 'const type' as being also a 'type'
*/
TOLUA_API void tolua_usertype (lua_State* L, const char* type)
{
    char ctype[128] = "const ";
    strncat(ctype,type,120);

    /* create both metatables */
    if (tolua_newmetatable(L,ctype) && tolua_newmetatable(L,type))
        mapsuper(L,type,ctype);             /* 'type' is also a 'const type' */
}


/* Begin module
    * It pushes the module (or class) table on the stack
*/
/**
 *  将模块表入栈，或者查询模块表中的内容
 *
 *  @param L    状态机
 *
 *  @param name 名字空间，模块表
 *  @param name 若为空 : 返回全局表(global)
 *  @param name 不为空 : 将global.name入栈
 */
TOLUA_API void tolua_beginmodule (lua_State* L, const char* name)
{
    if (name)
    {
        /* stack : global_t */
    
        lua_pushstring(L,name);
        lua_rawget(L,-2); /* stack : global_t global_t.name */
    }
    else
        lua_pushvalue(L,LUA_GLOBALSINDEX);
}

/* End module
    * It pops the module (or class) from the stack
*/
/**
 *  就是将模块表出栈
 *
 *  即lua_pop(L, 1)
 *
 *  @param L 状态机
 */
TOLUA_API void tolua_endmodule (lua_State* L)
{
    lua_pop(L,1);
}

/* Map module
    * It creates a new module
*/
/**
 *
 *  在global中查询，或者新建模块表
 *
 *  @param name 名字空间，模块表
 *  @param name 若为空则直接将全局表(global)入栈
 *  @param name PS : 不为空的前提是栈顶是全局表
 *
 *  @param hasvar   是否还有
 */
#if 1
TOLUA_API void tolua_module (lua_State* L, const char* name, int hasvar)
{
    
    if (name)
    {
        /* stack : global_t */
        /* tolua module */
        /* 在全局表中查询 global[name] 元素*/
        lua_pushstring(L,name);
        lua_rawget(L,-2); /* stack : global_t global_t.name */
        
        /* 检查是否存在 */
        if (!lua_istable(L,-1))  /* check if module already exists */
        {
            /* 将栈顶nil出栈 */
            lua_pop(L,1);           /* stack : global_t */
            
            /* 新建一个表 table */
            lua_newtable(L);        /* stack : global_t table */
            lua_pushstring(L,name); /* stack : global_t table name */
            lua_pushvalue(L,-2);    /* stack : global_t table name table */
            /* global_t[name] = table */
            lua_rawset(L,-4);       /* assing module into module */
            /* stack : global_t table */
        }
    }
    else
    {
        /* global table */
        /* 将全局表入栈 */
        lua_pushvalue(L,LUA_GLOBALSINDEX);
    }
    if (hasvar)
    {
        if (!tolua_ismodulemetatable(L))  /* check if it already has a module metatable */
        {
            /* create metatable to get/set C/C++ variable */
            lua_newtable(L);
            /* 设置栈顶表的__index和__newindex字段 */
            tolua_moduleevents(L);
            if (lua_getmetatable(L,-2))
                lua_setmetatable(L,-2);  /* set old metatable as metatable of metatable */
            lua_setmetatable(L,-2);
        }
    }
    lua_pop(L,1);               /* pop module */
}
#else
TOLUA_API void tolua_module (lua_State* L, const char* name, int hasvar)
{
    if (name)
    {
        /* tolua module */
        lua_pushstring(L,name);
        lua_newtable(L);
    }
    else
    {
        /* global table */
        lua_pushvalue(L,LUA_GLOBALSINDEX);
    }
    if (hasvar)
    {
        /* create metatable to get/set C/C++ variable */
        lua_newtable(L);
        tolua_moduleevents(L);
        if (lua_getmetatable(L,-2))
            lua_setmetatable(L,-2);  /* set old metatable as metatable of metatable */
        lua_setmetatable(L,-2);
    }
    if (name)
        lua_rawset(L,-3);       /* assing module into module */
    else
        lua_pop(L,1);           /* pop global table */
}
#endif

static void push_collector(lua_State* L, const char* type, lua_CFunction col) {

    /* push collector function, but only if it's not NULL, or if there's no
       collector already */
    if (!col) return;
    luaL_getmetatable(L,type);
    lua_pushstring(L,".collector");
    /*
    if (!col) {
        lua_pushvalue(L, -1);
        lua_rawget(L, -3);
        if (!lua_isnil(L, -1)) {
            lua_pop(L, 3);
            return;
        };
        lua_pop(L, 1);
    };
    //    */
    lua_pushcfunction(L,col);

    lua_rawset(L,-3);
    lua_pop(L, 1);
};

/* Map C class
    * It maps a C class, setting the appropriate inheritance and super classes.
*/
TOLUA_API void tolua_cclass (lua_State* L, const char* lname, const char* name, const char* base, lua_CFunction col)
{
    char cname[128] = "const ";
    char cbase[128] = "const ";
    strncat(cname,name,120);
    strncat(cbase,base,120);

    mapinheritance(L,name,base);
    mapinheritance(L,cname,name);

    mapsuper(L,cname,cbase);
    mapsuper(L,name,base);

    lua_pushstring(L,lname);

    push_collector(L, name, col);
    /*
    luaL_getmetatable(L,name);
    lua_pushstring(L,".collector");
    lua_pushcfunction(L,col);

    lua_rawset(L,-3);
    */

    luaL_getmetatable(L,name);
    lua_rawset(L,-3);              /* assign class metatable to module */

    /* now we also need to store the collector table for the const
       instances of the class */
    push_collector(L, cname, col);
    /*
    luaL_getmetatable(L,cname);
    lua_pushstring(L,".collector");
    lua_pushcfunction(L,col);
    lua_rawset(L,-3);
    lua_pop(L,1);
    */


}

/* Add base
    * It adds additional base classes to a class (for multiple inheritance)
    * (not for now)
    */
TOLUA_API void tolua_addbase(lua_State* L, char* name, char* base) {

    char cname[128] = "const ";
    char cbase[128] = "const ";
    strncat(cname,name,120);
    strncat(cbase,base,120);

    mapsuper(L,cname,cbase);
    mapsuper(L,name,base);
};


/* Map function
    * It assigns a function into the current module (or class)
*/
/**
 *  前提：栈顶有当前模块表，本函数在tolua_modulebegin之后调用
 * 
 *  即 module.name = func
 *
 *  @param L    状态机
 *
 *  @param name 模块名字
 *
 *  @param func 注册C函数
 */
TOLUA_API void tolua_function (lua_State* L, const char* name, lua_CFunction func)
{
    lua_pushstring(L,name);
    lua_pushcfunction(L,func);
    lua_rawset(L,-3);
}

/* sets the __call event for the class (expects the class' main table on top) */
/*    never really worked :(
TOLUA_API void tolua_set_call_event(lua_State* L, lua_CFunction func, char* type) {

    lua_getmetatable(L, -1);
    //luaL_getmetatable(L, type);
    lua_pushstring(L,"__call");
    lua_pushcfunction(L,func);
    lua_rawset(L,-3);
    lua_pop(L, 1);
};
*/

/* Map constant number
    * It assigns a constant number into the current module (or class)
*/
/**
 *  期望：栈顶是模块表
 *
 *  模块中的全局常量
 *
 *  module.const = value
 *
 *  @param L     状态机
 *  @param name  常量名
 *  @param value 常量值
 */
TOLUA_API void tolua_constant (lua_State* L, const char* name, lua_Number value)
{
    lua_pushstring(L,name);
    tolua_pushnumber(L,value);
    lua_rawset(L,-3);
}


/* Map variable
    * It assigns a variable into the current module (or class)
*/
/**
 *  期望：栈顶有模块表
 *
 *  设置变量的get函数和set函数
 *
 *  @param L    状态机
 *  @param name 变量名
 *  @param get  get方法
 *  @param set  set方法
 */
TOLUA_API void tolua_variable (lua_State* L, const char* name, lua_CFunction get, lua_CFunction set)
{
    /* get func */
    
    /************/
    /* get 函数  */
    /************/
    
    /* 获得模块表 module_t[".get"] */
    lua_pushstring(L,".get");
    lua_rawget(L,-2); /* stack : module_t module_t[".get"] */
    
    /* 若没有新建一个表 get_t 入栈 */
    if (!lua_istable(L,-1))
    {
        /* create .get table, leaving it at the top */
        lua_pop(L,1);
        lua_newtable(L); /* stack : module_t get_t */
        lua_pushstring(L,".get");
        lua_pushvalue(L,-2);
        /* module_t[".get"] = get_t */
        lua_rawset(L,-4);
    }
    
    /* 存入变量 */
    lua_pushstring(L,name);
    lua_pushcfunction(L,get);
    lua_rawset(L,-3);                  /* store variable */
    
    lua_pop(L,1);                      /* pop .get table */
    
    /* set func */
    
    /************/
    /* set 函数  */
    /************/
    
    /* 和get相似 */
    if (set)
    {
        lua_pushstring(L,".set");
        lua_rawget(L,-2);
        if (!lua_istable(L,-1))
        {
            /* create .set table, leaving it at the top */
            lua_pop(L,1);
            lua_newtable(L);
            lua_pushstring(L,".set");
            lua_pushvalue(L,-2);
            lua_rawset(L,-4);
        }
        lua_pushstring(L,name);
        lua_pushcfunction(L,set);
        lua_rawset(L,-3);                  /* store variable */
        lua_pop(L,1);                      /* pop .set table */
    }
}

/* Access const array
    * It reports an error when trying to write into a const array
*/
/**
 *  当试图改变一个数组中的常量的时候，引发一个错误
 *
 *  @param L 状态机
 *
 *  @return 返回0
 */
static int const_array (lua_State* L)
{
    luaL_error(L,"value of const array cannot be changed");
    return 0;
}

/* Map an array
    * It assigns an array into the current module (or class)
*/
/**
 *  期望：栈顶是一个模块表
 *
 *  设置数组的set和get
 *
 *  数组实际上是用到了table的数组部分
 *
 *  @param L    状态机
 *  @param name 变量名
 *  @param get  get方法
 *  @param set  set方法
 */
TOLUA_API void tolua_array (lua_State* L, const char* name, lua_CFunction get, lua_CFunction set)
{
    /* 获得 get_t */
    lua_pushstring(L,".get");
    lua_rawget(L,-2);
    if (!lua_istable(L,-1))
    {
        /* create .get table, leaving it at the top */
        lua_pop(L,1);
        lua_newtable(L);
        lua_pushstring(L,".get");
        lua_pushvalue(L,-2);
        lua_rawset(L,-4);
    }
    lua_pushstring(L,name);

    /* 新建一个表 table */
    lua_newtable(L);           /* create array metatable */
        /* 将自己设置为自己的元表 */
        lua_pushvalue(L,-1);
        lua_setmetatable(L,-2);    /* set the own table as metatable (for modules) */
    /* 设置表的__index属性为get */
    lua_pushstring(L,"__index");
    lua_pushcfunction(L,get);
    lua_rawset(L,-3);
    
    /* 设置表的__newindex属性为set */
    lua_pushstring(L,"__newindex");
    /* 若set为空，表示这个数组是一个常量数组 */
    lua_pushcfunction(L,set?set:const_array);
    lua_rawset(L,-3);

    /* 即 get_t[name] = table */
    lua_rawset(L,-3);                  /* store variable */
    lua_pop(L,1);                      /* pop .get table */
}


TOLUA_API void tolua_dobuffer(lua_State* L, char* B, unsigned int size, const char* name) {

#ifdef LUA_VERSION_NUM /* lua 5.1 */
    if (!luaL_loadbuffer(L, B, size, name)) lua_pcall(L, 0, 0, 0);
#else
    lua_dobuffer(L, B, size, name);
#endif
};

