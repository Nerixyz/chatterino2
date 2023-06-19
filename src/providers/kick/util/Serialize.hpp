#pragma once

#include <QString>
#include <QtGlobal>
#include <rapidjson/document.h>

namespace chatterino::ser {

class QStringWrapper
{
public:
    using Ch = char16_t;

    QStringWrapper(QString &s)
        : s_(s)
    {
    }
    QStringWrapper(const QStringWrapper &) = delete;
    QStringWrapper &operator=(const QStringWrapper &) = delete;

    Ch Peek() const
    {
        Q_ASSERT(false);
        return u'\0';
    }

    Ch Take()
    {
        Q_ASSERT(false);
        return u'\0';
    }

    size_t Tell() const
    {
    }

    Ch *PutBegin()
    {
        return nullptr;
    }

    void Put(Ch c)
    {
        s_.append(QChar(c));
    }

    void Flush()
    {
    }

    size_t PutEnd(Ch * /*begin*/)
    {
        return 0;
    }

    QString &s_;
};

inline rapidjson::GenericStringRef<char16_t> stringRef(const QString &s)
{
    return rapidjson::StringRef((const char16_t *)s.constData(), s.length());
}

}  // namespace chatterino::ser
