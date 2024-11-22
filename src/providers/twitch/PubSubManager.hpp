#pragma once

#include "providers/twitch/PubSubClientOptions.hpp"
#include "util/ExponentialBackoff.hpp"

#include <pajlada/signals/signal.hpp>
#include <QJsonObject>
#include <QString>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

#if __has_include(<gtest/gtest_prod.h>)
#    include <gtest/gtest_prod.h>
#endif

namespace chatterino {

class TwitchAccount;
class PubSubClient;

struct ClearChatAction;
struct DeleteAction;
struct ModeChangedAction;
struct ModerationStateAction;
struct BanAction;
struct UnbanAction;
struct PubSubAutoModQueueMessage;
struct AutomodAction;
struct AutomodUserAction;
struct AutomodInfoAction;
struct RaidAction;
struct UnraidAction;
struct WarnAction;
struct PubSubLowTrustUsersMessage;
struct PubSubWhisperMessage;

struct PubSubListenMessage;
struct PubSubMessage;
struct PubSubMessageMessage;

class PubSubPool;

/**
 * This handles the Twitch PubSub connection
 *
 * Known issues:
 *  - Upon closing a channel, we don't unsubscribe to its pubsub connections
 *  - Stop is never called, meaning we never do a clean shutdown
 */
class PubSub
{
    template <typename T>
    using Signal =
        pajlada::Signals::Signal<T>;  // type-id is vector<T, Alloc<T>>

    // Account credentials
    // Set from setAccount
    QString token_;
    QString userID_;

public:
    PubSub(const QString &host,
           std::chrono::seconds pingInterval = std::chrono::seconds(15));
    ~PubSub();

    PubSub(const PubSub &) = delete;
    PubSub(PubSub &&) = delete;
    PubSub &operator=(const PubSub &) = delete;
    PubSub &operator=(PubSub &&) = delete;

    /// Set up connections between itself & other parts of the application
    void initialize();

    struct {
        Signal<ClearChatAction> chatCleared;
        Signal<DeleteAction> messageDeleted;
        Signal<ModeChangedAction> modeChanged;
        Signal<ModerationStateAction> moderationStateChanged;

        Signal<RaidAction> raidStarted;
        Signal<UnraidAction> raidCanceled;

        Signal<BanAction> userBanned;
        Signal<UnbanAction> userUnbanned;
        Signal<WarnAction> userWarned;

        Signal<PubSubLowTrustUsersMessage> suspiciousMessageReceived;
        Signal<PubSubLowTrustUsersMessage> suspiciousTreatmentUpdated;

        // Message caught by automod
        //                                channelID
        pajlada::Signals::Signal<PubSubAutoModQueueMessage, QString>
            autoModMessageCaught;

        // Message blocked by moderator
        Signal<AutomodAction> autoModMessageBlocked;

        Signal<AutomodUserAction> automodUserMessage;
        Signal<AutomodInfoAction> automodInfoMessage;
    } moderation;

    struct {
        // Parsing should be done in PubSubManager as well,
        // but for now we just send the raw data
        Signal<const PubSubWhisperMessage &> received;
        Signal<const PubSubWhisperMessage &> sent;
    } whisper;

    struct {
        Signal<const QJsonObject &> redeemed;
    } pointReward;

    /**
     * Listen to incoming whispers for the currently logged in user.
     * This topic is relevant for everyone.
     *
     * PubSub topic: whispers.{currentUserID}
     */
    bool listenToWhispers();
    void unlistenWhispers();

    /**
     * Listen to moderation actions in the given channel.
     * This topic is relevant for everyone.
     * For moderators, this topic includes blocked/permitted terms updates,
     * roomstate changes, general mod/vip updates, all bans/timeouts/deletions.
     * For normal users, this topic includes moderation actions that are targetted at the local user:
     * automod catching a user's sent message, a moderator approving or denying their caught messages,
     * the user gaining/losing mod/vip, the user receiving a ban/timeout/deletion.
     *
     * PubSub topic: chat_moderator_actions.{currentUserID}.{channelID}
     */
    void listenToChannelModerationActions(const QString &channelID);
    void unlistenChannelModerationActions();

    /**
     * Listen to Automod events in the given channel.
     * This topic is only relevant for moderators.
     * This will send events about incoming messages that
     * are caught by Automod.
     *
     * PubSub topic: automod-queue.{currentUserID}.{channelID}
     */
    void listenToAutomod(const QString &channelID);
    void unlistenAutomod();

    /**
     * Listen to Low Trust events in the given channel.
     * This topic is only relevant for moderators.
     * This will fire events about suspicious treatment updates
     * and messages sent by restricted/monitored users.
     *
     * PubSub topic: low-trust-users.{currentUserID}.{channelID}
     */
    void listenToLowTrustUsers(const QString &channelID);
    void unlistenLowTrustUsers();

    /**
     * Listen to incoming channel point redemptions in the given channel.
     * This topic is relevant for everyone.
     *
     * PubSub topic: community-points-channel-v1.{channelID}
     */
    void listenToChannelPointRewards(const QString &channelID);
    void unlistenChannelPointRewards();

    struct {
        std::atomic<uint32_t> connectionsClosed{0};
        std::atomic<uint32_t> connectionsOpened{0};
        std::atomic<uint32_t> connectionsFailed{0};
        std::atomic<uint32_t> messagesReceived{0};
        std::atomic<uint32_t> messagesFailedToParse{0};
        std::atomic<uint32_t> failedListenResponses{0};
        std::atomic<uint32_t> listenResponses{0};
        std::atomic<uint32_t> unlistenResponses{0};
    } diag;

private:
    void setAccount(std::shared_ptr<TwitchAccount> account);

    std::atomic<bool> addingClient{false};
    ExponentialBackoff<5> connectBackoff{std::chrono::milliseconds(1000)};

    std::unordered_map<
        QString, std::function<void(const QJsonObject &, const QString &)>>
        moderationActionHandlers;

    std::unordered_map<
        QString, std::function<void(const QJsonObject &, const QString &)>>
        channelTermsActionHandlers;

    void handleMessageResponse(const PubSubMessageMessage &message);

    const QString host_;
    const PubSubClientOptions clientOptions_;

    std::unique_ptr<PubSubPool> pool_;

#ifdef FRIEND_TEST
    friend class FTest;

    FRIEND_TEST(TwitchPubSubClient, ServerRespondsToPings);
    FRIEND_TEST(TwitchPubSubClient, ServerDoesntRespondToPings);
    FRIEND_TEST(TwitchPubSubClient, DisconnectedAfter1s);
    FRIEND_TEST(TwitchPubSubClient, ExceedTopicLimit);
    FRIEND_TEST(TwitchPubSubClient, ExceedTopicLimitSingleStep);
    FRIEND_TEST(TwitchPubSubClient, ReceivedWhisper);
    FRIEND_TEST(TwitchPubSubClient, ModeratorActionsUserBanned);
    FRIEND_TEST(TwitchPubSubClient, MissingToken);
    FRIEND_TEST(TwitchPubSubClient, WrongToken);
    FRIEND_TEST(TwitchPubSubClient, CorrectToken);
    FRIEND_TEST(TwitchPubSubClient, AutoModMessageHeld);
#endif
};

}  // namespace chatterino
