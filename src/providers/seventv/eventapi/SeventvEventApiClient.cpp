#include "providers/seventv/eventapi/SeventvEventApiClient.hpp"

#include "providers/twitch/PubSubHelpers.hpp"

namespace chatterino {

SeventvEventApiClient::SeventvEventApiClient(
    liveupdates::WebsocketClient &websocketClient,
    liveupdates::WebsocketHandle handle)
    : BasicPubSubClient<SeventvEventApiSubscription>(websocketClient, handle)
    , lastHeartbeat_(std::chrono::steady_clock::now())
    , heartbeatInterval_(std::chrono::milliseconds(25000))
{
}

void SeventvEventApiClient::start()
{
    BasicPubSubClient::start();
    this->lastHeartbeat_.store(std::chrono::steady_clock::now(),
                               std::memory_order_release);
    this->checkHeartbeat();
}

void SeventvEventApiClient::setHeartbeatInterval(int intervalMs)
{
    qCDebug(chatterinoSeventvEventApi)
        << "Setting expected heartbeat interval to" << intervalMs << "ms";
    this->heartbeatInterval_.store(std::chrono::milliseconds(intervalMs),
                                   std::memory_order_release);
}

void SeventvEventApiClient::handleHeartbeat()
{
    this->lastHeartbeat_.store(std::chrono::steady_clock::now(),
                               std::memory_order_release);
}

void SeventvEventApiClient::checkHeartbeat()
{
    assert(this->isStarted());
    if ((std::chrono::steady_clock::now() - this->lastHeartbeat_.load()) >
        3 * this->heartbeatInterval_.load(std::memory_order_acquire))
    {
        qCDebug(chatterinoSeventvEventApi)
            << "Didn't receive a heartbeat in time, disconnecting!";
        this->close("Didn't receive a heartbeat in time");

        return;
    }

    auto self = std::dynamic_pointer_cast<SeventvEventApiClient>(
        this->shared_from_this());

    runAfter(this->websocketClient_.get_io_service(),
             this->heartbeatInterval_.load(std::memory_order_acquire),
             [self](auto) {
                 if (!self->isStarted())
                 {
                     return;
                 }
                 self->checkHeartbeat();
             });
}

}  // namespace chatterino
