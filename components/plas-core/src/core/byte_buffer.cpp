#include "plas/core/byte_buffer.h"

namespace plas::core {

ByteBuffer::ByteBuffer() = default;

ByteBuffer::ByteBuffer(size_t size) : buffer_(size) {}

ByteBuffer::ByteBuffer(std::initializer_list<uint8_t> init) : buffer_(init) {}

ByteBuffer::ByteBuffer(const uint8_t* data, size_t size)
    : buffer_(data, data + size) {}

const uint8_t* ByteBuffer::Data() const {
    return buffer_.data();
}

uint8_t* ByteBuffer::Data() {
    return buffer_.data();
}

size_t ByteBuffer::Size() const {
    return buffer_.size();
}

bool ByteBuffer::Empty() const {
    return buffer_.empty();
}

void ByteBuffer::Clear() {
    buffer_.clear();
}

void ByteBuffer::Resize(size_t new_size) {
    buffer_.resize(new_size);
}

void ByteBuffer::Append(const uint8_t* data, size_t size) {
    buffer_.insert(buffer_.end(), data, data + size);
}

uint8_t& ByteBuffer::operator[](size_t index) {
    return buffer_[index];
}

const uint8_t& ByteBuffer::operator[](size_t index) const {
    return buffer_[index];
}

}  // namespace plas::core
