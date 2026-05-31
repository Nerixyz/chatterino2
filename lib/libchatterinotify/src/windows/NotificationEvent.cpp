#include "NotificationEventPrivate.hpp"
#include "WinRTHelpers.hpp"

#include <chatterinotify/NotificationEvent.hpp>

namespace chatterinotify {

QString NotificationEvent::argument(const QString &key) const
{
    return helpers::logExceptions(
        [&] {
            auto optVal = this->private_->map.TryLookup(helpers::qtToHstr(key));
            if (optVal)
            {
                return helpers::hstrToQt(*optVal);
            }
            return QString{};
        },
        "Getting map key");
}

}  // namespace chatterinotify
