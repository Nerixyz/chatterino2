#pragma once

#include <rapidjson/document.h>

#include <optional>

namespace chatterino::kick {

using RJSourceVal = rapidjson::GenericValue<rapidjson::UTF8<char>>;

struct Chatroom {
    size_t id;
    size_t channelID;
    bool slowMode;
    bool followersMode;
    bool subscribersMode;
    bool emotesMode;
    size_t messageInterval;
    size_t followingMinDuration;

    static std::optional<Chatroom> fromJson(const RJSourceVal &val);
};

struct User {
    size_t id;
    Chatroom chat;

    static std::optional<User> fromJson(const RJSourceVal &val);
};

}  // namespace chatterino::kick
