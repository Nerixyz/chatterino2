#pragma once

#include <QHash>
#include <QString>

#include <functional>

#define QStringAlias(name)                                   \
    namespace chatterino {                                   \
    struct name {                                            \
        QString string;                                      \
        bool operator==(const name &other) const             \
        {                                                    \
            return this->string == other.string;             \
        }                                                    \
        bool operator!=(const name &other) const             \
        {                                                    \
            return this->string != other.string;             \
        }                                                    \
    };                                                       \
    } /* namespace chatterino */                             \
    namespace std {                                          \
    template <>                                              \
    struct hash<chatterino::name> {                          \
        size_t operator()(const chatterino::name &s) const   \
        {                                                    \
            return qHash(s.string);                          \
        }                                                    \
    };                                                       \
    } /* namespace std */                                    \
    namespace boost {                                        \
    inline std::size_t hash_value(const chatterino::name &s) \
    {                                                        \
        return qHash(s.string);                              \
    }                                                        \
    } /* namespace boost */

QStringAlias(UserName);
QStringAlias(UserId);
QStringAlias(Url);
QStringAlias(Tooltip);
QStringAlias(EmoteId);
QStringAlias(EmoteName);
QStringAlias(EmoteAuthor);
