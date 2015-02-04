tolua++源码注释
==============

虽然加上中文注释显得有点低端，但是做下这个事情吧。。

源码都是从cocos2dx 3.0版本中拷贝而来

## tolua++

从`PROJECTDIR/external/tolua`拷贝，其中代码对__lua5.1__和__lua5.2__做了兼容

- tolua++.h
- tolua\_event.c & h
- tolua\_map.c
- tolua\_push.c
- tolua\_to.c
- tolua\_is.c

## lua -- c api

其中.h文件是从`PROJECTDIR/external/luajit/include`

lauxlib.c则是从`PROJECTDIR/external/lua`拷贝，这里是__lua5.1__版本

- lua.h
- luaconf.h
- lualib.h
- lauxlib.c & h

## 说明

| 缩写 | 意义     |
|------|----------|------------------
| reg  | registry | LUA_REGISTRYINDEX
| G    | global   | LUA_GOLOBALINDEX

注释中有`reg.xxxx`或者`reg["xxxx"]`两种表示。
