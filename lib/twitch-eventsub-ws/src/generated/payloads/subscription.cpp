// WARNING: This file is automatically generated. Any changes will be lost.
#include "twitch-eventsub-ws/chrono.hpp"  // IWYU pragma: keep
#include "twitch-eventsub-ws/detail/errors.hpp"
#include "twitch-eventsub-ws/detail/variant.hpp"  // IWYU pragma: keep
#include "twitch-eventsub-ws/json.hpp"
#include "twitch-eventsub-ws/payloads/subscription.hpp"

#include <boost/json.hpp>

namespace chatterino::eventsub::lib::payload::subscription {

void tag_invoke(json::FromJsonTag<Transport> /* tag */, Transport &target,
                boost::system::error_code &ec, const boost::json::value &jvRoot)
{
    if (!jvRoot.is_object())
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::ExpectedObject);
    }
    const auto &root = jvRoot.get_object();

    const auto *jvmethod = root.if_contains("method");
    if (jvmethod == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.method, ec, *jvmethod))
    {
        return;
    }
    const auto *jvsessionID = root.if_contains("session_id");
    if (jvsessionID == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.sessionID, ec, *jvsessionID))
    {
        return;
    }
}

void tag_invoke(json::FromJsonTag<Subscription> /* tag */, Subscription &target,
                boost::system::error_code &ec, const boost::json::value &jvRoot)
{
    if (!jvRoot.is_object())
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::ExpectedObject);
    }
    const auto &root = jvRoot.get_object();

    const auto *jvid = root.if_contains("id");
    if (jvid == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.id, ec, *jvid))
    {
        return;
    }
    const auto *jvstatus = root.if_contains("status");
    if (jvstatus == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.status, ec, *jvstatus))
    {
        return;
    }
    const auto *jvtype = root.if_contains("type");
    if (jvtype == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.type, ec, *jvtype))
    {
        return;
    }
    const auto *jvversion = root.if_contains("version");
    if (jvversion == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.version, ec, *jvversion))
    {
        return;
    }
    const auto *jvtransport = root.if_contains("transport");
    if (jvtransport == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.transport, ec, *jvtransport))
    {
        return;
    }
    const auto *jvcreatedAt = root.if_contains("created_at");
    if (jvcreatedAt == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.createdAt, ec, *jvcreatedAt))
    {
        return;
    }
    const auto *jvcost = root.if_contains("cost");
    if (jvcost == nullptr)
    {
        EVENTSUB_INTO_BAIL_HERE(error::Kind::FieldMissing);
    }

    if (!json::fromJson(target.cost, ec, *jvcost))
    {
        return;
    }
}

}  // namespace chatterino::eventsub::lib::payload::subscription
