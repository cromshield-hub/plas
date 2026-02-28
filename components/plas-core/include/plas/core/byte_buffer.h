#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace plas::core {

class ByteBuffer {
public:
    ByteBuffer();
    explicit ByteBuffer(size_t size);
    ByteBuffer(std::initializer_list<uint8_t> init);
    ByteBuffer(const uint8_t* data, size_t size);

    const uint8_t* Data() const;
    uint8_t* Data();
    size_t Size() const;
    bool Empty() const;

    void Clear();
    void Resize(size_t new_size);
    void Append(const uint8_t* data, size_t size);

    uint8_t& operator[](size_t index);
    const uint8_t& operator[](size_t index) const;

private:
    std::vector<uint8_t> buffer_;
};

}  // namespace plas::core
