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
 *  在reg中创建一个新的表reg.name，在 表reg.name 中设置各种元方法
 *
 *  @param L    状态机
 *  @param name 注册名称
 *
 *  @return 1 : 成功
 *  @return 0 : 失败
 */
static int tolua_newmetatable (lua_State* L, const char* name)
{
    /* 创建一个 表t */
    /* 注册 表t */
    /* reg.name = {} */
    int r = luaL_newmetatable(L,name);

#ifdef LUA_VERSION_NUM          /* lua 5.1 */
    if (r) {
        lua_pushvalue(L, -1);
        lua_pushstring(L, name);
        
        /* lua5.1需要重新查询一次，将元表放到栈顶 */
        /* 是将返回的 表r 作为键，name作为值 */
        /* reg.r = name */
        lua_settable(L, LUA_REGISTRYINDEX);
    };
#endif

    if (r)
        tolua_classevents(L);   /* 重新绑定注册各个元方法 */
    
    /* 将这个表出栈 */
    lua_pop(L,1);
    return r;
}

/**
 *  Map super classes
 *
 *  It sets 'name' as being also a 'base', mapping all super classes of 'base' in 'name'
 * 
 *  将基类父类表所有内容加入到子类父类表中
 *
 *  1. 获得子类超类，若没有在tolua_super新建一个
 *  2. 获得基类超类
 *  3. 遍历基类超类，并加入子类超类中
 *
 *  @param L    状态机
 *  @param name 子类
 *  @param base 基类
 */
static void mapsuper (lua_State* L, const char* name, const char* base)
{
    /* 获得 表tolua_super */
    lua_pushstring(L,"tolua_super");
    lua_rawget(L,LUA_REGISTRYINDEX);    /* stack: super */
    
    /* 获得 表reg.name 入栈 */
    luaL_getmetatable(L,name);          /* stack: super mt:=reg.name */
    
    /* 获得 表reg.name 的 超类table */
    lua_rawget(L,-2);                   /* stack: super table:=super[mt] */
    
    if (lua_isnil(L,-1))                /* 若没有超类，则在tolua_super中新建一个 */
    {
        /* 先将栈顶的nil出栈 */
        lua_pop(L,1);
        
        /* 新建 超类table 并入栈 */
        lua_newtable(L);                /* stack: super table */
        
        /* 获得 表reg.name 并入栈 */
        luaL_getmetatable(L,name);      /* stack: super table mt:=reg.name */
        
        /* 复制新建表到栈顶 */
        lua_pushvalue(L,-2);            /* stack: super table mt table */
        /* 在tolua_super表中添加 表reg.name 和 超类table */
        /* tolua_super[mt] = table */
        lua_rawset(L,-4);               /* stack: super table */
    }

    /* set base as super class */
    
    /* 将 <base, true> 加入 超类table */
    lua_pushstring(L,base);
    lua_pushboolean(L,1);
    /* table.base = true */
    lua_rawset(L,-3);                   /* stack: super table */

    /* set all super class of base as super class of name */
    /* 获得 表reg.base */
    luaL_getmetatable(L,base);          /* stack: super table bmt:=reg.base */
    /* 查询 表reg.base 的父类 超类st */
    /* tolua_super[bmt] */
    lua_rawget(L,-3);                   /* stack: super table st:=super[bmt] */
    
    if (lua_istable(L,-1))              /* 若 超类st 是一个表 */
    {
        /* 遍历整个 超类st */
        /* 将所有的键值对加入到 超类table 中 */
        
        lua_pushnil(L);                 /* 先将nil入栈作为第一个查询的键 */
        
        /* lua_next会在栈上弹出一个键，将下一个键值对入栈，如果没有下个键则不入栈任何元素 */
        while (lua_next(L,-2) != 0)     /* stack: super table st k v */
        {
            /* 将键入栈 */
            lua_pushvalue(L,-2);        /* stack: super table st k v k */
            
            /* 其实就是交换栈顶两个元素的位置，即交换键值对的位置 */
            lua_insert(L,-2);           /* stack: super table st k k v*/
            
            /* 将键值对加入到 超类table 中 */
            lua_rawset(L,-5);           /* stack: super table st k */
        }
    }
    
    /* 将剩余元素出栈 */
    lua_pop(L,3);                       /* stack: <empty> */
}

