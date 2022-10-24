#pragma once

#include <memory>
#include "boost/optional.hpp"
#include "common/Aliases.hpp"
#include "common/Atomic.hpp"
#include "providers/bttv/liveupdate/BttvLiveUpdateMessages.hpp"
#include "providers/twitch/TwitchChannel.hpp"

namespace chatterino {

struct Emote;
using EmotePtr = std::shared_ptr<const Emote>;
class EmoteMap;

class BttvEmotes final
{
    static constexpr const char *globalEmoteApiUrl =
        "https://api.betterttv.net/3/cached/emotes/global";
    static constexpr const char *bttvChannelEmoteApiUrl =
        "https://api.betterttv.net/3/cached/users/twitch/";

public:
    BttvEmotes();

    std::shared_ptr<const EmoteMap> emotes() const;
    boost::optional<EmotePtr> emote(const EmoteName &name) const;
    void loadEmotes();
    static void loadChannel(std::weak_ptr<Channel> channel,
                            const QString &channelId,
                            const QString &channelDisplayName,
                            std::function<void(EmoteMap &&)> callback,
                            bool manualRefresh);

    static EmotePtr addEmote(const QString &channelDisplayName,
                             Atomic<std::shared_ptr<const EmoteMap>> &map,
                             const BttvLiveUpdateEmoteAddMessage &message);
    static boost::optional<QString> removeEmote(
        Atomic<std::shared_ptr<const EmoteMap>> &map,
        const BttvLiveUpdateEmoteRemoveMessage &message);

private:
    Atomic<std::shared_ptr<const EmoteMap>> global_;
};

}  // namespace chatterino
