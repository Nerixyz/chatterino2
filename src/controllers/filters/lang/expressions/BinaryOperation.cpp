// SPDX-FileCopyrightText: 2020 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/filters/lang/expressions/BinaryOperation.hpp"

#include <QColor>
#include <QRegularExpression>

using namespace Qt::Literals;

namespace chatterino::filters {

namespace {

template <std::invocable<const QVariant &, const QVariant &> auto Fn>
decltype(auto) invokeBinop(const RunContext &ctx, Expression *lhs,
                           Expression *rhs)
{
    return Fn(lhs->run(ctx), rhs->run(ctx));
}

template <
    std::invocable<const RunContext &, Expression *, Expression *> auto Fn>
decltype(auto) invokeBinop(const RunContext &ctx, Expression *lhs,
                           Expression *rhs)
{
    return Fn(ctx, lhs, rhs);
}

template <TokenType Op, Type Result, auto Fn>
struct BinaryExpression final : public Expression {
    BinaryExpression(ExpressionPtr lhs, ExpressionPtr rhs)
        : lhs(std::move(lhs))
        , rhs(std::move(rhs))
    {
    }

    QVariant run(const RunContext &ctx) override
    {
        return invokeBinop<Fn>(ctx, this->lhs.get(), this->rhs.get());
    }

    Type type() const override
    {
        return Result;
    }

    QString filterString() const override
    {
        const auto opText = [&]() -> QStringView {
            switch (Op)
            {
                case AND:
                    return u"&&";
                case OR:
                    return u"||";
                case PLUS:
                    return u"+";
                case MINUS:
                    return u"-";
                case MULTIPLY:
                    return u"*";
                case DIVIDE:
                    return u"/";
                case MOD:
                    return u"%";
                case EQ:
                    return u"==";
                case NEQ:
                    return u"!=";
                case LT:
                    return u"<";
                case GT:
                    return u">";
                case LTE:
                    return u"<=";
                case GTE:
                    return u">=";
                case CONTAINS:
                    return u"contains";
                case STARTS_WITH:
                    return u"startswith";
                case ENDS_WITH:
                    return u"endswith";
                case MATCH:
                    return u"match";
                default:
                    return {};
            }
        }();
        return '(' % this->lhs->filterString() % ' ' % opText % ' ' %
               this->rhs->filterString() % ')';
    }

