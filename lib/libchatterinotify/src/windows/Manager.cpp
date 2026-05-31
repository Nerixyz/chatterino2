#include "NotificationEventPrivate.hpp"
#include "NotificationPrivate.hpp"
#include "WinRTHelpers.hpp"

#include <chatterinotify/Log.hpp>
#include <chatterinotify/Manager.hpp>
#include <chatterinotify/Notification.hpp>
#include <chatterinotify/NotificationEvent.hpp>
#include <QLoggingCategory>

#include <map>

#include <winrt/Microsoft.Windows.AppNotifications.h>
#include <winrt/Windows.Foundation.h>

namespace winrt {
using namespace Microsoft::Windows::AppNotifications;
}  // namespace winrt

namespace {

using namespace chatterinotify;

class DefaultRegisterGuard
{
public:
    DefaultRegisterGuard() = default;

    ~DefaultRegisterGuard()
    {
        if (this->registered)
        {
            helpers::logExceptions(
                [] {
                    winrt::AppNotificationManager::Default().Unregister();
                },
                "Unregistering AppNotificationManager");
        }
    }

    DefaultRegisterGuard(const DefaultRegisterGuard &) = delete;
    DefaultRegisterGuard(DefaultRegisterGuard &&) = delete;
    DefaultRegisterGuard &operator=(const DefaultRegisterGuard &) = delete;
    DefaultRegisterGuard &operator=(DefaultRegisterGuard &&) = delete;

    void registerManager(const QString &applicationName)
    {
        helpers::logExceptions(
            [&] {
                winrt::AppNotificationManager::Default().Register(
                    helpers::qtToHstr(applicationName),
                    winrt::Windows::Foundation::Uri{nullptr});
            },
            "Registering AppNotificationManager");
        this->registered = true;
    }

private:
    bool registered = false;
};

struct ActionData {
    NotificationManager::ActionCallback cb;
};

}  // namespace

namespace chatterinotify {

class NotificationManagerPrivate
{
public:
    NotificationManagerPrivate();

    DefaultRegisterGuard registerGuard;
    winrt::AppNotificationManager manager;
    winrt::event_token notificationInvokedToken;

    std::map<NotificationManager::ActionID, ActionData> actions;
    NotificationManager::ActionID defaultActionID =
        NotificationManager::NO_ACTION;

    void onNotification(const winrt::AppNotificationActivatedEventArgs &args);
};

NotificationManagerPrivate::NotificationManagerPrivate()
    : manager(winrt::AppNotificationManager::Default())
{
    this->notificationInvokedToken = this->manager.NotificationInvoked(
        [this](const winrt::AppNotificationManager &,
               const winrt::AppNotificationActivatedEventArgs &args) {
            helpers::logExceptions(&NotificationManagerPrivate::onNotification,
                                   "Notification invoked", this, args);
        });
}

void NotificationManagerPrivate::onNotification(
    const winrt::AppNotificationActivatedEventArgs &args)
{
    auto actionIt = this->actions.find(this->defaultActionID);
    if (actionIt == this->actions.end() || !actionIt->second.cb)
    {
        return;
    }
    NotificationEventPrivate event(args.Arguments());
    actionIt->second.cb(NotificationEvent{&event});
}

NotificationManager::NotificationManager()
    : private_(new NotificationManagerPrivate)
{
}

NotificationManager::~NotificationManager() = default;

bool NotificationManager::hasCapability(Capability c)
{
    switch (c)
    {
        case Capability::SupportsLocalImage:
        case Capability::SupportsRemoteImage:
            return true;
    }
    return false;
}

void NotificationManager::initialize(const QString &applicationName)
{
    this->private_->registerGuard.registerManager(applicationName);
}

void NotificationManager::addAction(ActionID id,
                                    const QString & /* displayName */,
                                    ActionCallback cb)
{
    this->private_->actions.emplace(id, std::move(cb));
}

NotificationManager::ActionID NotificationManager::defaultAction() const
{
    return this->private_->defaultActionID;
}

void NotificationManager::setDefaultAction(ActionID id)
{
    this->private_->defaultActionID = id;
}

void NotificationManager::sendNotification(Notification notification)
{
    helpers::logExceptions(
        [&] {
            this->private_->manager.Show(
                notification.private_->builder.BuildNotification());
        },
        "Sending notification");
}

}  // namespace chatterinotify
