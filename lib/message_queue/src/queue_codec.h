// Binary format for saving the message queue across reboots (PROJECT.md F-004, D-022).
// Hardware-independent; the firmware writes the bytes to a file.
//
// Layout, little-endian:
//   'S' 'Q' version(1)  count(1)
//   per message, newest first:
//     kind(1) fontSize(1) color(1) durationS(4) receivedAt(4)
//     idLength(1) id  titleLength(1) title  valueLength(2) value
//   checksum(4): FNV-1a over all bytes before it
// receivedAt is UTC seconds, 0 for unknown; version 1 has no receivedAt and
// still decodes, with the arrival time unknown.
// Remaining time is not stored: timed messages restart their full duration (D-017).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "message_queue.h"

namespace mq {

constexpr uint8_t kCodecVersion = 2;
constexpr size_t kMaxEncodedSize =
    4 + kCapacity * (3 + 4 + 4 + 1 + kIdMaxLen + 1 + kTitleMaxLen + 2 + kValueMaxLen) + 4;

uint32_t fnv1a(const uint8_t* data, size_t size);

// Returns the number of bytes written, or 0 if capacity is too small.
size_t encodeQueue(const MessageQueue& queue, uint8_t* out, size_t capacity);

// Replaces the contents of queue with the decoded messages, in the saved order.
// Returns false, leaving queue unchanged, if the data is not a valid encoding.
// Messages the queue rejects (for example after a limit change) are skipped;
// restored reports how many were added.
bool decodeQueue(const uint8_t* data, size_t size, MessageQueue& queue, size_t& restored);

}  // namespace mq
