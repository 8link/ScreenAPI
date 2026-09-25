#include "queue_codec.h"

#include <string.h>

namespace mq {

namespace {

constexpr size_t kHeaderSize = 4;
constexpr size_t kChecksumSize = 4;

class Writer {
public:
    Writer(uint8_t* out, size_t capacity) : out_(out), capacity_(capacity) {}

    void bytes(const void* data, size_t size)
    {
        if (size_ + size > capacity_) {
            overflow_ = true;
            return;
        }
        memcpy(out_ + size_, data, size);
        size_ += size;
    }
    void u8(uint8_t value) { bytes(&value, 1); }
    void u16(uint16_t value)
    {
        const uint8_t b[2] = {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8)};
        bytes(b, 2);
    }
    void u32(uint32_t value)
    {
        const uint8_t b[4] = {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
                              static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
        bytes(b, 4);
    }

    size_t size() const { return size_; }
    bool overflow() const { return overflow_; }

private:
    uint8_t* out_;
    size_t capacity_;
    size_t size_ = 0;
    bool overflow_ = false;
};

class Reader {
public:
    Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    const uint8_t* bytes(size_t size)
    {
        if (pos_ + size > size_) {
            error_ = true;
            return nullptr;
        }
        const uint8_t* p = data_ + pos_;
        pos_ += size;
        return p;
    }
    uint8_t u8()
    {
        const uint8_t* p = bytes(1);
        return p != nullptr ? p[0] : 0;
    }
    uint16_t u16()
    {
        const uint8_t* p = bytes(2);
        return p != nullptr ? static_cast<uint16_t>(p[0] | p[1] << 8) : 0;
    }
    uint32_t u32()
    {
        const uint8_t* p = bytes(4);
        return p != nullptr ? static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 |
                                  static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24
                            : 0;
    }

    size_t position() const { return pos_; }
    bool error() const { return error_; }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
    bool error_ = false;
};

// One decoded message, pointing into the input data.
struct Entry {
    uint8_t kind;
    uint8_t fontSize;
    uint8_t color;
    uint32_t durationS;
    uint32_t receivedAt;
    char id[kIdMaxLen + 1];
    char title[kTitleMaxLen + 1];
    char value[kValueMaxLen + 1];
};

// Reads a length-prefixed string into dest; fails if it is longer than maxLength.
bool readText(Reader& reader, size_t length, char* dest, size_t maxLength)
{
    const uint8_t* p = reader.bytes(length);
    if (p == nullptr || length > maxLength) {
        return false;
    }
    memcpy(dest, p, length);
    dest[length] = '\0';
    return true;
}

bool readEntry(Reader& reader, uint8_t version, Entry& entry)
{
    entry.kind = reader.u8();
    entry.fontSize = reader.u8();
    entry.color = reader.u8();
    entry.durationS = reader.u32();
    entry.receivedAt = version >= 2 ? reader.u32() : 0;
    return readText(reader, reader.u8(), entry.id, kIdMaxLen) &&
           readText(reader, reader.u8(), entry.title, kTitleMaxLen) &&
           readText(reader, reader.u16(), entry.value, kValueMaxLen) && !reader.error();
}

}  // namespace

uint32_t fnv1a(const uint8_t* data, size_t size)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

size_t encodeQueue(const MessageQueue& queue, uint8_t* out, size_t capacity)
{
    Writer writer(out, capacity);
    writer.u8('S');
    writer.u8('Q');
    writer.u8(kCodecVersion);
    writer.u8(static_cast<uint8_t>(queue.size()));
    for (size_t i = 0; i < queue.size(); i++) {
        const Message& message = queue.at(i);
        writer.u8(static_cast<uint8_t>(message.kind));
        writer.u8(static_cast<uint8_t>(message.fontSize));
        writer.u8(static_cast<uint8_t>(message.color));
        writer.u32(message.durationS);
        // 32 bits hold UTC seconds until 2106.
        writer.u32(message.receivedAt > 0 ? static_cast<uint32_t>(message.receivedAt) : 0);
        const size_t idLength = strlen(message.id);
        const size_t titleLength = strlen(message.title);
        const size_t valueLength = strlen(message.value);
        writer.u8(static_cast<uint8_t>(idLength));
        writer.bytes(message.id, idLength);
        writer.u8(static_cast<uint8_t>(titleLength));
        writer.bytes(message.title, titleLength);
        writer.u16(static_cast<uint16_t>(valueLength));
        writer.bytes(message.value, valueLength);
    }
    if (writer.overflow()) {
        return 0;
    }
    writer.u32(fnv1a(out, writer.size()));
    return writer.overflow() ? 0 : writer.size();
}

bool decodeQueue(const uint8_t* data, size_t size, MessageQueue& queue, size_t& restored)
{
    restored = 0;
    if (size < kHeaderSize + kChecksumSize) {
        return false;
    }
    Reader checksumReader(data + size - kChecksumSize, kChecksumSize);
    if (checksumReader.u32() != fnv1a(data, size - kChecksumSize)) {
        return false;
    }

    Reader reader(data, size - kChecksumSize);
    if (reader.u8() != 'S' || reader.u8() != 'Q') {
        return false;
    }
    const uint8_t version = reader.u8();
    if (version < 1 || version > kCodecVersion) {
        return false;
    }
    const uint8_t count = reader.u8();
    if (count > kCapacity) {
        return false;
    }

    // First pass: check every entry before touching the queue, so bad data leaves
    // it unchanged. Only offsets are kept; one Entry is reused to save RAM.
    size_t offsets[kCapacity];
    Entry entry;
    for (size_t i = 0; i < count; i++) {
        offsets[i] = reader.position();
        if (!readEntry(reader, version, entry)) {
            return false;
        }
    }
    if (reader.position() != size - kChecksumSize) {
        return false;
    }

    // Second pass: saved newest first, so add oldest first to keep the order.
    queue.clear();
    for (size_t i = count; i > 0; i--) {
        Reader entryReader(data + offsets[i - 1], size - kChecksumSize - offsets[i - 1]);
        readEntry(entryReader, version, entry);
        const NewMessage input{entry.id,
                               entry.title,
                               entry.value,
                               static_cast<FontSize>(entry.fontSize),
                               static_cast<Color>(entry.color),
                               static_cast<Kind>(entry.kind),
                               entry.durationS,
                               static_cast<int64_t>(entry.receivedAt)};
        if (queue.add(input) == AddResult::Added) {
            restored++;
        }
    }
    return true;
}

}  // namespace mq
