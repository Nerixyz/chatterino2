#include "providers/kick/pusher/PusherSocket.hpp"

#include "common/QLogging.hpp"
#include "providers/kick/pusher/PusherMessage.hpp"
#include "providers/kick/util/Deserialize.hpp"
#include "providers/kick/util/Serialize.hpp"

#include <rapidjson/document.h>
#include <rapidjson/encodings.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stream.h>
#include <rapidjson/writer.h>

namespace chatterino {

using Utf16 = rapidjson::UTF16<char16_t>;
using Utf16Doc = rapidjson::GenericDocument<Utf16>;
using Utf16Val = rapidjson::GenericValue<Utf16>;

PusherSocket::PusherSocket(QUrl url, QObject *parent)
    : QObject(parent)
    , url_(std::move(url))

{
    this->ws_.open(this->url_);
    QObject::connect(&this->ws_, &QWebSocket::connected, this,
                     &PusherSocket::wsConnected);
    QObject::connect(&this->ws_, &QWebSocket::disconnected, this,
                     &PusherSocket::wsDisconnected);
    QObject::connect(&this->ws_, &QWebSocket::textMessageReceived, this,
                     &PusherSocket::textMessageReceived);
}

void PusherSocket::subscribe(const QString &channel, const QString &auth)
{
    if (this->confirmedChannels_.contains(channel))
    {
        return;
    }

    Utf16Doc doc(rapidjson::kObjectType);
    auto &alloc = doc.GetAllocator();
    doc.AddMember(u"event", u"pusher:subscribe", alloc);

    Utf16Val data(rapidjson::kObjectType);
    data.AddMember(u"auth", ser::stringRef(auth), alloc);
    data.AddMember(u"channel", ser::stringRef(channel), alloc);

    doc.AddMember(u"data", std::move(data), alloc);

    QString out;
    ser::QStringWrapper ser(out);
    rapidjson::Writer<ser::QStringWrapper, Utf16, Utf16> writer(ser);
    doc.Accept(writer);

    this->unconfirmedChannels_.insert(channel);
    this->sendOrQueue(out);
}

void PusherSocket::reconnect()
{
    if (this->ws_.state() == QAbstractSocket::ClosingState)
    {
        return;
    }

    if (this->ws_.state() == QAbstractSocket::UnconnectedState)
    {
        this->ws_.open(this->url_);
    }
    else
    {
        this->ws_.close();
    }
}

void PusherSocket::wsConnected()
{
}

void PusherSocket::wsDisconnected()
{
    // TODO: add timeout
    this->confirmedChannels_.clear();
    this->unconfirmedChannels_.clear();

    emit this->disconnected();

    this->ws_.open(this->url_);
}

void PusherSocket::textMessageReceived(const QString &string)
{
    auto msg = PusherMessage::parse(QString(string));
    if (!msg)
    {
        qCWarning(chatterinoPusher)
            << "Failed to parse message - original: " << string;
        return;
    }
    auto &parsedMsg = *msg;

    // check if it's an internal message and handle that first
    if (parsedMsg.event == QStringLiteral(u"pusher:connection_established"))
    {
        const auto *data = de::child(parsedMsg.doc, u"data");
        if (data == nullptr || !data->IsString())
        {
            return;
        }
        Utf16Doc inner;
        inner.ParseInsitu(const_cast<char16_t *>(data->GetString()));
        if (inner.HasParseError() || !inner.IsObject())
        {
            qCWarning(chatterinoPusher)
                << "Failed to parse connection established";
            return;
        }

        QStringView socketID;
        de::get(inner, u"socket_id", socketID);
        de::get(inner, u"activity_timeout", this->activityTimeout_);
        this->socketID_ = socketID.toString();

        // flush queue
        for (const auto &pending : this->messageBacklog_)
        {
            this->ws_.sendTextMessage(pending);
        }
        this->messageBacklog_.clear();

        emit this->connectionEstablished(this->socketID_);
        return;
    }

    if (parsedMsg.event ==
        QStringLiteral(u"pusher_internal:subscription_succeeded"))
    {
        auto channel = parsedMsg.channel.toString();

        this->unconfirmedChannels_.remove(channel);
        this->confirmedChannels_.insert(channel);

        emit this->subscriptionSucceeded(channel);
        return;
    }

    emit this->messageReceived(parsedMsg);
}

void PusherSocket::sendOrQueue(const QString &msg)
{
    if (this->ws_.state() == QAbstractSocket::ConnectedState)
    {
        this->ws_.sendTextMessage(msg);
    }
    else
    {
        this->messageBacklog_.append(msg);
    }
}

}  // namespace chatterino
