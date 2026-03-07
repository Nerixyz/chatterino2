// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "controllers/filters/lang/expressions/Expression.hpp"
#include "controllers/filters/lang/Tokenizer.hpp"

namespace chatterino::filters {

CreateResult createBinaryExpression(TokenType tt, ExpressionPtr lhs,
                                    ExpressionPtr rhs);

}  // namespace chatterino::filters
