#pragma once

#include "Application.hpp"
#include "controllers/tracing/TracingController.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "util/TypeName.hpp"

#include <QCoreApplication>

#include <chrono>

namespace chatterino {

// Taken from
// https://stackoverflow.com/questions/21646467/how-to-execute-a-functor-or-a-lambda-in-a-given-thread-in-qt-gcd-style
// Qt 5/4 - preferred, has least allocations
template <typename F>
static void postToThread(F &&fun, QObject *obj = QCoreApplication::instance())
{
    struct Event : public QEvent {
        using Fun = typename std::decay<F>::type;
        Fun fun;
        Event(Fun &&fun)
            : QEvent(QEvent::None)
            , fun(std::move(fun))
        {
        }
        Event(const Fun &fun)
            : QEvent(QEvent::None)
            , fun(fun)
        {
        }
        ~Event() override
        {
            auto *app = getApp();
            if (app)
            {
                app->getTracing()->asyncBegin(type_name<Fun>(), this);
            }
            fun();
            if (app)
            {
                app->getTracing()->asyncEnd(type_name<Fun>(), this);
            }
        }
    };
    QCoreApplication::postEvent(obj, new Event(std::forward<F>(fun)));
}

template <typename F>
static void runInGuiThread(F &&fun)
{
    if (isGuiThread())
    {
        fun();
    }
    else
    {
        postToThread(fun);
    }
}

template <typename F>
inline void postToGuiThread(F &&fun)
{
    assert(!isGuiThread() &&
           "postToGuiThread must be called from a non-GUI thread");

    postToThread(std::forward<F>(fun));
}

}  // namespace chatterino
