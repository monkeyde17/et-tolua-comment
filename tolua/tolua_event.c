/* tolua: event functions
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

#include <stdio.h>

#include "tolua++.h"

/**
 *  Store at ubox
 *
 *  It stores, creating the corresponding table if needed,
 *  the pair key/value in the corresponding ubox table
 *
 *  期望：栈中 obj ... k v
 *
 *  将键值对加入 用户数据obj 的 环境表env 中
 *
 *  obj.env[k] = v
 *
 *  @param L  状态机
 *  @param lo 用户数据栈中位置
 */
static void storeatubox (lua_State* L, int lo)
{
#ifdef LUA_VERSION_NUM                  /* lua 5.1 */
    /* 获得 用户数据obj 的 环境表 env */
    lua_getfenv(L, lo);
    
    if (lua_rawequal(L, -1, TOLUA_NOPEER)) { /* 若环境表为 registry */
        lua_pop(L, 1);
        
        /* 新建 表t */
        lua_newtable(L);
        
        /* 将 表t 设置成 用户数据obj 的环境表 */
        lua_pushvalue(L, -1);
        lua_setfenv(L, lo);
    };                                  /* stack: obj k v env */
    
    
    /* 将环境表移到键值对前 */
    lua_insert(L, -3);                  /* stack: obj env k v */
    /* 将键值加入 环境表env 中 */
    /* env.k = v */
    lua_settable(L, -3);    /* on lua 5.1, we trade the "tolua_peers" lookup for a settable call */
    
    /* 将 环境表env 出栈 */
    lua_pop(L, 1);
#else                                   /* lua 5.2 */
    
    /* 获得 表tolua_peers */
    lua_pushstring(L,"tolua_peers");
    lua_rawget(L,LUA_REGISTRYINDEX);    /* stack: obj k v ubox */
    
    /* 将 用户数据obj 入栈 */
    lua_pushvalue(L,lo);
    /* 即获得 环境表替代 */
    lua_rawget(L,-2);                   /* stack: obj k v ubox ubox[obj] */
    
    if (!lua_istable(L,-1)) /* 若没有对应环境表，新建一个 */
    {
        lua_pop(L,1);                   /* stack: obj k v ubox */
        lua_newtable(L);                /* stack: obj k v ubox table */
        lua_pushvalue(L,1);             /* stack: obj k v ubox table obj */
        lua_pushvalue(L,-2);            /* stack: obj k v ubox table obj table */
        lua_rawset(L,-4);               /* stack: obj k v ubox table ubox[obj]=table */
    }
    /* 将环境表 插入到键之前 */
    lua_insert(L,-4);                   /* stack: obj table k v ubox  */
    /* 弹出 tolua_peers */
    lua_pop(L,1);
    /* 将键值对加入到 环境表 中 */
    lua_rawset(L,-3);
    /* 将环境表出栈 */
    lua_pop(L,1);
#endif
}

/**
 *  Module index function
 *
 *  前提：在栈顶有table和key
 *
 *  模块表__index对应的绑定的函数
 *
 *  1. 直接调用table[".get"][key]()
 *  2. 调用模块元表index元方法
 *
 *  @param L 状态机
 *
 *  @return 返回1
 */
