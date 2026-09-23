// Message queue and lifecycle (PROJECT.md F-004).
// Hardware-independent: the caller passes the current time, so the logic
// runs unchanged in the native test environment.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace mq {

constexpr size_t kCapacity = 30;
constexpr size_t kIdMaxLen = 16;
constexpr size_t kTitleMaxLen = 64;
constexpr size_t kValueMaxLen = 512;
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
    uint32_t remainingMs;  // Timed only; counts down only while the message is shown
};

// Index 0 is the newest message. The cursor is the message on screen.
class MessageQueue {
public:
    AddResult add(const NewMessage& input);

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
    // Counts down the shown message by the time since the previous tick, if it
    // is timed, and removes it when its time runs out. The first call only sets
    // the starting point. Returns true if a message was removed.
    bool tick(uint64_t nowMs);

private:
    void removeAt(size_t index);
    int findById(const char* id) const;

    Message items_[kCapacity];
    size_t count_ = 0;
    size_t cursor_ = 0;
    bool ticking_ = false;
    uint64_t lastTickMs_ = 0;
};

}  // namespace mq
