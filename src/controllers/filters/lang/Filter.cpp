// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/filters/lang/Filter.hpp"

#include "controllers/filters/lang/FilterParser.hpp"

namespace chatterino::filters {

ExpectedStr<Filter> Filter::fromString(const QString &str)
{
    auto result = parseFilter(str);
    if (result)
    {
        auto ret = (*result)->type();
        return Filter{*std::move(result), ret};
    }
    return makeUnexpected(std::move(result).error());
}

Filter::Filter(ExpressionPtr expression, Type returnType)
    : expression_(std::move(expression))
    , returnType_(returnType)
{
}

Type Filter::returnType() const
{
    return this->returnType_;
}

QVariant Filter::execute(const RunContext &context) const
{
    return this->expression_->run(context);
}

QString Filter::filterString() const
{
    return this->expression_->filterString();
}

}  // namespace chatterino::filters
