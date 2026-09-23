#include "mcp_handler.h"

#include <ArduinoJson.h>
#include <stdio.h>
#include <string.h>

#include "text_clean.h"

namespace mcp {

namespace {

// Newest first; an unknown requested version is answered with the first one.
const char* const kProtocolVersions[] = {"2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"};

const char* const kColorNames[] = {"white", "blue", "green", "red"};
const char* const kFontNames[] = {"small", "large"};

const char* const kInstructions =
    "Shows short messages on the user's desk display (240 x 135 pixels, ASCII fonts). Use show_message for "
    "status updates and give repeated updates the same id, so each update replaces the previous one instead of "
    "filling the queue (30 messages). Timed messages disappear after duration_s seconds on screen; confirm "
    "messages stay until the user deletes them with a button.";

struct ToolOutcome {
    std::string text;
    bool isError = false;
    bool queueChanged = false;
    bool messageDropped = false;
};

std::string toJson(const JsonDocument& doc)
{
    std::string out;
    serializeJson(doc, out);
    return out;
}

Response jsonRpcError(JsonVariantConst id, int code, const char* message, int httpStatus = 200)
{
    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = id;
    doc["error"]["code"] = code;
    doc["error"]["message"] = message;
    Response response;
    response.httpStatus = httpStatus;
    response.body = toJson(doc);
    response.summary = message;
    return response;
}

// Returns the index of value in names, or -1.
int indexOf(const char* value, const char* const* names, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (strcmp(value, names[i]) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void addStringProperty(JsonObject properties, const char* name, const char* description, int maxLength)
{
    JsonObject property = properties[name].to<JsonObject>();
    property["type"] = "string";
    property["description"] = description;
    if (maxLength > 0) {
        property["maxLength"] = maxLength;
    }
}

void addEnumProperty(JsonObject properties, const char* name, const char* description, const char* const* values,
                     size_t count)
{
    JsonObject property = properties[name].to<JsonObject>();
    property["type"] = "string";
    property["description"] = description;
    JsonArray options = property["enum"].to<JsonArray>();
    for (size_t i = 0; i < count; i++) {
        options.add(values[i]);
    }
}

void describeTools(JsonArray tools)
{
    JsonObject show = tools.add<JsonObject>();
    show["name"] = "show_message";
    show["title"] = "Show message";
    show["description"] =
        "Show a message on the user's desk display. The newest message is shown first. Use an id for status "
        "updates so a new update replaces the previous one. Timed messages count down only while on screen; "
        "confirm messages stay until the user deletes them.";
    JsonObject schema = show["inputSchema"].to<JsonObject>();
    schema["type"] = "object";
    JsonObject properties = schema["properties"].to<JsonObject>();
    addStringProperty(properties, "title", "Heading, one line; a longer title scrolls sideways.",
                      static_cast<int>(mq::kTitleMaxLen));
    addStringProperty(properties, "value",
                      "Message text, word-wrapped; long text scrolls. Newlines allowed. Color parts with "
                      "{green}...{/}, {red}, {blue}, {white}; {/} returns to the message color. Tags count toward "
                      "the limit. Plain ASCII shows best; other characters are replaced.",
                      static_cast<int>(mq::kValueMaxLen));
    addEnumProperty(properties, "color", "Text color. Default white.", kColorNames, 4);
    addEnumProperty(properties, "font_size",
                    "small: about 6 lines of 30 characters visible; large: about 3 lines of 17. Default small.",
                    kFontNames, 2);
    const char* const kinds[] = {"timed", "confirm"};
    addEnumProperty(properties, "kind",
                    "timed: removed after duration_s seconds on screen. confirm: stays until the user deletes it. "
                    "Default: timed if duration_s is given, otherwise confirm.",
                    kinds, 2);
    JsonObject duration = properties["duration_s"].to<JsonObject>();
    duration["type"] = "integer";
    duration["description"] = "Seconds on screen for a timed message.";
    duration["minimum"] = 1;
    duration["maximum"] = mq::kMaxDurationS;
    addStringProperty(properties, "id",
                      "Optional. A message with the same id replaces the queued one, even when the queue is full.",
                      static_cast<int>(mq::kIdMaxLen));
    schema["required"].to<JsonArray>().add("value");

    JsonObject status = tools.add<JsonObject>();
    status["name"] = "queue_status";
    status["title"] = "Queue status";
    status["description"] =
        "Number of messages queued on the display and the capacity. When the queue is full, new messages are "
        "dropped until the user deletes some.";
    status["inputSchema"]["type"] = "object";
    status["inputSchema"]["properties"].to<JsonObject>();
}

ToolOutcome toolError(const std::string& text)
{
    ToolOutcome outcome;
    outcome.text = text;
    outcome.isError = true;
    return outcome;
}

std::string queueCountText(const mq::MessageQueue& queue)
{
    char text[48];
    snprintf(text, sizeof(text), "Queue: %u of %u messages.", static_cast<unsigned>(queue.size()),
             static_cast<unsigned>(mq::kCapacity));
    return text;
}

// Cleans an optional string argument into out. Returns an error text, or empty on success.
std::string readText(JsonObjectConst args, const char* key, char* out, size_t maxLength, size_t& replaced)
{
    replaced = 0;
    out[0] = '\0';
    JsonVariantConst value = args[key];
    if (value.isNull()) {
        return "";
    }
    if (!value.is<const char*>()) {
        return std::string(key) + " must be a string.";
    }
    const size_t needed = cleanText(value.as<const char*>(), out, maxLength + 1, replaced);
    if (needed > maxLength) {
        char text[128];
        snprintf(text, sizeof(text), "%s is %u characters; the limit is %u.", key, static_cast<unsigned>(needed),
                 static_cast<unsigned>(maxLength));
        return text;
    }
    return "";
}

// Reads an optional enum argument. Returns an error text, or empty on success.
std::string readEnum(JsonObjectConst args, const char* key, const char* const* names, size_t count, int& index)
{
    JsonVariantConst value = args[key];
    if (value.isNull()) {
        return "";
    }
    index = value.is<const char*>() ? indexOf(value.as<const char*>(), names, count) : -1;
    if (index >= 0) {
        return "";
    }
    std::string text = std::string(key) + " must be one of:";
    for (size_t i = 0; i < count; i++) {
        text += std::string(" ") + names[i];
    }
    return text + ".";
}

ToolOutcome showMessage(mq::MessageQueue& queue, JsonObjectConst args)
{
    static char id[mq::kIdMaxLen + 1];
    static char title[mq::kTitleMaxLen + 1];
    static char value[mq::kValueMaxLen + 1];
    size_t replacedId = 0;
    size_t replacedTitle = 0;
    size_t replacedValue = 0;
    int color = 0;
    int font = 0;
    int kind = -1;

    const char* const kinds[] = {"timed", "confirm"};
    // The first problem found is reported.
    const std::string errors[] = {
        readText(args, "id", id, mq::kIdMaxLen, replacedId),
        readText(args, "title", title, mq::kTitleMaxLen, replacedTitle),
        readText(args, "value", value, mq::kValueMaxLen, replacedValue),
        value[0] == '\0' ? std::string("value is required and must not be empty.") : std::string(),
        readEnum(args, "color", kColorNames, 4, color),
        readEnum(args, "font_size", kFontNames, 2, font),
        readEnum(args, "kind", kinds, 2, kind),
    };
    for (const std::string& error : errors) {
        if (!error.empty()) {
            return toolError(error);
        }
    }

    JsonVariantConst durationArg = args["duration_s"];
    uint32_t durationS = 0;
    if (!durationArg.isNull()) {
        if (!durationArg.is<long>() || durationArg.as<long>() < 1 ||
            durationArg.as<long>() > static_cast<long>(mq::kMaxDurationS)) {
            char text[80];
            snprintf(text, sizeof(text), "duration_s must be a whole number from 1 to %u.",
                     static_cast<unsigned>(mq::kMaxDurationS));
            return toolError(text);
        }
        durationS = static_cast<uint32_t>(durationArg.as<long>());
    }
    const bool timed = kind == 0 || (kind < 0 && durationS > 0);
    if (timed && durationS == 0) {
        return toolError("A timed message needs duration_s.");
    }

    const mq::NewMessage input{id,
                               title,
                               value,
                               static_cast<mq::FontSize>(font),
                               static_cast<mq::Color>(color),
                               timed ? mq::Kind::Timed : mq::Kind::Confirm,
                               timed ? durationS : 0};
    const mq::AddResult result = queue.add(input);

    ToolOutcome outcome;
    switch (result) {
    case mq::AddResult::Added:
        outcome.text = "Shown on the display. " + queueCountText(queue);
        outcome.queueChanged = true;
        break;
    case mq::AddResult::Replaced:
        outcome.text = std::string("Replaced the message with id \"") + id + "\" and showed it. " +
                       queueCountText(queue);
        outcome.queueChanged = true;
        break;
    case mq::AddResult::Full:
        outcome = toolError(
            "Queue full (30 of 30): the message was dropped and not shown. The user must delete messages on the "
            "device first. A message with the id of a queued message still replaces it.");
        outcome.messageDropped = true;
        return outcome;
    case mq::AddResult::Invalid:
        return toolError("The display rejected the message.");
    }
    const size_t replaced = replacedId + replacedTitle + replacedValue;
    if (replaced > 0) {
        char text[96];
        snprintf(text, sizeof(text), " %u non-ASCII characters were shown as '?'.", static_cast<unsigned>(replaced));
        outcome.text += text;
    }
    return outcome;
}

Response toolCall(mq::MessageQueue& queue, JsonVariantConst id, JsonObjectConst params)
{
    const char* name = params["name"];
    if (name == nullptr) {
        return jsonRpcError(id, -32602, "Invalid params: name is required");
    }
    ToolOutcome outcome;
    if (strcmp(name, "show_message") == 0) {
        outcome = showMessage(queue, params["arguments"]);
    } else if (strcmp(name, "queue_status") == 0) {
        outcome.text = queueCountText(queue);
    } else {
        return jsonRpcError(id, -32602, (std::string("Unknown tool: ") + name).c_str());
    }

    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = id;
    JsonObject content = doc["result"]["content"].to<JsonArray>().add<JsonObject>();
    content["type"] = "text";
    content["text"] = outcome.text;
    doc["result"]["isError"] = outcome.isError;

    Response response;
    response.body = toJson(doc);
    response.queueChanged = outcome.queueChanged;
    response.messageDropped = outcome.messageDropped;
    response.summary = std::string(name) + ": " + outcome.text;
    return response;
}

Response initialize(JsonVariantConst id, JsonObjectConst params, const char* serverVersion)
{
    const char* requested = params["protocolVersion"];
    const char* version = kProtocolVersions[0];
    if (requested != nullptr) {
        const int index = indexOf(requested, kProtocolVersions, sizeof(kProtocolVersions) / sizeof(kProtocolVersions[0]));
        if (index >= 0) {
            version = kProtocolVersions[index];
        }
    }

    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = id;
    JsonObject result = doc["result"].to<JsonObject>();
    result["protocolVersion"] = version;
    result["capabilities"]["tools"]["listChanged"] = false;
    result["serverInfo"]["name"] = "ScreenAPI";
    result["serverInfo"]["version"] = serverVersion;
    result["instructions"] = kInstructions;

    Response response;
    response.body = toJson(doc);
    response.summary = std::string("initialize, protocol ") + version;
    return response;
}

Response simpleResult(JsonVariantConst id, const char* summary)
{
    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = id;
    doc["result"].to<JsonObject>();
    Response response;
    response.body = toJson(doc);
    response.summary = summary;
    return response;
}

Response toolsList(JsonVariantConst id)
{
    JsonDocument doc;
    doc["jsonrpc"] = "2.0";
    doc["id"] = id;
    describeTools(doc["result"]["tools"].to<JsonArray>());
    Response response;
    response.body = toJson(doc);
    response.summary = "tools/list";
    return response;
}

Response accepted(const char* summary)
{
    Response response;
    response.httpStatus = 202;
    response.summary = summary;
    return response;
}

}  // namespace

Response Handler::handlePost(const char* body, size_t length)
{
    if (length > kMaxRequestSize) {
        return jsonRpcError(JsonVariantConst(), -32600, "Request too large", 413);
    }
    JsonDocument request;
    if (deserializeJson(request, body, length) != DeserializationError::Ok) {
        return jsonRpcError(JsonVariantConst(), -32700, "Parse error", 400);
    }
    if (!request.is<JsonObject>()) {
        return jsonRpcError(JsonVariantConst(), -32600, "Invalid Request: expected one JSON-RPC object", 400);
    }
    JsonObjectConst message = request.as<JsonObjectConst>();
    JsonVariantConst id = message["id"];
    if (message["jsonrpc"] != "2.0") {
        return jsonRpcError(id, -32600, "Invalid Request: jsonrpc must be \"2.0\"", 400);
    }
    const char* method = message["method"];
    if (method == nullptr) {
        return accepted("client response");  // a response to a server request; this server sends none
    }
    if (id.isNull()) {
        return accepted(method);  // notification, for example notifications/initialized
    }

    JsonObjectConst params = message["params"];
    if (strcmp(method, "initialize") == 0) {
        return initialize(id, params, serverVersion_);
    }
    if (strcmp(method, "ping") == 0) {
        return simpleResult(id, "ping");
    }
    if (strcmp(method, "tools/list") == 0) {
        return toolsList(id);
    }
    if (strcmp(method, "tools/call") == 0) {
        return toolCall(queue_, id, params);
    }
    return jsonRpcError(id, -32601, (std::string("Method not found: ") + method).c_str());
}

}  // namespace mcp
