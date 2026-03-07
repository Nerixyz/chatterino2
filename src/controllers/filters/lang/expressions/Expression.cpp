// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/filters/lang/expressions/Expression.hpp"

namespace chatterino::filters {

QVariant Expression::asConstant() const
{
    return {};
}

}  // namespace chatterino::filters
