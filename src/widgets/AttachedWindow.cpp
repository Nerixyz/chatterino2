#include "widgets/AttachedWindow.hpp"

#ifdef USEWINSDK

#    include "Application.hpp"
#    include "common/QLogging.hpp"
#    include "singletons/Settings.hpp"
#    include "util/DebugCount.hpp"
#    include "widgets/splits/Split.hpp"

#    include <QTimer>
#    include <QVBoxLayout>

#    include <memory>

#    ifdef USEWINSDK
#        include "util/WindowsHelper.hpp"

// clang-format off
// don't even think about reordering these
#    include <Windows.h>
#    include <Psapi.h>
// clang-format on
#        pragma comment(lib, "Dwmapi.lib")
#    endif

namespace {

constexpr std::chrono::milliseconds RESIZE_TIMER_FAST{1};
constexpr std::chrono::milliseconds RESIZE_TIMER_SLOW{1000};

QRect qrectFromRECT(RECT r)
{
    return {
        r.left,
        r.top,
        r.right - r.left,
        r.bottom - r.top,
    };
}

using TaskbarHwnds = std::vector<HWND>;

BOOL CALLBACK enumWindows(HWND hwnd, LPARAM lParam)
{
    constexpr int length = 16;

    std::array<WCHAR, length + 1> className{};
    GetClassName(hwnd, className.data(), length);

    if (lstrcmp(className.data(), L"Shell_TrayWnd") == 0 ||
        lstrcmp(className.data(), L"Shell_Secondary") == 0)
    {
        std::bit_cast<TaskbarHwnds *>(lParam)->push_back(hwnd);
    }

    return TRUE;
}

}  // namespace

namespace chatterino {

AttachedWindow::AttachedWindow(HWND _target)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::Window)
    , target_(_target)
{
    QLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    this->setLayout(layout);

    auto *split = new Split(this);
    this->ui_.split = split;
    split->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::MinimumExpanding);
    layout->addWidget(split);

    DebugCount::increase("attached window");
}

AttachedWindow::~AttachedWindow()
{
    for (auto it = items.begin(); it != items.end(); it++)
    {
        if (it->window == this)
        {
            items.erase(it);
            break;
        }
    }

    DebugCount::decrease("attached window");
}

AttachedWindow *AttachedWindow::get(HWND target, const GetArgs &args)
{
    AttachedWindow *window = [&]() {
        for (Item &item : items)
        {
            if (item.hwnd == target)
            {
                return item.window;
            }
        }

        auto *window = new AttachedWindow(target);
        items.push_back(Item{
            .hwnd = target,
            .window = window,
            .winId = args.winId,
        });
        return window;
    }();

    bool show = true;
    QSize size = window->size();

    window->fullscreen_ = args.fullscreen;

    window->x_ = args.x;
    window->pixelRatio_ = args.pixelRatio;

    if (args.height != -1)
    {
        if (args.height == 0)
        {
            window->hide();
            show = false;
        }
        else
        {
            window->height_ = args.height;
            size.setHeight(args.height);
        }
    }
    if (args.width != -1)
    {
        if (args.width == 0)
        {
            window->hide();
            show = false;
        }
        else
        {
            window->width_ = args.width;
            size.setWidth(args.width);
        }
    }

    if (show)
    {
        window->updateWindowRect(window->target_);
        window->show();
    }

    return window;
}

#    ifdef USEWINSDK
AttachedWindow *AttachedWindow::getForeground(const GetArgs &args)
{
    return AttachedWindow::get(::GetForegroundWindow(), args);
}
#    endif

void AttachedWindow::detach(const QString &winId)
{
    for (Item &item : items)
    {
        if (item.winId == winId)
        {
            item.window->deleteLater();
        }
    }
}

// technically, const is fine here, but semantically, it doesn't make sense
// NOLINTNEXTLINE(readability-make-member-function-const)
void AttachedWindow::setChannel(ChannelPtr channel)
{
    if (this->ui_.split->getChannel() == channel)
    {
        return;
    }
    this->ui_.split->setChannel(std::move(channel));
}

void AttachedWindow::showEvent(QShowEvent * /*event*/)
{
    this->attachToHwnd(this->target_);
}