/**
 *  creates a 'tolua_ubox' table for base clases, and
 *
 *  expects the metatable and base metatable on the stack
 *
 *  期望：栈顶子类元表mt 和 基类元表bmt
 *
 *  将bmt的tolua_ubox加入到mt中，若bmt.tolua_ubox为空，则为mt新建一个，bmt不需要加入
 *
 *  mt.tolua_ubox = bmt.tolua_ubox or {__mode = "v"}
 *
 *  @param L 状态机
 */
static void set_ubox(lua_State* L) {

    if (!lua_isnil(L, -1)) {        /* 若栈顶不为nil */
        lua_pushstring(L, "tolua_ubox");
        /* 获得 表bmt.tolua_ubox */
        lua_rawget(L,-2);           /* stack: mt bmt bubox:=bmt.tolua_ubox */
    } else {
        lua_pushnil(L);
    };                              /* stack: mt bmt bubox */
    
    if (!lua_isnil(L,-1)) {         /* 若基类元表有tolua_ubox */
        /* mt.tolua_ubox = bubox */
        lua_pushstring(L, "tolua_ubox");
        lua_insert(L, -2);          /* stack: mt bmt str:="tolua_ubox" bubox */
        lua_rawset(L,-4);
    } else {                        /* 若基类元表没有tolua_ubox，则创建一个 */
        /* 先将栈顶nil出栈 */
        lua_pop(L, 1);              /* stack: mt bmt */
        
        lua_pushstring(L,"tolua_ubox");
        /* 新建 表bubox */
        lua_newtable(L);            /* stack: mt bmt str:="tolua_ubox" bubox */
        
        /* 新建 值弱引用表 {__mode = "v"} */
        lua_newtable(L);
        lua_pushliteral(L, "__mode");
        lua_pushliteral(L, "v");
        lua_rawset(L, -3);          /* stack: mt bmt str bubox bbmt:={__mode="v"} */
        
        /* 将 表bubox 的元表设置成 表bbmt */
        lua_setmetatable(L, -2);    /* stack: mt bmt str bubox */
        
        /* 将 mt.str = bubox */
        lua_rawset(L,-4);
    };                              /* stack: mt bmt */

};

/**
 *  Map inheritance
 *
 *  It sets 'name' as derived from 'base' by setting 'base' as metatable of 'name'
 *
 *  1. 子类元表 从 基类元表获得tolua_ubox
 *  2. 设置 子类 的元表为 基类
 *
 *  @param L    状态机
 *  @param name 子类名字
 *  @param base 基类名字
 */
static void mapinheritance (lua_State* L, const char* name, const char* base)
{
    /* set metatable inheritance */
    
    /* 获得 表reg.name */
    luaL_getmetatable(L,name);          /* stack : mt:=reg.name */
    
    if (base && *base)                  /* 当需要从 基类base 继承 */
        /* 获得 表reg.base */
        luaL_getmetatable(L,base);      /* stack : mt bmt:=reg.base */
    else                                /* 从公共父类继承 */
    {
        if (lua_getmetatable(L, -1)) {  /* 子类已经有元表，则不需要再设置 */
            lua_pop(L, 2);              /* stack : <empty> */
            return;
        };
        /* 获得 公共父类tolua_commonclass */
        luaL_getmetatable(L,"tolua_commonclass");
    };                                  /* stack : mt bmt */
    
    /* mt.ubox = bmt.ubox or {__mode="v"} */
    set_ubox(L);

    /* 将 表mt 的元表设置成 表bmt */
    lua_setmetatable(L,-2);
    
    lua_pop(L,1);                       /* stack : <empty> */
}

/**
 *  Object type
 *
 *  获得栈顶数据类型
 *
 *  tolua.type(type)
 *
 *  @param L 状态机
 *
 *  @return 1 : 表示成功调用
 */
static int tolua_bnd_type (lua_State* L)
{
    tolua_typename(L,lua_gettop(L));
    return 1;
}

/**
 *  Take ownership
 *
 *  tolua.takeowership(userdata)
 *
 *  lua垃圾回收机制，增加对象的引用？
 *
 *  @param L 状态机
 *
 *  @return 1 : 成功，不论如何都是调用成功
 *  @return 0 : 失败
 */
