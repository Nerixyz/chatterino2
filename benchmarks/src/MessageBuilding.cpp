// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "MessageBuilding.hpp"

#include "messages/Emote.hpp"
#include "providers/seventv/SeventvEmoteProvider.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

using namespace Qt::Literals;

std::optional<QJsonDocument> tryReadJsonFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly))
    {
        return std::nullopt;
    }

    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(file.readAll(), &e);
    if (e.error != QJsonParseError::NoError)
    {
        return std::nullopt;
    }

    return doc;
}

QJsonDocument readJsonFile(const QString &path)
{
    auto opt = tryReadJsonFile(path);
    if (!opt)
    {
        _exit(1);
    }
    return *opt;
}

}  // namespace

namespace chatterino::bench {

MockMessageApplication::MockMessageApplication()
    : highlights(this->settings, &this->accounts)
{
    this->settings.disableSave();
}

MessageBenchmark::MessageBenchmark(QString name)
    : name(std::move(name))
    , chan(std::make_shared<TwitchChannel>(this->name))
{
    const auto seventvEmotes =
        tryReadJsonFile(u":/bench/seventvemotes-%1.json"_s.arg(this->name));

    if (seventvEmotes)
    {
        auto map = std::make_shared<EmoteMap>();
        for (const auto el : seventvEmotes->object()["emote_set"_L1]
                                 .toObject()["emotes"_L1]
                                 .toArray())
        {
            auto emote = seventv::detail::parseEmote(el.toObject(), false);
            if (!emote)
            {
                continue;
            }
            auto name = emote->name;
            auto ptr = std::make_shared<const Emote>(*std::move(emote));
            (*map)[name] = std::move(ptr);
        }
    }

    this->messages =
        readJsonFile(u":/bench/recentmessages-%1.json"_s.arg(this->name));
}

}  // namespace chatterino::bench
