#include "providers/bttv/liveupdate/BttvLiveUpdateMessages.hpp"

namespace chatterino {

BttvLiveUpdateEmoteAddMessage::BttvLiveUpdateEmoteAddMessage(
    const QJsonObject &json)
{
    auto channel = json["channel"].toString();
    if (channel.length() > 7)
    {
        channel.remove(0, 7);  // "twitch:"
    }
    this->channelId = channel;
    this->jsonEmote = json["emote"].toObject();
    this->emoteName = this->jsonEmote["code"].toString();
    this->emoteId = this->jsonEmote["id"].toString();
}

BttvLiveUpdateEmoteRemoveMessage::BttvLiveUpdateEmoteRemoveMessage(
    const QJsonObject &json)
{
    auto channel = json["channel"].toString();
    if (channel.length() > 7)
    {
        channel.remove(0, 7);  // "twitch:"
    }
    this->channelId = channel;
    this->emoteId = json["emoteId"].toString();
}

}  // namespace chatterino
