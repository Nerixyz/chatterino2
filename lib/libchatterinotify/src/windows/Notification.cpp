#include "NotificationPrivate.hpp"
#include "WinRTHelpers.hpp"

#include <chatterinotify/Notification.hpp>

#include <winrt/Windows.Foundation.h>

namespace chatterinotify {

Notification::Notification(const QString &title, const QString &description)
    : private_(new NotificationPrivate)
{
    helpers::logExceptions(
        [&] {
            auto &builder = this->private_->builder;
            builder.AddText(helpers::qtToHstr(title));
            builder.AddText(helpers::qtToHstr(description));
        },
        "Creating notification");
}
Notification::~Notification() = default;

void Notification::setLocalIconPath(const QUrl &url)
{
    this->private_->setIconPath(url);
}

void Notification::setRemoteIconPath(const QUrl &url)
{
    this->private_->setIconPath(url);
}

void NotificationPrivate::setIconPath(const QUrl &url)
{
    helpers::logExceptions(
        [&] {
            this->builder.SetAppLogoOverride(winrt::Windows::Foundation::Uri(
                url.toString(QUrl::FullyEncoded).toStdWString()));
        },
        "Setting image URI");
}

void Notification::setMetadata(const QString &key, const QString &value)
{
    helpers::logExceptions(
        [&] {
            this->private_->builder.AddArgument(helpers::qtToHstr(key),
                                                helpers::qtToHstr(value));
        },
        "Setting metadata");
}

}  // namespace chatterinotify
