// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "controllers/filters/lang/Types.hpp"
#include "util/Expected.hpp"

#include <QString>
#include <QVariant>

#include <memory>
#include <vector>

namespace chatterino {
struct Message;
class Channel;
}  // namespace chatterino

namespace chatterino::filters {

struct RunContext {
    const Message &message;
    Channel *channel;
};

class Expression
{
public:
    virtual ~Expression() = default;

    virtual QString filterString() const = 0;

    virtual QVariant run(const RunContext &ctx) = 0;
    virtual QVariant asConstant() const;
    virtual Type type() const = 0;
};

using ExpressionPtr = std::unique_ptr<Expression>;
using ExpressionList = std::vector<std::unique_ptr<Expression>>;

using CreateResult = ExpectedStr<ExpressionPtr>;

}  // namespace chatterino::filters
