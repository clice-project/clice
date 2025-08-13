# Architecture

## Protocol

Use C++ to describe type definitions in the [Language Server Protocol](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/).

## AST

Some convenient wrappers for clang AST interfaces.

## Async

Wrapper for libuv coroutines using C++20 coroutines.

## Compiler

Wrapper for clang compilation interfaces, responsible for actual compilation processes and obtaining various compilation information.

## Feature

Specific implementations of various LSP features.

## Server

clice is a language server, first and foremost a server. It uses [libuv](https://github.com/libuv/libuv) as the event library, adopting a common event-driven compilation model. The main thread is responsible for handling requests and dispatching tasks, while the thread pool is responsible for executing time-consuming tasks, such as compilation tasks. Related code is located in the `Server` directory.

## Support

Some other utility libraries.
