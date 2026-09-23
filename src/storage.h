// Message persistence on LittleFS (PROJECT.md F-004, D-022).
#pragma once

#include <message_queue.h>

namespace storage {

// Mounts LittleFS on the "spiffs" data partition, formatting it if it has no
// file system yet. Returns false if it cannot be mounted.
bool begin();

// Restores saved messages into queue. Returns the number restored; 0 when
// nothing was saved or the file was not valid.
size_t load(mq::MessageQueue& queue);

// Writes the queue to flash. Returns false on failure.
bool save(const mq::MessageQueue& queue);

}  // namespace storage
