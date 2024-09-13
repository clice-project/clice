#include <uv.h>
#include <Server/Server.h>
#include <llvm/ADT/SmallString.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <Support/FileSystem.h>
#include <Server/Command.h>
#include <Support/JSON.h>
#include <Server/Transport .h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace clice {

Server Server::instance;

static uv_loop_t* loop;

Server::Server() {
    handlers.try_emplace("initialize", [](json::Value id, json::Value value) {
        auto result = instance.initialize(json::deserialize<protocol::InitializeParams>(value));
        instance.response(std::move(id), json::serialize(result));
    });
}

auto Server::initialize(protocol::InitializeParams params) -> protocol::InitializeResult {

    instance.option.parse("/home/ykiko/C++/test");

    return protocol::InitializeResult();
}

int Server::run(int argc, const char** argv) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
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

    auto logger = spdlog::stdout_color_mt("clice");
    logger->flush_on(spdlog::level::trace);
    spdlog::set_default_logger(logger);

    loop = uv_default_loop();
    transport = std::make_unique<Socket>(
        loop,
        [](std::string_view message) {
            instance.handleMessage(message);
        },
        "127.0.0.1",
        50505);

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
        // FIXME: notify do not have a ID.
        if(id) {
            scheduler.dispatch(std::move(*id), method, *params);
        } else {
            scheduler.dispatch(nullptr, method, *params);
        }
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

    // FIXME: use more flexible way to do this.
    transport->write(s);
    spdlog::info("Response: {}", s);
}

}  // namespace clice