static int tolua_bnd_takeownership (lua_State* L)
{
    int success = 0;
    /* 检查第一个参数是否是用户数据 */
    if (lua_isuserdata(L,1))
    {
        /* 检查是有元表的 */
        if (lua_getmetatable(L,1))        /* if metatable? */
        {
            /* 将元表出栈 */
            lua_pop(L,1);             /* clear metatable off stack */
            /* force garbage collection to avoid C to reuse a to-be-collected address */
#ifdef LUA_VERSION_NUM
            /* 强行进行一次垃圾回收 */
            lua_gc(L, LUA_GCCOLLECT, 0);
#else
            lua_setgcthreshold(L,0);
#endif

            success = tolua_register_gc(L,1);
        }
    }
    /* 将结果入栈 */
    lua_pushboolean(L,success!=0);
    /* 不论如何都是调用成功 */
    return 1;
}

/**
 *  Release ownership
 *
 *  tolua.releaseownership(userdata)
 *
 *  lua垃圾回收机制，释放cpp对象？
 *
 *  @param L 状态机
 *
 *  @return 1 : 成功调用
 */
static int tolua_bnd_releaseownership (lua_State* L)
{
    int done = 0;
    if (lua_isuserdata(L,1))
    {
        /* 获得用户数据地址 */
        void* u = *((void**)lua_touserdata(L,1));
        /* force garbage collection to avoid releasing a to-be-collected address */
#ifdef LUA_VERSION_NUM
        /* 强制垃圾回收 */
        lua_gc(L, LUA_GCCOLLECT, 0);
#else
        lua_setgcthreshold(L,0);
#endif
        /* 入栈reg.tolua_gc表 */
        lua_pushstring(L,"tolua_gc");
        lua_rawget(L,LUA_REGISTRYINDEX);
        
        /* 入栈用户数据地址 */
        lua_pushlightuserdata(L,u);
        /* 在reg.tolua_gc表中查询 */
        /* 在reg.tolua_gc[用户数据地址] */
        lua_rawget(L,-2);
        /* 获取用户数据的元表 */
        lua_getmetatable(L,1);
        /* 比较用户数据元表和reg.tolua_gc中查询到的值是否一致 */
        /* 保证释放对了对象 */
        if (lua_rawequal(L,-1,-2))  /* check that we are releasing the correct type */
        {
            /* 在reg.tolua_gc[用户数据地址] = nil */
            lua_pushlightuserdata(L,u);
            lua_pushnil(L);
            lua_rawset(L,-5);
            
            done = 1;
        }
    }
    /* 返回结果 */
    lua_pushboolean(L,done!=0);
    return 1;
}

/**
 *  Type casting
 *
 *  tolua.cast(userdata)
 *  tolua.cast(table)
 *
 *  对象强制转换
 *
 *  @param L 状态机
 *
 *  @return 1 : 表示成功调用
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
    /* 若是用户数据地址 */
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

/**
 *  Test userdata is null
 *
 *  tolua.isnull(userdata)
 *
 *  测试用户数据是否是nil，将结果入栈
 *
 *  @param L 状态机
 *
 *  @return 1 : 表示成功调用
 */
static int tolua_bnd_isnulluserdata (lua_State* L) {
    void **ud = (void**)lua_touserdata(L, -1);
    tolua_pushboolean(L, ud == NULL || *ud == NULL);
    return 1;
}


/**
 *  Inheritance
 *
 *  tolua.inherit(luaobj, cobj)
 *
 *  设置.c_instace的值，在lua对象中加入c对象
 *
 *  @param L 状态机
 *
 *  @return 0
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
/**
 *
 *  tolua.setpeer(userdata, [table])
 * 
 *  当talbe缺省时，将TOLUA_NOPEER入栈（registry)
 *
 *  设置用户数据的环境表
 *
 *  @param L 状态机
 *
 *  @return 0
 */
static int tolua_bnd_setpeer(lua_State* L) {

    /* stack: userdata, table */
    /* 检查栈中对象是否合法 */
    if (!lua_isuserdata(L, -2)) {
        lua_pushstring(L, "Invalid argument #1 to setpeer: userdata expected.");
        lua_error(L);
    };

    if (lua_isnil(L, -1)) { /* 若栈顶为空 */
        lua_pop(L, 1);
        /* 将环境表入栈 */
        lua_pushvalue(L, TOLUA_NOPEER);
    };
    /* 将用户数据的环境表设置为TOLUA_NOPEER */
    lua_setfenv(L, -2);

    return 0;
};

/**
 *  tolua.getpeer(userdata)
 *
 *  @param L 状态机
 *
 *  @return 1
 */
