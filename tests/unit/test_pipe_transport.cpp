#include <atomic>
#include <future>
#include <string>

#include <gtest/gtest.h>

#include "insights/archive.h"
#include "insights/pipe_transport.h"
#include "test_support.h"

namespace {

// Each test listens on its own pipe instead of DATA_PIPE_NAME or
// CONTROL_PIPE_NAME, so the tests never collide with a running viewer or with
// another test process.
class PipeTransportTest : public insights::test::InsightsTest {
protected:
    void SetUp() override {
        InsightsTest::SetUp();

        static std::atomic<int> counter = 0;
        pipe_name_ = L"\\\\.\\pipe\\cpp-insights-test-" + std::to_wstring(GetCurrentProcessId()) +
                     L"-" + std::to_wstring(counter++);
    }

    // A client can open the pipe as soon as Listen() returns; Accept() then
    // completes immediately, so no thread is needed to connect.
    void ConnectPair(DWORD server_access, DWORD client_access) {
        auto result = server_.Listen(pipe_name_.c_str(), server_access);
        ASSERT_TRUE(result) << result.error.message();

        result = client_.Connect(pipe_name_.c_str(), client_access);
        ASSERT_TRUE(result) << result.error.message();

        result = server_.Accept();
        ASSERT_TRUE(result) << result.error.message();

        ASSERT_TRUE(server_.IsConnected());
        ASSERT_TRUE(client_.IsConnected());
    }

    static insights::ByteBuffer MakePayload(size_t size) {
        insights::ByteBuffer payload(size);
        for (size_t i = 0; i < size; ++i) {
            payload[i] = static_cast<std::byte>(i * 31 + 7);
        }
        return payload;
    }

