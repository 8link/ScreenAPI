// MCP (Model Context Protocol) request handling for the Streamable HTTP
// transport, without streaming or sessions (PROJECT.md F-003, D-025).
// Hardware-independent: the HTTP layer passes the POST body in and sends the
// response out.
#pragma once

#include <message_queue.h>
#include <stddef.h>

#include <string>

namespace mcp {

constexpr size_t kMaxRequestSize = 8192;

struct Response {
    int httpStatus = 200;
    std::string body;  // JSON; empty for 202 Accepted
    bool queueChanged = false;
    bool messageDropped = false;  // show_message hit a full queue
    bool messageShown = false;    // show_message added or replaced a message
    std::string summary;          // one line for the serial log
};

class Handler {
public:
    Handler(mq::MessageQueue& queue, const char* serverVersion) : queue_(queue), serverVersion_(serverVersion) {}

    // Handles one JSON-RPC message POSTed to the MCP endpoint.
    Response handlePost(const char* body, size_t length);

private:
    mq::MessageQueue& queue_;
    const char* serverVersion_;
};

}  // namespace mcp
