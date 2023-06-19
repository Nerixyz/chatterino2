#include "providers/kick/pusher/PusherMessage.hpp"

#include "providers/kick/util/Deserialize.hpp"

namespace chatterino {

std::optional<PusherMessage> PusherMessage::parse(QString &&backingMessage)
{
    PusherMessage msg(std::move(backingMessage));
    msg.backingMessage_.append(QChar(u'\0'));

    msg.doc.ParseInsitu((char16_t *)(msg.backingMessage_.data()));
    if (msg.doc.HasParseError())
    {
        return std::nullopt;
    }
    if (!msg.doc.IsObject())
    {
        return std::nullopt;
    }

    if (!de::get(msg.doc, u"event", msg.event))
    {
        return std::nullopt;
    }
    de::get(msg.doc, u"channel", msg.channel);

    return std::move(msg);
}

}  // namespace chatterino

QDebug operator<<(QDebug debug, const chatterino::PusherMessage &m)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << "PusherMessage(event: " << m.event
                    << ", channel: " << m.channel << ")";
    return debug;
}
