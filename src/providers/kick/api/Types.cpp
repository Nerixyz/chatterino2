#include "providers/kick/api/Types.hpp"

#include "providers/kick/util/Deserialize.hpp"

namespace chatterino::kick {

std::optional<Chatroom> Chatroom::fromJson(const RJSourceVal &val)
{
    Chatroom room{};
    bool ok = de::get(val, "id", room.id) &&
              de::get(val, "channel_id", room.channelID) &&
              de::get(val, "slow_mode", room.slowMode) &&
              de::get(val, "followers_mode", room.followersMode) &&
              de::get(val, "subscribers_mode", room.subscribersMode) &&
              de::get(val, "emotes_mode", room.emotesMode) &&
              de::get(val, "message_interval", room.messageInterval) &&
              de::get(val, "following_min_duration", room.followingMinDuration);

    if (!ok)
    {
        return std::nullopt;
    }
    return room;
}

std::optional<User> User::fromJson(const RJSourceVal &val)
{
    User user{};
    if (!de::get(val, "id", user.id))
    {
        return std::nullopt;
    }
    const auto *child = de::child(val, "chatroom");
    if (child == nullptr)
    {
        return std::nullopt;
    }
    std::optional<Chatroom> room = Chatroom::fromJson(*child);
    if (!room)
    {
        return std::nullopt;
    }
    user.chat = *room;

    return user;
}

}  // namespace chatterino::kick
