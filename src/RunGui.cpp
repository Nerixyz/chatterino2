// SPDX-FileCopyrightText: 2018 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "RunGui.hpp"

#include "Application.hpp"
#include "common/Args.hpp"
#include "common/Modes.hpp"
#include "common/network/NetworkManager.hpp"
#include "common/QLogging.hpp"
#include "singletons/CrashHandler.hpp"
#include "singletons/Paths.hpp"
#include "singletons/Resources.hpp"
#include "singletons/Settings.hpp"
#include "singletons/Updates.hpp"
#include "util/CombinePath.hpp"
#include "util/SelfCheck.hpp"
#include "util/UnixSignalHandler.hpp"
#include "widgets/dialogs/LastRunCrashDialog.hpp"

#include <private/qcoreapplication_p.h>
#include <private/qobject_p.h>
#include <private/qthread_p.h>
#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>
#include <Qt>
#include <QtConcurrent>
#include <rapidjson/filewritestream.h>
#include <rapidjson/writer.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <tuple>

#ifdef USEWINSDK
#    include "util/WindowsHelper.hpp"
#endif

#ifdef C_USE_BREAKPAD
#    include <QBreakpadHandler.h>
#endif

#ifdef Q_OS_MAC
#    include "corefoundation/CFBundle.h"
#endif

// Forward declaration (Qt doesn't declare this in headers)
// NOLINTNEXTLINE(readability-identifier-naming)
extern void qt_set_sequence_auto_mnemonic(bool b);

template <>
struct magic_enum::customize::enum_range<QEvent::Type> {
    constexpr static int min = QEvent::None;                   // required
    constexpr static int max = QEvent::SafeAreaMarginsChange;  // required
};

