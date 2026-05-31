#pragma once

#include <winrt/Microsoft.Windows.AppNotifications.h>

namespace chatterinotify {

class NotificationEventPrivate
{
public:
    using Map = winrt::Windows::Foundation::Collections::IMap<winrt::hstring,
                                                              winrt::hstring>;

    NotificationEventPrivate(Map map)
        : map(std::move(map))
    {
    }

    Map map;
};

}  // namespace chatterinotify
