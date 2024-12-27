# Configuration

This is the document of

## Server

| Name                   | Type      | Default |
| ---------------------- | --------- | ------- |
| `server.moduleSupport` | `boolean` | `false` |

Whether to enable module support.
<br>

| Name                | Type      | Default |
| ------------------- | --------- | ------- |
| `server.overSearch` | `boolean` | `true`  |

- `false`: Limits the symbol search scope to files connected through the **include graph**, which is efficient but does not handle symbols defined independently in other files.

For example: 

```cpp
/// a.h
struct Foo {};

/// b.cpp
#include "a.h"
Foo foo1;

/// c.cpp
#include "a.h"
Foo foo2;
```

If you look up the symbol `Foo` in `b.cpp`, the include graph guides the search path as follows: `b.cpp` -> `a.h` -> `c.cpp`. All other files are ignored. When you have a really large project, this can save a lot of time.

- `true`: Expands the search to all index files, ignoring the include graph. This is less efficient but ensures all references to a symbol can be found, even if they are not linked through `#include`.

For example, consider the following files:

```cpp
/// a.cpp 
struct Foo {};
Foo foo1;

/// b.cpp
void foo(struct Foo foo2);
```

In such case, because the symbol `Foo` is independently declared in multiple files. To find all references to `Foo`, it becomes necessary to search all index files.
<br>

## Rule

`[rules]` represents that it is an array of objects. Each object has the following properties. Note that the order of rules matters. clice applies the first matching rule to the file. 
<br>

| Name              | Type     |
| ----------------- | -------- |
| `[rules].pattern` | `string` |

Glob pattern for matching files. If the pattern matches the file path, clice will apply the rule to the file.

Normally, the pattern is a file path. However, you can also use the following syntax to match multiple files:

- `*`: Matches one or more characters in a path segment.
- `?`: Matches a single character in a path segment.
- `**`: Matches any number of path segments, including none.
- `{}`: Groups conditions (e.g., `**/*.{ts,js}` matches all TypeScript and JavaScript files).
- `[]`: Declares a range of characters to match in a path segment(e.g., `example.[0-9]` matches `example.0`, `example.1`, etc.).
- `[!...]`: Negates a range of characters to match in a path segment(e.g., `example.[!0-9]` matches `example.a`, `example.b`, but not `example.0`).
<br>

| Name             | Type                | Default |
| ---------------- | ------------------- | ------- |
| `[rules].append` | `array` of `string` | `[]`    |

Commands to append to the original command list. For example, `append = ["-std=c++17"]`.
<br>

| Name             | Type                | Default |
| ---------------- | ------------------- | ------- |
| `[rules].remove` | `array` of `string` | `[]`    |

Commands to remove from the original command list. For example, `remove = ["-std=c++11"]`.
<br>

| Name               | Type     | Default  |
| ------------------ | -------- | -------- |
| `[rules].readonly` | `string` | `"auto"` |
 
Controls whether the file is treated as readonly. Value could be one of `"auto"`, `"always"` and `"never"`.

- `"auto"`: Treats the file as readonly until you edit it.
- `"always"`: Always treats the file as readonly.
- `"never"`: Always treats the file as non-readonly.

Readonly means the file is not editable, and LSP requests such as code actions or completions will not be triggered on it. This avoids dynamic computation and allows pre-indexed results to be loaded directly, improving performance.
<br>

| Name             | Type     | Default  |
| ---------------- | -------- | -------- |
| `[rules].header` | `string` | `"auto"` |

Controls how header files are treated. Value could be one of `"auto"`, `"always"` and `"never"`.

- `"auto"`: Attempts to infer the header context first. If no header context is found, the file will be treated as a normal source file.
- `"always"`: Always treats the file as a header file. If no header context is found, errors will be reported.
- `"never"`: Always treats the file as a source file.

Header context refers to the related source files or additional metadata linked to the header file.
<br>

| Name               | Type                | Default |
| ------------------ | ------------------- | ------- |
| `[rules].contexts` | `array` of `string` | `[]`    |

Specifies extra header contexts (file paths) for the file.

Normally, header contexts are inferred automatically once the file is indexed. However, if you need immediate context before indexing completes, you can provide it manually using this field.

## Cache

| Name        | Type     | Default                       |
| ----------- | -------- | ----------------------------- |
| `cache.dir` | `string` | `"${workspace}/.clice/cache"` |

Directory for storing PCH and PCM 
<br>

| Name          | Type     | Default |
| ------------- | -------- | ------- |
| `cache.limit` | `number` | `0`     |

Maximum number of cache files to keep. If the total exceeds this limit, clice deletes the oldest files automatically. Set to `0` to disable the limit. 
<br>
 
## Index

| Name        | Type     | Default                       |
| ----------- | -------- | ----------------------------- |
| `index.dir` | `string` | `"${workspace}/.clice/index"` |

Directory for storing index files.
<br>

| Name               | Type      | Default |
| ------------------ | --------- | ------- |
| `index.background` | `boolean` | `true`  |

Whether index files in the background. If `true`, clice will index files in the background when the server is idle. If `false`, you need to send an index request to index files.
<br>

| Name                  | Type      | Default |
| --------------------- | --------- | ------- |
| `index.instantiation` | `boolean` | `true`  |

Whether index entities inside template instantiation. For example

```cpp
struct X { static void foo(); };
struct Y { static void foo(); };

template <typename T>
void foo() {
    T::foo();
}

template void foo<X>();

int main() {
    foo<Y>();
}
```

If `index.instantiation` is `true`, clice will traverse declarations in template instantiation, such as `foo<X>` and `foo<Y>`, and index them. As a result, if you trigger `go-to-definition` on `foo` in `T::foo()`, clice will return the locations of `X::foo` and `Y::foo`.

If `index.instantiation` is `false`, clice will not index entities inside template instantiations, and `go-to-definition` will return no results.
<br>
