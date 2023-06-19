#pragma once

#include "common/Singleton.hpp"
#include "providers/kick/pusher/PusherSocket.hpp"

#include <QHash>
#include <QObject>

#include <memory>

namespace chatterino {

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;
struct PusherMessage;
class KickChannel;
class KickRealtime : public QObject, public Singleton
{
    Q_OBJECT

public:
    KickRealtime();
    ~KickRealtime() override;

    ChannelPtr getOrAddChannel(size_t id);

    void forEachChannel(std::function<void(KickChannel &)> fn);

private slots:
    void pusherMessageReceived(PusherMessage &msg);
    void pusherConnected(const QString &socketID);
    void pusherDisconnected();
    void pusherSubSucceeded(const QString &sub);

private:
    std::shared_ptr<KickChannel> kickChannel(size_t id);

    PusherSocket socket_;

    QHash<size_t, std::weak_ptr<Channel>> channels_;

    template <typename Fn>
    void withChannel(size_t id, Fn fn)
    {
        auto chan = this->kickChannel(id);
        if (chan)
        {
            fn(*chan);
        }
    }
};

}  // namespace chatterino