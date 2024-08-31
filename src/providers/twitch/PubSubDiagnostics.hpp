#pragma once

#include <atomic>

namespace chatterino {

struct PubSubDiagnostics {
    std::atomic<uint32_t> connectionsClosed{0};
    std::atomic<uint32_t> connectionsOpened{0};
    std::atomic<uint32_t> connectionsFailed{0};

    std::atomic<uint32_t> messagesReceived{0};
    std::atomic<uint32_t> messagesFailedToParse{0};

    std::atomic<uint32_t> failedListenResponses{0};
    std::atomic<uint32_t> listenResponses{0};
    std::atomic<uint32_t> unlistenResponses{0};
};

}  // namespace chatterino
