#pragma once

#include "common/QLogging.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace chatterino {

struct PubSubMessageMessage {
    QString topic;

    QJsonObject messageObject;

    template <class InnerClass>
    std::optional<InnerClass> toInner() const;
};

template <class InnerClass>
std::optional<InnerClass> PubSubMessageMessage::toInner() const
{
    if (this->messageObject.empty())
    {
        return std::nullopt;
    }

    return InnerClass{this->messageObject};
}

}  // namespace chatterino
