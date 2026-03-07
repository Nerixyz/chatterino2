// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/filters/lang/expressions/ListExpression.hpp"

namespace chatterino::filters {

namespace {

struct ListExpressionBase : public Expression {
    ListExpressionBase(ExpressionList list)
        : expressions(std::move(list))
    {
    }

    QString filterString() const override
    {
        QStringList strings;
        for (const auto &exp : this->expressions)
        {
            strings.append(exp->filterString());
        }
        return QString("{%1}").arg(strings.join(", "));
    }

    ExpressionList expressions;
};

struct ConstListExpression final : public ListExpressionBase {
    ConstListExpression(ExpressionList list, Type type, QVariant value)
        : ListExpressionBase(std::move(list))
        , type_(type)
        , value(std::move(value))
    {
    }

    QVariant run(const RunContext & /*ctx*/) override
    {
        return this->value;
    }

    QVariant asConstant() const override
    {
        return this->value;
    }

    Type type() const override
    {
        return this->type_;
    }

    Type type_;  // NOLINT(readability-identifier-naming)
    QVariant value;
};

struct DynStringListExpression final : public ListExpressionBase {
    DynStringListExpression(ExpressionList list)
        : ListExpressionBase(std::move(list))
    {
    }

    QVariant run(const RunContext &ctx) override
    {
        QStringList l;
        for (const auto &expr : this->expressions)
        {
            l.emplace_back(expr->run(ctx).toString());
        }
        return l;
    }

    Type type() const override
    {
        return Type::StringList;
    }
};

struct DynVariantListExpression final : public ListExpressionBase {
    DynVariantListExpression(ExpressionList list)
        : ListExpressionBase(std::move(list))
    {
    }

    QVariant run(const RunContext &ctx) override
    {
        QVariantList l;
        for (const auto &expr : this->expressions)
        {
            l.emplace_back(expr->run(ctx));
        }
        return l;
    }

    Type type() const override
    {
        return Type::List;
    }
};

struct DynMatchingSpecifierExpression final : public ListExpressionBase {
    DynMatchingSpecifierExpression(ExpressionList list)
        : ListExpressionBase(std::move(list))
    {
        assert(this->expressions.size() == 2);
    }

    QVariant run(const RunContext &ctx) override
    {
        auto re = this->expressions[0]->run(ctx).toRegularExpression();
        auto idx = this->expressions[1]->run(ctx).toInt();
        return QVariant::fromValue(std::pair{re, idx});
    }

    Type type() const override
    {
        return Type::MatchingSpecifier;
    }
};

}  // namespace

CreateResult createListExpression(ExpressionList list)
{
    if (list.empty())
    {
        return std::make_unique<ConstListExpression>(
            std::move(list), Type::List, QVariantList());
    }

    // special case for {regex, index}
    if (list.size() == 2 && list[0]->type() == Type::RegularExpression &&
        list[1]->type() == Type::Int)
    {
        auto re = list[0]->asConstant();
        auto idx = list[1]->asConstant();
        if (re.isValid() && idx.isValid())
        {
            return std::make_unique<ConstListExpression>(
                std::move(list), Type::MatchingSpecifier,
                QVariant::fromValue(
                    std::pair{re.toRegularExpression(), idx.toInt()}));
        }
        return std::make_unique<DynMatchingSpecifierExpression>(
            std::move(list));
    }

    bool allStrings = std::ranges::all_of(list, [](const auto &it) {
        return it->type() == Type::String;
    });
    if (allStrings)
    {
        QStringList constList;
        for (const auto &it : list)
        {
            auto val = it->asConstant();
            if (!val.isValid())
            {
                return std::make_unique<DynStringListExpression>(
                    std::move(list));
            }
            constList.emplace_back(val.toString());
        }
        return std::make_unique<ConstListExpression>(
            std::move(list), Type::StringList, std::move(constList));
    }

    QVariantList constList;
    for (const auto &it : list)
    {
        auto val = it->asConstant();
        if (!val.isValid())
        {
            return std::make_unique<DynVariantListExpression>(std::move(list));
        }
        constList.emplace_back(val);
    }
    return std::make_unique<ConstListExpression>(std::move(list), Type::List,
                                                 std::move(constList));
}

}  // namespace chatterino::filters
