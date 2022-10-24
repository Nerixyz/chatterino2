#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

namespace chatterino {
enum class BttvLiveUpdateSubscriptionType {
    Channel,

    INVALID,
};

struct BttvLiveUpdateSubscription {
    bool operator==(const BttvLiveUpdateSubscription &rhs) const;
    bool operator!=(const BttvLiveUpdateSubscription &rhs) const;
    QString twitchId;
    BttvLiveUpdateSubscriptionType type;

    QByteArray encodeSubscribe() const;
    QByteArray encodeUnsubscribe() const;

    friend QDebug &operator<<(QDebug &dbg,
                              const BttvLiveUpdateSubscription &subscription);
};
}  // namespace chatterino

namespace std {
template <>
struct hash<chatterino::BttvLiveUpdateSubscription> {
    size_t operator()(const chatterino::BttvLiveUpdateSubscription &sub) const
    {
        return (size_t)qHash(sub.twitchId, qHash((int)sub.type));
    }
};
}  // namespace std
