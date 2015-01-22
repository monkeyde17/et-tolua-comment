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

/* Store at ubox
    * It stores, creating the corresponding table if needed,
    * the pair key/value in the corresponding ubox table
*/
/**
 *  期望：在栈顶包含键值对
 *
 *  @param L  状态机
 *  @param lo 表位置
 */
static void storeatubox (lua_State* L, int lo)
{
#ifdef LUA_VERSION_NUM
    /* 获得环境表 */
    lua_getfenv(L, lo);
    if (lua_rawequal(L, -1, TOLUA_NOPEER)) { /* 若环境表不为 register */
        lua_pop(L, 1);
        
        /* 新建一个表 */
        lua_newtable(L);
        
        /* 将新建的表设置成lo处表的环境表 */
        lua_pushvalue(L, -1);
        lua_setfenv(L, lo);    /* stack: k,v,table  */
    };
    /* 将环境表移到键值对前 */
    /* 然后设置键值对 */
    lua_insert(L, -3);
    lua_settable(L, -3); /* on lua 5.1, we trade the "tolua_peers" lookup for a settable call */
    
    /* 将环境表出栈 */
    lua_pop(L, 1);
#else
    /* stack: key value (to be stored) */
    lua_pushstring(L,"tolua_peers");
    lua_rawget(L,LUA_REGISTRYINDEX);        /* stack: k v ubox */
    lua_pushvalue(L,lo);
    lua_rawget(L,-2);                       /* stack: k v ubox ubox[u] */
    if (!lua_istable(L,-1))
    {
        lua_pop(L,1);                          /* stack: k v ubox */
        lua_newtable(L);                       /* stack: k v ubox table */
        lua_pushvalue(L,1);
        lua_pushvalue(L,-2);                   /* stack: k v ubox table u table */
        lua_rawset(L,-4);                      /* stack: k v ubox ubox[u]=table */
    }
    lua_insert(L,-4);                       /* put table before k */
    lua_pop(L,1);                           /* pop ubox */
    lua_rawset(L,-3);                       /* store at table */
    lua_pop(L,1);                           /* pop ubox[u] */
#endif
}

/* Module index function
*/

/**
 *  前提：在栈顶有table和key
 *
 *  模块表__index对应的绑定的函数
 *
 *  @param L 状态机
 *
 *  @return 返回1
 */
static int module_index_event (lua_State* L)
{
    /* 获取表table[".get"]的值 get_t */
    lua_pushstring(L,".get");   /* stack : table key ".get" */
    lua_rawget(L,-3);           /* stack : table key get_t */
    
    /* 是否是个表 */
    if (lua_istable(L,-1))
    {
        lua_pushvalue(L,2);     /* stack : table key get_t key */
        lua_rawget(L,-2);       /* stack : table key get_t get_t.key */
        /* 若是栈顶是一个c函数 */
        if (lua_iscfunction(L,-1))
        {
            lua_call(L,0,1);    /* stack : table key get_t return_value */
            return 1;
        }
        /* 若是一个表 */
        else if (lua_istable(L,-1))
            return 1;           /* stack : table key get_t get_t.key */
    }
    /* call old index meta event */
    /* 如果不是一个表，则查询第一个参数的元表 */
    if (lua_getmetatable(L,1))  /* stack : table key get_t mt */
    {
        /* 获取__index元方法 */
        lua_pushstring(L,"__index");
        lua_rawget(L,-2);       /* stack : table key get_t mt.__index */
        
        lua_pushvalue(L,1);     /* stack : table key get_t mt.__index table */
        lua_pushvalue(L,2);     /* stack : table key get_t mt.__index table key */
        if (lua_isfunction(L,-1)) /* 对于这个-1我只能猜想key和mt.index是同一种类型了 */
        {
            lua_call(L,2,1);    /* stack : table key get_t return_val */
            return 1;
        }
        else if (lua_istable(L,-1))
        {
            lua_gettable(L,-3); /* stack : table key get_t mt.__index table mt.__index.key */
            return 1;
        }
    }
    lua_pushnil(L);
    return 1;
}

/* Module newindex function
*/
/**
 *  前提：栈上有table，key，value
 *
 *  @param L 状态机
 *
 *  @return 0
 */
