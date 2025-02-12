// WARNING: This file is automatically generated. Any changes will be lost.
#include "twitch-eventsub-ws/chrono.hpp"  // IWYU pragma: keep
#include "twitch-eventsub-ws/detail/errors.hpp"
#include "twitch-eventsub-ws/detail/variant.hpp"  // IWYU pragma: keep
#include "twitch-eventsub-ws/json.hpp"
#include "twitch-eventsub-ws/payloads/stream-offline-v1.hpp"

#include <boost/json.hpp>

namespace chatterino::eventsub::lib::payload::stream_offline::v1 {

void tag_invoke(json::FromJsonTag<Event> /* tag */, Event &target,
                boost::system::error_code &ec, const boost::json::value &jvRoot)
{
    if (!jvRoot.is_object())
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::ExpectedObject);
    }
    const auto &root = jvRoot.get_object();

    const auto *jvbroadcasterUserID = root.if_contains("broadcaster_user_id");
    if (jvbroadcasterUserID == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.broadcasterUserID, ec, *jvbroadcasterUserID))
    {
        return;
    }
    const auto *jvbroadcasterUserLogin =
        root.if_contains("broadcaster_user_login");
    if (jvbroadcasterUserLogin == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.broadcasterUserLogin, ec,
                        *jvbroadcasterUserLogin))
    {
        return;
    }
    const auto *jvbroadcasterUserName =
        root.if_contains("broadcaster_user_name");
    if (jvbroadcasterUserName == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.broadcasterUserName, ec, *jvbroadcasterUserName))
    {
        return;
    }
}

void tag_invoke(json::FromJsonTag<Payload> /* tag */, Payload &target,
                boost::system::error_code &ec, const boost::json::value &jvRoot)
{
    if (!jvRoot.is_object())
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::ExpectedObject);
    }
    const auto &root = jvRoot.get_object();

    const auto *jvsubscription = root.if_contains("subscription");
    if (jvsubscription == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.subscription, ec, *jvsubscription))
    {
        return;
    }
    const auto *jvevent = root.if_contains("event");
    if (jvevent == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.event, ec, *jvevent))
    {
        return;
    }
}

}  // namespace chatterino::eventsub::lib::payload::stream_offline::v1
