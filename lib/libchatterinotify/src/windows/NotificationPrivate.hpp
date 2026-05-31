#include <QUrl>

#include <winrt/windows.data.xml.dom.h>

namespace chatterinotify {

class NotificationPrivate
{
public:
    winrt::Windows::Data::Xml::Dom::XmlDocument doc;

    void setIconPath(const QUrl &url);
};

}  // namespace chatterinotify
