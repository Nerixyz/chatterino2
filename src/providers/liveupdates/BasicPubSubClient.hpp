#pragma once
#include <pajlada/signals/signal.hpp>
#include "providers/liveupdates/BasicPubSubWebsocket.hpp"

#include <atomic>
#include <chrono>
#include <unordered_set>

namespace chatterino {
template <class Subscription>
class BasicPubSubClient
    : public std::enable_shared_from_this<BasicPubSubClient<Subscription>>
{
public:
    // The max amount of subscriptions on a single connection
    static constexpr size_t MAX_SUBSCRIPTIONS = 100;

    BasicPubSubClient(liveupdates::WebsocketClient &websocketClient,
                      liveupdates::WebsocketHandle handle);
    virtual ~BasicPubSubClient() = default;

    BasicPubSubClient(const BasicPubSubClient &) = delete;
    BasicPubSubClient(const BasicPubSubClient &&) = delete;
    BasicPubSubClient &operator=(const BasicPubSubClient &) = delete;
    BasicPubSubClient &operator=(const BasicPubSubClient &&) = delete;

    virtual void start();
    void stop();

    void close(const std::string &reason,
               websocketpp::close::status::value code =
                   websocketpp::close::status::normal);

    bool subscribe(const Subscription &subscription);
    bool unsubscribe(const Subscription &subscription);

    bool isSubscribedTo(const Subscription &subscription);

    std::unordered_set<Subscription> getSubscriptions() const;

protected:
    bool send(const char *payload);

    bool isStarted() const
    {
        return this->started_.load(std::memory_order_acquire);
    }

    liveupdates::WebsocketClient &websocketClient_;

private:
    liveupdates::WebsocketHandle handle_;
    std::unordered_set<Subscription> subscriptions_;

    std::atomic<bool> started_{false};
};
}  // namespace chatterino
