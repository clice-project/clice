# Architecture

## Protocol

使用 C++ 来描述 [Language Server Protocol](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/) 中的类型定义。

## AST

对 clang AST 接口的一些方便的封装

## Async

使用 C++20 coroutine 对 libuv 协程的封装

## Compiler

对 clang 编译接口的封装，负责实际的编译过程，以及各种编译信息的获取。

## Feature

各种 LSP 特性的具体实现。

## Server

clice 是一个语言服务器，首先是一个服务器。它使用 [libuv](https://github.com/libuv/libuv) 作为事件库，采用常见的事件驱动的编译模型。主线程负责处理请求以及分发任务，线程池负责执行耗时的任务，比如编译任务。相关的代码位于 `Server` 目录下。

## Support

一些其它的工具库。