static int tolua_bnd_getpeer(lua_State* L) {

    /* stack: userdata */
    /* 获取用户数据的环境表 */
    lua_getfenv(L, -1);
    
    if (lua_rawequal(L, -1, TOLUA_NOPEER)) { /* 如果环境表不是TOLUA_NOPEER */
        lua_pop(L, 1);
        /* 返回 nil */
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
 *      1. reg.tolua_opened = true
 *      2. reg.tolua_value_root = {} -- TOLUA_VALUE_ROOT
 *      3. reg.tolua_peers = {__mode = "k"} -- for lua 5.1
 *      4. reg.tolua_ubox = {__mode = "v"}
 *      5. reg.tolua_super = {}
 *      6. reg.tolua_gc = {}
 *      7. reg.tolua_gc_event = cfunc_calss_gc_event(tolua_gc, tolua_super) ... end
 *      8. reg.tolua_commonclass = {
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
 *      2. global.tolua.takeownership = cfunc
 *      3. global.tolua.cast = cfunc
 *      4. global.tolua.isnull = cfunc
 *      5. global.tolua.inherit = cfunc
 *      6. global.tolua.getpeer = cfunc
 *      7. global.tolua.setpeer = cfunc
 *
 *  @param L 状态机
 */
TOLUA_API void tolua_open (lua_State* L)
{
    int top = lua_gettop(L);
    lua_pushstring(L,"tolua_opened");
    lua_rawget(L,LUA_REGISTRYINDEX);
    
    if (!lua_isboolean(L,-1)) /* 检查是否已经打开过 */ {
        /* 将tolua_opened设置为true */
        /* 标记已经配置好tolua环境 */
        lua_pushstring(L,"tolua_opened");
        lua_pushboolean(L,1);
        lua_rawset(L,LUA_REGISTRYINDEX);
        
        /* TOLUA_VALUE_ROOT 为 tolua_value_root */
        /* reg.tolua_value_root = {} */
        lua_pushstring(L, TOLUA_VALUE_ROOT);
        /* 创建一个 表 并入栈 */
        lua_newtable(L);
        /* 注册 表tolua_value_root */
        lua_rawset(L, LUA_REGISTRYINDEX);

        /* 创建 表tolua_peers 替代lua5.1中的环境表 */
#ifndef LUA_VERSION_NUM     /* 只对lua5.2有效 */
        lua_pushstring(L, "tolua_peers");
        /* 创建一个 表t */
        lua_newtable(L);
        /* 创建 表t 的 元表mt */
        /* 表t 的键都是 userdata对象 */
        /* 所以需要 表t 是一个键弱引用表 */
        lua_newtable(L);
        
        /* 将 表mt 的__mode字段设置成"k" */
        /* 表示 表mt 的键是若引用的 */
        lua_pushliteral(L, "__mode");
        lua_pushliteral(L, "k");
        lua_rawset(L, -3);                  /* stack: string peers mt */
        
        /* 将 表t 的元表设置成 表mt */
        lua_setmetatable(L, -2);            /* stack: string peers */
    
        /* 注册 表tolua_peer */
        lua_rawset(L,LUA_REGISTRYINDEX);
#endif
        /* create object ptr -> udata mapping table */
        /* 创建 表tolua_ubox */
        /* 表tolua_ubox 是 对象地址 到 对象数据 的映射 */
        lua_pushstring(L,"tolua_ubox");
        lua_newtable(L);
        /* 值弱引用表能够让用户数据被垃圾回收 */
        lua_newtable(L);
        
        /* 将 表mt 的__mode字段设置成"v" */
        /* 表示 表mt 的值是弱引用的 */
        lua_pushliteral(L, "__mode");
        lua_pushliteral(L, "v");
        lua_rawset(L, -3);                  /* stack: string ubox mt */
        
        lua_setmetatable(L, -2);            /* stack: string ubox */
        /* 注册 表tolua_ubox */
        lua_rawset(L,LUA_REGISTRYINDEX);
        
//        /* create object ptr -> class type mapping table */
//        lua_pushstring(L, "tolua_ptr2type");
//        lua_newtable(L);
//        lua_rawset(L, LUA_REGISTRYINDEX);

        /* 注册 表tolua_super */
        lua_pushstring(L,"tolua_super");
        lua_newtable(L);
        lua_rawset(L,LUA_REGISTRYINDEX);
        
        /* 注册 表tolua_gc */
        lua_pushstring(L,"tolua_gc");
        lua_newtable(L);
        lua_rawset(L,LUA_REGISTRYINDEX);

        /* create gc_event closure */
        /* 创建一个 闭包tolua_gc_event */
        lua_pushstring(L, "tolua_gc_event");
            /* 先将闭包的upvalue入栈 */
            lua_pushstring(L, "tolua_gc");
            lua_rawget(L, LUA_REGISTRYINDEX);
            lua_pushstring(L, "tolua_super");
            lua_rawget(L, LUA_REGISTRYINDEX);
        lua_pushcclosure(L, class_gc_event, 2);
        /* 注册 闭包gc_event */
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

/**
 *  Copy a C object
 *
 *  拷贝一个c对象
 *
 *  @param L     状态机
 *  @param value 用户数据
 *  @param size  大小
 * 
 *  @return 拷贝对象
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

/**
 *  Default collect function
 *
 *  默认垃圾回收函数
 *
 *  @param tolua_S 状态机
 *
 *  @return 0
 */
TOLUA_API int tolua_default_collect (lua_State* tolua_S)
{
    /* 获取到用户数据 */
    void* self = tolua_tousertype(tolua_S,1,0);
    /* 将其释放 */
    free(self);
    return 0;
}


/**
 *  Do clone
 *
 *  垃圾回收注册
 *
 *  @param L  状态机
 *  @param lo 栈中位置
 *
 *  @return 1 : 成功
 *  @return 0 : 失败
 */
TOLUA_API int tolua_register_gc (lua_State* L, int lo)
{
    int success = 1;
    /* 获得用户数据地址 */
    void *value = *(void **)lua_touserdata(L,lo);
    
    /* 将 表tolua_gc 入栈 */
    lua_pushstring(L,"tolua_gc");
    lua_rawget(L,LUA_REGISTRYINDEX);
    
    /* 将用户数据地址入栈 */
    lua_pushlightuserdata(L,value);
    
    /* 在 表tolua_gc 中查找用户数据对应的值 */
    lua_rawget(L,-2);
    
    /* 如果不为空，则失败，表示这个对象已经被引用 */
    if (!lua_isnil(L,-1)) /* make sure that object is not already owned */
        success = 0;
    else /* 如果为空的话 */
    {
        /* 将对象加入tolua_gc中 */
        lua_pushlightuserdata(L,value);
        lua_getmetatable(L,lo);
        lua_rawset(L,-4);
    }
    lua_pop(L,2);
    return success;
}

/**
 *  Register a usertype
 *
 *  It creates the correspoding metatable in the registry, for both 'type' and 'const type'.
 *
 *  It maps 'const type' as being also a 'type'
 *
 *  在reg中加入两个类型type和const type
 *
 *  并且将type从const type继承？
 *
 *  @param L    状态机
 *  @param type 类型
 */
TOLUA_API void tolua_usertype (lua_State* L, const char* type)
{
    char ctype[128] = "const ";
    strncat(ctype,type,120);

    /* 创建reg["const xxxx"] 和 reg["xxxx"] */
    if (tolua_newmetatable(L,ctype) && tolua_newmetatable(L,type))
        mapsuper(L,type,ctype);             /* 'type' is also a 'const type' */
}


/**
 *  Begin module
 *
 *  It pushes the module (or class) table on the stack
 *
 *  将模块表入栈，或者查询模块表中的内容
 *
 *  @param L    状态机
 *
 *  @param name 名字空间，模块表
 *  @param name 若为空 : 返回全局表G
 *  @param name 不为空 : 将G.name入栈
 */
TOLUA_API void tolua_beginmodule (lua_State* L, const char* name)
{
    if (name)
    {
        lua_pushstring(L,name);
        lua_rawget(L,-2);       /* stack : G G.name */
    }
    else
        lua_pushvalue(L,LUA_GLOBALSINDEX);
}

/**
 *  End module
 *
 *  It pops the module (or class) from the stack
 *
 *  就是将模块表出栈
 *  即lua_pop(L, 1)
 *
 *  需要和tolua_beginmodule配合使用
 *
 *  @param L 状态机
 */
TOLUA_API void tolua_endmodule (lua_State* L)
{
    lua_pop(L,1);
}

/**
 *  Map module
 *
 *  It creates a new module
 *
 *  在G中查询，或者新建模块表
 *
 *  @param name 名字空间，模块表
 *  @param name 若为空则直接将全局表G入栈
 *  @param name PS : 不为空的前提是栈顶是全局表
 *
 *  @param hasvar   是否还有变量
 */
#if 1
TOLUA_API void tolua_module (lua_State* L, const char* name, int hasvar)
{
    if (name)                               /* 若name不为空，则表示栈顶为G */
    {
        /* 在全局表中查询 G[name] 元素*/
        lua_pushstring(L,name);
        lua_rawget(L,-2);                   /* stack : G table:=G.name */
        
        /* 检查是否存在 */
        if (!lua_istable(L,-1))             /* check if module already exists */
        {
            /* 将栈顶nil出栈 */
            lua_pop(L,1);                   /* stack : G */
            
            /* 新建一个表 table */
            lua_newtable(L);                /* stack : G table */
            lua_pushstring(L,name);         /* stack : G table name */
            lua_pushvalue(L,-2);            /* stack : G table name table */
            /* G[name] = table */
            lua_rawset(L,-4);               /* stack : G table */
        }
    }
    else                                    /* 若name为空，入栈G */
    {
        lua_pushvalue(L,LUA_GLOBALSINDEX);
    }
    
    if (hasvar)                             /* 若含有共有变量 */
    {
        if (!tolua_ismodulemetatable(L))    /* 若还没有设置过模块元表 */
        {
            /* 创建存储cpp成员变量的 表vt */
            lua_newtable(L);                /* stack : G table vt */
            
            /* 设置 表vt 的index和newindex字段 */
            tolua_moduleevents(L);
            
            /* 获得 表table 的 元表mt */
            if (lua_getmetatable(L,-2))     /* stack : G table vt mt:=(table's meta) */
                /* setmetatable(vt, mt) */
                lua_setmetatable(L,-2);     /* stack : G table vt */
            
            /* 更新 表table 的元表为 表vt */
            lua_setmetatable(L,-2);
        }
        /* 对于模块，必须要有一个模块元表 */
        /* 对于模快老元表(上一个的元表)，设置成模块元表的元表 */
        
        /* 所以当访问一个模块的时候 */
        /* 先在 模块元表 中查询方法，然后才是在 老元表 中查询 */
    }
    /* 将模块表出栈 */
    lua_pop(L,1);
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

/**
 *  Map C class
 *
 *  It maps a C class, setting the appropriate inheritance and super classes.
 *
 *
 *  期望：栈顶有模块表
 *
 *  @param L     状态机
 *  @param lname 模块中的名字
 *  @param name  子类名字
 *  @param base  基类名字
 *  @param col   垃圾回收函数
 */
TOLUA_API void tolua_cclass (lua_State* L, const char* lname, const char* name, const char* base, lua_CFunction col)
{
    char cname[128] = "const ";
    char cbase[128] = "const ";
    strncat(cname,name,120);
    strncat(cbase,base,120);

    /* cname.ubox = name.ubox = base.ubox */
    mapinheritance(L,name,base);
    mapinheritance(L,cname,name);

    /* super.cname <- super.cbase */
    mapsuper(L,cname,cbase);
    /* super.name <- super.base */
    mapsuper(L,name,base);

    lua_pushstring(L,lname);        /* stack: module lname */

    push_collector(L, name, col);
    
    /*
    luaL_getmetatable(L,name);
    lua_pushstring(L,".collector");
    lua_pushcfunction(L,col);
    lua_rawset(L,-3);
    */

    luaL_getmetatable(L,name);      /* stack : module lname mt:=reg.name */
    /* module.lname = mt */
    lua_rawset(L,-3);               /* stack : module */

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

/**
 *  Add base
 *
 *  It adds additional base classes to a class (for multiple inheritance)
 *  (not for now)
 *
 *
 *  @param L    L description
 *  @param name name description
 *  @param base base description
 */
TOLUA_API void tolua_addbase(lua_State* L, char* name, char* base) {

    char cname[128] = "const ";
    char cbase[128] = "const ";
    strncat(cname,name,120);
    strncat(cbase,base,120);

    mapsuper(L,cname,cbase);
    mapsuper(L,name,base);
};


/**
 *  Map function
 *
 *  It assigns a function into the current module (or class)
 *
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

/**
 *  Map constant number
 *
 *  It assigns a constant number into the current module (or class)
 *
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


/**
 *  Map variable
 *
 *  It assigns a variable into the current module (or class)
 *
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

/**
 *  Access const array
 *
 *  It reports an error when trying to write into a const array
 *
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

/**
 *  Map an array
 *
 *  It assigns an array into the current module (or class)
 *
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