static int module_newindex_event (lua_State* L)
{
    lua_pushstring(L,".set");
    lua_rawget(L,-4);
    if (lua_istable(L,-1))      /* stack : table key value set_t */
    {
        lua_pushvalue(L,2);     /* stack : table key value set_t key */
        lua_rawget(L,-2);       /* stack : table key value set_t set_t.key */
        if (lua_iscfunction(L,-1))
        {
            /* only to be compatible with non-static vars */
            lua_pushvalue(L,1); /* stack : table key value set_t func table */
            /* value */
            lua_pushvalue(L,3); /* stack : table key value set_t func table value */
            lua_call(L,2,0);    /* stack : table key value set_t */
            return 0;
        }
    }
    /* call old newindex meta event */
    if (lua_getmetatable(L,1) && lua_getmetatable(L,-1))
    {
        /* stack : t k v nil t_mt v_mt */
        lua_pushstring(L,"__newindex");
        lua_rawget(L,-2);       /* stack : t k v t_mt v_mt v_mt.__newindex */
        if (lua_isfunction(L,-1))
        {
            lua_pushvalue(L,1);
            lua_pushvalue(L,2);
            lua_pushvalue(L,3); /* stack : t k v t_mt v_mt v_mt.__newindex t k v */
            lua_call(L,3,0);    /* stack : t k v t_mt v_mt */
        }
    }
    lua_settop(L,3);            /* stack : t k v */
    /* t[k] = v */
    lua_rawset(L,-3);           /* stack : t */
    return 0;
}

/* Class index function
    * If the object is a userdata (ie, an object), it searches the field in
    * the alternative table stored in the corresponding "ubox" table.
*/
/**
 *
 *
 *  @param L 状态机
 *
 *  @return 1 : 成功
 *  @return 0 : 失败
 */
