// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/filters/lang/expressions/UnaryOperation.hpp"

namespace chatterino::filters {

namespace {

struct NotExpression final : public Expression {
    NotExpression(ExpressionPtr rhs)
        : rhs(std::move(rhs))
    {
    }

    QString filterString() const override
    {
        return '!' + this->rhs->filterString();
    }

    QVariant run(const RunContext &ctx) override
    {
        return !this->rhs->run(ctx).toBool();
    }

    QVariant asConstant() const override
    {
        auto r = this->rhs->asConstant();
        if (r.isValid())
        {
            return !r.toBool();
        }
        return {};
    }

    Type type() const override
    {
        return Type::Bool;
    }

    ExpressionPtr rhs;
};

}  // namespace

CreateResult createUnaryExpression(TokenType tt, ExpressionPtr rhs)
{
    switch (tt)
    {
        case NOT:
            if (rhs->type() != Type::Bool)
            {
                return makeUnexpected("not: right side must return a bool");
            }
            return std::make_unique<NotExpression>(std::move(rhs));
        default:
            return makeUnexpected(tokenTypeToInfoString(tt) %
                                  " can't create a unary expression");
    }
}

}  // namespace chatterino::filters
