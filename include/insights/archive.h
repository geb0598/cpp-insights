#pragma once

#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace insights {

// -------------------------------------------------
// Archive 
// -------------------------------------------------
// One operator<< serves both directions; keep the read and write paths of every
// type symmetric. Loaded bytes come from a pipe or a .trace file and are
// untrusted: check a length against the bytes remaining before allocating.
class Archive {
public:
    virtual ~Archive() = default;

    virtual bool IsLoading() const = 0;
    virtual void Serialize(void* data, size_t size) = 0;

    template <typename T>
    std::enable_if_t<std::is_trivially_copyable_v<T>, Archive&>
    operator<<(T& value) {
        Serialize(&value, sizeof(T));
        return *this;
    }

    template <typename T>
    Archive& operator<<(std::vector<T>& value) {
        uint32_t length = static_cast<uint32_t>(value.size());
        *this << length;

        if (IsLoading()) {
            value.resize(length);
        }

        if (length == 0) {
            return *this;
        }

        if constexpr (std::is_trivially_copyable_v<T>) {
            Serialize(value.data(), length * sizeof(T));
        } else {
            for (T& element : value) {
                *this << element;
            }
        }

        return *this;
    }

    Archive& operator<<(std::string& value) {
        uint32_t length = static_cast<uint32_t>(value.size());
        *this << length;

        if (IsLoading()) {
            value.resize(length);
        }

        if (length == 0) {
            return *this;
        }

        Serialize(value.data(), length * sizeof(char));
        return *this;
    }
};

// -------------------------------------------------
// BinaryWriter 
// -------------------------------------------------

using ByteBuffer = std::vector<std::byte>;

class BinaryWriter : public Archive {
public:
    bool IsLoading() const override { return false; }

    void Serialize(void* data, size_t size) override {
        const auto* bytes = reinterpret_cast<const std::byte*>(data);
        buffer_.insert(buffer_.end(), bytes, bytes + size);
    }

    const ByteBuffer& GetBuffer() const & { return buffer_; }
    ByteBuffer        GetBuffer() &&      { return std::move(buffer_); }
    void              Clear()             { buffer_.clear(); }

private:
    ByteBuffer buffer_;
};

// -------------------------------------------------
// BinaryReader
// -------------------------------------------------

class BinaryReader : public Archive {
public:
    explicit BinaryReader(const ByteBuffer& buffer)
        : buffer_(buffer), pos_(0) {}

    bool IsLoading() const override { return true; }

    void Serialize(void* data, size_t size) override {
        if (pos_ + size > buffer_.size()) {
            throw std::out_of_range(""); // @todo
        }
        std::memcpy(data, buffer_.data() + pos_, size);
        pos_ += size;
    }

    bool IsEof() const { return pos_ >= buffer_.size(); }

private:
    const ByteBuffer& buffer_;
    size_t            pos_;
};

} // namespace insightss