    ExpressionPtr lhs;
    ExpressionPtr rhs;
};

template <typename Op, typename T>
QVariant anyBinOp(const QVariant &lhs, const QVariant &rhs)
{
    return Op{}(lhs.value<T>(), rhs.value<T>());
}

template <typename Op>
QVariant binOpOnVariant(const QVariant &lhs, const QVariant &rhs)
{
    return Op{}(lhs, rhs);
}

template <typename Op>
QVariant strStrCompareOp(const QVariant &lhs, const QVariant &rhs)
{
    return Op{}(QString::compare(lhs.value<QString>(), rhs.value<QString>(),
                                 Qt::CaseInsensitive),
                0);
}

QVariant andOp(const RunContext &ctx, Expression *lhs, Expression *rhs)
{
    return lhs->run(ctx).value<bool>() && rhs->run(ctx).value<bool>();
}
QVariant orOp(const RunContext &ctx, Expression *lhs, Expression *rhs)
{
    return lhs->run(ctx).value<bool>() || rhs->run(ctx).value<bool>();
}

template <typename Op>
CreateResult getEqualsImpl(ExpressionPtr lhs, ExpressionPtr rhs)
{
    Type lTyp = lhs->type();
    Type rTyp = rhs->type();

    if (lTyp != rTyp)
    {
        if (lTyp > rTyp)
        {
            std::swap(lTyp, rTyp);
            std::swap(lhs, rhs);
        }

        // ORDER: String < Int
        if (lTyp == Type::String && rTyp == Type::Int)
        {
            return std::make_unique<BinaryExpression<TokenType::EQ, Type::Bool,
                                                     &anyBinOp<Op, QString>>>(
                std::move(lhs), std::move(rhs));
        }
        // ORDER: String < Color
        if (lTyp == Type::String && rTyp == Type::Color)
        {
            return std::make_unique<BinaryExpression<TokenType::EQ, Type::Bool,
                                                     &anyBinOp<Op, QColor>>>(
                std::move(lhs), std::move(rhs));
        }

        return makeUnexpected(u"Can't compare %1 and %2 for equality"_s.arg(
            typeToString(lTyp), typeToString(rTyp)));
    }

    if (lTyp == Type::String)
    {
        return std::make_unique<
            BinaryExpression<TokenType::EQ, Type::Bool, &strStrCompareOp<Op>>>(
            std::move(lhs), std::move(rhs));
    }

    // FIXME: unwrap here?
    return std::make_unique<
        BinaryExpression<TokenType::EQ, Type::Bool, &binOpOnVariant<Op>>>(
        std::move(lhs), std::move(rhs));
}

QString makeTypeError(TokenType tt, Type expL, Type expR, Type gotL, Type gotR)
{
    return u"%1: Type mismatch - expected {lhs=%2, rhs=%3}, got {lhs=%4, rhs=%5}"_s
        .arg(tokenTypeToInfoString(tt), typeToString(expL), typeToString(expR),
             typeToString(gotL), typeToString(gotR));
}

template <TokenType Op, Type Result, auto Fn>
CreateResult argcheckAndMake(Type expL, ExpressionPtr lhs, Type expR,
                             ExpressionPtr rhs)
{
    auto lTyp = lhs->type();
    auto rTyp = rhs->type();
    if (lTyp != expL || rTyp != expR)
    {
        return makeUnexpected(makeTypeError(Op, expL, expR, lTyp, rTyp));
    }
    return std::make_unique<BinaryExpression<Op, Result, Fn>>(std::move(lhs),
                                                              std::move(rhs));
}

template <TokenType Op>
QVariant containsStartsEndsImpl(auto &&lhs, auto &&rhs, auto &&...args)
{
    if constexpr (Op == TokenType::CONTAINS)
    {
        return lhs.contains(std::forward<decltype(rhs)>(rhs), args...);
    }
    else if constexpr (Op == TokenType::STARTS_WITH)
    {
        return lhs.startsWith(std::forward<decltype(rhs)>(rhs), args...);
    }
    else
    {
        static_assert(Op == TokenType::ENDS_WITH);
        return lhs.endsWith(std::forward<decltype(rhs)>(rhs), args...);
    }
}

template <Type LeftTy, TokenType Op>
QVariant containsStartsEnds(const QVariant &lhs, const QVariant &rhs)
{
    if constexpr (LeftTy == Type::String)
    {
        return containsStartsEndsImpl<Op>(lhs.toString(), rhs.toString(),
                                          Qt::CaseInsensitive);
    }
    else if constexpr (LeftTy == Type::List)
    {
        return containsStartsEndsImpl<Op>(lhs.toList(), rhs);
    }
    else
    {
        static_assert(LeftTy == Type::StringList);
        if constexpr (Op == TokenType::CONTAINS)
        {
            return lhs.toStringList().contains(rhs.toString(),
                                               Qt::CaseInsensitive);
        }
        else if constexpr (Op == TokenType::STARTS_WITH)
        {
            auto list = lhs.toStringList();
            return !list.empty() &&
                   list.first().compare(rhs.toString(), Qt::CaseInsensitive) ==
                       0;
        }
        else
        {
            static_assert(Op == TokenType::ENDS_WITH);
            auto list = lhs.toStringList();
            return !list.empty() &&
                   list.last().compare(rhs.toString(), Qt::CaseInsensitive) ==
                       0;
        }
    }
}

template <TokenType Op>
CreateResult makeContainsStartsEnds(ExpressionPtr lhs, ExpressionPtr rhs)
{
    switch (lhs->type())
    {
        case Type::String:
            return argcheckAndMake<Op, Type::Bool,
                                   containsStartsEnds<Type::String, Op>>(
                Type::String, std::move(lhs), Type::String, std::move(rhs));
        case Type::StringList:
            return argcheckAndMake<Op, Type::Bool,
                                   containsStartsEnds<Type::StringList, Op>>(
                Type::StringList, std::move(lhs), Type::String, std::move(rhs));
        case Type::List:
            return std::make_unique<BinaryExpression<
                Op, Type::Bool, containsStartsEnds<Type::List, Op>>>(
                std::move(lhs), std::move(rhs));
        default:
            return makeUnexpected("Unreachable type in containsStartsEnds");
    }
}

}  // namespace

CreateResult createBinaryExpression(TokenType tt, ExpressionPtr lhs,
                                    ExpressionPtr rhs)
{
    auto lTyp = lhs->type();
    auto rTyp = rhs->type();
    switch (tt)
    {
        case AND:
            return argcheckAndMake<AND, Type::Bool, &andOp>(
                Type::Bool, std::move(lhs), Type::Bool, std::move(rhs));
        case OR:
            return argcheckAndMake<OR, Type::Bool, &orOp>(
                Type::Bool, std::move(lhs), Type::Bool, std::move(rhs));
        case EQ:
            return getEqualsImpl<std::equal_to<>>(std::move(lhs),
                                                  std::move(rhs));
        case NEQ:
            return getEqualsImpl<std::not_equal_to<>>(std::move(lhs),
                                                      std::move(rhs));
        case LT:
            return argcheckAndMake<LT, Type::Bool, &anyBinOp<std::less<>, int>>(
                Type::Int, std::move(lhs), Type::Int, std::move(rhs));
        case GT:
            return argcheckAndMake<GT, Type::Bool,
                                   &anyBinOp<std::greater<>, int>>(
                Type::Int, std::move(lhs), Type::Int, std::move(rhs));
        case LTE:
            return argcheckAndMake<LTE, Type::Bool,
                                   &anyBinOp<std::less_equal<>, int>>(
                Type::Int, std::move(lhs), Type::Int, std::move(rhs));
        case GTE:
            return argcheckAndMake<GTE, Type::Bool,
                                   &anyBinOp<std::greater_equal<>, int>>(
                Type::Int, std::move(lhs), Type::Int, std::move(rhs));
        case CONTAINS:
            return makeContainsStartsEnds<CONTAINS>(std::move(lhs),
                                                    std::move(rhs));
        case STARTS_WITH:
            return makeContainsStartsEnds<STARTS_WITH>(std::move(lhs),
                                                       std::move(rhs));
        case ENDS_WITH:
            return makeContainsStartsEnds<ENDS_WITH>(std::move(lhs),
                                                     std::move(rhs));
        case MATCH:
            if (lTyp != Type::String)
            {
                return makeUnexpected(
                    "match: left type must be a string, got " %
                    typeToString(lTyp));
            }
            if (rTyp == Type::RegularExpression)
            {
                return std::make_unique<BinaryExpression<
                    MATCH, Type::Bool,
                    [](const QVariant &lhs, const QVariant &rhs) -> QVariant {
                        return rhs.toRegularExpression()
                            .match(lhs.toString())
                            .hasMatch();
                    }>>(std::move(lhs), std::move(rhs));
            }
            if (rTyp == Type::MatchingSpecifier)
            {
                return std::make_unique<BinaryExpression<
                    MATCH, Type::String,
                    [](const QVariant &lhs, const QVariant &rhs) -> QVariant {
                        auto [re, idx] = rhs.value<MatchingSpecifier>();
                        auto match = re.match(lhs.toString());
                        if (match.hasMatch())
                        {
                            return match.captured(idx);
                        }
                        return QString{};
                    }>>(std::move(lhs), std::move(rhs));
            }
            return makeUnexpected("match: Expected right type to be a regular "
                                  "expression or matching specifier, got " %
                                  typeToString(rTyp));

        case PLUS:
            if (lTyp == Type::String &&
                (rTyp == Type::String || rTyp == Type::Int))
            {
                // string + (string|int)
                return std::make_unique<BinaryExpression<
                    PLUS, Type::String, anyBinOp<std::plus<>, QString>>>(
                    std::move(lhs), std::move(rhs));
            }
            else
            {
                // int + int
                return argcheckAndMake<PLUS, Type::Int,
                                       &anyBinOp<std::plus<>, int>>(
                    Type::Int, std::move(lhs), Type::Int, std::move(rhs));
            }
        case MINUS:
            return argcheckAndMake<MINUS, Type::Int,
                                   &anyBinOp<std::minus<>, int>>(
                Type::Int, std::move(lhs), Type::Int, std::move(rhs));
        case MULTIPLY:
            return argcheckAndMake<MULTIPLY, Type::Int,
                                   &anyBinOp<std::multiplies<>, int>>(
                Type::Int, std::move(lhs), Type::Int, std::move(rhs));
        case DIVIDE:
            return argcheckAndMake<DIVIDE, Type::Int,
                                   &anyBinOp<std::divides<>, int>>(
                Type::Int, std::move(lhs), Type::Int, std::move(rhs));
        case MOD:
            return argcheckAndMake<MOD, Type::Int,
                                   &anyBinOp<std::modulus<>, int>>(
                Type::Int, std::move(lhs), Type::Int, std::move(rhs));

        default:
            return makeUnexpected(
                tokenTypeToInfoString(tt) %
                " is not a binary operation (attempted to create one).");
    }
}

}  // namespace chatterino::filters
