#include <uv.h>
#include <Server/Server.h>
#include <llvm/ADT/SmallString.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <Support/FileSystem.h>
#include <Server/Command.h>
#include <Support/JSON.h>

namespace clice {

Server Server::instance;

static uv_loop_t* loop;
static uv_pipe_t stdin_pipe;
static uv_pipe_t stdout_pipe;

class Buffer {
    std::vector<char> buffer;
    std::size_t max = 0;

public:
    void write(std::string_view message) { buffer.insert(buffer.end(), message.begin(), message.end()); }

    std::string_view read() {
        std::string_view view = std::string_view(buffer.data(), buffer.size());
        auto start = view.find("Content-Length: ") + 16;
        auto end = view.find("\r\n\r\n");

        if(start != std::string_view::npos && end != std::string_view::npos) {
            std::size_t length = std::stoul(std::string(view.substr(start, end - start)));
            if(view.size() >= length + end + 4) {
                this->max = length + end + 4;
                return view.substr(end + 4, length);
            }
        }

        return {};
    }

    void clear() {
        if(max != 0) {
            buffer.erase(buffer.begin(), buffer.begin() + max);
            max = 0;
        }
    }
};

void alloc_buffer(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
    static llvm::SmallString<4096> buffer;
    buffer.resize(suggested_size);
    buf->base = buffer.data();
    buf->len = buffer.size();
}

Buffer buffer;

void read_stdin(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if(nread > 0) {
        buffer.write(std::string_view(buf->base, nread));
        if(auto message = buffer.read(); !message.empty()) {
            Server::instance.handleMessage(message);
            buffer.clear();
        }
    } else if(nread < 0) {
        if(nread != UV_EOF) {
            spdlog::error("Read error: {}", uv_err_name(nread));
        }
        uv_close((uv_handle_t*)stream, NULL);
    }
}

Server::Server() {
    handlers.try_emplace("initialize", [](json::Value id, json::Value value) {
        auto result = instance.initialize(json::deserialize<protocol::InitializeParams>(value));
        instance.response(std::move(id), json::serialize(result));
    });
}

auto Server::initialize(protocol::InitializeParams params) -> protocol::InitializeResult {

    // instance.option.parse()

    return protocol::InitializeResult();
}

int Server::run(int argc, const char** argv) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    option.argc = argc;
    option.argv = argv;

    llvm::SmallString<128> temp;
    temp.append(path::parent_path(path::parent_path(argv[0])));
    path::append(temp, "logs");

    auto error = llvm::sys::fs::make_absolute(temp);
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm = *std::localtime(&now_c);

    std::ostringstream timeStream;
    timeStream << std::put_time(&now_tm, "%Y-%m-%d_%H-%M-%S");  // 格式化为 "YYYY-MM-DD_HH-MM-SS"

    std::string logFileName = "clice_" + timeStream.str() + ".log";
    path::append(temp, logFileName);

    auto logger = spdlog::basic_logger_mt("clice", std::string(temp.str()));
    logger->flush_on(spdlog::level::trace);
    spdlog::set_default_logger(logger);

    loop = uv_default_loop();
    uv_pipe_init(loop, &stdin_pipe, 0);
    uv_pipe_open(&stdin_pipe, 0);

    uv_pipe_init(loop, &stdout_pipe, 0);
    uv_pipe_open(&stdout_pipe, 1);

    uv_read_start((uv_stream_t*)&stdin_pipe, alloc_buffer, read_stdin);

    uv_run(loop, UV_RUN_DEFAULT);

    uv_loop_close(loop);

    return 0;
}

void Server::handleMessage(std::string_view message) {
    auto result = json::parse(message);
    if(!result) {
        spdlog::error("Error parsing JSON: {}", llvm::toString(result.takeError()));
    }

    spdlog::info("Received message: {}", message);
    auto input = result->getAsObject();
    auto id = input->get("id");
    std::string_view method = input->get("method")->getAsString().value();
    auto params = input->get("params");

    if(auto handler = handlers.find(method); handler != handlers.end()) {
        handler->second(*id, *params);
    } else {
        scheduler.dispatch(method, *params);
    }
}

void Server::response(json::Value id, json::Value result) {
    json::Object response;
    response.try_emplace("jsonrpc", "2.0");
    response.try_emplace("id", std::move(id));
    response.try_emplace("result", result);

    json::Value responseValue = std::move(response);

    std::string s;
    llvm::raw_string_ostream stream(s);
    stream << responseValue;
    stream.flush();

    s = "Content-Length: " + std::to_string(s.size()) + "\r\n\r\n" + s;

    static uv_buf_t buf = uv_buf_init(s.data(), s.size());
    static uv_write_t req;
    auto state = uv_write(&req, (uv_stream_t*)&stdout_pipe, &buf, 1, NULL);
    if(state < 0) {
        spdlog::error("Error writing to stdout: {}", uv_strerror(state));
    }
}

}  // namespace clice