void AttachedWindow::attachToHwnd(HWND attached)
{
    if (this->attached_)
    {
        if (this->validProcessName_ &&
            this->timer_.intervalAsDuration() == RESIZE_TIMER_SLOW)
        {
            // force an update immediately
            this->updateWindowRect(attached);
        }

        return;
    }

    this->attached_ = true;

    // FAST TIMER - used to resize/reorder windows
    // once the attached-to window remains in its position, this slows down to
    // RESIZE_TIMER_SLOW.
    this->timer_.setInterval(RESIZE_TIMER_FAST);
    QObject::connect(&this->timer_, &QTimer::timeout, [this, attached] {
        // check process id
        if (!this->validProcessName_)
        {
            DWORD processId{};
            ::GetWindowThreadProcessId(attached, &processId);

            HANDLE process = ::OpenProcess(
                PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, processId);

            std::array<TCHAR, 512> filename{};
            DWORD filenameLength = ::GetModuleFileNameEx(
                process, nullptr, filename.data(), filename.size());
            auto qfilename = QString::fromWCharArray(
                filename.data(),
                static_cast<QString::size_type>(filenameLength));

            if (!getSettings()->attachExtensionToAnyProcess)
            {
                // We don't attach to non-browser processes by default.
                if (!qfilename.endsWith("chrome.exe") &&
                    !qfilename.endsWith("firefox.exe") &&
                    !qfilename.endsWith("vivaldi.exe") &&
                    !qfilename.endsWith("opera.exe") &&
                    !qfilename.endsWith("msedge.exe") &&
                    !qfilename.endsWith("brave.exe"))

                {
                    qCWarning(chatterinoWidget)
                        << "NM Illegal caller" << qfilename;
                    this->timer_.stop();
                    this->deleteLater();
                    return;
                }
            }
            this->validProcessName_ = true;
        }

        this->updateWindowRect(attached);
    });

    this->timer_.start();

    // SLOW TIMER - used to hide taskbar behind fullscreen window
    this->slowTimer_.setInterval(2000);
    QObject::connect(&this->slowTimer_, &QTimer::timeout, [this, attached] {
        if (this->fullscreen_)
        {
            std::vector<HWND> taskbarHwnds;
            ::EnumWindows(&enumWindows, std::bit_cast<LPARAM>(&taskbarHwnds));

            for (auto *taskbarHwnd : taskbarHwnds)
            {
                ::SetWindowPos(taskbarHwnd,
                               GetNextWindow(attached, GW_HWNDNEXT), 0, 0, 0, 0,
                               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
    });
    this->slowTimer_.start();
}

void AttachedWindow::updateWindowRect(HWND attached)
{
    // NOLINTNEXTLINE
    auto *hwnd = reinterpret_cast<HWND>(this->winId());

    // We get the window rect first so we can close this window when it returns
    // an error. If we query the process first and check the filename then it
    // will return and empty string that doens't match.
    ::SetLastError(0);
    RECT rect;
    ::GetWindowRect(attached, &rect);

    if (::GetLastError() != 0)
    {
        qCWarning(chatterinoWidget) << "NM GetLastError()" << ::GetLastError();

        this->timer_.stop();
        this->deleteLater();
        return;
    }

    // update timer interval
    // if the rect changed recently, change the timer to be every tick
    // if it stayed the same for some time, backoff
    auto qCurrentRect = qrectFromRECT(rect);
    if (qCurrentRect == this->lastAttachedRect_)
    {
        if (this->lastRectEqCounter_ == 0)
        {
            if (this->timer_.intervalAsDuration() != RESIZE_TIMER_SLOW)
            {
                this->timer_.setInterval(RESIZE_TIMER_SLOW);
            }
        }
        else
        {
            this->lastRectEqCounter_--;
        }
    }
    else
    {
        this->lastAttachedRect_ = qCurrentRect;
        this->lastRectEqCounter_ = 512;
        if (this->timer_.intervalAsDuration() != RESIZE_TIMER_FAST)
        {
            this->timer_.setInterval(RESIZE_TIMER_FAST);
        }
    }

    // set the correct z-order
    if (HWND next = ::GetNextWindow(attached, GW_HWNDPREV))
    {
        ::SetWindowPos(hwnd, next ? next : HWND_TOPMOST, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    qreal scale = 1.F;
    qreal ourScale = 1.F;
    if (auto dpi = getWindowDpi(attached))
    {
        scale = static_cast<qreal>(*dpi) / 96.F;
        ourScale = scale / this->devicePixelRatio();

        for (auto *w : this->ui_.split->findChildren<BaseWidget *>())
        {
            w->setOverrideScale(ourScale);
        }
        this->ui_.split->setOverrideScale(ourScale);
    }

    if (this->height_ == -1 || this->pixelRatio_ == -1)
    {
        // old extension versions (older than 3y)
        return;
    }

    this->ui_.split->setFixedWidth(static_cast<int>(this->width_ * ourScale));

    // inset of the window
    int windowInset = this->fullscreen_ ? 0 : 8;

    QRect myRect{
        static_cast<int>(rect.left + (this->x_ * scale * this->pixelRatio_) +
                         windowInset - 2),
        static_cast<int>(rect.bottom - (this->height_ * scale) - windowInset),
        static_cast<int>(this->width_ * scale),
        static_cast<int>(this->height_ * scale),
    };

    RECT myPrevRect;
    ::GetWindowRect(hwnd, &myPrevRect);

    // avoid moving the window if nothing changed
    // (accounts for QRect's off-by one definition)
    if (myPrevRect.left == myRect.left() && myPrevRect.top == myRect.top() &&
        myPrevRect.right == myRect.right() + 1 &&
        myPrevRect.bottom == myRect.bottom() + 1)
    {
        return;
    }

    ::MoveWindow(hwnd, myRect.left(), myRect.top(), myRect.width(),
                 myRect.height(), TRUE);
}

std::vector<AttachedWindow::Item> AttachedWindow::items;

}  // namespace chatterino

#endif
