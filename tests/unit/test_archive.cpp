#include <cstdint>
#include <stdexcept>

#include <gtest/gtest.h>

#include "insights/insights.h"
#include "test_support.h"

namespace {

class ArchiveTest : public insights::test::InsightsTest {
protected:
    insights::BinaryWriter writer;

    // The writer's buffer minus its last byte_count bytes.
    insights::ByteBuffer Truncated(size_t byte_count) const {
        const auto& buffer = writer.GetBuffer();
        return insights::ByteBuffer(buffer.begin(), buffer.end() - static_cast<std::ptrdiff_t>(byte_count));
    }
};

// -------------------------------------------------
// Round trips
// -------------------------------------------------

TEST_F(ArchiveTest, WriteAndReadInt32) {
    int32_t value = 42;
    writer << value;

    insights::BinaryReader reader(writer.GetBuffer());
    int32_t result = 0;
    reader << result;

    EXPECT_EQ(value, result);
    EXPECT_TRUE(reader.IsEof());
}

TEST_F(ArchiveTest, WriteAndReadString) {
    std::string value = "hello";
    writer << value;

    insights::BinaryReader reader(writer.GetBuffer());
    std::string result;
    reader << result;

    EXPECT_EQ(value, result);
    EXPECT_TRUE(reader.IsEof());
}

TEST_F(ArchiveTest, WriteAndReadEmptyString) {
    std::string value;
    writer << value;

    insights::BinaryReader reader(writer.GetBuffer());
    std::string result = "stale";
    reader << result;

    EXPECT_TRUE(result.empty());
    EXPECT_TRUE(reader.IsEof());
}

TEST_F(ArchiveTest, WriteAndReadVector) {
    std::vector<int32_t> value = { 1, 2, 3, 4, 5 };
    writer << value;

    insights::BinaryReader reader(writer.GetBuffer());
    std::vector<int32_t> result;
    reader << result;

    EXPECT_EQ(value, result);
    EXPECT_TRUE(reader.IsEof());
}

TEST_F(ArchiveTest, WriteAndReadMultiple) {
    int32_t     a = 10;
    std::string b = "world";
    int32_t     c = 20;
    writer << a << b << c;

    insights::BinaryReader reader(writer.GetBuffer());
    int32_t     ra = 0, rc = 0;
    std::string rb;
    reader << ra << rb << rc;

    EXPECT_EQ(a, ra);
    EXPECT_EQ(b, rb);
    EXPECT_EQ(c, rc);
    EXPECT_TRUE(reader.IsEof());
}

TEST_F(ArchiveTest, WriteAndReadScopeRecord) {
    insights::ScopeRecord value = { 7, -3, INT64_MAX, 2, insights::TrackId::GPU_BASE };
    writer << value;

    insights::BinaryReader reader(writer.GetBuffer());
    insights::ScopeRecord result = {};
    reader << result;

    EXPECT_EQ(value, result);
    EXPECT_TRUE(reader.IsEof());
}

TEST_F(ArchiveTest, WriteAndReadFrameRecord) {
    insights::FrameRecord value = {
        { 2, 100, 200, 1, 0u },
        { 3, 200, 300, 1, 0u },
        { 0,   0, 400, 0, 0u },
    };
    writer << value;

    insights::BinaryReader reader(writer.GetBuffer());
    insights::FrameRecord result;
    reader << result;

    EXPECT_EQ(value, result);
    EXPECT_TRUE(reader.IsEof());
}

TEST_F(ArchiveTest, WriteAndReadGroupAndDescriptor) {
    insights::Group      group("Physics");
    insights::Descriptor step("Step", group);
    insights::Descriptor solve("Solve", group);
    writer << group << step << solve;

    insights::BinaryReader reader(writer.GetBuffer());
    insights::Group      loaded_group;
    insights::Descriptor loaded_step;
    insights::Descriptor loaded_solve;
    reader << loaded_group << loaded_step << loaded_solve;

    EXPECT_EQ(loaded_group.GetId(),          group.GetId());
    EXPECT_EQ(loaded_group.GetName(),        "Physics");
    EXPECT_EQ(loaded_group.GetDescriptors(), group.GetDescriptors());
    EXPECT_EQ(loaded_step.GetId(),           step.GetId());
    EXPECT_EQ(loaded_step.GetName(),         "Step");
    EXPECT_EQ(loaded_solve.GetId(),          solve.GetId());
    EXPECT_EQ(loaded_solve.GetName(),        "Solve");
    EXPECT_TRUE(reader.IsEof());
}

// -------------------------------------------------
// Truncated input
// -------------------------------------------------

TEST_F(ArchiveTest, ReadFromEmptyBufferThrows) {
    insights::ByteBuffer   empty;
    insights::BinaryReader reader(empty);
    int32_t result = 0;

    EXPECT_THROW(reader << result, std::out_of_range);
}

TEST_F(ArchiveTest, ReadTruncatedStringThrows) {
    std::string value = "hello";
    writer << value;

    auto buffer = Truncated(1);
    insights::BinaryReader reader(buffer);
    std::string result;

    EXPECT_THROW(reader << result, std::out_of_range);
}

TEST_F(ArchiveTest, ReadTruncatedFrameRecordThrows) {
    insights::FrameRecord value = { { 2, 100, 200, 1, 0u }, { 0, 0, 400, 0, 0u } };
    writer << value;

    auto buffer = Truncated(1);
    insights::BinaryReader reader(buffer);
    insights::FrameRecord result;

    EXPECT_THROW(reader << result, std::out_of_range);
}

TEST_F(ArchiveTest, ReadTruncatedScopeRecordThrows) {
    insights::ScopeRecord value = { 1, 10, 20, 1, 0u };
    writer << value;

    auto buffer = Truncated(1);
    insights::BinaryReader reader(buffer);
    insights::ScopeRecord result = {};

    EXPECT_THROW(reader << result, std::out_of_range);
}

TEST_F(ArchiveTest, ReadTruncatedGroupThrows) {
    insights::Group      group("Physics");
    insights::Descriptor step("Step", group);
    writer << group;

    auto buffer = Truncated(1);
    insights::BinaryReader reader(buffer);
    insights::Group result;

    EXPECT_THROW(reader << result, std::out_of_range);
}

// Archive resizes to a loaded length before checking it against the bytes
// remaining, so this allocates gigabytes instead of throwing. Enable once the
// length is validated first.
TEST_F(ArchiveTest, DISABLED_ReadOversizedLengthThrows) {
    uint32_t length = UINT32_MAX;
    writer << length;

    insights::BinaryReader reader(writer.GetBuffer());
    std::vector<int64_t> result;

    EXPECT_THROW(reader << result, std::out_of_range);
}

} // namespace