    std::wstring         pipe_name_;
    insights::PipeServer server_;
    insights::PipeClient client_;
};

TEST_F(PipeTransportTest, ClientSendsHandshakeToServer) {
    ConnectPair(PIPE_ACCESS_INBOUND, GENERIC_WRITE);

    insights::BinaryWriter writer;
    std::string msg = "hello";
    writer << msg;

    auto result = client_.Send(insights::PacketType::HANDSHAKE, writer.GetBuffer());
    ASSERT_TRUE(result) << result.error.message();

    insights::PacketHeader header;
    insights::ByteBuffer   payload;
    result = server_.Receive(header, payload);
    ASSERT_TRUE(result) << result.error.message();

    EXPECT_EQ(header.type,         insights::PacketType::HANDSHAKE);
    EXPECT_EQ(header.payload_size, writer.GetBuffer().size());

    insights::BinaryReader reader(payload);
    std::string received;
    reader << received;
    EXPECT_EQ(received, msg);
}

TEST_F(PipeTransportTest, ServerSendsEmptyPacketToClient) {
    ConnectPair(PIPE_ACCESS_OUTBOUND, GENERIC_READ);

    auto result = server_.Send(insights::PacketType::SESSION_START, {});
    ASSERT_TRUE(result) << result.error.message();

    insights::PacketHeader header;
    insights::ByteBuffer   payload = MakePayload(4);
    result = client_.Receive(header, payload);
    ASSERT_TRUE(result) << result.error.message();

    EXPECT_EQ(header.type,         insights::PacketType::SESSION_START);
    EXPECT_EQ(header.payload_size, 0u);
    EXPECT_TRUE(payload.empty());
}

TEST_F(PipeTransportTest, PacketsArriveInOrder) {
    ConnectPair(PIPE_ACCESS_INBOUND, GENERIC_WRITE);

    ASSERT_TRUE(client_.Send(insights::PacketType::FRAME,        MakePayload(3)));
    ASSERT_TRUE(client_.Send(insights::PacketType::SESSION_STOP, {}));
    ASSERT_TRUE(client_.Send(insights::PacketType::FRAME,        MakePayload(5)));

    insights::PacketHeader header;
    insights::ByteBuffer   payload;

    ASSERT_TRUE(server_.Receive(header, payload));
    EXPECT_EQ(header.type, insights::PacketType::FRAME);
    EXPECT_EQ(payload,     MakePayload(3));

    ASSERT_TRUE(server_.Receive(header, payload));
    EXPECT_EQ(header.type, insights::PacketType::SESSION_STOP);
    EXPECT_TRUE(payload.empty());

    ASSERT_TRUE(server_.Receive(header, payload));
    EXPECT_EQ(header.type, insights::PacketType::FRAME);
    EXPECT_EQ(payload,     MakePayload(5));
}

TEST_F(PipeTransportTest, PayloadLargerThanPipeBufferArrivesIntact) {
    ConnectPair(PIPE_ACCESS_INBOUND, GENERIC_WRITE);

    // The pipe buffers 4 KB, so the write blocks until the server reads.
    auto expected = MakePayload(64 * 1024);
    auto sent = std::async(std::launch::async, [&]() {
        return client_.Send(insights::PacketType::FRAME, expected);
    });

    insights::PacketHeader header;
    insights::ByteBuffer   payload;
    auto result = server_.Receive(header, payload);
    if (!result) {
        server_.Disconnect(); // unblock the writer before the future waits on it
    }
    ASSERT_TRUE(result) << result.error.message();
    ASSERT_TRUE(sent.get());

    EXPECT_EQ(header.type, insights::PacketType::FRAME);
    EXPECT_EQ(payload,     expected);
}

TEST_F(PipeTransportTest, ServerDetectsClientDisconnect) {
    ConnectPair(PIPE_ACCESS_INBOUND, GENERIC_WRITE);

    client_.Disconnect();

    insights::PacketHeader header;
    insights::ByteBuffer   payload;
    auto result = server_.Receive(header, payload);

    EXPECT_FALSE(result);
    EXPECT_TRUE(result.IsDisconnected()) << result.error.message();
    EXPECT_FALSE(server_.IsConnected());
}

TEST_F(PipeTransportTest, ConnectWithoutServerFails) {
    auto result = client_.Connect(pipe_name_.c_str(), GENERIC_WRITE);

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error.value(), ERROR_FILE_NOT_FOUND) << result.error.message();
    EXPECT_FALSE(client_.IsConnected());
}

TEST_F(PipeTransportTest, SendWithoutConnectionFails) {
    auto client_result = client_.Send(insights::PacketType::FRAME, MakePayload(1));
    auto server_result = server_.Send(insights::PacketType::FRAME, MakePayload(1));

    EXPECT_EQ(client_result.error.value(), ERROR_PIPE_NOT_CONNECTED);
    EXPECT_EQ(server_result.error.value(), ERROR_PIPE_NOT_CONNECTED);
}

// -------------------------------------------------
// GetPipeName
// -------------------------------------------------
class GetPipeNameTest : public insights::test::InsightsTest {
protected:
    void TearDown() override {
        SetEnvironmentVariableW(insights::PIPE_SUFFIX_ENV, nullptr);
        InsightsTest::TearDown();
    }
};

TEST_F(GetPipeNameTest, AppendsSuffixOnlyWhenSet) {
    SetEnvironmentVariableW(insights::PIPE_SUFFIX_ENV, nullptr);
    EXPECT_EQ(insights::GetPipeName(insights::DATA_PIPE_NAME), insights::DATA_PIPE_NAME);

    SetEnvironmentVariableW(insights::PIPE_SUFFIX_ENV, L"");
    EXPECT_EQ(insights::GetPipeName(insights::DATA_PIPE_NAME), insights::DATA_PIPE_NAME);

    SetEnvironmentVariableW(insights::PIPE_SUFFIX_ENV, L"test-42");
    EXPECT_EQ(insights::GetPipeName(insights::CONTROL_PIPE_NAME),
              std::wstring(insights::CONTROL_PIPE_NAME) + L"-test-42");
}

} // namespace
