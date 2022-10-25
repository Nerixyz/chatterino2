#pragma once

#include "providers/bttv/liveupdate/BttvLiveUpdateMessages.hpp"
#include "providers/bttv/liveupdate/BttvLiveUpdateSubscription.hpp"
#include "providers/liveupdates/BasicPubSubManager.hpp"
#include "util/QStringHash.hpp"

#include <pajlada/signals/signal.hpp>

namespace chatterino {

class BttvLiveUpdates : public BasicPubSubManager<BttvLiveUpdateSubscription>
{
    template <typename T>
    using Signal =
        pajlada::Signals::Signal<T>;  // type-id is vector<T, Alloc<T>>

public:
    BttvLiveUpdates(QString host);

    struct {
        Signal<BttvLiveUpdateEmoteAddMessage> emoteAdded;
        Signal<BttvLiveUpdateEmoteRemoveMessage> emoteRemoved;
    } signals_;  // NOLINT(readability-identifier-naming)

    void joinChannel(const QString &id);
    void partChannel(const QString &id);

protected:
    void onMessage(
        websocketpp::connection_hdl hdl,
        BasicPubSubManager<BttvLiveUpdateSubscription>::WebsocketMessagePtr msg)
        override;

private:
    std::unordered_set<QString> joinedChannels_;
};

}  // namespace chatterino
