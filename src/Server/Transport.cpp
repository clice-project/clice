
#include <uv.h>
#include <cstdio>
#include <cstdlib>

#include <llvm/ADT/SmallString.h>
#include <coroutine>
#include <variant>

namespace clice {

void on_alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    static llvm::SmallString<4096> buffer;
    buffer.resize_for_overwrite(suggested_size);
    buf->base = buffer.data();
    buf->len = suggested_size;
}

void on_read(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if(nread > 0) {
        printf("Received from stdin: %.*s", (int)nread, buf->base);
    } else if(nread < 0) {
        if(nread != UV_EOF) {
            fprintf(stderr, "Error reading from stdin: %s\n", uv_strerror(nread));
        }
        uv_close((uv_handle_t*)stream, NULL);
    }
}

void on_write(uv_write_t* req, int status) {
    if(status < 0) {
        fprintf(stderr, "Write error: %s\n", uv_strerror(status));
    } else {
        printf("Write completed successfully.\n");
    }
    free(req);
}

void test_pipe(const char* data) {
    static uv_pipe_t stdin_pipe;
    static uv_pipe_t stdout_pipe;

    auto loop = uv_default_loop();
    uv_pipe_init(loop, &stdin_pipe, 0);
    uv_pipe_open(&stdin_pipe, 0);

    uv_pipe_init(loop, &stdout_pipe, 0);
    uv_pipe_open(&stdout_pipe, 1);

    uv_read_start((uv_stream_t*)&stdin_pipe, on_alloc_buffer, on_read);

    uv_buf_t buf = uv_buf_init((char*)data, strlen(data));
    uv_write_t* write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    uv_write(write_req, (uv_stream_t*)&stdout_pipe, &buf, 1, on_write);
}

void test_socket(const char* data) {
    static uv_loop_t* loop = uv_default_loop();
    static uv_tcp_t server;
    static uv_tcp_t client;

    uv_tcp_init(loop, &server);
    uv_tcp_init(loop, &client);

    sockaddr_in addr;
    uv_ip4_addr("0.0.0.0", 7000, &addr);
    uv_tcp_bind(&server, (const sockaddr*)&addr, 0);

    int r = uv_listen((uv_stream_t*)&server, 128, [](uv_stream_t* server, int status) {
        if(status < 0) {
            fprintf(stderr, "New connection error\n");
            return;
        }

        if(uv_accept(server, (uv_stream_t*)&client) == 0) {
            printf("Client connected.\n");
            uv_read_start((uv_stream_t*)&client, on_alloc_buffer, on_read);
        } else {
            uv_close((uv_handle_t*)&client, NULL);
        }
    });

    if(r) {
        fprintf(stderr, "Listen error: %s\n", uv_strerror(r));
    }

    uv_buf_t buf = uv_buf_init((char*)data, strlen(data));
    uv_write_t* write_req = (uv_write_t*)malloc(sizeof(uv_write_t));
    uv_write(write_req, (uv_stream_t*)&client, &buf, 1, on_write);
}

void test_thread_pool() {
    uv_loop_t* loop = uv_default_loop();
    uv_work_t* work = new uv_work_t();
    auto work_cb = [](uv_work_t* req) {
        // in thread pool
        printf("Hello from thread pool.\n");
    };
    auto after_work_cb = [](uv_work_t* req, int status) {
        /// in main thread
        printf("Thread pool work completed.\n");
        free(req);
    };
    uv_queue_work(loop, work, work_cb, after_work_cb);
}

void test_fs_event() {
    uv_loop_t* loop = uv_default_loop();
    uv_fs_event_t fs_event;
    uv_fs_event_init(loop, &fs_event);
    auto on_fs_event = [](uv_fs_event_t* handle, const char* filename, int events, int status) {
        if(status < 0) {
            fprintf(stderr, "Error: %s\n", uv_strerror(status));
            return;
        }

        char path[1024];
        size_t size = sizeof(path);
        int ret = uv_fs_event_getpath(handle, path, &size);

        if(ret == 0) {
            path[size] = '\0';
            printf("File change detected in: %s\n", path);
        } else {
            fprintf(stderr, "Failed to get path: %s\n", uv_strerror(ret));
        }

        if(filename) {
            printf("  Affected file: %s\n", filename);
        }

        if(events & UV_RENAME) {
            printf("  Event: File renamed\n");
        }
        if(events & UV_CHANGE) {
            printf("  Event: File changed\n");
        }
    };

    const char* path = "./watched_dir";
    int ret = uv_fs_event_start(&fs_event, on_fs_event, path, 0);
    if(ret < 0) {
        fprintf(stderr, "Failed to start file watcher: %s\n", uv_strerror(ret));
    }
}

void test_event_loop() {
    auto on_prepare = [](uv_prepare_t* handle) {
        static int count = 0;
        printf("Prepare callback: count = %d\n", count++);
        if(count >= 5) {
            uv_prepare_stop(handle);
        }
    };

    uv_loop_t* loop = uv_default_loop();
    uv_prepare_t prepare_handle;
    uv_prepare_init(loop, &prepare_handle);

    uv_prepare_start(&prepare_handle, on_prepare);
}

}  // namespace clice


