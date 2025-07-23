# Design of clice

## Server
clice is a language server, which is also a kind of server. It uses [libuv](https://github.com/libuv/libuv) as the event library and follows the common event-driven model. The main thread handles requests and distributes tasks, while the thread pool executes the actual tasks. Related code is located in the `Server` directory. It generally is responsible for the following tasks:

- communicate with the client
- initialize the server
- distrubute tasks to the thread pool
- manage all opened files

## Protocol

describe the LSP [specification](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/) with C++ struct definition.

## AST

The main focus is to encapsulate some compiler interfaces of Clang.
- build preamble
- build AST
- capture macros
- capture diagnostics
- resolve template
- implement `SelectionTree`

TODO: add more details

## Index

mainly about how to build index and how to use it.

TODO: add more details

## Feature

specific LSP feature implementations.

## Support

some useful utilities.

