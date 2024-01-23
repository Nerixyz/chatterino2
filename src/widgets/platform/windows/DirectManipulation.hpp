#pragma once

#include <boost/signals2/connection.hpp>
#include <directmanipulation.h>
#include <QObject>
#include <QWidget>
#include <Windows.h>
#include <winrt/base.h>

namespace chatterino {

class BaseWindow;

namespace windows {

    class DirectManipulation : public QObject
    {
        Q_OBJECT

    public:
        struct Event {
            enum class Type : std::uint8_t {
                ScrollBegin,
                Scroll,
                ScrollEnd,
                MoveBegin,
                Move,
                MoveEnd,
            };
            QPoint delta;
            Type type{};
        };

        DirectManipulation(QWidget *parent);
        ~DirectManipulation() override;
        DirectManipulation(const DirectManipulation &) = delete;
        DirectManipulation(DirectManipulation &&) = delete;
        DirectManipulation &operator=(const DirectManipulation &) = delete;
        DirectManipulation &operator=(DirectManipulation &&) = delete;

        void updateWindowSize(QSize size);
        void onPointerHitTest(WPARAM wParam);

    signals:
        void manipulationEvent(const Event &event);

    private:
        class EventHandler;

        void shutdown();

        winrt::com_ptr<IDirectManipulationManager> manager;
        winrt::com_ptr<IDirectManipulationUpdateManager> updateManager;
        winrt::com_ptr<IDirectManipulationViewport> viewport;
        winrt::com_ptr<EventHandler> eventHandler;
        HWND window{};
        DWORD viewportEventHandlerCookie = 0;
        QTimer updateTimer;

        boost::signals2::scoped_connection interactingConn;
        boost::signals2::scoped_connection manipulationEventConn;
    };

}  // namespace windows
}  // namespace chatterino
