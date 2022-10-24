#pragma once

#include "providers/liveupdates/BasicPubSubClient.hpp"
#include "providers/seventv/eventapi/SeventvEventApiSubscription.hpp"

namespace chatterino {

class SeventvEventApiClient
    : public BasicPubSubClient<SeventvEventApiSubscription>
{
public:
    SeventvEventApiClient(liveupdates::WebsocketClient &websocketClient,
                          liveupdates::WebsocketHandle handle);

    void setHeartbeatInterval(int intervalMs);
    void handleHeartbeat();

    void start() override;

private:
    void checkHeartbeat();

    std::atomic<std::chrono::time_point<std::chrono::steady_clock>>
        lastHeartbeat_;
    std::atomic<std::chrono::milliseconds> heartbeatInterval_;
};

}  // namespace chatterino
