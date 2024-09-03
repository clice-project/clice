#include <Protocol/Basic.h>

namespace clice::protocol {

/// A request message to describe a request between the client and the server.
/// Every processed request must send a response back to the sender of the request.
template <typename Params>
struct Request {
    String jsonrpc = "2.0";

    /// The request id.
    Integer id;

    /// The method to be invoked.
    String method;

    /// The method's params.
    Params params;
};

enum class ErrorCode : Integer {
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
};

struct ResponseError {
    /// A number indicating the error type that occurred.
    ErrorCode code;

    /// A string providing a short description of the error.
    String message;

    /// A Primitive or Structured value that contains additional information about the error.
    // TODO: std::optional<Integer> data;
};

/// A Response Message sent as a result of a request.  If a request doesn’t provide a result value
/// the receiver of a request still needs to return a response message to conform to the JSON-RPC
/// specification. The result property of the ResponseMessage should be set to null in this case to signal a
/// successful request.
template <typename Result>
struct Response {
    String jsonrpc = "2.0";

    /// The request id.
    Integer id;

    /// The result of the request.
    std::optional<Result> result;

    /// The error of the request.
    std::optional<ResponseError> error;
};

/// A notification message to inform the server that the client has successfully registered itself.
template <typename RegistrationOptions>
struct Registration {
    String jsonrpc = "2.0";

    /// The method to be invoked.
    String method;

    /// The registration's id.
    Integer id;

    /// The registration's options.
    RegistrationOptions registerOptions;
};

}  // namespace clice::protocol
