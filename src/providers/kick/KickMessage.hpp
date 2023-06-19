#pragma once

#include "providers/kick/pusher/PusherMessage.hpp"

#include <QDateTime>
#include <QString>

#include <optional>

namespace chatterino {

class KickMessage
{
public:
    enum class Type : uint8_t { Message };
    struct Sender {
        QStringView username;
        QStringView slug;
        QStringView color;
    };

    static std::optional<KickMessage> parse(PusherMessage &&msg);

    QStringView id() const
    {
        return this->id_;
    }
    size_t chatroomID() const
    {
        return this->chatroomID_;
    }
    QStringView content() const
    {
        return this->content_;
    }
    const Sender &sender() const
    {
        return this->sender_;
    }
    QDateTime createdAt() const
    {
        return this->createdAt_;
    }

private:
    KickMessage(PusherMessage &&backingMessage);

    QStringView id_;
    size_t chatroomID_ = 0;
    QStringView content_;
    Sender sender_;
    QDateTime createdAt_;

    PusherMessage backingMessage_;
    PusherMessage::Doc innerDoc_;
};

}  // namespace chatterino
