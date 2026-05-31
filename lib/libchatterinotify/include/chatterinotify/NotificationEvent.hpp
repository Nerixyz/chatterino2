#pragma once

#include <chatterinotify/Config.hpp>
#include <QString>

namespace chatterinotify {

class NotificationEventPrivate;
class CHATTERINOTIFY_GSL_POINTER NotificationEvent
{
public:
    QString argument(const QString &key) const;

private:
    NotificationEvent(const NotificationEventPrivate *priv)
        : private_(priv) {};
    const NotificationEventPrivate *private_ = nullptr;

    friend class NotificationManager;
    friend class NotificationManagerPrivate;
};

}  // namespace chatterinotify