static int class_index_event (lua_State* L)
{
    /* 调用该函数时会调用改方法，并在栈中压入参数 */
    // 例如 ：
    // t.key -- 这个key不在t中，则会调用如下函数 __index(t, key) 或者 __index.key
    
    /* 获取栈中第一个参数的类型 */
    int t = lua_type(L,1);
    if (t == LUA_TUSERDATA) /* 若是用户数据 */
    {
        /* Access alternative table */
#ifdef LUA_VERSION_NUM /* new macro on version 5.1 */
        /* 对于lua5.1 */
        /* 获得用户数据的环境表usr_env，并压入栈中 */
        lua_getfenv(L,1);
        /* usr_evn表是否是 TOLUA_NOPEER(就是register) */
        if (!lua_rawequal(L, -1, TOLUA_NOPEER)) { /* 若不是，则为tolua_peers表 */
            /* 将栈中的键入栈 */
            lua_pushvalue(L, 2); /* key */
            /* 在tolua_peers表中查找 */
            /* 即tolua_peers[key] */
            lua_gettable(L, -2); /* on lua 5.1, we trade the "tolua_peers" lookup for a gettable call */
            if (!lua_isnil(L, -1))
                return 1; /* 若不为空则返回 1 */
        };
#else
        /* 对于lua 5.2 来说就直接查找 register.tolua_peers */
        lua_pushstring(L,"tolua_peers");
        lua_rawget(L,LUA_REGISTRYINDEX);        /* stack: obj key ubox */
        lua_pushvalue(L,1);
        lua_rawget(L,-2);                       /* stack: obj key ubox ubox[u] */
        if (lua_istable(L,-1))
        {
            lua_pushvalue(L,2);  /* key */
            lua_rawget(L,-2);                      /* stack: obj key ubox ubox[u] value */
            if (!lua_isnil(L,-1))
                return 1;
        }
#endif
        /* 栈中保留对象和键，其余清空 */
        lua_settop(L,2);                        /* stack: obj key */
        /* Try metatables */
        /* 将对象的元表入栈 */
        lua_pushvalue(L,1);                     /* stack: obj key obj */
        while (lua_getmetatable(L,-1))
        {   /* stack: obj key obj mt */
            lua_remove(L,-2);                      /* stack: obj key mt */
            /* 若键是一个数字 */
            if (lua_isnumber(L,2))                 /* check if key is a numeric value */
            {
                /* try operator[] */
                /* 在元表中访问元表中的 ".geti" 字段 */
                lua_pushstring(L,".geti");
                lua_rawget(L,-2);                      /* stack: obj key mt func */
                
                /* 若 ".geti" 字段的值是一个函数，则调用，参数为obj和key */
                if (lua_isfunction(L,-1))
                {
                    lua_pushvalue(L,1);
                    lua_pushvalue(L,2);
                    lua_call(L,2,1);
                    return 1;
                }
            }
            else /* 若是一个非数字的键 */
            {
                /* 在 register 中再查询一次 */
                lua_pushvalue(L,2);                    /* stack: obj key mt key */
                lua_rawget(L,-2);                      /* stack: obj key mt value */
                if (!lua_isnil(L,-1))
                    return 1; /* 若找到该值，则返回 */
                else
                    lua_pop(L,1); /* 若找不到该值，则将这个nil出栈 */
                /* try C/C++ variable */
                
                /* 将tolua_peers[".get"]入栈 */
                lua_pushstring(L,".get");
                lua_rawget(L,-2);                      /* stack: obj key mt tget */
                if (lua_istable(L,-1))
                {
                    /* 在.get表中查找该键 */
                    lua_pushvalue(L,2);
                    lua_rawget(L,-2);                      /* stack: obj key mt value */
                    /* 若找到的值是一个函数 */
                    if (lua_iscfunction(L,-1))
                    {
                        /* 调用这个函数，参数是obj和key */
                        lua_pushvalue(L,1);
                        lua_pushvalue(L,2);
                        lua_call(L,2,1);
                        return 1;
                    }
                    else if (lua_istable(L,-1)) /* 若找到的是一个表 */
                    {
                        /* deal with array: create table to be returned and cache it in ubox */
                        void* u = *((void**)lua_touserdata(L,1));
                        lua_newtable(L);                /* stack: obj key mt value table */
                            /* 将.self字段设置成用户数据的地址 */
                            lua_pushstring(L,".self");
                            lua_pushlightuserdata(L,u);
                            lua_rawset(L,-3);               /* store usertype in ".self" */
                        lua_insert(L,-2);               /* stack: obj key mt table value */
                        lua_setmetatable(L,-2);         /* set stored value as metatable */
                        lua_pushvalue(L,-1);            /* stack: obj key met table table */
                        lua_pushvalue(L,2);             /* stack: obj key mt table table key */
                        lua_insert(L,-2);               /*  stack: obj key mt table key table */
                        storeatubox(L,1);               /* stack: obj key mt table */
                        return 1;
                    }
                }
            }
            /* 若没有查找到，则在元表的元表中继续查找 */
            lua_settop(L,3);
        }
        /* 递归查询完所有的元表之后还没找到则返回，并入栈nil */
        lua_pushnil(L);
        return 1;
    }
    else if (t== LUA_TTABLE) /* 若第一个参数是一个表 */
    {
        /* 这个时候栈中参数为 table key */
        module_index_event(L);
        return 1;
    }
    /* 所有情况都没有找到，返回，并入栈nil */
    lua_pushnil(L);
    return 1;
}

