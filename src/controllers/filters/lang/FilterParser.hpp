// SPDX-FileCopyrightText: 2020 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "controllers/filters/lang/expressions/Expression.hpp"
#include "controllers/filters/lang/Tokenizer.hpp"
#include "controllers/filters/lang/Types.hpp"

namespace chatterino::filters {

ExpectedStr<ExpressionPtr> parseFilter(const QString &text);

class FilterParser
{
public:
    FilterParser(const QString &text);

    CreateResult parse();

private:
    CreateResult parseExpression(bool top = false);
    CreateResult parseAnd();
    CreateResult parseUnary();
    CreateResult parseParentheses();
    CreateResult parseCondition();
    CreateResult parseValue();
    CreateResult parseList();

    QString text_;
    Tokenizer tokenizer_;
};

}  // namespace chatterino::filters
