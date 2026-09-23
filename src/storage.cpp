#include "storage.h"

#include <LittleFS.h>
#include <queue_codec.h>
#include <stdlib.h>

namespace storage {

namespace {

constexpr const char* kQueuePath = "/queue.bin";
constexpr const char* kTempPath = "/queue.tmp";

bool mounted = false;

// Replaces kQueuePath with kTempPath. LittleFS rename is atomic, so a power
// cut leaves either the old or the new file, never a partial one.
bool replaceQueueFile()
{
    if (LittleFS.rename(kTempPath, kQueuePath)) {
        return true;
    }
    // Fallback if rename cannot overwrite; not atomic.
    Serial.println("Storage: rename failed, removing old file first");
    LittleFS.remove(kQueuePath);
    return LittleFS.rename(kTempPath, kQueuePath);
}

}  // namespace

bool begin()
{
    mounted = LittleFS.begin(true);
    if (mounted) {
        Serial.printf("Storage: LittleFS %u of %u bytes used\n", static_cast<unsigned>(LittleFS.usedBytes()),
                      static_cast<unsigned>(LittleFS.totalBytes()));
    } else {
        Serial.println("Storage: LittleFS mount failed, messages will not be saved");
    }
    return mounted;
}

size_t load(mq::MessageQueue& queue)
{
    if (!mounted || !LittleFS.exists(kQueuePath)) {
        return 0;
    }
    File file = LittleFS.open(kQueuePath, "r");
    const size_t size = file.size();
    if (!file || size > mq::kMaxEncodedSize) {
        Serial.printf("Storage: %s unreadable or too large (%u bytes)\n", kQueuePath, static_cast<unsigned>(size));
        return 0;
    }
    // Heap buffer only while loading; the worst case is about 18 KB.
    uint8_t* data = static_cast<uint8_t*>(malloc(size));
    if (data == nullptr) {
        Serial.println("Storage: no memory to load messages");
        return 0;
    }
    const size_t read = file.read(data, size);
    file.close();

    size_t restored = 0;
    if (read != size || !mq::decodeQueue(data, size, queue, restored)) {
        Serial.printf("Storage: %s is not valid, ignoring it\n", kQueuePath);
        restored = 0;
    }
    free(data);
    return restored;
}

bool save(const mq::MessageQueue& queue)
{
    if (!mounted) {
        return false;
    }
    uint8_t* data = static_cast<uint8_t*>(malloc(mq::kMaxEncodedSize));
    if (data == nullptr) {
        Serial.println("Storage: no memory to save messages");
        return false;
    }
    const size_t size = mq::encodeQueue(queue, data, mq::kMaxEncodedSize);

    bool ok = false;
    File file = LittleFS.open(kTempPath, "w");
    if (file) {
        ok = file.write(data, size) == size;
        file.close();
    }
    free(data);
    ok = ok && replaceQueueFile();
    if (ok) {
        Serial.printf("Storage: saved %u messages, %u bytes\n", static_cast<unsigned>(queue.size()),
                      static_cast<unsigned>(size));
    } else {
        Serial.println("Storage: saving messages failed");
    }
    return ok;
}

}  // namespace storage
