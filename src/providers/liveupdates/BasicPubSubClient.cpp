#include "providers/liveupdates/BasicPubSubClient.hpp"

#include "common/QLogging.hpp"
#include "providers/bttv/liveupdate/BttvLiveUpdateSubscription.hpp"
#include "providers/seventv/eventapi/SeventvEventApiSubscription.hpp"
#include "singletons/Settings.hpp"
#include "util/DebugCount.hpp"
#include "util/Helpers.hpp"

namespace chatterino {
template <class Subscription>
BasicPubSubClient<Subscription>::BasicPubSubClient(
    liveupdates::WebsocketClient &websocketClient,
    liveupdates::WebsocketHandle handle)
    : websocketClient_(websocketClient)
    , handle_(handle)
{
}

template <class Subscription>
void BasicPubSubClient<Subscription>::start()
{
    assert(!this->isStarted());
    this->started_.store(true, std::memory_order_release);
}

template <class Subscription>
void BasicPubSubClient<Subscription>::stop()
{
    assert(this->isStarted());
    this->started_.store(false, std::memory_order_release);
}

template <class Subscription>
void BasicPubSubClient<Subscription>::close(
    const std::string &reason, websocketpp::close::status::value code)
{
    liveupdates::WebsocketErrorCode ec;

    auto conn = this->websocketClient_.get_con_from_hdl(this->handle_, ec);
    if (ec)
    {
        qCDebug(chatterinoLiveupdates)
            << "Error getting connection:" << ec.message().c_str();
        return;
    }

    conn->close(code, reason, ec);
    if (ec)
    {
        qCDebug(chatterinoLiveupdates)
            << "Error closing:" << ec.message().c_str();
        return;
    }
}

template <class Subscription>
bool BasicPubSubClient<Subscription>::subscribe(
    const Subscription &subscription)
{
    if (this->subscriptions_.size() >= BasicPubSubClient::MAX_SUBSCRIPTIONS)
    {
        return false;
    }

    if (!this->subscriptions_.emplace(subscription).second)
    {
        qCWarning(chatterinoLiveupdates)
            << "Tried subscribing to" << subscription
            << "but we're already subscribed!";
        return true;  // true because the subscription already exists
    }

    qCDebug(chatterinoLiveupdates) << "Subscribing to" << subscription;
    DebugCount::increase("LiveUpdates subscriptions");

    QByteArray encoded = subscription.encodeSubscribe();
    this->send(encoded);

    return true;
}

template <class Subscription>
bool BasicPubSubClient<Subscription>::unsubscribe(
    const Subscription &subscription)
{
    if (this->subscriptions_.erase(subscription) <= 0)
    {
        return false;
    }

    qCDebug(chatterinoLiveupdates) << "Unsubscribing from" << subscription;
    DebugCount::decrease("LiveUpdates subscriptions");

    QByteArray encoded = subscription.encodeUnsubscribe();
    this->send(encoded);

    return true;
}

template <class Subscription>
bool BasicPubSubClient<Subscription>::isSubscribedTo(
    const Subscription &subscription)
{
    return this->subscriptions_.find(subscription) !=
           this->subscriptions_.end();
}

template <class Subscription>
std::unordered_set<Subscription>
    BasicPubSubClient<Subscription>::getSubscriptions() const
{
    return this->subscriptions_;
}

template <class Subscription>
bool BasicPubSubClient<Subscription>::send(const char *payload)
{
    liveupdates::WebsocketErrorCode ec;
    this->websocketClient_.send(this->handle_, payload,
                                websocketpp::frame::opcode::text, ec);

    if (ec)
    {
        qCDebug(chatterinoLiveupdates) << "Error sending message" << payload
                                       << ":" << ec.message().c_str();
        return false;
    }

    return true;
}

template class BasicPubSubClient<SeventvEventApiSubscription>;
template class BasicPubSubClient<BttvLiveUpdateSubscription>;

}  // namespace chatterino
