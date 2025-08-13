# Configuration

This is the documentation for `clice.toml`.

## Server

## Rule

`[[rules]]` represents an array of objects, where each object has the following properties:
<br>

| Name               | Type                |
| ------------------ | ------------------- |
| `[rules].pattern`  | `array` of `string` |

Glob patterns for matching file paths, following LSP's [standard](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#documentFilter).

- `*`: Matches one or more characters in a path segment.
- `?`: Matches a single character in a path segment.
- `**`: Matches any number of path segments, including zero.
- `{}`: Used for grouping conditions (e.g., `**/*.{ts,js}` matches all TypeScript and JavaScript files).
- `[]`: Declares a character range to match in a path segment (e.g., `example.[0-9]` matches `example.0`, `example.1`, etc.).
- `[!...]`: Excludes a character range to match in a path segment (e.g., `example.[!0-9]` matches `example.a`, `example.b`, but not `example.0`).
<br>

| Name              | Type                | Default |
| ----------------- | ------------------- | ------- |
| `[rules].append`  | `array` of `string` | `[]`    |

Commands to append to the original command list. For example, `append = ["-std=c++17"]`.
<br>

| Name              | Type                | Default |
| ----------------- | ------------------- | ------- |
| `[rules].remove`  | `array` of `string` | `[]`    |

Commands to remove from the original command list. For example, `remove = ["-std=c++11"]`.
<br>

| Name                | Type     | Default |
| ------------------- | -------- | ------- |
| `[rules].readonly`  | `string` | `"auto"` |

Controls whether the file is treated as read-only. Values can be one of `"auto"`, `"always"`, and `"never"`.

- `"auto"`: The file is treated as read-only before you edit it.
- `"always"`: Always treat the file as read-only.
- `"never"`: Always treat the file as non-read-only.

Read-only means the file is not editable, and LSP requests like code actions or completions won't be triggered on it. This avoids dynamic computation and allows direct loading of pre-indexed results, improving performance.
<br>

| Name              | Type     | Default |
| ----------------- | -------- | ------- |
| `[rules].header`  | `string` | `"auto"` |

Controls how to handle header files. Values can be one of `"auto"`, `"always"`, and `"never"`.

- `"auto"`: First try to infer header file context. If no header file context is found, the file will be treated as a regular source file.
- `"always"`: Always treat the file as a header file. If no header file context is found, an error will be reported.
- `"never"`: Always treat the file as a source file.

Header file context refers to the source files or other metadata associated with that header file.
<br>

| Name                 | Type                | Default |
| -------------------- | ------------------- | ------- |
| `[rules].contexts`   | `array` of `string` | `[]`    |

Specify additional header file contexts (file paths) for the file.

Usually, once a file is indexed, header file context is automatically inferred. However, if you need immediate context before indexing is complete, you can manually provide it using this field.

## Cache

| Name        | Type     | Default                        |
| ----------- | -------- | ------------------------------ |
| `cache.dir` | `string` | `"${workspace}/.clice/cache"`  |

Folder for storing PCH and PCM caches.
<br>

## Index

| Name        | Type     | Default                        |
| ----------- | -------- | ------------------------------ |
| `index.dir` | `string` | `"${workspace}/.clice/index"`  |

Folder for storing index files.
<br>

## Feature
