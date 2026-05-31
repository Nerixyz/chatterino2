#pragma once

#include <chatterinotify/Log.hpp>
#include <QLoggingCategory>
#include <QString>
#include <winrt/base.h>

namespace chatterinotify::helpers {

QString hstrToQt(const winrt::hstring &h);
winrt::hstring qtToHstr(const QString &qs);

decltype(auto) logExceptions(auto &&cb, const char *context, auto &&...args)
{
    using Result = std::invoke_result_t<decltype(cb), decltype(args)...>;
    try
    {
        return std::invoke(cb, std::forward<decltype(args)>(args)...);
    }
    catch (const std::exception &ex)
    {
        qCWarning(chatterinotify::log) << context << "exception:" << ex.what();
    }
    catch (const winrt::hresult_error &hr)
    {
        qCWarning(chatterinotify::log)
            << context << "hresult:" << hstrToQt(hr.message());
    }

    return Result{};
}

}  // namespace chatterinotify::helpers
