#include "providers/twitch/TwitchAccount.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "common/Env.hpp"
#include "common/Literals.hpp"
#include "common/network/NetworkResult.hpp"
#include "common/QLogging.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "messages/Emote.hpp"
#include "messages/Message.hpp"
#include "messages/MessageBuilder.hpp"
#include "providers/IvrApi.hpp"
#include "providers/seventv/SeventvAPI.hpp"
#include "providers/seventv/SeventvEmotes.hpp"
#include "providers/seventv/SeventvPersonalEmotes.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "providers/twitch/TwitchCommon.hpp"
#include "providers/twitch/TwitchUsers.hpp"
#include "singletons/Emotes.hpp"
#include "util/CancellationToken.hpp"
#include "util/Helpers.hpp"
#include "util/QStringHash.hpp"
#include "util/RapidjsonHelpers.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <QStringBuilder>
#include <QThread>

namespace chatterino {

using namespace literals;

std::optional<TwitchAccountData> TwitchAccountData::loadRaw(
    const std::string &key)
{
    auto username = QStringSetting::get("/accounts/" + key + "/username");
    auto userID = QStringSetting::get("/accounts/" + key + "/userID");
    auto clientID = QStringSetting::get("/accounts/" + key + "/clientID");
    auto oauthToken = QStringSetting::get("/accounts/" + key + "/oauthToken");

    if (username.isEmpty() || userID.isEmpty() || clientID.isEmpty() ||
        oauthToken.isEmpty())
    {
        return std::nullopt;
    }

    auto accountType = EnumStringSetting<TwitchAccount::Type>::get(
        "/accounts/" + key + "/accountType",
        TwitchAccount::Type::ImplicitGrant);
    auto refreshToken =
        QStringSetting::get("/accounts/" + key + "/refreshToken");
    auto expiresAtStr = QStringSetting::get("/accounts/" + key + "/expiresAt");
    QDateTime expiresAt;
    if (accountType == TwitchAccount::Type::DeviceAuth)
    {
        expiresAt = QDateTime::fromString(expiresAtStr, Qt::ISODate);
    }

    return TwitchAccountData{
        .username = username.trimmed(),
        .userID = userID.trimmed(),
        .clientID = clientID.trimmed(),
        .oauthToken = oauthToken.trimmed(),
        .ty = accountType,
        .refreshToken = refreshToken,
        .expiresAt = expiresAt,
    };
}

void TwitchAccountData::save() const
{
    using QStringSetting = pajlada::Settings::Setting<QString>;

    auto basePath = "/accounts/uid" + this->userID.toStdString();
    QStringSetting::set(basePath + "/username", this->username);
    QStringSetting::set(basePath + "/userID", this->userID);
    QStringSetting::set(basePath + "/clientID", this->clientID);
    QStringSetting::set(basePath + "/oauthToken", this->oauthToken);
    EnumStringSetting<TwitchAccount::Type>::set(basePath + "/accountType",
                                                this->ty);
    QStringSetting::set(basePath + "/refreshToken", this->refreshToken);
    if (this->ty == TwitchAccount::Type::DeviceAuth)
    {
        QStringSetting::set(basePath + "/expiresAt",
                            this->expiresAt.toString(Qt::ISODate));
    }
}

TwitchAccount::TwitchAccount(const TwitchAccountData &data)
    : Account(ProviderId::Twitch)
    , oauthClient_(data.clientID)
    , oauthToken_(data.oauthToken)
    , userName_(data.username)
    , userId_(data.userID)
    , type_(data.ty)
    , refreshToken_(data.refreshToken)
    , expiresAt_(data.expiresAt)
    , isAnon_(data.username == ANONYMOUS_USERNAME)
    , emoteSets_(std::make_shared<TwitchEmoteSetMap>())
    , emotes_(std::make_shared<EmoteMap>())
{
}

TwitchAccount::~TwitchAccount() = default;

QString TwitchAccount::toString() const
{
    return this->getUserName();
}

const QString &TwitchAccount::getUserName() const
{
    return this->userName_;
}

const QString &TwitchAccount::getOAuthClient() const
{
    return this->oauthClient_;
}

const QString &TwitchAccount::getOAuthToken() const
{
    return this->oauthToken_;
}

const QString &TwitchAccount::getUserId() const
{
    return this->userId_;
}

const QString &TwitchAccount::refreshToken() const
{
    return this->refreshToken_;
}

const QDateTime &TwitchAccount::expiresAt() const
{
    return this->expiresAt_;
}

TwitchAccount::Type TwitchAccount::type() const
{
    return this->type_;
}

QColor TwitchAccount::color()
{
    return this->color_.get();
}

void TwitchAccount::setColor(QColor color)
{
    this->color_.set(std::move(color));
}

bool TwitchAccount::setData(const TwitchAccountData &data)
{
    assert(this->userName_ == data.username && this->userId_ == data.userID);

    bool anyUpdate = false;

    if (this->oauthToken_ != data.oauthToken)
    {
        this->oauthToken_ = data.oauthToken;
        anyUpdate = true;
    }
    if (this->oauthClient_ != data.clientID)
    {
        this->oauthClient_ = data.clientID;
        anyUpdate = true;
    }
    if (this->refreshToken_ != data.refreshToken)
    {
        this->refreshToken_ = data.refreshToken;
        anyUpdate = true;
    }
    if (this->expiresAt_ != data.expiresAt)
    {
        this->expiresAt_ = data.expiresAt;
        anyUpdate = true;
    }
    if (this->type_ != data.ty)
    {
        this->type_ = data.ty;
        anyUpdate = true;
    }

    return anyUpdate;
}

bool TwitchAccount::isAnon() const
{
    return this->isAnon_;
}

void TwitchAccount::loadBlocks()
{
    assertInGuiThread();

    auto token = CancellationToken(false);
    this->blockToken_ = token;
    this->ignores_.clear();
    this->ignoresUserIds_.clear();

    getHelix()->loadBlocks(
        getApp()->getAccounts()->twitch.getCurrent()->userId_,
        [this](const std::vector<HelixBlock> &blocks) {
            assertInGuiThread();

            for (const HelixBlock &block : blocks)
            {
                TwitchUser blockedUser;
                blockedUser.fromHelixBlock(block);
                this->ignores_.insert(blockedUser);
                this->ignoresUserIds_.insert(blockedUser.id);
            }
        },
        [](auto error) {
            qCWarning(chatterinoTwitch).noquote()
                << "Fetching blocks failed:" << error;
        },
        std::move(token));
}

void TwitchAccount::blockUser(const QString &userId, const QObject *caller,
                              std::function<void()> onSuccess,
                              std::function<void()> onFailure)
{
    getHelix()->blockUser(
        userId, caller,
        [this, userId, onSuccess = std::move(onSuccess)] {
            assertInGuiThread();

            TwitchUser blockedUser;
            blockedUser.id = userId;
            this->ignores_.insert(blockedUser);
            this->ignoresUserIds_.insert(blockedUser.id);
            onSuccess();
        },
        std::move(onFailure));
}

void TwitchAccount::unblockUser(const QString &userId, const QObject *caller,
                                std::function<void()> onSuccess,
                                std::function<void()> onFailure)
{
    getHelix()->unblockUser(
        userId, caller,
        [this, userId, onSuccess = std::move(onSuccess)] {
            assertInGuiThread();

            TwitchUser ignoredUser;
            ignoredUser.id = userId;
            this->ignores_.erase(ignoredUser);
            this->ignoresUserIds_.erase(ignoredUser.id);
            onSuccess();
        },
        std::move(onFailure));
}

void TwitchAccount::blockUserLocally(const QString &userID)
{
    assertInGuiThread();
    assert(getApp()->isTest());

    TwitchUser blockedUser;
    blockedUser.id = userID;
    this->ignores_.insert(blockedUser);
    this->ignoresUserIds_.insert(blockedUser.id);
}

const std::unordered_set<TwitchUser> &TwitchAccount::blocks() const
{
    assertInGuiThread();
    return this->ignores_;
}

const std::unordered_set<QString> &TwitchAccount::blockedUserIds() const
{
    assertInGuiThread();
    return this->ignoresUserIds_;
}

// AutoModActions
void TwitchAccount::autoModAllow(const QString msgID, ChannelPtr channel)
{
    getHelix()->manageAutoModMessages(
        this->getUserId(), msgID, "ALLOW",
        [] {
            // success
        },
        [channel](auto error) {
            // failure
            QString errorMessage("Failed to allow AutoMod message - ");

            switch (error)
            {
                case HelixAutoModMessageError::MessageAlreadyProcessed: {
                    errorMessage += "message has already been processed.";
                }
                break;

                case HelixAutoModMessageError::UserNotAuthenticated: {
                    errorMessage += "you need to re-authenticate.";
                }
                break;

                case HelixAutoModMessageError::UserNotAuthorized: {
                    errorMessage +=
                        "you don't have permission to perform that action";
                }
                break;

                case HelixAutoModMessageError::MessageNotFound: {
                    errorMessage += "target message not found.";
                }
                break;

                // This would most likely happen if the service is down, or if the JSON payload returned has changed format
                case HelixAutoModMessageError::Unknown:
                default: {
                    errorMessage += "an unknown error occured.";
                }
                break;
            }

            channel->addSystemMessage(errorMessage);
        });
}

void TwitchAccount::autoModDeny(const QString msgID, ChannelPtr channel)
{
    getHelix()->manageAutoModMessages(
        this->getUserId(), msgID, "DENY",
        [] {
            // success
        },
        [channel](auto error) {
            // failure
            QString errorMessage("Failed to deny AutoMod message - ");

            switch (error)
            {
                case HelixAutoModMessageError::MessageAlreadyProcessed: {
                    errorMessage += "message has already been processed.";
                }
                break;

                case HelixAutoModMessageError::UserNotAuthenticated: {
                    errorMessage += "you need to re-authenticate.";
                }
                break;

                case HelixAutoModMessageError::UserNotAuthorized: {
                    errorMessage +=
                        "you don't have permission to perform that action";
                }
                break;

                case HelixAutoModMessageError::MessageNotFound: {
                    errorMessage += "target message not found.";
                }
                break;

                // This would most likely happen if the service is down, or if the JSON payload returned has changed format
                case HelixAutoModMessageError::Unknown:
                default: {
                    errorMessage += "an unknown error occured.";
                }
                break;
            }

            channel->addSystemMessage(errorMessage);
        });
}

const QString &TwitchAccount::getSeventvUserID() const
{
    return this->seventvUserID_;
}

void TwitchAccount::loadSeventvUserID()
{
    if (this->isAnon())
    {
        return;
    }
    if (!this->seventvUserID_.isEmpty())
    {
        return;
    }

    const auto loadPersonalEmotes = [](const QString &twitchUserID,
                                       const QString &emoteSetID) {
        SeventvEmotes::getEmoteSet(
            emoteSetID,
            [twitchUserID, emoteSetID](auto &&emoteMap,
                                       const auto & /*emoteSetName*/) {
                getApp()->getSeventvPersonalEmotes()->addEmoteSetForUser(
                    emoteSetID, std::forward<decltype(emoteMap)>(emoteMap),
                    twitchUserID);
            },
            [twitchUserID, emoteSetID](const auto &error) {
                qCDebug(chatterinoSeventv)
                    << "Failed to fetch personal emote-set. emote-set-id:"
                    << emoteSetID << "twitch-user-id" << twitchUserID
                    << "error:" << error;
            });
    };

    auto *seventv = getApp()->getSeventvAPI();
    if (!seventv)
    {
        qCWarning(chatterinoSeventv)
            << "Not loading 7TV User ID because the 7TV API is not initialized";
        return;
    }

    seventv->getUserByTwitchID(
        this->getUserId(),
        [this, loadPersonalEmotes](const auto &json) {
            const auto user = json["user"].toObject();
            const auto id = user["id"].toString();
            if (id.isEmpty())
            {
                return;
            }
            this->seventvUserID_ = id;

            for (const auto &emoteSetJson : user["emote_sets"].toArray())
            {
                const auto emoteSet = emoteSetJson.toObject();
                if (SeventvEmoteSetFlags(
                        SeventvEmoteSetFlag(emoteSet["flags"].toInt()))
                        .has(SeventvEmoteSetFlag::Personal))
                {
                    loadPersonalEmotes(this->getUserId(),
                                       emoteSet["id"].toString());
                    break;
                }
            }
        },
        [](const auto &result) {
            qCDebug(chatterinoSeventv)
                << "Failed to load 7TV user-id:" << result.formatError();
        });
}

bool TwitchAccount::hasEmoteSet(const EmoteSetId &id) const
{
    auto emotes = this->emoteSets_.accessConst();
    return emotes->get()->contains(id);
}

SharedAccessGuard<std::shared_ptr<const TwitchEmoteSetMap>>
    TwitchAccount::accessEmoteSets() const
{
    return this->emoteSets_.accessConst();
}

SharedAccessGuard<std::shared_ptr<const EmoteMap>> TwitchAccount::accessEmotes()
    const
{
    return this->emotes_.accessConst();
}

void TwitchAccount::setEmotes(std::shared_ptr<const EmoteMap> emotes)
{
    assert(getApp()->isTest());
    *this->emotes_.access() = std::move(emotes);
}

std::optional<EmotePtr> TwitchAccount::twitchEmote(const EmoteName &name) const
{
    auto emotes = this->emotes_.accessConst();
    auto it = (*emotes)->find(name);
    if (it != (*emotes)->end())
    {
        return it->second;
    }
    return std::nullopt;
}

void TwitchAccount::reloadEmotes(void *caller)
{
    if (this->isAnon() || getApp()->isTest())
    {
        return;
    }

    CancellationToken token(false);
    this->emoteToken_ = token;

    auto sets = std::make_shared<TwitchEmoteSetMap>();
    auto emoteMap = std::make_shared<EmoteMap>();
    auto nCalls = std::make_shared<size_t>();

    auto *twitchEmotes = getApp()->getEmotes()->getTwitchEmotes();
    auto *twitchUsers = getApp()->getTwitchUsers();

    auto addEmote = [sets, emoteMap, twitchEmotes,
                     twitchUsers](const HelixChannelEmote &emote) {
        EmoteId id{emote.id};
        EmoteName name{emote.name};
        auto meta = getTwitchEmoteSetMeta(emote);

        auto emotePtr = twitchEmotes->getOrCreateEmote(id, name);
        if (!emoteMap->try_emplace(emotePtr->name, emotePtr).second)
        {
            // if the emote already exists, we don't want to add it to a set as
            // those are assumed to be disjoint
            return;
        }

        auto set = sets->find(EmoteSetId{meta.setID});
        if (set == sets->end())
        {
            auto owner = [&]() {
                if (meta.isSubLike)
                {
                    return twitchUsers->resolveID({emote.ownerID});
                }

                return std::make_shared<TwitchUser>(TwitchUser{
                    .id = u"[x-c2-global-owner]"_s,
                    .name = {},
                    .displayName = {},
                });
            }();
            set = sets->emplace(EmoteSetId{meta.setID},
                                TwitchEmoteSet{
                                    .owner = owner,
                                    .emotes = {},
                                    .isBits = meta.isBits,
                                    .isSubLike = meta.isSubLike,
                                })
                      .first;
        }
        set->second.emotes.emplace_back(std::move(emotePtr));
    };

    auto userID = this->getUserId();
    qDebug(chatterinoTwitch).nospace()
        << "Loading Twitch emotes - userID: " << userID
        << ", broadcasterID: none, manualRefresh: " << (caller != nullptr);

    getHelix()->getUserEmotes(
        this->getUserId(), {},
        [this, caller, emoteMap, sets, addEmote, nCalls](
            const auto &emotes, const auto &state) mutable {
            assert(emoteMap && sets);
            (*nCalls)++;
            qDebug(chatterinoTwitch).nospace()
                << "Got " << emotes.size() << " more emote(s)";

            emoteMap->reserve(emoteMap->size() + emotes.size());
            for (const auto &emote : emotes)
            {
                addEmote(emote);
            }

            if (state.done)
            {
                qDebug(chatterinoTwitch).nospace()
                    << "Loaded " << emoteMap->size() << " Twitch emotes ("
                    << *nCalls << " requests)";

                for (auto &[id, set] : *sets)
                {
                    std::ranges::sort(
                        set.emotes, [](const auto &l, const auto &r) {
                            return l->name.string < r->name.string;
                        });
                }

                *this->emotes_.access() = std::move(emoteMap);
                *this->emoteSets_.access() = std::move(sets);
                getApp()->getAccounts()->twitch.emotesReloaded.invoke(caller,
                                                                      {});
            }
        },
        [caller](const auto &error) {
            qDebug(chatterinoTwitch)
                << "Failed to load Twitch emotes:" << error;
            getApp()->getAccounts()->twitch.emotesReloaded.invoke(
                caller, makeUnexpected(error));
        },
        std::move(token));
}

}  // namespace chatterino