namespace chatterino {
namespace {
void installCustomPalette()
{
    // borrowed from
    // https://stackoverflow.com/questions/15035767/is-the-qt-5-dark-fusion-theme-available-for-windows
    auto dark = QApplication::palette();

    dark.setColor(QPalette::Window, QColor(22, 22, 22));
    dark.setColor(QPalette::WindowText, Qt::white);
    dark.setColor(QPalette::Text, Qt::white);
    dark.setColor(QPalette::Base, QColor("#333"));
    dark.setColor(QPalette::AlternateBase, QColor("#444"));
    dark.setColor(QPalette::ToolTipBase, Qt::white);
    dark.setColor(QPalette::ToolTipText, Qt::black);
    dark.setColor(QPalette::Dark, QColor(35, 35, 35));
    dark.setColor(QPalette::Shadow, QColor(20, 20, 20));
    dark.setColor(QPalette::Button, QColor(70, 70, 70));
    dark.setColor(QPalette::ButtonText, Qt::white);
    dark.setColor(QPalette::BrightText, Qt::red);
    dark.setColor(QPalette::Link, QColor(42, 130, 218));
    dark.setColor(QPalette::Highlight, QColor(42, 130, 218));
    dark.setColor(QPalette::HighlightedText, Qt::white);
    dark.setColor(QPalette::PlaceholderText, QColor(127, 127, 127));

    dark.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
    dark.setColor(QPalette::Disabled, QPalette::HighlightedText,
                  QColor(127, 127, 127));
    dark.setColor(QPalette::Disabled, QPalette::ButtonText,
                  QColor(127, 127, 127));
    dark.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    dark.setColor(QPalette::Disabled, QPalette::WindowText,
                  QColor(127, 127, 127));

    QApplication::setPalette(dark);
}

bool doNotify(QObject *receiver, QEvent *event)
{
    Q_ASSERT(event);

    // ### Qt 7: turn into an assert
    if (receiver == nullptr)
    {  // serious error
        qWarning("QCoreApplication::notify: Unexpected null receiver");
        return true;
    }

#ifndef QT_NO_DEBUG
    QCoreApplicationPrivate::checkReceiverThread(receiver);
#endif

    return receiver->isWidgetType()
               ? false
               : QCoreApplicationPrivate::notify_helper(receiver, event);
}

class EventTracer : public QObject
{
public:
    /// Handler for QInternal::EventNotifyCallback.
    ///
    /// This is called by Qt before an event is handled:
    /// https://github.com/qt/qtbase/blob/9d046435577d105406681df4a2440aea517af485/src/corelib/kernel/qcoreapplication.cpp#L1113-L1119
    static bool cb(void **args)
    {
        auto *receiver = static_cast<QObject *>(args[0]);
        auto *event = static_cast<QEvent *>(args[1]);
        auto *result = static_cast<bool *>(args[2]);
        const auto *eventTy = &typeid(*event);
        QEvent::Type ty = event->type();
        auto tid = static_cast<int32_t>(
            reinterpret_cast<size_t>(QThread::currentThreadId()));

        // Hack: Get the d_ptr of `receiver`. We can't access that for other
        // classes directly, because we're not a friend. However, since we
        // derive from `QObject` we can get a member pointer and access it this
        // way.
        auto dPtrMember = &QObject::d_ptr;
        auto *d = reinterpret_cast<QObjectPrivate *>(
            qGetPtrHelper(receiver->*dPtrMember));
        auto *threadData = d->threadData.loadAcquire();
        bool selfRequired = threadData->requiresCoreApplication;

        // This mimics the code in QCoreApplication::notifyInternal2:
        // https://github.com/qt/qtbase/blob/9d046435577d105406681df4a2440aea517af485/src/corelib/kernel/qcoreapplication.cpp#L1121-L1129
        // ```cpp
        //     QScopedScopeLevelCounter scopeLevelCounter(threadData);
        //     if (!selfRequired)
        //          return doNotify(receiver, event);
        //     return qApp->notify(receiver, event);
        // ```
        QScopedScopeLevelCounter scopeLevelCounter(threadData);
        if (!selfRequired)
        {
            auto start = std::chrono::steady_clock::now();
            *result = doNotify(receiver, event);
            auto end = std::chrono::steady_clock::now();
            {
                std::lock_guard g(STATE->mtx);
                STATE->events.emplace_back(Event{
                    .typ = ty,
                    .dur = static_cast<int32_t>(
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            end - start)
                            .count()),
                    .start = start,
                    .tid = tid,
                    .eventTy = eventTy,
                });
            }
            return true;
        }

        auto start = std::chrono::steady_clock::now();
        *result = qApp->notify(receiver, event);
        auto end = std::chrono::steady_clock::now();
        {
            std::lock_guard g(STATE->mtx);
            STATE->events.emplace_back(Event{
                .typ = ty,
                .dur = static_cast<int32_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(end -
                                                                          start)
                        .count()),
                .start = start,
                .tid = tid,
                .eventTy = eventTy,
            });
        }
        return true;
    }

    struct Event {
        QEvent::Type typ{};
        int32_t dur = 0;
        std::chrono::steady_clock::time_point start;
        int32_t tid = 0;
        const std::type_info *eventTy{};
    };
    struct State {
        std::mutex mtx;
        std::vector<Event> events;
    };

    static State *STATE;
    static int MAIN_TID;
};
constinit EventTracer::State *EventTracer::STATE = nullptr;
constinit int EventTracer::MAIN_TID = 0;

void initQt(const Args &args)
{
    if (args.useOldScaling)
    {
        qCWarning(chatterinoApp) << "Using old scaling";
        QApplication::setAttribute(Qt::AA_Use96Dpi, true);
    }

#ifdef Q_OS_WIN32
    // Avoid promoting child widgets to child windows
    // This causes bugs with frameless windows as not all child events
    // get sent to the parent - effectively making the window immovable.
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif

    QApplication::setStyle(QStyleFactory::create("Fusion"));

#ifndef Q_OS_MAC
    QApplication::setWindowIcon(QIcon(":/icon.ico"));
#endif

#ifdef Q_OS_MAC
    // On the Mac/Cocoa platform this attribute is enabled by default
    // We override it to ensure shortcuts show in context menus on that platform
    QApplication::setAttribute(Qt::AA_DontShowShortcutsInContextMenus, false);

    // Enable mnemonics (menu hotkeys) on macOS - they are disabled by default
    qt_set_sequence_auto_mnemonic(true);
#endif

    QInternal::registerCallback(QInternal::EventNotifyCallback,
                                &EventTracer::cb);

    installCustomPalette();
}

