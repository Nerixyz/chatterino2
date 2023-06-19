#include "providers/kick/KickMessage.hpp"

#include "common/QLogging.hpp"
#include "providers/kick/util/Deserialize.hpp"

namespace chatterino {

KickMessage::KickMessage(PusherMessage &&backingMessage)
    : backingMessage_(std::move(backingMessage))
{
}

std::optional<KickMessage> KickMessage::parse(PusherMessage &&msg)
{
    KickMessage out(std::move(msg));
    const auto *dataStr = de::child(out.backingMessage_.doc, u"data");
    if (dataStr == nullptr || !dataStr->IsString())
    {
        return std::nullopt;
    }
    out.innerDoc_.ParseInsitu(const_cast<char16_t *>(dataStr->GetString()));
    if (out.innerDoc_.HasParseError() || !out.innerDoc_.IsObject())
    {
        qCWarning(chatterinoKick) << "Failed to parse inner message data";
        return std::nullopt;
    }

    const auto &data = out.innerDoc_;

    const auto *sender = de::child(data, u"sender");
    if (sender == nullptr)
    {
        return std::nullopt;
    }
    const auto *senderIdent = de::child(*sender, u"identity");
    if (senderIdent == nullptr)
    {
        return std::nullopt;
    }

    QStringView createdAt;

    bool ok = de::get(data, u"id", out.id_) &&
              de::get(data, u"chatroom_id", out.chatroomID_) &&
              de::get(data, u"created_at", createdAt) &&
              de::get(data, u"content", out.content_) &&
              de::get(*sender, u"username", out.sender_.username) &&
              de::get(*sender, u"slug", out.sender_.slug) &&
              de::get(*senderIdent, u"color", out.sender_.color);
    if (!ok)
    {
        return std::nullopt;
    }

    out.createdAt_ = QDateTime::fromString(createdAt, Qt::ISODate);

    return std::move(out);
}

}  // namespace chatterino
