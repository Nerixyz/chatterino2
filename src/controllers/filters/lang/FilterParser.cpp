// SPDX-FileCopyrightText: 2020 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/filters/lang/FilterParser.hpp"

#include "controllers/filters/lang/expressions/BinaryOperation.hpp"
#include "controllers/filters/lang/expressions/Expression.hpp"
#include "controllers/filters/lang/expressions/ListExpression.hpp"
#include "controllers/filters/lang/expressions/UnaryOperation.hpp"
#include "controllers/filters/lang/expressions/ValueExpression.hpp"

namespace {

using namespace chatterino::filters;

}  // namespace

namespace chatterino::filters {

ExpectedStr<ExpressionPtr> parseFilter(const QString &text)
{
    FilterParser parser(text);
    return parser.parse();
}

FilterParser::FilterParser(const QString &text)
    : text_(text)
    , tokenizer_(Tokenizer(text))
{
}

CreateResult FilterParser::parse()
{
    return this->parseExpression(true);
}

CreateResult FilterParser::parseExpression(bool top)
{
    auto e = this->parseAnd();
    if (!e)
    {
        return e;
    }
    while (this->tokenizer_.hasNext() &&
           this->tokenizer_.nextTokenType() == TokenType::OR)
    {
        this->tokenizer_.next();
        auto nextAnd = this->parseAnd();
        if (!nextAnd)
        {
            return nextAnd;
        }
        e = createBinaryExpression(TokenType::OR, *std::move(e),
                                   *std::move(nextAnd));
    }

    if (this->tokenizer_.hasNext() && top)
    {
        return makeUnexpected(QString("Unexpected token at end: %1")
                                  .arg(this->tokenizer_.preview()));
    }

    return e;
}

CreateResult FilterParser::parseAnd()
{
    auto e = this->parseUnary();
    if (!e)
    {
        return e;
    }
    while (this->tokenizer_.hasNext() &&
           this->tokenizer_.nextTokenType() == TokenType::AND)
    {
        this->tokenizer_.next();
        auto nextUnary = this->parseUnary();
        if (!nextUnary)
        {
            return nextUnary;
        }
        e = createBinaryExpression(TokenType::AND, *std::move(e),
                                   *std::move(nextUnary));
    }
    return e;
}

CreateResult FilterParser::parseUnary()
{
    if (this->tokenizer_.hasNext() && this->tokenizer_.nextTokenIsUnaryOp())
    {
        this->tokenizer_.next();
        auto type = this->tokenizer_.tokenType();
        auto nextCondition = this->parseCondition();
        if (!nextCondition)
        {
            return nextCondition;
        }
        return createUnaryExpression(type, *std::move(nextCondition));
    }

    return this->parseCondition();
}

CreateResult FilterParser::parseParentheses()
{
    // Don't call .next() before calling this method
    assert(this->tokenizer_.nextTokenType() == TokenType::LP);

    this->tokenizer_.next();
    auto e = this->parseExpression();
    if (!e)
    {
        return e;
    }
    if (this->tokenizer_.hasNext() &&
        this->tokenizer_.nextTokenType() == TokenType::RP)
    {
        this->tokenizer_.next();
        return e;
    }

    const auto message =
        this->tokenizer_.hasNext()
            ? QString("Missing closing parentheses: got %1")
                  .arg(this->tokenizer_.preview())
            : "Missing closing parentheses at end of statement";
    return makeUnexpected(message);
}

CreateResult FilterParser::parseCondition()
{
    CreateResult value;
    // parse expression wrapped in parentheses
    if (this->tokenizer_.hasNext() &&
        this->tokenizer_.nextTokenType() == TokenType::LP)
    {
        // get value inside parentheses
        value = this->parseParentheses();
    }
    else
    {
        // get current value
        value = this->parseValue();
    }

    if (!value)
    {
        return value;
    }

    // expecting an operator or nothing
    while (this->tokenizer_.hasNext())
    {
        if (this->tokenizer_.nextTokenIsBinaryOp())
        {
            this->tokenizer_.next();
            auto type = this->tokenizer_.tokenType();
            auto nextValue = this->parseValue();
            if (!nextValue)
            {
                return nextValue;
            }
            return createBinaryExpression(type, *std::move(value),
                                          *std::move(nextValue));
        }
        if (this->tokenizer_.nextTokenIsMathOp())
        {
            this->tokenizer_.next();
            auto type = this->tokenizer_.tokenType();
            auto nextValue = this->parseValue();
            if (!nextValue)
            {
                return nextValue;
            }
            value = createBinaryExpression(type, *std::move(value),
                                           *std::move(nextValue));
            if (!value)
            {
                return value;
            }
        }
        else if (this->tokenizer_.nextTokenType() == TokenType::RP)
        {
            // RP, so move on
            break;
        }
        else if (!this->tokenizer_.nextTokenIsOp())
        {
            return makeUnexpected(QString("Expected an operator but got %1 %2")
                                      .arg(this->tokenizer_.preview())
                                      .arg(tokenTypeToInfoString(
                                          this->tokenizer_.nextTokenType())));
        }
        else
        {
            break;
        }
    }

    return value;
}

CreateResult FilterParser::parseValue()
{
    // parse a literal or an expression wrapped in parenthsis
    if (this->tokenizer_.hasNext())
    {
        auto type = this->tokenizer_.nextTokenType();
        if (type == TokenType::INT)
        {
            return createValueExpression(this->tokenizer_.next().toInt(), type);
        }
        if (type == TokenType::STRING)
        {
            auto before = this->tokenizer_.next();
            // remove quote marks
            auto val = before.mid(1);
            val.chop(1);
            val = val.replace("\\\"", "\"");
            return createValueExpression(val, type);
        }
        if (type == TokenType::IDENTIFIER)
        {
            return createValueExpression(this->tokenizer_.next(), type);
        }
        if (type == TokenType::REGULAR_EXPRESSION)
        {
            auto before = this->tokenizer_.next();
            // remove quote marks and r/ri
            bool caseInsensitive = before.startsWith("ri");
            auto val = before.mid(caseInsensitive ? 3 : 2);
            val.chop(1);
            val = val.replace("\\\"", "\"");
            return createRegexExpression(val, caseInsensitive);
        }
        if (type == TokenType::LP)
        {
            return this->parseParentheses();
        }
        if (type == TokenType::LIST_START)
        {
            return this->parseList();
        }

        this->tokenizer_.next();
        return makeUnexpected(QString("Expected value but got %1 %2")
                                  .arg(this->tokenizer_.current())
                                  .arg(tokenTypeToInfoString(type)));
    }

    return makeUnexpected("Unexpected end of statement");
}

CreateResult FilterParser::parseList()
{
    // Don't call .next() before calling this method
    assert(this->tokenizer_.nextTokenType() == TokenType::LIST_START);
    this->tokenizer_.next();

    ExpressionList list;
    bool first = true;

    while (this->tokenizer_.hasNext())
    {
        if (this->tokenizer_.nextTokenType() == TokenType::LIST_END)
        {
            this->tokenizer_.next();
            return createListExpression(std::move(list));
        }
        if (this->tokenizer_.nextTokenType() == TokenType::COMMA && !first)
        {
            this->tokenizer_.next();
            auto v = this->parseValue();
            if (!v)
            {
                return v;
            }
            list.push_back(*std::move(v));
            first = false;
        }
        else if (first)
        {
            auto v = this->parseValue();
            if (!v)
            {
                return v;
            }
            list.push_back(*std::move(v));
            first = false;
        }
        else
        {
            break;
        }
    }

    const auto message =
        this->tokenizer_.hasNext()
            ? QString("Missing closing list braces: got %1")
                  .arg(this->tokenizer_.preview())
            : "Missing closing list braces at end of statement";
    return makeUnexpected(message);
}

}  // namespace chatterino::filters
