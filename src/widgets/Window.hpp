#pragma once

#include "widgets/BaseWindow.hpp"

#include <boost/signals2.hpp>
#include <pajlada/settings/setting.hpp>
#include <pajlada/signals/signal.hpp>
#include <pajlada/signals/signalholder.hpp>

class QSystemTrayIcon;

namespace chatterino {

class Theme;
class UpdateDialog;
class SplitNotebook;
class Channel;

enum class WindowType { Main, Popup, Attached };

class Window : public BaseWindow
{
    Q_OBJECT

public:
    explicit Window(WindowType type, QWidget *parent);

    WindowType getType();
    SplitNotebook &getNotebook();

#ifndef Q_OS_MAC
    QSystemTrayIcon *trayIcon();
#endif

    bool handleMinimizeEvent() override;
    bool handleCloseEvent() override;

    pajlada::Signals::NoArgSignal closed;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;
    void themeChangedEvent() override;

private:
    void addCustomTitlebarButtons();
    void addDebugStuff(
        std::map<QString, std::function<QString(std::vector<QString>)>>
            &actions);
    void addShortcuts() override;
    void addLayout();
    void onAccountSelected();
    void addMenuBar();

    WindowType type_;

    SplitNotebook *notebook_;
    EffectLabel *userLabel_ = nullptr;
    std::shared_ptr<UpdateDialog> updateDialogHandle_;

    pajlada::Signals::SignalHolder signalHolder_;
    std::vector<boost::signals2::scoped_connection> bSignals_;

#ifndef Q_OS_MAC
    QSystemTrayIcon *trayIcon_ = nullptr;
#endif

    // this is only used on Windows and only on the main window, for the one used otherwise, see SplitNotebook in Notebook.hpp
    TitleBarButton *streamerModeTitlebarIcon_ = nullptr;
    void updateStreamerModeIcon();

    friend class Notebook;
};

}  // namespace chatterino
