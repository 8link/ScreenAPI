// Message queue and lifecycle (PROJECT.md F-004).
// Hardware-independent: the caller passes the current time, so the logic
// runs unchanged in the native test environment.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mq {

constexpr size_t kCapacity = 30;
constexpr size_t kIdMaxLen = 16;
constexpr size_t kTitleMaxLen = 30;
constexpr size_t kValueMaxLen = 160;
constexpr uint32_t kMaxDurationS = 86400;

enum class FontSize : uint8_t { Small, Large };
enum class Color : uint8_t { White, Blue, Green, Red };
enum class Kind : uint8_t { Timed, Confirm };

enum class AddResult : uint8_t { Added, Replaced, Full, Invalid };

// Input for add(). id and title may be null or empty; value must not be.
struct NewMessage {
    const char* id;
    const char* title;
    const char* value;
    FontSize fontSize;
    Color color;
    Kind kind;
    uint32_t durationS;  // Timed only: 1..kMaxDurationS
};

struct Message {
    char id[kIdMaxLen + 1];  // empty = no id
    char title[kTitleMaxLen + 1];
    char value[kValueMaxLen + 1];
    FontSize fontSize;
    Color color;
    Kind kind;
    uint32_t durationS;
    uint64_t expiresAtMs;  // Timed only
};

// Index 0 is the newest message. The cursor is the message on screen.
class MessageQueue {
public:
    AddResult add(const NewMessage& input, uint64_t nowMs);

    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    bool full() const { return count_ == kCapacity; }

    // nullptr when the queue is empty.
    const Message* current() const;
    size_t cursor() const { return cursor_; }
    const Message& at(size_t index) const { return items_[index]; }

    // Shows the next older message, wrapping from the oldest to the newest.
    void scrollNext();
    // Removes the shown message. Returns false when the queue is empty.
    bool deleteCurrent();
    void clear();
    // Removes timed messages whose time has run out. Returns true if any were removed.
    bool expire(uint64_t nowMs);

private:
    void removeAt(size_t index);
    int findById(const char* id) const;

    Message items_[kCapacity];
    size_t count_ = 0;
    size_t cursor_ = 0;
};

}  // namespace mq
