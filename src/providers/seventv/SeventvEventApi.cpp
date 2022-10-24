#include "providers/seventv/SeventvEventApi.hpp"

#include "providers/seventv/eventapi/SeventvEventApiClient.hpp"
#include "providers/seventv/eventapi/SeventvEventApiMessage.hpp"

namespace chatterino {

SeventvEventApi::SeventvEventApi(const QString &host)
    : BasicPubSubManager(host)
{
}

void SeventvEventApi::subscribeUser(const QString &userId,
                                    const QString &emoteSetId)
{
    if (this->subscribedUsers_.insert(userId).second)
    {
        this->subscribe({userId, SeventvEventApiSubscriptionType::UpdateUser});
    }
    if (this->subscribedEmoteSets_.insert(emoteSetId).second)
    {
        this->subscribe(
            {emoteSetId, SeventvEventApiSubscriptionType::UpdateEmoteSet});
    }
}

void SeventvEventApi::unsubscribeEmoteSet(const QString &id)
{
    if (this->subscribedEmoteSets_.erase(id) > 0)
    {
        this->unsubscribe(
            {id, SeventvEventApiSubscriptionType::UpdateEmoteSet});
    }
}

void SeventvEventApi::unsubscribeUser(const QString &id)
{
    if (this->subscribedUsers_.erase(id) > 0)
    {
        this->unsubscribe({id, SeventvEventApiSubscriptionType::UpdateUser});
    }
}

std::shared_ptr<BasicPubSubClient<SeventvEventApiSubscription>>
    SeventvEventApi::createClient(liveupdates::WebsocketClient &client,
                                  websocketpp::connection_hdl hdl)
{
    auto shared = std::make_shared<SeventvEventApiClient>(client, hdl);
    return std::static_pointer_cast<
        BasicPubSubClient<SeventvEventApiSubscription>>(std::move(shared));
}

void SeventvEventApi::onMessage(
    websocketpp::connection_hdl hdl,
    BasicPubSubManager<SeventvEventApiSubscription>::WebsocketMessagePtr msg)
{
    const auto &payload = QString::fromStdString(msg->get_payload());

    auto pMessage = parseSeventvEventApiBaseMessage(payload);

    if (!pMessage)
    {
        qCDebug(chatterinoSeventvEventApi)
            << "Unable to parse incoming event-api message: " << payload;
        return;
    }
    auto message = *pMessage;
    switch (message.op)
    {
        case SeventvEventApiOpcode::Hello: {
            if (auto client = this->findClient(hdl))
            {
                if (auto *stvClient =
                        dynamic_cast<SeventvEventApiClient *>(client.get()))
                {
                    stvClient->setHeartbeatInterval(
                        message.data["heartbeat_interval"].toInt());
                }
            }
        }
        break;
        case SeventvEventApiOpcode::Heartbeat: {
            if (auto client = this->findClient(hdl))
            {
                if (auto *stvClient =
                        dynamic_cast<SeventvEventApiClient *>(client.get()))
                {
                    stvClient->handleHeartbeat();
                }
            }
        }
        break;
        case SeventvEventApiOpcode::Dispatch: {
            auto dispatch = message.toInner<SeventvEventApiDispatch>();
            if (!dispatch)
            {
                qCDebug(chatterinoSeventvEventApi)
                    << "Malformed dispatch" << payload;
                return;
            }
            this->handleDispatch(*dispatch);
        }
        break;
        default: {
            qCDebug(chatterinoSeventvEventApi) << "Unhandled op: " << payload;
        }
        break;
    }
}

void SeventvEventApi::handleDispatch(const SeventvEventApiDispatch &dispatch)
{
    switch (dispatch.type)
    {
        case SeventvEventApiSubscriptionType::UpdateEmoteSet: {
            for (const auto pushed_ : dispatch.body["pushed"].toArray())
            {
                auto pushed = pushed_.toObject();
                if (pushed["key"].toString() != "emotes")
                {
                    continue;
                }
                SeventvEventApiEmoteAddDispatch added(
                    dispatch, pushed["value"].toObject());
                this->signals_.emoteAdded.invoke(added);
            }
            for (const auto updated_ : dispatch.body["updated"].toArray())
            {
                auto updated = updated_.toObject();
                if (updated["key"].toString() != "emotes")
                {
                    continue;
                }
                SeventvEventApiEmoteUpdateDispatch update(dispatch, updated);
                if (update.emoteName != update.oldEmoteName)
                {
                    this->signals_.emoteUpdated.invoke(update);
                }
            }
            for (const auto pulled_ : dispatch.body["pulled"].toArray())
            {
                auto pulled = pulled_.toObject();
                if (pulled["key"].toString() != "emotes")
                {
                    continue;
                }
                SeventvEventApiEmoteRemoveDispatch removed(
                    dispatch, pulled["old_value"].toObject());
                this->signals_.emoteRemoved.invoke(removed);
            }
        }
        break;
        case SeventvEventApiSubscriptionType::UpdateUser: {
            for (const auto updated_ : dispatch.body["updated"].toArray())
            {
                auto updated = updated_.toObject();
                if (updated["key"].toString() != "connections")
                {
                    continue;
                }
                for (const auto value_ : updated["value"].toArray())
                {
                    auto value = value_.toObject();
                    if (value["key"].toString() != "emote_set")
                    {
                        continue;
                    }
                    SeventvEventApiUserConnectionUpdateDispatch update(dispatch,
                                                                       value);
                    this->signals_.userUpdated.invoke(update);
                }
            }
        }
        break;
        default: {
            qCDebug(chatterinoSeventvEventApi)
                << "Unknown subscription type:" << (int)dispatch.type
                << "body:" << dispatch.body;
        }
        break;
    }
}

}  // namespace chatterino
