#pragma once

#include "common/Aliases.hpp"
#include "common/Atomic.hpp"
#include "common/UniqueAccess.hpp"
#include "controllers/accounts/Account.hpp"
#include "messages/Emote.hpp"
#include "providers/twitch/TwitchUser.hpp"
#include "util/CancellationToken.hpp"
#include "util/QStringHash.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <rapidjson/document.h>

#include <functional>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace chatterino {

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;

struct TwitchAccountData;

class TwitchAccount : public Account
{
public:
    enum class Type : uint32_t {
        Regular,
        DeviceAuth,
    };
    struct TwitchEmote {
        EmoteId id;
        EmoteName name;
    };

    struct EmoteSet {
        QString key;
        QString channelName;
        QString channelID;
        QString text;
        bool subscriber{false};
        bool local{false};
        std::vector<TwitchEmote> emotes;
    };

    struct TwitchAccountEmoteData {
        std::vector<std::shared_ptr<EmoteSet>> emoteSets;

        // this EmoteMap should contain all emotes available globally
        // excluding locally available emotes, such as follower ones
        EmoteMap emotes;
    };

    TwitchAccount(const TwitchAccountData &data);

    QString toString() const override;

    const QString &getUserName() const;
    const QString &getOAuthToken() const;
    const QString &getOAuthClient() const;
    const QString &getUserId() const;
    [[nodiscard]] const QString &refreshToken() const;
    [[nodiscard]] const QDateTime &expiresAt() const;
    [[nodiscard]] Type type() const;

    /**
     * The Seventv user-id of the current user. 
     * Empty if there's no associated Seventv user with this twitch user.
     */
    const QString &getSeventvUserID() const;

    QColor color();
    void setColor(QColor color);

    /// Attempts to update the account data
    /// @pre The name and userID must match this account.
    /// @returns true if the value has changed, otherwise false
    bool setData(const TwitchAccountData &data);

    bool isAnon() const;

    void loadBlocks();
    void blockUser(const QString &userId, const QObject *caller,
                   std::function<void()> onSuccess,
                   std::function<void()> onFailure);
    void unblockUser(const QString &userId, const QObject *caller,
                     std::function<void()> onSuccess,
                     std::function<void()> onFailure);

    [[nodiscard]] const std::unordered_set<TwitchUser> &blocks() const;
    [[nodiscard]] const std::unordered_set<QString> &blockedUserIds() const;

    void loadEmotes(std::weak_ptr<Channel> weakChannel = {});
    // loadUserstateEmotes loads emote sets that are part of the USERSTATE emote-sets key
    // this function makes sure not to load emote sets that have already been loaded
    void loadUserstateEmotes(std::weak_ptr<Channel> weakChannel = {});
    // setUserStateEmoteSets sets the emote sets that were parsed from the USERSTATE emote-sets key
    // Returns true if the newly inserted emote sets differ from the ones previously saved
    [[nodiscard]] bool setUserstateEmoteSets(QStringList newEmoteSets);
    SharedAccessGuard<const TwitchAccountEmoteData> accessEmotes() const;
    SharedAccessGuard<const std::unordered_map<QString, EmoteMap>>
        accessLocalEmotes() const;

    // Automod actions
    void autoModAllow(const QString msgID, ChannelPtr channel);
    void autoModDeny(const QString msgID, ChannelPtr channel);

    void loadSeventvUserID();

private:
    QString oauthClient_;
    QString oauthToken_;
    QString userName_;
    QString userId_;
    Type type_ = Type::Regular;
    QString refreshToken_;
    QDateTime expiresAt_;
    const bool isAnon_;
    Atomic<QColor> color_;

    QStringList userstateEmoteSets_;

    ScopedCancellationToken blockToken_;
    std::unordered_set<TwitchUser> ignores_;
    std::unordered_set<QString> ignoresUserIds_;

    //    std::map<UserId, TwitchAccountEmoteData> emotes;
    UniqueAccess<TwitchAccountEmoteData> emotes_;
    UniqueAccess<std::unordered_map<QString, EmoteMap>> localEmotes_;

    QString seventvUserID_;
};

struct TwitchAccountData {
    QString username;
    QString userID;
    QString clientID;
    QString oauthToken;
    TwitchAccount::Type ty = TwitchAccount::Type::Regular;
    QString refreshToken;
    QDateTime expiresAt;

    static std::optional<TwitchAccountData> loadRaw(const std::string &key);
    void save() const;
};

}  // namespace chatterino
