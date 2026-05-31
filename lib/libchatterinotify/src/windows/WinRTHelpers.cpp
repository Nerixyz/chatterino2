#include "WinrtHelpers.hpp"

namespace chatterinotify::helpers {

QString hstrToQt(const winrt::hstring &h)
{
    return QString::fromWCharArray(h.c_str(), h.size());
}

winrt::hstring qtToHstr(const QString &qs)
{
    static_assert(sizeof(ushort) == sizeof(wchar_t));
    return {
        reinterpret_cast<const wchar_t *>(qs.utf16()),  // NOLINT
        static_cast<winrt::hstring::size_type>(qs.length()),
    };
}

}  // namespace chatterinotify::helpers
