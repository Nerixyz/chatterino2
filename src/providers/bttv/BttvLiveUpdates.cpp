#include "providers/bttv/BttvLiveUpdates.hpp"

#include <QJsonDocument>

namespace chatterino {

BttvLiveUpdates::BttvLiveUpdates(const QString &host)
    : BasicPubSubManager(host)
{
}

void BttvLiveUpdates::joinChannel(const QString &id)
{
    if (this->joinedChannels_.insert(id).second)
    {
        this->subscribe({id, BttvLiveUpdateSubscriptionType::Channel});
    }
}

void BttvLiveUpdates::partChannel(const QString &id)
{
    if (this->joinedChannels_.erase(id) > 0)
    {
        this->unsubscribe({id, BttvLiveUpdateSubscriptionType::Channel});
    }
}

void BttvLiveUpdates::onMessage(
    websocketpp::connection_hdl /*hdl*/,
    BasicPubSubManager<BttvLiveUpdateSubscription>::WebsocketMessagePtr msg)
{
    const auto &payload = QString::fromStdString(msg->get_payload());
    QJsonDocument jsonDoc(QJsonDocument::fromJson(payload.toUtf8()));

    if (jsonDoc.isNull())
    {
        qCDebug(chatterinoBttv) << "Failed to parse live update JSON";
        return;
    }
    auto json = jsonDoc.object();
    auto eventType = json["name"].toString();
    auto eventData = json["data"].toObject();

    if (eventType == "emote_create")
    {
        this->signals_.emoteAdded.invoke(
            BttvLiveUpdateEmoteAddMessage(eventData));
    }
    else if (eventType == "emote_delete")
    {
        this->signals_.emoteRemoved.invoke(
            BttvLiveUpdateEmoteRemoveMessage(eventData));
    }
    else
    {
        qCDebug(chatterinoBttv) << "Unhandled event:" << json;
    }
}

}  // namespace chatterino
