#pragma once

#include <QHash>
#include <QString>

#include <functional>

namespace std {

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
template <>
struct hash<QString> {
    std::size_t operator()(const QString &s) const
    {
        return qHash(s);
    }
};
#endif

}  // namespace std

namespace boost {

inline size_t hash_value(const QString &s)
{
    return qHash(s);
}

}  // namespace boost
