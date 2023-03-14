#pragma once

#include "common/ChatterinoSetting.hpp"
#include "common/Singleton.hpp"
#include "GeneratedTheme.hpp"

#include <pajlada/settings/setting.hpp>
#include <QBrush>
#include <QColor>

namespace chatterino {

class WindowManager;

class Theme final : public Singleton, public theme::GeneratedTheme
{
public:
    Theme();

    bool isLightTheme() const;
    QString inputStyleSheet() const;

    struct {
        QPixmap copy;
        QPixmap pin;
    } buttons;

    enum class IconSet : uint8_t {
        Dark,
        Light,
    };

    void normalizeColor(QColor &color);
    void update();

    pajlada::Signals::NoArgSignal updated;

    QStringSetting themeName{"/appearance/theme/name", "Dark"};
    DoubleSetting themeHue{"/appearance/theme/hue", 0.0};

private:
    IconSet iconSet_ = IconSet::Dark;
    QString inputStyleSheet_;

    void actuallyUpdate(const QString &newThemeName);

    pajlada::Signals::NoArgSignal repaintVisibleChatWidgets_;

    friend class WindowManager;
};

Theme *getTheme();
}  // namespace chatterino
