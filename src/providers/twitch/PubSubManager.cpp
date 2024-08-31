#include "providers/twitch/PubSubManager.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/twitch/PubSubActions.hpp"
#include "providers/twitch/PubSubHelpers.hpp"
#include "providers/twitch/PubSubMessages.hpp"
#include "providers/twitch/TwitchAccount.hpp"
#include "providers/websocket/PubSubPool.hpp"

#include <QJsonArray>
#include <QScopeGuard>

#include <memory>

using namespace std::chrono_literals;

namespace chatterino {

PubSub::PubSub(const QString &host, std::chrono::seconds pingInterval)
    : host_(host)
    , pool_(std::make_shared<PubSubPool>(ws::Endpoint::fromUrl(host),
                                         PubSubClientOptions{pingInterval}))
{
    // XXX: this should only run in tests
    this->pool_->initDiagnostics();

    this->pool_->setMessageHandler(
        [this](const auto &topic, const auto &message) {
            this->handleMessageResponse({
                .topic = topic,
                .messageObject = message,
            });
        });
    this->pool_->setTokenProvider([this] {
        return this->token_;
    });

    this->moderationActionHandlers["clear"] = [this](const auto &data,
                                                     const auto &roomID) {
        ClearChatAction action(data, roomID);

        this->moderation.chatCleared.invoke(action);
    };

    this->moderationActionHandlers["slowoff"] = [this](const auto &data,
                                                       const auto &roomID) {
        ModeChangedAction action(data, roomID);

        action.mode = ModeChangedAction::Mode::Slow;
        action.state = ModeChangedAction::State::Off;

        this->moderation.modeChanged.invoke(action);
    };

    this->moderationActionHandlers["slow"] = [this](const auto &data,
                                                    const auto &roomID) {
        ModeChangedAction action(data, roomID);

        action.mode = ModeChangedAction::Mode::Slow;
        action.state = ModeChangedAction::State::On;

        const auto args = data.value("args").toArray();

        if (args.empty())
        {
            qCDebug(chatterinoPubSub)
                << "Missing duration argument in slowmode on";
            return;
        }

        bool ok;

        action.duration = args.at(0).toString().toUInt(&ok, 10);

        this->moderation.modeChanged.invoke(action);
    };

    this->moderationActionHandlers["r9kbetaoff"] = [this](const auto &data,
                                                          const auto &roomID) {
        ModeChangedAction action(data, roomID);

        action.mode = ModeChangedAction::Mode::R9K;
        action.state = ModeChangedAction::State::Off;

        this->moderation.modeChanged.invoke(action);
    };

    this->moderationActionHandlers["r9kbeta"] = [this](const auto &data,
                                                       const auto &roomID) {
        ModeChangedAction action(data, roomID);

        action.mode = ModeChangedAction::Mode::R9K;
        action.state = ModeChangedAction::State::On;

        this->moderation.modeChanged.invoke(action);
    };

    this->moderationActionHandlers["subscribersoff"] =
        [this](const auto &data, const auto &roomID) {
            ModeChangedAction action(data, roomID);

            action.mode = ModeChangedAction::Mode::SubscribersOnly;
            action.state = ModeChangedAction::State::Off;

            this->moderation.modeChanged.invoke(action);
        };

    this->moderationActionHandlers["subscribers"] = [this](const auto &data,
                                                           const auto &roomID) {
        ModeChangedAction action(data, roomID);

        action.mode = ModeChangedAction::Mode::SubscribersOnly;
        action.state = ModeChangedAction::State::On;

        this->moderation.modeChanged.invoke(action);
    };

    this->moderationActionHandlers["emoteonlyoff"] =
        [this](const auto &data, const auto &roomID) {
            ModeChangedAction action(data, roomID);

            action.mode = ModeChangedAction::Mode::EmoteOnly;
            action.state = ModeChangedAction::State::Off;

            this->moderation.modeChanged.invoke(action);
        };

    this->moderationActionHandlers["emoteonly"] = [this](const auto &data,
                                                         const auto &roomID) {
        ModeChangedAction action(data, roomID);

        action.mode = ModeChangedAction::Mode::EmoteOnly;
        action.state = ModeChangedAction::State::On;

        this->moderation.modeChanged.invoke(action);
    };

    this->moderationActionHandlers["unmod"] = [this](const auto &data,
                                                     const auto &roomID) {
        ModerationStateAction action(data, roomID);

        action.target.id = data.value("target_user_id").toString();

        const auto args = data.value("args").toArray();

        if (args.isEmpty())
        {
            return;
        }

        action.target.login = args[0].toString();

        action.modded = false;

        this->moderation.moderationStateChanged.invoke(action);
    };

    this->moderationActionHandlers["mod"] = [this](const auto &data,
                                                   const auto &roomID) {
        ModerationStateAction action(data, roomID);
        action.modded = true;

        auto innerType = data.value("type").toString();
        if (innerType == "chat_login_moderation")
        {
            // Don't display the old message type
            return;
        }

        action.target.id = data.value("target_user_id").toString();
        action.target.login = data.value("target_user_login").toString();

        this->moderation.moderationStateChanged.invoke(action);
    };

    this->moderationActionHandlers["timeout"] = [this](const auto &data,
                                                       const auto &roomID) {
        BanAction action(data, roomID);

        action.source.id = data.value("created_by_user_id").toString();
        action.source.login = data.value("created_by").toString();

        action.target.id = data.value("target_user_id").toString();

        const auto args = data.value("args").toArray();

        if (args.size() < 2)
        {
            return;
        }

        action.target.login = args[0].toString();
        bool ok;
        action.duration = args[1].toString().toUInt(&ok, 10);
        action.reason = args[2].toString();  // May be omitted

        this->moderation.userBanned.invoke(action);
    };

    this->moderationActionHandlers["delete"] = [this](const auto &data,
                                                      const auto &roomID) {
        DeleteAction action(data, roomID);

        action.source.id = data.value("created_by_user_id").toString();
        action.source.login = data.value("created_by").toString();

        action.target.id = data.value("target_user_id").toString();

        const auto args = data.value("args").toArray();

        if (args.size() < 3)
        {
            return;
        }

        action.target.login = args[0].toString();
        action.messageText = args[1].toString();
        action.messageId = args[2].toString();

        this->moderation.messageDeleted.invoke(action);
    };

    this->moderationActionHandlers["ban"] = [this](const auto &data,
                                                   const auto &roomID) {
        BanAction action(data, roomID);

        action.source.id = data.value("created_by_user_id").toString();
        action.source.login = data.value("created_by").toString();

        action.target.id = data.value("target_user_id").toString();

        const auto args = data.value("args").toArray();

        if (args.isEmpty())
        {
            return;
        }

        action.target.login = args[0].toString();
        action.reason = args[1].toString();  // May be omitted

        this->moderation.userBanned.invoke(action);
    };

    this->moderationActionHandlers["unban"] = [this](const auto &data,
                                                     const auto &roomID) {
        UnbanAction action(data, roomID);

        action.source.id = data.value("created_by_user_id").toString();
        action.source.login = data.value("created_by").toString();

        action.target.id = data.value("target_user_id").toString();

        action.previousState = UnbanAction::Banned;

        const auto args = data.value("args").toArray();

        if (args.isEmpty())
        {
            return;
        }

        action.target.login = args[0].toString();

        this->moderation.userUnbanned.invoke(action);
    };

    this->moderationActionHandlers["untimeout"] = [this](const auto &data,
                                                         const auto &roomID) {
        UnbanAction action(data, roomID);

        action.source.id = data.value("created_by_user_id").toString();
        action.source.login = data.value("created_by").toString();

        action.target.id = data.value("target_user_id").toString();

        action.previousState = UnbanAction::TimedOut;

        const auto args = data.value("args").toArray();

        if (args.isEmpty())
        {
            return;
        }

        action.target.login = args[0].toString();

        this->moderation.userUnbanned.invoke(action);
    };

    this->moderationActionHandlers["warn"] = [this](const auto &data,
                                                    const auto &roomID) {
        WarnAction action(data, roomID);

        action.source.id = data.value("created_by_user_id").toString();
        action.source.login =
            data.value("created_by").toString();  // currently always empty

        action.target.id = data.value("target_user_id").toString();
        action.target.login = data.value("target_user_login").toString();

        const auto reasons = data.value("args").toArray();
        bool firstArg = true;
        for (const auto &reasonValue : reasons)
        {
            if (firstArg)
            {
                // Skip first arg in the reasons array since it's not a reason
                firstArg = false;
                continue;
            }
            const auto &reason = reasonValue.toString();
            if (!reason.isEmpty())
            {
                action.reasons.append(reason);
            }
        }

        this->moderation.userWarned.invoke(action);
    };

    this->moderationActionHandlers["raid"] = [this](const auto &data,
                                                    const auto &roomID) {
        RaidAction action(data, roomID);

        action.source.id = data.value("created_by_user_id").toString();
        action.source.login = data.value("created_by").toString();

        const auto args = data.value("args").toArray();

        if (args.isEmpty())
        {
            return;
        }

        action.target = args[0].toString();

        this->moderation.raidStarted.invoke(action);
    };

    this->moderationActionHandlers["unraid"] = [this](const auto &data,
                                                      const auto &roomID) {
        UnraidAction action(data, roomID);

        action.source.id = data.value("created_by_user_id").toString();
        action.source.login = data.value("created_by").toString();

        this->moderation.raidCanceled.invoke(action);
    };

    this->moderationActionHandlers["automod_message_rejected"] =
        [this](const auto &data, const auto &roomID) {
            AutomodInfoAction action(data, roomID);
            action.type = AutomodInfoAction::OnHold;
            this->moderation.automodInfoMessage.invoke(action);
        };

    this->moderationActionHandlers["automod_message_denied"] =
        [this](const auto &data, const auto &roomID) {
            AutomodInfoAction action(data, roomID);
            action.type = AutomodInfoAction::Denied;
            this->moderation.automodInfoMessage.invoke(action);
        };

    this->moderationActionHandlers["automod_message_approved"] =
        [this](const auto &data, const auto &roomID) {
            AutomodInfoAction action(data, roomID);
            action.type = AutomodInfoAction::Approved;
            this->moderation.automodInfoMessage.invoke(action);
        };

    this->channelTermsActionHandlers["add_permitted_term"] =
        [this](const auto &data, const auto &roomID) {
            // This term got a pass through automod
            AutomodUserAction action(data, roomID);
            action.source.id = data.value("created_by_user_id").toString();
            action.source.login = data.value("created_by").toString();

            action.type = AutomodUserAction::AddPermitted;
            action.message = data.value("text").toString();
            action.source.login = data.value("requester_login").toString();

            this->moderation.automodUserMessage.invoke(action);
        };

    this->channelTermsActionHandlers["add_blocked_term"] =
        [this](const auto &data, const auto &roomID) {
            // A term has been added
            AutomodUserAction action(data, roomID);
            action.source.id = data.value("created_by_user_id").toString();
            action.source.login = data.value("created_by").toString();

            action.type = AutomodUserAction::AddBlocked;
            action.message = data.value("text").toString();
            action.source.login = data.value("requester_login").toString();

            this->moderation.automodUserMessage.invoke(action);
        };

    this->moderationActionHandlers["delete_permitted_term"] =
        [this](const auto &data, const auto &roomID) {
            // This term got deleted
            AutomodUserAction action(data, roomID);
            action.source.id = data.value("created_by_user_id").toString();
            action.source.login = data.value("created_by").toString();

            const auto args = data.value("args").toArray();
            action.type = AutomodUserAction::RemovePermitted;

            if (args.isEmpty())
            {
                return;
            }

            action.message = args[0].toString();

            this->moderation.automodUserMessage.invoke(action);
        };

    this->channelTermsActionHandlers["delete_permitted_term"] =
        [this](const auto &data, const auto &roomID) {
            // This term got deleted
            AutomodUserAction action(data, roomID);
            action.source.id = data.value("created_by_user_id").toString();
            action.source.login = data.value("created_by").toString();

            action.type = AutomodUserAction::RemovePermitted;
            action.message = data.value("text").toString();
            action.source.login = data.value("requester_login").toString();

            this->moderation.automodUserMessage.invoke(action);
        };

    this->moderationActionHandlers["delete_blocked_term"] =
        [this](const auto &data, const auto &roomID) {
            // This term got deleted
            AutomodUserAction action(data, roomID);

            action.source.id = data.value("created_by_user_id").toString();
            action.source.login = data.value("created_by").toString();

            const auto args = data.value("args").toArray();
            action.type = AutomodUserAction::RemoveBlocked;

            if (args.isEmpty())
            {
                return;
            }

            action.message = args[0].toString();

            this->moderation.automodUserMessage.invoke(action);
        };
    this->channelTermsActionHandlers["delete_blocked_term"] =
        [this](const auto &data, const auto &roomID) {
            // This term got deleted
            AutomodUserAction action(data, roomID);

            action.source.id = data.value("created_by_user_id").toString();
            action.source.login = data.value("created_by").toString();

            action.type = AutomodUserAction::RemoveBlocked;
            action.message = data.value("text").toString();
            action.source.login = data.value("requester_login").toString();

            this->moderation.automodUserMessage.invoke(action);
        };

    this->moderationActionHandlers["denied_automod_message"] =
        [](const auto &data, const auto &roomID) {
            // This message got denied by a moderator
            // qCDebug(chatterinoPubSub) << rj::stringify(data);
        };

    this->moderationActionHandlers["approved_automod_message"] =
        [](const auto &data, const auto &roomID) {
            // This message got approved by a moderator
            // qCDebug(chatterinoPubSub) << rj::stringify(data);
        };
}

PubSub::~PubSub()
{
    this->stop();
}

void PubSub::initialize()
{
    this->setAccount(getApp()->getAccounts()->twitch.getCurrent());
    this->start();

    getApp()->getAccounts()->twitch.currentUserChanged.connect(
        [this] {
            this->unlistenChannelModerationActions();
            this->unlistenAutomod();
            this->unlistenLowTrustUsers();
            this->unlistenChannelPointRewards();

            this->setAccount(getApp()->getAccounts()->twitch.getCurrent());
        },
        boost::signals2::at_front);
}

void PubSub::setAccount(std::shared_ptr<TwitchAccount> account)
{
    this->token_ = account->getOAuthToken();
    this->userID_ = account->getUserId();
}

void PubSub::start()
{
    this->pool_->run();
}

void PubSub::stop()
{
    this->pool_->shutdown();
}

bool PubSub::listenToWhispers()
{
    if (this->userID_.isEmpty())
    {
        qCDebug(chatterinoPubSub)
            << "Unable to listen to whispers topic, no user logged in";
        return false;
    }

    static const QString topicFormat("whispers.%1");
    auto topic = topicFormat.arg(this->userID_);

    qCDebug(chatterinoPubSub) << "Listen to whispers" << topic;

    this->pool_->subscribe(topic);

    return true;
}

void PubSub::unlistenWhispers()
{
    this->pool_->unsubscribePrefix("whispers.");
}

void PubSub::listenToChannelModerationActions(const QString &channelID)
{
    if (this->userID_.isEmpty())
    {
        qCDebug(chatterinoPubSub) << "Unable to listen to moderation actions "
                                     "topic, no user logged in";
        return;
    }

    static const QString topicFormat("chat_moderator_actions.%1.%2");
    assert(!channelID.isEmpty());

    auto topic = topicFormat.arg(this->userID_, channelID);

    if (this->pool_->isSubscribed(topic))
    {
        return;
    }

    qCDebug(chatterinoPubSub) << "Listen to topic" << topic;

    this->pool_->subscribe(topic);
}

void PubSub::unlistenChannelModerationActions()
{
    this->pool_->unsubscribePrefix("chat_moderator_actions.");
}

void PubSub::listenToAutomod(const QString &channelID)
{
    if (this->userID_.isEmpty())
    {
        qCDebug(chatterinoPubSub)
            << "Unable to listen to automod topic, no user logged in";
        return;
    }

    static const QString topicFormat("automod-queue.%1.%2");
    assert(!channelID.isEmpty());

    auto topic = topicFormat.arg(this->userID_, channelID);

    if (this->pool_->isSubscribed(topic))
    {
        return;
    }

    qCDebug(chatterinoPubSub) << "Listen to topic" << topic;

    this->pool_->subscribe(topic);
}

void PubSub::unlistenAutomod()
{
    this->pool_->unsubscribePrefix("automod-queue.");
}

void PubSub::listenToLowTrustUsers(const QString &channelID)
{
    if (this->userID_.isEmpty())
    {
        qCDebug(chatterinoPubSub)
            << "Unable to listen to low trust users topic, no user logged in";
        return;
    }

    static const QString topicFormat("low-trust-users.%1.%2");
    assert(!channelID.isEmpty());

    auto topic = topicFormat.arg(this->userID_, channelID);

    if (this->pool_->isSubscribed(topic))
    {
        return;
    }

    qCDebug(chatterinoPubSub) << "Listen to topic" << topic;

    this->pool_->subscribe(topic);
}

void PubSub::unlistenLowTrustUsers()
{
    this->pool_->unsubscribePrefix("low-trust-users.");
}

void PubSub::listenToChannelPointRewards(const QString &channelID)
{
    static const QString topicFormat("community-points-channel-v1.%1");
    assert(!channelID.isEmpty());

    auto topic = topicFormat.arg(channelID);

    if (this->pool_->isSubscribed(topic))
    {
        return;
    }
    qCDebug(chatterinoPubSub) << "Listen to topic" << topic;

    this->pool_->subscribe(topic);
}

void PubSub::unlistenChannelPointRewards()
{
    this->pool_->unsubscribePrefix("community-points-channel-v1.");
}

const PubSubDiagnostics *PubSub::diag() const
{
    return this->pool_->diag();
}

void PubSub::handleMessageResponse(const PubSubMessageMessage &message)
{
    QString topic = message.topic;

    if (topic.startsWith("whispers."))
    {
        auto oInnerMessage = message.toInner<PubSubWhisperMessage>();
        if (!oInnerMessage)
        {
            return;
        }
        auto whisperMessage = *oInnerMessage;

        switch (whisperMessage.type)
        {
            case PubSubWhisperMessage::Type::WhisperReceived: {
                this->whisper.received.invoke(whisperMessage);
            }
            break;
            case PubSubWhisperMessage::Type::WhisperSent: {
                this->whisper.sent.invoke(whisperMessage);
            }
            break;
            case PubSubWhisperMessage::Type::Thread: {
                // Handle thread?
            }
            break;

            case PubSubWhisperMessage::Type::INVALID:
            default: {
                qCDebug(chatterinoPubSub)
                    << "Invalid whisper type:" << whisperMessage.typeString;
            }
            break;
        }
    }
    else if (topic.startsWith("chat_moderator_actions."))
    {
        auto oInnerMessage =
            message.toInner<PubSubChatModeratorActionMessage>();
        if (!oInnerMessage)
        {
            return;
        }

        auto innerMessage = *oInnerMessage;
        auto topicParts = topic.split(".");
        assert(topicParts.length() == 3);

        // Channel ID where the moderator actions are coming from
        auto channelID = topicParts[2];

        switch (innerMessage.type)
        {
            case PubSubChatModeratorActionMessage::Type::ModerationAction: {
                QString moderationAction =
                    innerMessage.data.value("moderation_action").toString();

                auto handlerIt =
                    this->moderationActionHandlers.find(moderationAction);

                if (handlerIt == this->moderationActionHandlers.end())
                {
                    qCDebug(chatterinoPubSub)
                        << "No handler found for moderation action"
                        << moderationAction;
                    return;
                }
                // Invoke handler function
                handlerIt->second(innerMessage.data, channelID);
            }
            break;
            case PubSubChatModeratorActionMessage::Type::ChannelTermsAction: {
                QString channelTermsAction =
                    innerMessage.data.value("type").toString();

                auto handlerIt =
                    this->channelTermsActionHandlers.find(channelTermsAction);

                if (handlerIt == this->channelTermsActionHandlers.end())
                {
                    qCDebug(chatterinoPubSub)
                        << "No handler found for channel terms action"
                        << channelTermsAction;
                    return;
                }
                // Invoke handler function
                handlerIt->second(innerMessage.data, channelID);
            }
            break;

            case PubSubChatModeratorActionMessage::Type::INVALID:
            default: {
                qCDebug(chatterinoPubSub) << "Invalid moderator action type:"
                                          << innerMessage.typeString;
            }
            break;
        }
    }
    else if (topic.startsWith("community-points-channel-v1."))
    {
        auto oInnerMessage =
            message.toInner<PubSubCommunityPointsChannelV1Message>();
        if (!oInnerMessage)
        {
            return;
        }

        auto innerMessage = *oInnerMessage;

        switch (innerMessage.type)
        {
            case PubSubCommunityPointsChannelV1Message::Type::
                AutomaticRewardRedeemed:
            case PubSubCommunityPointsChannelV1Message::Type::RewardRedeemed: {
                auto redemption =
                    innerMessage.data.value("redemption").toObject();
                this->pointReward.redeemed.invoke(redemption);
            }
            break;

            case PubSubCommunityPointsChannelV1Message::Type::INVALID:
            default: {
                qCDebug(chatterinoPubSub)
                    << "Invalid point event type:" << innerMessage.typeString;
            }
            break;
        }
    }
    else if (topic.startsWith("automod-queue."))
    {
        auto oInnerMessage = message.toInner<PubSubAutoModQueueMessage>();
        if (!oInnerMessage)
        {
            return;
        }

        auto innerMessage = *oInnerMessage;

        auto topicParts = topic.split(".");
        assert(topicParts.length() == 3);

        // Channel ID where the moderator actions are coming from
        auto channelID = topicParts[2];

        this->moderation.autoModMessageCaught.invoke(innerMessage, channelID);
    }
    else if (topic.startsWith("low-trust-users."))
    {
        auto oInnerMessage = message.toInner<PubSubLowTrustUsersMessage>();
        if (!oInnerMessage)
        {
            return;
        }

        auto innerMessage = *oInnerMessage;

        switch (innerMessage.type)
        {
            case PubSubLowTrustUsersMessage::Type::UserMessage: {
                this->moderation.suspiciousMessageReceived.invoke(innerMessage);
            }
            break;

            case PubSubLowTrustUsersMessage::Type::TreatmentUpdate: {
                this->moderation.suspiciousTreatmentUpdated.invoke(
                    innerMessage);
            }
            break;

            case PubSubLowTrustUsersMessage::Type::INVALID: {
                qCWarning(chatterinoPubSub)
                    << "Invalid low trust users event type:"
                    << innerMessage.typeString;
            }
            break;
        }
    }
    else
    {
        qCDebug(chatterinoPubSub) << "Unknown topic:" << topic;
        return;
    }
}

}  // namespace chatterino