void showLastCrashDialog(const Args &args, const Paths &paths)
{
    auto *dialog = new LastRunCrashDialog(args, paths);
    // Use exec() over open() to block the app from being loaded
    // and to be able to set the safe mode.
    dialog->exec();
}

#if defined(NDEBUG) && !defined(CHATTERINO_WITH_CRASHPAD)
std::chrono::steady_clock::time_point signalsInitTime;

[[noreturn]] void handleSignal(int signum)
{
    using namespace std::chrono_literals;

    if (std::chrono::steady_clock::now() - signalsInitTime > 30s &&
        getApp()->getCrashHandler()->shouldRecover())
    {
        QProcess proc;

#    ifdef Q_OS_MAC
        // On macOS, programs are bundled into ".app" Application bundles,
        // when restarting Chatterino that bundle should be opened with the "open"
        // terminal command instead of directly starting the underlying executable,
        // as those are 2 different things for the OS and i.e. do not use
        // the same dock icon (resulting in a second Chatterino icon on restarting)
        CFURLRef appUrlRef = CFBundleCopyBundleURL(CFBundleGetMainBundle());
        CFStringRef macPath =
            CFURLCopyFileSystemPath(appUrlRef, kCFURLPOSIXPathStyle);
        const char *pathPtr =
            CFStringGetCStringPtr(macPath, CFStringGetSystemEncoding());

        proc.setProgram("open");
        proc.setArguments({pathPtr, "-n", "--args", "--crash-recovery"});

        CFRelease(appUrlRef);
        CFRelease(macPath);
#    else
        proc.setProgram(QApplication::applicationFilePath());
        proc.setArguments({"--crash-recovery"});
#    endif

        proc.startDetached();
    }

    std::_Exit(signum);
}
#endif

// We want to restart Chatterino when it crashes and the setting is set to
// true.
void initSignalHandler()
{
#if defined(NDEBUG) && !defined(CHATTERINO_WITH_CRASHPAD)
    signalsInitTime = std::chrono::steady_clock::now();

    signal(SIGSEGV, handleSignal);
#endif

#if defined(Q_OS_UNIX)
    auto *sigintHandler = new UnixSignalHandler(SIGINT);
    QObject::connect(sigintHandler, &UnixSignalHandler::signalFired, [] {
        qCInfo(chatterinoApp) << "Received SIGINT, request application quit";
        QApplication::quit();
    });
    auto *sigtermHandler = new UnixSignalHandler(SIGTERM);
    QObject::connect(sigtermHandler, &UnixSignalHandler::signalFired, [] {
        qCInfo(chatterinoApp) << "Received SIGTERM, request application quit";
        QApplication::quit();
    });
#endif
}

// We delete cache files that haven't been modified in 14 days. This strategy may be
// improved in the future.
void clearCache(const QDir &dir)
{
    size_t deletedCount = 0;
    for (const auto &info : dir.entryInfoList(QDir::Files))
    {
        if (info.lastModified().addDays(14) < QDateTime::currentDateTime())
        {
            bool res = QFile(info.absoluteFilePath()).remove();
            if (res)
            {
                ++deletedCount;
            }
        }
    }
    qCDebug(chatterinoCache)
        << "Deleted" << deletedCount << "files in" << dir.path();
}

