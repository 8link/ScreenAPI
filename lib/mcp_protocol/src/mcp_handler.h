// MCP (Model Context Protocol) request handling for the Streamable HTTP
// transport, without streaming or sessions (PROJECT.md F-003, D-025).
// Hardware-independent: the HTTP layer passes the POST body in and sends the
// response out.
#pragma once

#include <message_queue.h>
#include <stddef.h>
#include <stdint.h>

#include <string>

namespace mcp {

constexpr size_t kMaxRequestSize = 8192;

// The sound a request asks for (PROJECT.md F-013, D-046).
enum class Sound : uint8_t {
    None,
    NewMessage,  // confirm message, no other message waiting
    Queued,      // confirm message, other messages already waiting
    Timed,       // timed message, with or without others waiting
    Rejected,    // show_message dropped by a full queue
};

struct Response {
    int httpStatus = 200;
    std::string body;  // JSON; empty for 202 Accepted
    bool queueChanged = false;
    bool messageDropped = false;  // show_message hit a full queue
    Sound sound = Sound::None;
    std::string summary;          // one line for the serial log
};

// Returns the current UTC time in seconds, or 0 while it is not known.
using WallClock = int64_t (*)();

class Handler {
public:
    // wallClock stamps each show_message with its arrival time; without one,
    // the arrival time is unknown.
    Handler(mq::MessageQueue& queue, const char* serverVersion, WallClock wallClock = nullptr)
        : queue_(queue), serverVersion_(serverVersion), wallClock_(wallClock)
    {
    }

    // Handles one JSON-RPC message POSTed to the MCP endpoint.
    Response handlePost(const char* body, size_t length);

private:
    mq::MessageQueue& queue_;
    const char* serverVersion_;
    WallClock wallClock_;
};

}  // namespace mcp