/* Newindex function
    * It first searches for a C/C++ varaible to be set.
    * Then, it either stores it in the alternative ubox table (in the case it is
    * an object) or in the own table (that represents the class or module).
*/
static int class_newindex_event (lua_State* L)
{
    int t = lua_type(L,1);
    if (t == LUA_TUSERDATA)
    {
        /* Try accessing a C/C++ variable to be set */
        lua_getmetatable(L,1);
        while (lua_istable(L,-1))                /* stack: t k v mt */
        {
            if (lua_isnumber(L,2))                 /* check if key is a numeric value */
            {
                /* try operator[] */
                lua_pushstring(L,".seti");
                lua_rawget(L,-2);                      /* stack: obj key mt func */
                if (lua_isfunction(L,-1))
                {
                    lua_pushvalue(L,1);
                    lua_pushvalue(L,2);
                    lua_pushvalue(L,3);
                    lua_call(L,3,0);
                    return 0;
                }
            }
            else
            {
                lua_pushstring(L,".set");
                lua_rawget(L,-2);                      /* stack: t k v mt tset */
                if (lua_istable(L,-1))
                {
                    lua_pushvalue(L,2);
                    lua_rawget(L,-2);                     /* stack: t k v mt tset func */
                    if (lua_iscfunction(L,-1))
                    {
                        lua_pushvalue(L,1);
                        lua_pushvalue(L,3);
                        lua_call(L,2,0);
                        return 0;
                    }
                    lua_pop(L,1);                          /* stack: t k v mt tset */
                }
                lua_pop(L,1);                           /* stack: t k v mt */
                if (!lua_getmetatable(L,-1))            /* stack: t k v mt mt */
                    lua_pushnil(L);
                lua_remove(L,-2);                       /* stack: t k v mt */
            }
        }
        lua_settop(L,3);                          /* stack: t k v */

        /* then, store as a new field */
        storeatubox(L,1);
    }
    else if (t== LUA_TTABLE)
    {
        /* stack : table key value */
        module_newindex_event(L);
    }
    return 0;
}

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
 *
 *
 *  @param L  状态机
 *  @param op 操作
 *
 *  @return 1 : 成功
 *  @return 0 : 出错
 */
static int do_operator (lua_State* L, const char* op)
{
    if (lua_isuserdata(L,1))
    {
        /* Try metatables */
        lua_pushvalue(L,1);                     /* stack: op1 op2 */
        while (lua_getmetatable(L,-1))
        {   /* stack: op1 op2 op1 mt */
            lua_remove(L,-2);                      /* stack: op1 op2 mt */
            lua_pushstring(L,op);                  /* stack: op1 op2 mt key */
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
    }
    tolua_error(L,"Attempt to perform operation on an invalid operand",NULL);
    return 0;
}

static int class_add_event (lua_State* L)
{
    return do_operator(L,".add");
}

int class_sub_event (lua_State* L)
{
    return do_operator(L,".sub");
}

static int class_mul_event (lua_State* L)
{
    return do_operator(L,".mul");
}

static int class_div_event (lua_State* L)
{
    return do_operator(L,".div");
}

static int class_lt_event (lua_State* L)
{
    return do_operator(L,".lt");
}

static int class_le_event (lua_State* L)
{
    return do_operator(L,".le");
}

static int class_eq_event (lua_State* L)
{
    /* copying code from do_operator here to return false when no operator is found */
    if (lua_isuserdata(L,1))
    {
        /* Try metatables */
        lua_pushvalue(L,1);                     /* stack: op1 op2 */
        while (lua_getmetatable(L,-1))
        {   /* stack: op1 op2 op1 mt */
            lua_remove(L,-2);                      /* stack: op1 op2 mt */
            lua_pushstring(L,".eq");                  /* stack: op1 op2 mt key */
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
    }

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


/* Register module events
    * It expects the metatable on the top of the stack
*/
/**
 *  前提：栈顶有元表
 *
 *  设置模块表的__index和__newindex字段
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

/* Check if the object on the top has a module metatable
*/
/**
 *  检查栈顶元素是否为一个模块元表
 * 
 *  @return 1 : 是
 *  @return 0 : 否
 */
TOLUA_API int tolua_ismodulemetatable (lua_State* L)
{
    int r = 0;
    /* 先获取栈顶元素的元表，并入栈 */
    /* stack : table */
    if (lua_getmetatable(L,-1)) /* stack : table mt */
    {
        /* 查询元表的__index的值 */
        lua_pushstring(L,"__index");
        lua_rawget(L,-2); /* stack : table mt mt.__index */
        
        r = (lua_tocfunction(L,-1) == module_index_event);
        lua_pop(L,2);
    }
    return r;
}

/* Register class events
    * It expects the metatable on the top of the stack
*/

/**
 *  前提：元表在栈顶
 *
 *  将c函数绑定到各个元方法
 *
 *  @param L 状态机
 */
TOLUA_API void tolua_classevents (lua_State* L)
{
    /***********/
    /* 运算函数 */
    /***********/
    
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

