#include <gtest/gtest.h>

#include "plas/core/byte_buffer.h"

namespace plas::core {
namespace {

// --- Constructors ---

TEST(ByteBufferTest, DefaultConstructorIsEmpty) {
    ByteBuffer buf;
    EXPECT_TRUE(buf.Empty());
    EXPECT_EQ(buf.Size(), 0u);
}

TEST(ByteBufferTest, SizeConstructor) {
    ByteBuffer buf(10);
    EXPECT_FALSE(buf.Empty());
    EXPECT_EQ(buf.Size(), 10u);
    // All bytes should be zero-initialized
    for (size_t i = 0; i < buf.Size(); ++i) {
        EXPECT_EQ(buf[i], 0x00);
    }
}

TEST(ByteBufferTest, InitializerListConstructor) {
    ByteBuffer buf{0x01, 0x02, 0x03, 0x04};
    EXPECT_EQ(buf.Size(), 4u);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x02);
    EXPECT_EQ(buf[2], 0x03);
    EXPECT_EQ(buf[3], 0x04);
}

TEST(ByteBufferTest, RawPointerConstructor) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    ByteBuffer buf(data, 3);
    EXPECT_EQ(buf.Size(), 3u);
    EXPECT_EQ(buf[0], 0xAA);
    EXPECT_EQ(buf[2], 0xCC);
}

// --- Accessors ---

TEST(ByteBufferTest, DataOnNonEmpty) {
    ByteBuffer buf{0x10, 0x20};
    EXPECT_NE(buf.Data(), nullptr);
    EXPECT_EQ(buf.Data()[0], 0x10);
}

TEST(ByteBufferTest, ConstData) {
    const ByteBuffer buf{0x10, 0x20};
    const uint8_t* data = buf.Data();
    EXPECT_NE(data, nullptr);
    EXPECT_EQ(data[1], 0x20);
}

TEST(ByteBufferTest, EmptyOnEmptyBuffer) {
    ByteBuffer buf;
    EXPECT_TRUE(buf.Empty());
}

TEST(ByteBufferTest, EmptyOnNonEmptyBuffer) {
    ByteBuffer buf{0x01};
    EXPECT_FALSE(buf.Empty());
}

// --- Mutation ---

TEST(ByteBufferTest, Clear) {
    ByteBuffer buf{0x01, 0x02, 0x03};
    buf.Clear();
    EXPECT_TRUE(buf.Empty());
    EXPECT_EQ(buf.Size(), 0u);
}

TEST(ByteBufferTest, ResizeGrow) {
    ByteBuffer buf{0x01, 0x02};
    buf.Resize(5);
    EXPECT_EQ(buf.Size(), 5u);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x02);
}

TEST(ByteBufferTest, ResizeShrink) {
    ByteBuffer buf{0x01, 0x02, 0x03, 0x04};
    buf.Resize(2);
    EXPECT_EQ(buf.Size(), 2u);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x02);
}

TEST(ByteBufferTest, ResizeToZero) {
    ByteBuffer buf{0x01};
    buf.Resize(0);
    EXPECT_TRUE(buf.Empty());
}

TEST(ByteBufferTest, AppendData) {
    ByteBuffer buf{0x01, 0x02};
    uint8_t extra[] = {0x03, 0x04};
    buf.Append(extra, 2);
    EXPECT_EQ(buf.Size(), 4u);
    EXPECT_EQ(buf[2], 0x03);
    EXPECT_EQ(buf[3], 0x04);
}

TEST(ByteBufferTest, AppendToEmptyBuffer) {
    ByteBuffer buf;
    uint8_t data[] = {0xAA, 0xBB};
    buf.Append(data, 2);
    EXPECT_EQ(buf.Size(), 2u);
    EXPECT_EQ(buf[0], 0xAA);
}

// --- Indexing ---

TEST(ByteBufferTest, IndexReadWrite) {
    ByteBuffer buf(3);
    buf[0] = 0x10;
    buf[1] = 0x20;
    buf[2] = 0x30;
    EXPECT_EQ(buf[0], 0x10);
    EXPECT_EQ(buf[1], 0x20);
    EXPECT_EQ(buf[2], 0x30);
}

// --- Edge cases ---

TEST(ByteBufferTest, EmptyInitializerList) {
    ByteBuffer buf(std::initializer_list<uint8_t>{});
    EXPECT_TRUE(buf.Empty());
    EXPECT_EQ(buf.Size(), 0u);
}

TEST(ByteBufferTest, ResizeSameSize) {
    ByteBuffer buf{0x01, 0x02};
    buf.Resize(2);
    EXPECT_EQ(buf.Size(), 2u);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x02);
}

}  // namespace
}  // namespace plas::core
