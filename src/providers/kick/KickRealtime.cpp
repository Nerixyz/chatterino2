#include "providers/kick/KickRealtime.hpp"

#include "common/QLogging.hpp"
#include "messages/MessageBuilder.hpp"
#include "providers/kick/KickChannel.hpp"
#include "providers/kick/KickMessage.hpp"
#include "providers/kick/KickMessageBuilder.hpp"
#include "providers/kick/pusher/PusherMessage.hpp"
#include "providers/kick/pusher/PusherSocket.hpp"

#include <memory>

namespace {

std::optional<size_t> subToChannelID(QStringView sub)
{
    if (!sub.startsWith(QStringLiteral("chatrooms.")) ||
        !sub.endsWith(QStringLiteral(".v2")))
    {
        return std::nullopt;
    }
    sub = sub.sliced(10);
    sub = sub.first(sub.length() - 3);

    bool ok = false;
    size_t n = sub.toULongLong(&ok);

    if (!ok)
    {
        return std::nullopt;
    }
    return n;
}

}  // namespace

namespace chatterino {

KickRealtime::KickRealtime()
    : socket_(QUrl(QStringLiteral("wss://ws-us2.pusher.com/app/"
                                  "eb1d5f283081a78b932c?protocol=7&client=js&"
                                  "version=7.6.0&flash=false")))
{
    QObject::connect(&this->socket_, &PusherSocket::connectionEstablished, this,
                     &KickRealtime::pusherConnected);
    QObject::connect(&this->socket_, &PusherSocket::disconnected, this,
                     &KickRealtime::pusherDisconnected);
    QObject::connect(&this->socket_, &PusherSocket::messageReceived, this,
                     &KickRealtime::pusherMessageReceived);
    QObject::connect(&this->socket_, &PusherSocket::subscriptionSucceeded, this,
                     &KickRealtime::pusherSubSucceeded);
}

KickRealtime::~KickRealtime() = default;

ChannelPtr KickRealtime::getOrAddChannel(size_t id)
{
    auto it = this->channels_.find(id);
    if (it != this->channels_.end())
    {
        auto locked = it->lock();
        if (locked)
        {
            return locked;
        }
    }

    this->socket_.subscribe(QStringLiteral("chatrooms.") + QString::number(id) +
                            QStringLiteral(".v2"));
    auto chan = std::make_shared<KickChannel>(id);
    this->channels_.insert(id, chan);
    return chan;
}

void KickRealtime::forEachChannel(std::function<void(KickChannel &)> fn)
{
    for (const auto &c : this->channels_)
    {
        auto locked = c.lock();
        if (!locked)
        {
            continue;
        }
        auto *chan = dynamic_cast<KickChannel *>(locked.get());
        if (chan)
        {
            fn(*chan);
        }
    }
}

std::shared_ptr<KickChannel> KickRealtime::kickChannel(size_t id)
{
    auto it = this->channels_.find(id);
    if (it != this->channels_.end())
    {
        auto locked = it->lock();
        if (locked)
        {
            return std::dynamic_pointer_cast<KickChannel>(locked);
        }
    }
    return {};
}

void KickRealtime::pusherMessageReceived(PusherMessage &msg)
{
    if (msg.event != QStringLiteral("App\\Events\\ChatMessageEvent"))
    {
        return;
    }
    auto chanID = subToChannelID(msg.channel);
    if (!chanID)
    {
        return;
    }
    auto chan = this->kickChannel(*chanID);
    if (!chan)
    {
        return;
    }

    auto chat = KickMessage::parse(std::move(msg));
    if (!chat)
    {
        qCWarning(chatterinoKick) << "Failed to parse kick message";
        return;
    }
    chan->addMessage(KickMessageBuilder(chan.get(), *chat).build());
}

void KickRealtime::pusherConnected(const QString & /*socketID*/)
{
    this->forEachChannel([](auto &chan) {
        chan.addMessage(makeSystemMessage("Connected"));
    });
}

void KickRealtime::pusherDisconnected()
{
    this->forEachChannel([](auto &chan) {
        chan.addMessage(makeSystemMessage("Disconnected"));
    });
}

void KickRealtime::pusherSubSucceeded(const QString &sub)
{
    auto id = subToChannelID(sub);
    if (id)
    {
        this->withChannel(*id, [](auto &chan) {
            chan.addMessage(makeSystemMessage("Joined"));
        });
    }
}

}  // namespace chatterino
