// HTTP endpoint for MCP on port 80 and the board's mDNS name, for example
// mcpvue.local (PROJECT.md F-003, D-025).
#pragma once

#include <mcp_handler.h>
#include <message_queue.h>

namespace mcp_server {

struct Events {
    bool queueChanged = false;
    bool messageDropped = false;
    mcp::Sound sound = mcp::Sound::None;  // at most one request is handled per poll
};

void begin(mq::MessageQueue& queue);

// Starts the server and mDNS on the first call while connected, then handles
// pending requests. Call every loop.
Events poll(bool connected);

}  // namespace mcp_server
