// SPDX-FileCopyrightText: 2024 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/IrcHelpers.hpp"

#include "Application.hpp"

namespace {

using namespace chatterino;
using namespace Qt::Literals;

QDateTime calculateMessageTimeBase(const Communi::IrcMessage *message)
{
    // Check if message is from recent-messages API
    if (message->tags().contains(u"historical"_s))
    {
        bool customReceived = false;
        auto ts = message->tags()
                      .value(u"rm-received-ts"_s)
                      .toLongLong(&customReceived);
        if (!customReceived)
        {
            ts = message->tags().value(u"tmi-sent-ts"_s).toLongLong();
        }

        return QDateTime::fromMSecsSinceEpoch(ts);
    }

    // If present, handle tmi-sent-ts tag and use it as timestamp
    if (message->tags().contains(u"tmi-sent-ts"_s))
    {
        auto ts = message->tags().value(u"tmi-sent-ts"_s).toLongLong();
        return QDateTime::fromMSecsSinceEpoch(ts);
    }

    // Some IRC Servers might have server-time tag containing UTC date in ISO format, use it as timestamp
    // See: https://ircv3.net/irc/#server-time
    if (message->tags().contains(u"time"_s))
    {
        QString timedate = message->tags().value(u"time"_s).toString();

        auto date = QDateTime::fromString(timedate, Qt::ISODate);
        date.setTimeZone(QTimeZone::utc());
        return date.toLocalTime();
    }

    // Fallback to current time
#ifdef CHATTERINO_WITH_TESTS
    if (getApp()->isTest())
    {
        return QDateTime::fromMSecsSinceEpoch(0, QTimeZone::utc());
    }
#endif

    return QDateTime::currentDateTime();
}

}  // namespace

namespace chatterino {

QDateTime calculateMessageTime(const Communi::IrcMessage *message)
{
    auto dt = calculateMessageTimeBase(message);

#ifdef CHATTERINO_WITH_TESTS
    if (getApp()->isTest())
    {
        return dt.toUTC();
    }
#endif

    return dt;
}

}  // namespace chatterino
