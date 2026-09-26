#include "mcp_server.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <mcp_handler.h>

#include "board.h"
#include "clock.h"

namespace mcp_server {

namespace {

constexpr const char* kHostname = board::kHostname;

WebServer server(80);
mcp::Handler* handler = nullptr;
bool started = false;
Events pending;

// Browsers send Origin on cross-site requests; MCP requires rejecting foreign
// origins so a web page cannot post to the device (DNS rebinding). Clients such
// as Claude Code send no Origin header.
bool originAllowed()
{
    if (!server.hasHeader("Origin")) {
        return true;
    }
    const String origin = server.header("Origin");
    return origin == "http://" + WiFi.localIP().toString() || origin == String("http://") + kHostname + ".local";
}

void handlePost()
{
    if (!originAllowed()) {
        Serial.printf("MCP: rejected request from origin %s\n", server.header("Origin").c_str());
        server.send(403, "text/plain", "Origin not allowed");
        return;
    }
    const String& body = server.arg("plain");
    const mcp::Response response = handler->handlePost(body.c_str(), body.length());
    pending.queueChanged |= response.queueChanged;
    pending.messageDropped |= response.messageDropped;
    if (response.sound != mcp::Sound::None) {
        pending.sound = response.sound;
    }
    Serial.printf("MCP: %s\n", response.summary.c_str());
    if (response.body.empty()) {
        server.send(response.httpStatus);
    } else {
        server.send(response.httpStatus, "application/json", response.body.c_str());
    }
}

// No server-to-client stream and no sessions, so only POST is offered.
void handleNotAllowed()
{
    server.sendHeader("Allow", "POST");
    server.send(405, "text/plain", "Method not allowed");
}

void start()
{
    server.begin();
    if (MDNS.begin(kHostname)) {
        MDNS.addService("http", "tcp", 80);
    } else {
        Serial.println("MCP: mDNS start failed");
    }
    started = true;
    Serial.printf("MCP: http://%s/mcp and http://%s.local/mcp\n", WiFi.localIP().toString().c_str(), kHostname);
}

}  // namespace

void begin(mq::MessageQueue& queue)
{
    static mcp::Handler instance(queue, FW_VERSION, clock_sync::utcNow);
    handler = &instance;
    const char* headers[] = {"Origin"};
    server.collectHeaders(headers, 1);
    server.on("/mcp", HTTP_POST, handlePost);
    server.on("/mcp", HTTP_GET, handleNotAllowed);
    server.on("/mcp", HTTP_DELETE, handleNotAllowed);
    server.onNotFound([]() { server.send(404, "text/plain", "Not found; the MCP endpoint is /mcp"); });
}

Events poll(bool connected)
{
    if (connected && !started) {
        start();
    }
    if (started) {
        server.handleClient();
    }
    const Events events = pending;
    pending = Events();
    return events;
}

}  // namespace mcp_server