// We delete all but the five most recent crashdumps. This strategy may be
// improved in the future.
void clearCrashes(QDir dir)
{
    // crashpad crashdumps are stored inside the Crashes/report directory
    if (!dir.cd("reports"))
    {
        // no reports directory exists = no files to delete
        return;
    }

    dir.setNameFilters({"*.dmp"});

    size_t deletedCount = 0;
    // TODO: use std::views::drop once supported by all compilers
    size_t filesToSkip = 5;
    for (auto &&info : dir.entryInfoList(QDir::Files, QDir::Time))
    {
        if (filesToSkip > 0)
        {
            filesToSkip--;
            continue;
        }

        if (QFile(info.absoluteFilePath()).remove())
        {
            deletedCount++;
        }
    }
    qCDebug(chatterinoApp) << "Deleted" << deletedCount << "crashdumps";
}
}  // namespace

void runGui(QApplication &a, const Modes &modes, const Paths &paths,
            Settings &settings, const Args &args, Updates &updates)
{
    EventTracer::MAIN_TID = static_cast<int32_t>(
        reinterpret_cast<size_t>(QThread::currentThreadId()));
    EventTracer::STATE = new EventTracer::State();
    initQt(args);
    initResources();
    initSignalHandler();

#ifdef Q_OS_WIN
    if (args.crashRecovery)
    {
        showLastCrashDialog(args, paths);
    }
#endif

    selfcheck::checkWebp();

    updates.deleteOldFiles();

    // Clear the cache 1 minute after start.
    QTimer::singleShot(60 * 1000, [cachePath = paths.cacheDirectory(),
                                   crashDirectory = paths.crashdumpDirectory,
                                   avatarPath = paths.twitchProfileAvatars] {
        std::ignore = QtConcurrent::run([cachePath] {
            clearCache(cachePath);
        });
        std::ignore = QtConcurrent::run([avatarPath] {
            clearCache(avatarPath);
        });
        std::ignore = QtConcurrent::run([crashDirectory] {
            clearCrashes(crashDirectory);
        });
    });

    chatterino::NetworkManager::init();
    updates.checkForUpdates();

    QObject::connect(qApp, &QApplication::aboutToQuit, [] {
        auto *app = dynamic_cast<Application *>(tryGetApp());
        assert(app != nullptr);
        app->aboutToQuit();

        getSettings()->requestSave();
        getSettings()->disableSave();

        app->stop();
    });

    Application app(settings, paths, args, updates);
    app.initialize(settings, modes, paths);
    app.run();

    chatterino::NetworkManager::deinit();

#ifdef USEWINSDK
    // flushing windows clipboard to keep copied messages
    flushClipboard();
#endif

    // Uses Google's trace format.
    FILE *fp = fopen("events.json", "wb+");

    std::array<char, 65536> writeBuffer{};
    rapidjson::FileWriteStream os(fp, writeBuffer.data(), writeBuffer.size());

    rapidjson::Writer<rapidjson::FileWriteStream> writer(os);
    writer.StartArray();
    {
        std::lock_guard g(EventTracer::STATE->mtx);
        for (const auto &el : EventTracer::STATE->events)
        {
            // Ignore anything below 10ms.
            if (el.dur < 1000 * 10)
            {
                continue;
            }
            writer.StartObject();
            std::string_view n = magic_enum::enum_name(el.typ);
            writer.Key("cat");
            std::string eventName;
            if (n.empty())
            {
                eventName = std::to_string(el.typ);
                writer.String(eventName);
            }
            else
            {
                eventName += n;
                writer.String(n.data(), n.size());
            }
            eventName += " (";
            eventName += el.eventTy->name();
            eventName += ')';
            writer.Key("name");
            writer.String(eventName);
            writer.Key("ph");
            writer.String("X");
            writer.Key("ts");
            writer.Int64(std::chrono::duration_cast<std::chrono::microseconds>(
                             el.start.time_since_epoch())
                             .count());
            writer.Key("dur");
            writer.Int(el.dur);
            writer.Key("tid");
            writer.Int(el.tid);
            writer.EndObject();
        }
    }
    writer.EndArray();

    (void)fclose(fp);
}

}  // namespace chatterino
