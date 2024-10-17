#include <uv.h>
#include <Support/JSON.h>
#include <Server/Server.h>
#include <Server/Logger.h>
#include <Server/Config.h>
#include <Support/URI.h>

namespace clice {

Server Server::instance;

namespace {

class MessageBuffer {
    std::vector<char> buffer;
    std::size_t max = 0;

public:
    void write(std::string_view message) {
        buffer.insert(buffer.end(), message.begin(), message.end());
    }

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

uv_loop_t* unique_loop;
uv_idle_t idle;

static llvm::SmallVector<char, 4096> buffer;

void alloc(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
    buffer.resize(size);
    buf->base = buffer.data();
    buf->len = buffer.size();
}

MessageBuffer messageBuffer;

void onRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
    if(nread > 0) {
        messageBuffer.write(std::string_view(buf->base, nread));
    } else if(nread < 0) {
        logger::error("Read error: {}", uv_strerror(nread));
    }
}

struct Pipe {
    inline static uv_pipe_t stdin;
    inline static uv_pipe_t stdout;

    static void initialize() {
        uv_pipe_init(unique_loop, &stdin, 0);
        uv_pipe_open(&stdin, 0);

        uv_pipe_init(unique_loop, &stdout, 0);
        uv_pipe_open(&stdout, 1);

        uv_read_start(reinterpret_cast<uv_stream_t*>(&stdin), alloc, onRead);
    }

    inline static uv_buf_t buf;
    inline static uv_write_t req;

    static void write(std::string_view message) {
        buf.base = const_cast<char*>(message.data());
        buf.len = message.size();
        int state = uv_write(&req, reinterpret_cast<uv_stream_t*>(&stdout), &buf, 1, nullptr);
        if(state < 0) {
            logger::error("Error writing to client: {}", uv_strerror(state));
        }
    }
};

struct Socket {
    inline static uv_tcp_t server;
    inline static uv_tcp_t client;

    static void initialize(const char* ip, int port) {
        uv_tcp_init(unique_loop, &server);

        sockaddr_in addr;
        uv_ip4_addr(ip, port, &addr);

        uv_tcp_bind(&server, reinterpret_cast<const struct sockaddr*>(&addr), 0);
        uv_listen(reinterpret_cast<uv_stream_t*>(&server), 1, [](uv_stream_t* server, int status) {
            if(status < 0) {
                logger::error("Listen error: {}", uv_strerror(status));
                return;
            }

            uv_tcp_init(unique_loop, &client);
            uv_accept(server, reinterpret_cast<uv_stream_t*>(&client));
            uv_read_start(reinterpret_cast<uv_stream_t*>(&client), alloc, onRead);
        });
    }

    inline static uv_buf_t buf;
    inline static uv_write_t req;

    static void write(std::string_view message) {
        buf.base = const_cast<char*>(message.data());
        buf.len = message.size();
        int state = uv_write(&req, reinterpret_cast<uv_stream_t*>(&client), &buf, 1, nullptr);
        if(state < 0) {
            logger::error("Error writing to client: {}", uv_strerror(state));
        }
    }
};

}  // namespace

Server::Server() {

    handlers.try_emplace("initialize", [](json::Value id, json::Value value) {
        auto result = instance.initialize(json::deserialize<protocol::InitializeParams>(value));
        instance.response(std::move(id), json::serialize(result));
    });
}

void eventloop(uv_idle_t* handle) {
    if(auto message = messageBuffer.read(); !message.empty()) {
        Server::instance.handleMessage(message);
        messageBuffer.clear();
    }
}

auto Server::initialize(protocol::InitializeParams params) -> protocol::InitializeResult {
    config::init(URI::resolve(params.workspaceFolders[0].uri));

    // TODO: sacn module:

    // TODO: load index result

    // TODO: initialize dependencies

    return protocol::InitializeResult();
}

int Server::run(int argc, const char** argv) {
    std::this_thread::sleep_for(std::chrono::seconds(2));

    logger::init("console", argv[0]);

    if(auto err = config::parse(argc, argv); err < 0) {
        return err;
    };

    unique_loop = uv_default_loop();
    Socket::initialize("127.0.0.1", 50505);

    uv_idle_init(unique_loop, &idle);
    uv_idle_start(&idle, eventloop);

    uv_run(unique_loop, UV_RUN_DEFAULT);

    uv_loop_close(unique_loop);

    return 0;
}

void Server::handleMessage(std::string_view message) {
    auto result = json::parse(message);
    if(!result) {
        logger::error("Error parsing JSON: {}", llvm::toString(result.takeError()));
    }

    logger::info("Received message: {}", message);
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
    Socket::write(s);
    logger::info("Response: {}", s);
}

}  // namespace clice
