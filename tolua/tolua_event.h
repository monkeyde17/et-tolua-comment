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

#ifndef TOLUA_EVENT_H
#define TOLUA_EVENT_H

#include "tolua++.h"

/**
 * 前提：栈顶有元表
 *
 * 设置栈顶元表的__index和__newindex字段
 */
TOLUA_API void tolua_moduleevents (lua_State* L);

/**
 * 检查栈顶元素是否为一个模块元表
 * 
 * @return 是否为模块元表
 *      1 : 是
 *      0 : 否
 */
TOLUA_API int tolua_ismodulemetatable (lua_State* L);

/**
 * 前提：栈顶有元表
 *
 * 将c函数绑定到各个元方法
 */
TOLUA_API void tolua_classevents (lua_State* L);

#endif