static int module_index_event (lua_State* L)
{
    /*******************/
    /* 调用设置的get函数 */
    /*******************/
    
    /* 获取表table[".get"]的值*/
    lua_pushstring(L,".get");
    lua_rawget(L,-3);                   /* stack : t k get_t:=t[".get"] */
    
    /* 这里是模块内的变量访问的方法 */
    if (lua_istable(L,-1))              /* get_t是一个表 */
    {
        lua_pushvalue(L,2);             /* stack : t k get_t key */
        lua_rawget(L,-2);               /* stack : t k get_t value:=get_t[key] */
        if (lua_iscfunction(L,-1))      /* value是get函数，直接调用 */
        {
            lua_call(L,0,1);
            return 1;
        }
        else if (lua_istable(L,-1))     /* value是一个表，直接返回该value*/
            return 1;                   /* stack : t k get_t value */
    }
    
    /************************/
    /* 调用模块元表index元方法 */
    /************************/
    
    /* 如果不是一个表，则查询 表t 的 元表mt */
    if (lua_getmetatable(L,1))          /* stack : t k get_t mt */
    {
        /* 获取__index元方法 */
        lua_pushstring(L,"__index");
        lua_rawget(L,-2);               /* stack : t k get_t idx:=mt.__index */
        
        lua_pushvalue(L,1);
        lua_pushvalue(L,2);             /* stack : t k get_t idx t k */
        if (lua_isfunction(L,-1))       /* 对于这个-1我只能猜想k和idx是同一种类型了 */
        {
            lua_call(L,2,1);
            return 1;
        }
        else if (lua_istable(L,-1))     /* 键是一个表 */
        {
            lua_gettable(L,-3);         /* stack : t k get_t idx t idx[k] */
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}


/**
 *  Module newindex function
 *
 *  前提：栈上有table，key，value
 *
 *  1. 直接调用table[".set"][key](table, value)
 *  2. 调用老元表newindex元方法
 *      这里需要跳过模块元表，所以需要分步
 *
 *  @param L 状态机
 *
 *  @return 0
 */
static int module_newindex_event (lua_State* L)
{
    
    /*******************/
    /* 调用设置的set函数 */
    /*******************/
    
    lua_pushstring(L,".set");
    lua_rawget(L,-4);
    
    if (lua_istable(L,-1))              /* stack : t k v set_t:=t[".set"] */
    {
        lua_pushvalue(L,2);             /* stack : t k v set_t k */
        lua_rawget(L,-2);               /* stack : t k v set_t set_t.key */
        if (lua_iscfunction(L,-1))      /* stack : t k v set_t func:=set_t.key */
        {
            /* only to be compatible with non-static vars */
            
            lua_pushvalue(L,1);         /* stack : t k v set_t func t */
            lua_pushvalue(L,3);         /* stack : t k v set_t func t v */
            /* 调用set函数 */
            lua_call(L,2,0);            /* stack : t k v set_t */
            return 0;
        }
    }
    
    /*************************/
    /* 调用老元表newindex元方法 */
    /*************************/
    
    /* 获得 表t 的 元表mt ，元表mt的元表 mmt */
    /* mmt 是 老元表，具体见 lua_module#tolua_map.c# */
    if (lua_getmetatable(L,1) && lua_getmetatable(L,-1))
    {
        /* stack : t k v nil mt:=meta_t mmt:=meta_meta_t */
        
        lua_pushstring(L,"__newindex");
        lua_rawget(L,-2);               /* stack : t k v mt mmt mmt_nidx:=mmt.__newindex */
        if (lua_isfunction(L,-1))       /* mmt_nidx 为函数 */
        {
            lua_pushvalue(L,1);
            lua_pushvalue(L,2);
            lua_pushvalue(L,3);         /* stack : t k v mt mmt mmt_nidx t k v */
            /* 调用 mmt_nidx(t, k, v) */
            lua_call(L,3,0);            /* stack : t k v mt mmt */
        }
    }
    
    /* 跳过模块元表 */
    
    lua_settop(L,3);                    /* stack : t k v */
    /* 将键值对加入 表t 中 */
    lua_rawset(L,-3);                   /* stack : t */
    return 0;
}

/**
 *  Class index function
 *
 *  If the object is a userdata (ie, an object), it searches the field in
 *  the alternative table stored in the corresponding "ubox" table.
 *
 *  调用该函数时会调用该方法，并在栈中压入参数
 *
 *  例如 ：
 *  t.key -- 这个key不在t中，则会调用如下函数 __index(t, key)，或者访问表 __index.key
 *
 *  @param L 状态机
 *
 *  @return 1 : 成功
 *  @return 0 : 失败
 */
static int class_index_event (lua_State* L)
{
    /* 获取栈中第一个参数的类型 */
    int t = lua_type(L,1);
    if (t == LUA_TUSERDATA)                         /* 若是用户数据 */
    {
        /* Access alternative table */
        
        /*****************************/
        /* 先是查找用户数据对象的成员变量 */
        /*****************************/
        
#ifdef LUA_VERSION_NUM                              /* 对于 lua5.1 */
        /* 获得 用户数据 的 环境表env，并压入栈中 */
        lua_getfenv(L,1);
        
        if (!lua_rawequal(L, -1, TOLUA_NOPEER)) {   /* 表env 不是reg，则为自定义环境表 */
            /* 将栈中的键入栈 */
            lua_pushvalue(L, 2);                    /* stack: obj key env key */
            /* 在表env中查找 */
            /* 即env[key] */
            /* on lua 5.1, we trade the "tolua_peers" lookup for a gettable call */
            lua_gettable(L, -2);                    /* stack: obj key env[key] */
            if (!lua_isnil(L, -1))                  /* 若不为空则返回 1 */
                return 1;
        };
#else                                               /* lua 5.2 */
        /* 直接入栈 reg.tolua_peers */
        lua_pushstring(L,"tolua_peers");
        lua_rawget(L,LUA_REGISTRYINDEX);            /* stack: obj key peer */
        
        lua_pushvalue(L,1);                         /* stack: obj key peer obj */
        /* 获得对象对应的 环境表env */
        lua_rawget(L,-2);                           /* stack: obj key peer env:=peer[obj] */
        
        if (lua_istable(L,-1))
        {
            lua_pushvalue(L,2);                     /* stack: obj key peer env key */
            /* 在 环境表env 中查询 */
            lua_rawget(L,-2);                       /* stack: obj key peer env value */
            if (!lua_isnil(L,-1))
                return 1;
        }
#endif
        
        /***************************/
        /* 然后是查找用户函数的get函数 */
        /***************************/
        
        /* 重置栈顶 */
        lua_settop(L,2);                            /* stack: obj key */
        
        /* 将 用户数据obj 入栈 */
        lua_pushvalue(L,1);                         /* stack: obj key obj */
        while (lua_getmetatable(L,-1))              /* stack: obj key obj mt */
        {
            /* 删除 用户数据obj 的副本 */
            lua_remove(L,-2);                       /* stack: obj key mt */
            if (lua_isnumber(L,2))                  /* 若键是一个数字 */
            {
                /* 访问 元表mt 中的 ".geti" 字段 */
                /* mt[".geti"] */
                lua_pushstring(L,".geti");
                lua_rawget(L,-2);                   /* stack: obj key mt func:=mt[".geti"] */
                
                if (lua_isfunction(L,-1))
                {
                    lua_pushvalue(L,1);
                    lua_pushvalue(L,2);
                    /* 调用获得的get函数 */
                    lua_call(L,2,1);
                    return 1;
                }
            }
            else                                    /* 若键非数字 */
            {
                lua_pushvalue(L,2);                 /* stack: obj key mt key */
                lua_rawget(L,-2);                   /* stack: obj key mt value */
                
                if (!lua_isnil(L,-1))               /* 检查value，若是有效值则返回 */
                    return 1;
                else                                /* 若是nil，则将这个nil出栈 */
                    lua_pop(L,1);
                
                /* 元表mt 中的 ".get" 字段 */
                lua_pushstring(L,".get");
                lua_rawget(L,-2);                   /* stack: obj key mt tget:=mt[".get"] */
                if (lua_istable(L,-1))              /* tget是表 */
                {
                    lua_pushvalue(L,2);
                    lua_rawget(L,-2);               /* stack: obj key mt tget value:=tget[key] */
                    if (lua_iscfunction(L,-1))      /* value为函数，get函数，则直接调用 */
                    {
                        /* 调用这个函数，参数是obj和key */
                        lua_pushvalue(L,1);
                        lua_pushvalue(L,2);
                        lua_call(L,2,1);
                        return 1;
                    }
                    else if (lua_istable(L,-1))      /* value是表，get表 */
                    {
                        /* 获得用户数据地址 */
                        void* u = *((void**)lua_touserdata(L,1));
                        lua_newtable(L);             /* stack: obj key mt tget value table */
                        
                        
                        /* table[".self"] = 用户数据地址 */
                        lua_pushstring(L,".self");
                        lua_pushlightuserdata(L,u);
                        lua_rawset(L,-3);
                        
                        /* 交换 table 和 value 的位置 */
                        lua_insert(L,-2);            /* stack: obj key mt tget table value */
                        /* 设置table的元表为value */
                        lua_setmetatable(L,-2);
                        /* 拷贝一份至栈顶 */
                        lua_pushvalue(L,-1);         /* stack: obj key mt tget table table */
                        lua_pushvalue(L,2);          /* stack: obj key mt tget table table key */
                        lua_insert(L,-2);            /* stack: obj key mt tget table key table */
                        
                        
                        /* obj.env[key] = table */
                        storeatubox(L,1);            /* stack: obj key mt tget table */
                        return 1;
                    }
                }
            }
            /* 在元表的元表中继续查找 */
            lua_settop(L,3);
        }
        /* 查询完所有的元表之后还没找到，则返回nil */
        lua_pushnil(L);
        return 1;
    }
    else if (t == LUA_TTABLE)                        /* 模块表 */
    {
        /* 调用模块index事件 */
        module_index_event(L);
        return 1;
    }
    
    /* 所有情况都没有找到，返回nil */
    lua_pushnil(L);
    return 1;
}


/**
 *  Newindex function
 *
 *  It first searches for a C/C++ varaible to be set.
 *  Then, it either stores it in the alternative ubox table (in the case it is
 *  an object) or in the own table (that represents the class or module).
 *
 *  当用户数据 的 set方法
 *
 *  @param L 状态机
 *
 *  @return 是否成功
 */
static int class_newindex_event (lua_State* L)
{
    int t = lua_type(L,1);
    if (t == LUA_TUSERDATA)                     /* 若是用户数据 */
    {
        /* Try accessing a C/C++ variable to be set */
        
        /* 获得 用户数据obj 的 元表mt */
        lua_getmetatable(L,1);
        while (lua_istable(L,-1))               /* stack: obj k v mt */
        {
            if (lua_isnumber(L,2))              /* 键是否为 数字 */
            {
                /* 在 元表mt 中查询，获得set函数 */
                lua_pushstring(L,".seti");
                lua_rawget(L,-2);               /* stack: obj k v mt func:=mt[".seti"] */
                if (lua_isfunction(L,-1))
                {
                    lua_pushvalue(L,1);
                    lua_pushvalue(L,2);
                    lua_pushvalue(L,3);
                    lua_call(L,3,0);
                    return 0;
                }
            }
            else                                /* 键不为数字 */
            {
                /* 获得 mt[".set"] */
                /* 获得 表tset */
                lua_pushstring(L,".set");
                lua_rawget(L,-2);               /* stack: obj k v mt tset:=mt[".set"] */
                if (lua_istable(L,-1))
                {
                    /* 根据键查找 set函数 */
                    lua_pushvalue(L,2);
                    lua_rawget(L,-2);           /* stack: obj k v mt tset func:=tset[k] */
                    if (lua_iscfunction(L,-1))
                    {
                        lua_pushvalue(L,1);
                        lua_pushvalue(L,3);
                        lua_call(L,2,0);
                        return 0;
                    }
                    lua_pop(L,1);               /* stack: obj k v mt tset */
                }
                lua_pop(L,1);                   /* stack: obj k v mt */
                
                /* 查找 元表mt 的 元表mmt */
                if (!lua_getmetatable(L,-1))    /* stack: obj k v mt mmt */
                    lua_pushnil(L);
                
                /* 删除 元表mt */
                /* 然后进行下一轮迭代 */
                lua_remove(L,-2);               /* stack: obj k v mmt */
            }
        }
        
        /* 若没有找到set函数 */
        lua_settop(L,3);                        /* stack: obj k v */

        /* then, store as a new field */
        /* 新建一个设置函数 */
        storeatubox(L,1);
    }
    else if (t== LUA_TTABLE)                    /* 模块表 */
    {
        /* stack : table key value */
        module_newindex_event(L);
    }
    return 0;
}

/**
 *  调用.call方法
 *
 *  @param L 状态机
 *
 *  @return 1 : 成功
 *  @return 0 : 出错
 */
static int class_call_event(lua_State* L) {

    if (lua_istable(L, 1)) {
        lua_pushstring(L, ".call");
        lua_rawget(L, 1);
        if (lua_isfunction(L, -1)) {

            lua_insert(L, 1);
            lua_call(L, lua_gettop(L)-1, 1);

            return 1;
        };
    };
    tolua_error(L,"Attempt to call a non-callable object.",NULL);
    return 0;
};

/**
 *  前提：栈中含有两个对象以供操作
 *
 *  直接查找第一个对象的元表中的元方法
 *
 *  @param L  状态机
 *  @param op 操作
 *
 *  @return 1 : 成功
 *  @return 0 : 出错
 */
static int do_operator (lua_State* L, const char* op)
{
    /* 要保证第一个参数为用户数据 */
    if (lua_isuserdata(L,1))
    {
        /* Try metatables */
        lua_pushvalue(L,1);                     /* stack: op1       op2 op1 */
        while (lua_getmetatable(L,-1))
        {   /* stack: op1 op2 op1 mt */
            lua_remove(L,-2);                   /* stack: op1       op2 mt */
            lua_pushstring(L,op);               /* stack: op1       op2 mt key:=op2 */
            lua_rawget(L,-2);                   /* stack: obj:=op1  key mt func:=mt.key */
            if (lua_isfunction(L,-1))
            {
                lua_pushvalue(L,1);
                lua_pushvalue(L,2);
                lua_call(L,2,1);
                return 1;
            }
            lua_settop(L,3);
        }
    }
    /* 错误调用，返回出错信息 */
    tolua_error(L,"Attempt to perform operation on an invalid operand",NULL);
    return 0;
}

/**
 *  add方法
 *
 *  @param L 状态机
 *
 *  @return 1 : 正确调用
 *  @return 0 : 错误调用
 */
static int class_add_event (lua_State* L)
{
    return do_operator(L,".add");
}

/**
 *  sub方法
 *
 *  @param L 状态机
 *
 *  @return 1 : 正确调用
 *  @return 0 : 错误调用
 */
int class_sub_event (lua_State* L)
{
    return do_operator(L,".sub");
}

/**
 *  mul方法
 *
 *  @param L 状态机
 *
 *  @return 1 : 正确调用
 *  @return 0 : 错误调用
 */
static int class_mul_event (lua_State* L)
{
    return do_operator(L,".mul");
}

/**
 *  div方法
 *
 *  @param L 状态机
 *
 *  @return 1 : 正确调用
 *  @return 0 : 错误调用
 */
static int class_div_event (lua_State* L)
{
    return do_operator(L,".div");
}

/**
 *  lt方法
 *
 *  @param L 状态机
 *
 *  @return 1 : 正确调用
 *  @return 0 : 错误调用
 */
static int class_lt_event (lua_State* L)
{
    return do_operator(L,".lt");
}

/**
 *  le方法
 *
 *  @param L 状态机
 *
 *  @return 1 : 正确调用
 *  @return 0 : 错误调用
 */
static int class_le_event (lua_State* L)
{
    return do_operator(L,".le");
}

/**
 *  eq方法
 *
 *  @param L 状态机
 *
 *  @return 1 : 正确调用
 *  @return 0 : 错误调用
 */
static int class_eq_event (lua_State* L)
{
    /* copying code from do_operator here to return false when no operator is found */
    if (lua_isuserdata(L,1))
    {
        /* Try metatables */
        lua_pushvalue(L,1);                        /* stack: op1 op2 op1 */
        while (lua_getmetatable(L,-1))
        {   /* stack: op1 op2 op1 mt */
            lua_remove(L,-2);                      /* stack: op1 op2 mt */
            lua_pushstring(L,".eq");               /* stack: op1 op2 mt key */
            lua_rawget(L,-2);                      /* stack: obj key mt func */
            if (lua_isfunction(L,-1))
            {
                lua_pushvalue(L,1);
                lua_pushvalue(L,2);
                lua_call(L,2,1);
                return 1;
            }
            lua_settop(L,3);
        }
    } /* 到此为止都和 do_operator 一样 */
    

    /* 对于等于比较永远不会出错，成功调用，入栈0 */
    lua_settop(L, 3);
    lua_pushboolean(L, 0);
    return 1;
}

/*
static int class_gc_event (lua_State* L)
{
    void* u = *((void**)lua_touserdata(L,1));
    fprintf(stderr, "collecting: looking at %p\n", u);
    lua_pushstring(L,"tolua_gc");
    lua_rawget(L,LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L,u);
    lua_rawget(L,-2);
    if (lua_isfunction(L,-1))
    {
        lua_pushvalue(L,1);
        lua_call(L,1,0);
         lua_pushlightuserdata(L,u);
        lua_pushnil(L);
        lua_rawset(L,-3);
    }
    lua_pop(L,2);
    return 0;
}
*/

/**
 *  垃圾回收事件
 *
 *  @param L 状态机
 *
 *  @return 1 : 成功调用
 *  @return 0 : 失败调用
 */
TOLUA_API int class_gc_event (lua_State* L)
{
    void* u = *((void**)lua_touserdata(L,1));
    int top;
    
    /*fprintf(stderr, "collecting: looking at %p\n", u);*/
    /*
    lua_pushstring(L,"tolua_gc");
    lua_rawget(L,LUA_REGISTRYINDEX);
    */
    
    lua_pushvalue(L, lua_upvalueindex(1));
    lua_pushlightuserdata(L,u);
    lua_rawget(L,-2);            /* stack: gc umt    */
    lua_getmetatable(L,1);       /* stack: gc umt mt */
    /*fprintf(stderr, "checking type\n");*/
    top = lua_gettop(L);
    if (tolua_fast_isa(L,top,top-1, lua_upvalueindex(2))) /* make sure we collect correct type */
    {
        /*fprintf(stderr, "Found type!\n");*/
        /* get gc function */
        lua_pushliteral(L,".collector");
        lua_rawget(L,-2);           /* stack: gc umt mt collector */
        if (lua_isfunction(L,-1)) {
            /*fprintf(stderr, "Found .collector!\n");*/
        }
        else {
            lua_pop(L,1);
            /*fprintf(stderr, "Using default cleanup\n");*/
            lua_pushcfunction(L,tolua_default_collect);
        }

        lua_pushvalue(L,1);         /* stack: gc umt mt collector u */
        lua_call(L,1,0);

        lua_pushlightuserdata(L,u); /* stack: gc umt mt u */
        lua_pushnil(L);             /* stack: gc umt mt u nil */
        lua_rawset(L,-5);           /* stack: gc umt mt */
    }
    lua_pop(L,3);
    return 0;
}

/**
 *  Register module events
 *
 *  It expects the metatable on the top of the stack
 *
 *  前提：栈顶有表
 *
 *  设置表的index和newindex方法
 *
 *  @param L 状态机
 */
TOLUA_API void tolua_moduleevents (lua_State* L)
{
    lua_pushstring(L,"__index");
    lua_pushcfunction(L,module_index_event);
    lua_rawset(L,-3);
    
    lua_pushstring(L,"__newindex");
    lua_pushcfunction(L,module_newindex_event);
    lua_rawset(L,-3);
}

/**
 *  Check if the object on the top has a module metatable
 *
 *  检查栈顶模块表的index元方法是否为 module_index_event
 * 
 *  @return 1 : 是
 *  @return 0 : 否
 */
TOLUA_API int tolua_ismodulemetatable (lua_State* L)
{
    int r = 0;
    /* 获取 表t 的 元表mt ，并入栈 */
    if (lua_getmetatable(L,-1))         /* stack: t mt */
    {
        /* 查询元表的__index的值 */
        lua_pushstring(L,"__index");
        lua_rawget(L,-2);               /* stack: t mt idx:=mt.__index */
        
        /* 检查idx是否为 module_index_event */
        r = (lua_tocfunction(L,-1) == module_index_event);
        /* 恢复之前栈的情况 */
        lua_pop(L,2);
    }
    /* 返回结果 */
    return r;
}

/**
 *  Register class events
 *
 *  It expects the metatable on the top of the stack
 *
 *  前提：元表在栈顶
 *
 *  将c函数绑定到各个元方法
 *
 *  @param L 状态机
 */
TOLUA_API void tolua_classevents (lua_State* L)
{
    /**************/
    /* 添加查询函数 */
    /**************/
    
    /* 绑定到__index */
    lua_pushstring(L,"__index");
    lua_pushcfunction(L,class_index_event);
    lua_rawset(L,-3);
    /* 绑定到__newindex */
    lua_pushstring(L,"__newindex");
    lua_pushcfunction(L,class_newindex_event);
    lua_rawset(L,-3);

    
    /***********/
    /* 运算函数 */
    /***********/
    
    /* 绑定到__add */
    lua_pushstring(L,"__add");
    lua_pushcfunction(L,class_add_event);
    lua_rawset(L,-3);
    /* 绑定到__sub */
    lua_pushstring(L,"__sub");
    lua_pushcfunction(L,class_sub_event);
    lua_rawset(L,-3);
    /* 绑定到__mul */
    lua_pushstring(L,"__mul");
    lua_pushcfunction(L,class_mul_event);
    lua_rawset(L,-3);
    /* 绑定到__div */
    lua_pushstring(L,"__div");
    lua_pushcfunction(L,class_div_event);
    lua_rawset(L,-3);

    /***********/
    /* 比较函数 */
    /***********/
    
    /* 绑定到__lt */
    lua_pushstring(L,"__lt");
    lua_pushcfunction(L,class_lt_event);
    lua_rawset(L,-3);
    lua_pushstring(L,"__le");
    lua_pushcfunction(L,class_le_event);
    lua_rawset(L,-3);
    lua_pushstring(L,"__eq");
    lua_pushcfunction(L,class_eq_event);
    lua_rawset(L,-3);

    lua_pushstring(L,"__call");
    lua_pushcfunction(L,class_call_event);
    lua_rawset(L,-3);

    lua_pushstring(L,"__gc");
    lua_pushstring(L, "tolua_gc_event");
    lua_rawget(L, LUA_REGISTRYINDEX);
    /*lua_pushcfunction(L,class_gc_event);*/
    lua_rawset(L,-3);
}

