#include "messages/Link.hpp"

#include "util/QMagicEnum.hpp"

#include <QJsonObject>


namespace chatterino {

using namespace Qt::StringLiterals;

Link::Link()
    : type(None)
    , value(QString())
{
}

Link::Link(Type _type, const QString &_value)
    : type(_type)
    , value(_value)
{
}

bool Link::isValid() const
{
    return this->type != None;
}

bool Link::isUrl() const
{
    return this->type == Url;
}

QJsonObject Link::toJson() const
{
    return {
        {"type"_L1, qmagicenum::enumNameString(this->type)},
        {"value"_L1, this->value},
    };
}

}  // namespace chatterino
