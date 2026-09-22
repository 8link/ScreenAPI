#include "message_queue.h"

#include <string.h>

namespace mq {

namespace {

bool isBlank(const char* text)
{
    return text == nullptr || text[0] == '\0';
}

bool fits(const char* text, size_t maxLen)
{
    return text == nullptr || strlen(text) <= maxLen;
}

void copyText(char* dest, const char* src)
{
    // Lengths are validated before copying, so this never truncates.
    strcpy(dest, src == nullptr ? "" : src);
}

bool isValid(const NewMessage& input)
{
    if (isBlank(input.value)) {
        return false;
    }
    if (!fits(input.id, kIdMaxLen) || !fits(input.title, kTitleMaxLen) || !fits(input.value, kValueMaxLen)) {
        return false;
    }
    if (input.fontSize > FontSize::Large || input.color > Color::Red || input.kind > Kind::Confirm) {
        return false;
    }
    if (input.kind == Kind::Timed && (input.durationS == 0 || input.durationS > kMaxDurationS)) {
        return false;
    }
    return true;
}

}  // namespace

AddResult MessageQueue::add(const NewMessage& input, uint64_t nowMs)
{
    if (!isValid(input)) {
        return AddResult::Invalid;
    }

    const int existing = findById(input.id);
    if (existing < 0 && full()) {
        return AddResult::Full;
    }
    if (existing >= 0) {
        removeAt(static_cast<size_t>(existing));
    }

    memmove(&items_[1], &items_[0], count_ * sizeof(Message));
    Message& message = items_[0];
    copyText(message.id, input.id);
    copyText(message.title, input.title);
    copyText(message.value, input.value);
    message.fontSize = input.fontSize;
    message.color = input.color;
    message.kind = input.kind;
    message.durationS = input.kind == Kind::Timed ? input.durationS : 0;
    message.expiresAtMs = input.kind == Kind::Timed ? nowMs + input.durationS * 1000ULL : 0;
    count_++;
    cursor_ = 0;  // newest first, jump to it

    return existing >= 0 ? AddResult::Replaced : AddResult::Added;
}

const Message* MessageQueue::current() const
{
    return empty() ? nullptr : &items_[cursor_];
}

void MessageQueue::scrollNext()
{
    if (!empty()) {
        cursor_ = (cursor_ + 1) % count_;
    }
}

bool MessageQueue::deleteCurrent()
{
    if (empty()) {
        return false;
    }
    removeAt(cursor_);
    return true;
}

void MessageQueue::clear()
{
    count_ = 0;
    cursor_ = 0;
}

bool MessageQueue::expire(uint64_t nowMs)
{
    bool removed = false;
    for (size_t i = count_; i > 0; i--) {
        const Message& message = items_[i - 1];
        if (message.kind == Kind::Timed && nowMs >= message.expiresAtMs) {
            removeAt(i - 1);
            removed = true;
        }
    }
    return removed;
}

void MessageQueue::removeAt(size_t index)
{
    memmove(&items_[index], &items_[index + 1], (count_ - index - 1) * sizeof(Message));
    count_--;
    // Keep showing the same message if it moved up; after removing the shown
    // message this shows the next older one.
    if (index < cursor_) {
        cursor_--;
    }
    if (cursor_ >= count_) {
        cursor_ = 0;
    }
}

int MessageQueue::findById(const char* id) const
{
    if (isBlank(id)) {
        return -1;
    }
    for (size_t i = 0; i < count_; i++) {
        if (strcmp(items_[i].id, id) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace mq
