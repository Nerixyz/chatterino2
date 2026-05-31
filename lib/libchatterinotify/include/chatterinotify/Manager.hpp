#pragma once

#include <QString>

#include <memory>

namespace chatterinotify {

class Notification;
class NotificationEvent;

class NotificationManagerPrivate;
class NotificationManager
{
public:
    NotificationManager();
    ~NotificationManager();

    enum class Capability : uint8_t {
        SupportsLocalImage,
        SupportsRemoteImage,
    };
    static bool hasCapability(Capability c);

    void initialize(const QString &applicationName);

    using ActionID = uint16_t;
    using ActionCallback = std::function<void(NotificationEvent)>;
    constexpr static ActionID NO_ACTION = 0xffff;

    void addAction(ActionID id, const QString &displayName, ActionCallback cb);

    ActionID defaultAction() const;
    void setDefaultAction(ActionID id);

    void sendNotification(Notification notification);

private:
    std::unique_ptr<NotificationManagerPrivate> private_;
};

}  // namespace chatterinotify
