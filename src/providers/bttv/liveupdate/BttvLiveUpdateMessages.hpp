#pragma once

#include <QJsonObject>

namespace chatterino {

struct BttvLiveUpdateEmoteAddMessage {
    BttvLiveUpdateEmoteAddMessage(const QJsonObject &json);

    QString channelId;

    QJsonObject jsonEmote;
    QString emoteName;
    QString emoteId;
};

struct BttvLiveUpdateEmoteRemoveMessage {
    BttvLiveUpdateEmoteRemoveMessage(const QJsonObject &json);

    QString channelId;
    QString emoteId;
};

}  // namespace chatterino
