#include <Server/Async.h>


int main() {
    promise<int> p = test();

    uv_idle_t idle;
    uv_idle_init(loop, &idle);
    idle.data = &p;

    uv_idle_start(&idle, [](uv_idle_t* handle) {
        auto& p = uv_cast<promise<int>>(handle);
        p.resume();
        uv_idle_stop(handle);
    });

    uv_run(loop, UV_RUN_DEFAULT);

    return 0;
}
