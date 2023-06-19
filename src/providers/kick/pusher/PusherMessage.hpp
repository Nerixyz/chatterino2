#pragma once

#include <QDebug>
#include <QString>
#include <rapidjson/document.h>
#include <rapidjson/encodings.h>

namespace chatterino {

struct PusherMessage {
    using Encoding = rapidjson::UTF16<char16_t>;
    using Doc = rapidjson::GenericDocument<Encoding>;
    using Value = rapidjson::GenericValue<Encoding>;

    PusherMessage(PusherMessage &&other)
        : channel(other.channel)
        , event(other.event)
        , doc(std::move(other.doc))
        , backingMessage_(std::move(other.backingMessage_))

    {
    }

    PusherMessage &operator=(PusherMessage &&other)
    {
        this->channel = other.channel;
        this->event = other.event;
        this->doc = std::move(other.doc);
        this->backingMessage_ = std::move(other.backingMessage_);
        return *this;
    }

    QStringView channel;
    QStringView event;
    Doc doc;

    static std::optional<PusherMessage> parse(QString &&backingMessage);

private:
    QString backingMessage_;

    PusherMessage(QString &&backingMessage)
        : backingMessage_(std::move(backingMessage))
    {
    }
};

}  // namespace chatterino

QDebug operator<<(QDebug debug, const chatterino::PusherMessage &m);
