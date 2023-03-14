
#include "singletons/Theme.hpp"

#include "Application.hpp"
#include "common/QLogging.hpp"
#include "singletons/Resources.hpp"

#include <QColor>
#include <QFile>
#include <QStringView>
#include <QUrl>

#include <cmath>
#include <cstring>

#define LOOKUP_COLOR_COUNT 360

namespace {

const QSet<QString> SHIPPED_THEMES = {"Black", "Dark", "Light", "White"};

QString getThemePath(const QString &name)
{
    if (SHIPPED_THEMES.contains(name))
    {
        return QStringLiteral(":/themes/%1.c2theme").arg(name);
    }
    return name;
}

bool applyTheme(const QString &path, auto metaCb, auto colorCb)
{
    QFile file(path);
    if (!file.open(QFile::ReadOnly))
    {
        return false;
    }

    enum class Mode : uint8_t {
        None,
        Meta,
        Color,
    };
    auto mode = Mode::None;
    while (!file.atEnd())
    {
        auto line = file.readLine();
        line.chop(1);
        if (line.endsWith('\r'))
        {
            line.chop(1);
        }

        if (line == "@colors")
        {
            mode = Mode::Color;
            continue;
        }
        if (line == "@meta")
        {
            mode = Mode::Meta;
            continue;
        }

        auto idx = line.indexOf('=');
        if (idx < 0 || mode == Mode::None)
        {
            continue;  // invalid line - ignore
        }
        auto key = line.mid(0, idx);
        auto value = line.mid(idx + 1);

        switch (mode)
        {
            case Mode::Color: {
                colorCb(key, value);
            }
            break;
            case Mode::Meta: {
                metaCb(key, value);
            }
            break;
            default:
                break;
        }
    }
    return true;
}
}  // namespace

namespace chatterino {

bool Theme::isLightTheme() const
{
    return this->iconSet_ == IconSet::Light;
}

Theme::Theme()
{
    this->update();

    this->themeName.connectSimple(
        [this](auto) {
            this->update();
        },
        false);
    this->themeHue.connectSimple(
        [this](auto) {
            this->update();
        },
        false);
}

void Theme::update()
{
    this->actuallyUpdate(this->themeName);

    this->updated.invoke();
}

// hue: theme color (0 - 1)
// multiplier: 1 = white, 0.8 = light, -0.8 dark, -1 black
void Theme::actuallyUpdate(const QString &newThemeName)
{
    auto metaCb = [this](const auto &key, const auto &value) {
        if (key == "iconset")
        {
            if (value == "light")
            {
                this->iconSet_ = IconSet::Light;
            }
            else
            {
                this->iconSet_ = IconSet::Dark;
            }
        }
        else if (key == "reset" && value == "true")
        {
            this->reset();
        }
    };
    auto colorCb = [this](const auto &key, const auto &value) {
        if (!this->setColor(key, QColor(QString(value))))
        {
            qCWarning(chatterinoApp)
                << "Couldn't set color - key:" << key << "value:" << value;
        }
    };

    auto themePath = getThemePath(newThemeName);
    if (!applyTheme(themePath, metaCb, colorCb))
    {
        qCWarning(chatterinoApp)
            << "Failed to apply theme - path:" << themePath;
        this->reset();
    }
    this->applyChanges();

    // Copy button
    if (this->isLightTheme())
    {
        this->buttons.copy = getResources().buttons.copyDark;
        this->buttons.pin = getResources().buttons.pinDisabledDark;
    }
    else
    {
        this->buttons.copy = getResources().buttons.copyLight;
        this->buttons.pin = getResources().buttons.pinDisabledLight;
    }

    this->inputStyleSheet_ =
        "background:" + this->splits.input.background.name() + ";" +
        "border:" + this->tabs.selected.backgrounds.regular.name() + ";" +
        "color:" + this->messages.textColors.regular.name() + ";" +
        "selection-background-color:" +
        (this->isLightTheme() ? "#68B1FF"
                              : this->tabs.selected.backgrounds.regular.name());
}

QString Theme::inputStyleSheet() const
{
    return this->inputStyleSheet_;
}

void Theme::normalizeColor(QColor &color)
{
    if (this->isLightTheme())
    {
        if (color.lightnessF() > 0.5)
        {
            color.setHslF(color.hueF(), color.saturationF(), 0.5);
        }

        if (color.lightnessF() > 0.4 && color.hueF() > 0.1 &&
            color.hueF() < 0.33333)
        {
            color.setHslF(color.hueF(), color.saturationF(),
                          color.lightnessF() - sin((color.hueF() - 0.1) /
                                                   (0.3333 - 0.1) * 3.14159) *
                                                   color.saturationF() * 0.4);
        }
    }
    else
    {
        if (color.lightnessF() < 0.5)
        {
            color.setHslF(color.hueF(), color.saturationF(), 0.5);
        }

        if (color.lightnessF() < 0.6 && color.hueF() > 0.54444 &&
            color.hueF() < 0.83333)
        {
            color.setHslF(
                color.hueF(), color.saturationF(),
                color.lightnessF() + sin((color.hueF() - 0.54444) /
                                         (0.8333 - 0.54444) * 3.14159) *
                                         color.saturationF() * 0.4);
        }
    }
}

Theme *getTheme()
{
    return getApp()->themes;
}

}  // namespace chatterino
