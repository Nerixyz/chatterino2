#pragma once

#include <QString>
#include <QUrl>

#include <memory>

namespace chatterinotify {

class NotificationPrivate;
class Notification
{
public:
    Notification(const QString &title, const QString &description);
    ~Notification();

    void setLocalIconPath(const QUrl &url);
    void setRemoteIconPath(const QUrl &url);
    void setMetadata(const QString &key, const QString &value);

private:
    std::unique_ptr<NotificationPrivate> private_;

    friend class NotificationManager;
};

}  // namespace chatterinotify
