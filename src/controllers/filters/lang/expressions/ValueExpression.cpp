// SPDX-FileCopyrightText: 2023 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "controllers/filters/lang/expressions/ValueExpression.hpp"

#include "Application.hpp"
#include "controllers/filters/lang/Tokenizer.hpp"
#include "messages/Message.hpp"
#include "messages/MessageFlag.hpp"
#include "providers/twitch/TwitchBadge.hpp"
#include "providers/twitch/TwitchChannel.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"

namespace {

using namespace chatterino;
using namespace chatterino::filters;
using namespace Qt::Literals;

template <typename T>
struct MemberPointerTraits;

template <typename C, typename T>
struct MemberPointerTraits<T C::*> {
    using Type = T;
    using Class = C;
};

template <typename T>
struct Narrow {
    using Type = T;
};
template <typename T>
    requires(!std::is_void_v<typename filters::detail::TypeTraits<T>::Narrow>)
struct Narrow<T> {
    using Type = filters::detail::TypeTraits<T>::Narrow;
};

template <typename T>
QVariant makeVariantFor(T &&v)
{
    return QVariant::fromValue(std::forward<typename Narrow<T>::Type &&>(v));
}

struct AccessorExpressionBase : public Expression {
    AccessorExpressionBase(QString name, Type type)
        : name(std::move(name))
        , type_(type)
    {
    }

    virtual std::unique_ptr<AccessorExpressionBase> clone() const = 0;

    QString filterString() const override
    {
        return this->name;
    }

    Type type() const override
    {
        return this->type_;
    }

    QString name;
    Type type_;  // NOLINT(readability-identifier-naming)
};

template <auto Fn>
struct AccessorExpression final : public AccessorExpressionBase {
    using AccessorExpressionBase::AccessorExpressionBase;

    QVariant run(const RunContext &ctx) override
    {
        return Fn(ctx);
    }

    std::unique_ptr<AccessorExpressionBase> clone() const override
    {
        return std::make_unique<AccessorExpression<Fn>>(this->name,
                                                        this->type_);
    }
};

template <auto Fn>
std::unique_ptr<AccessorExpressionBase> makeAccessor(QString name, Type type)
{
    return std::make_unique<AccessorExpression<Fn>>(std::move(name), type);
}

template <auto Fn>
std::unique_ptr<AccessorExpressionBase> makeAccessor(QString name)
{
    return std::make_unique<AccessorExpression<Fn>>(
        std::move(name),
        TYPE_OF_V<std::invoke_result_t<decltype(Fn), const RunContext &>>);
}

template <auto Ptr>
auto memberAccessor(QString name)
{
    return makeAccessor<[](const RunContext &ctx) -> QVariant {
        return makeVariantFor(ctx.message.*Ptr);
    }>(std::move(name),
       TYPE_OF_V<typename MemberPointerTraits<decltype(Ptr)>::Type>);
}

template <MessageFlag Flag>
auto flagAccessor(QString name)
{
    return makeAccessor<[](const RunContext &ctx) {
        return ctx.message.flags.has(Flag);
    }>(std::move(name));
}

using AccessorMap = std::map<QString, const AccessorExpressionBase *>;
const AccessorMap &identifierMap()
{
    static std::array accessors{
        // author.*
        makeAccessor<[](const RunContext &ctx) {
            QStringList badges(
                static_cast<qsizetype>(ctx.message.twitchBadges.size()));
            for (const auto &e : ctx.message.twitchBadges)
            {
                badges.emplace_back(e.key_);
            }
            return badges;
        }>(u"author.badges"_s),
        memberAccessor<&Message::externalBadges>(u"author.external_badges"_s),
        memberAccessor<&Message::usernameColor>(u"author.color"_s),
        memberAccessor<&Message::displayName>(u"author.name"_s),
        memberAccessor<&Message::userID>(u"author.user_id"_s),
        makeAccessor<[](const RunContext &ctx) {
            return !ctx.message.usernameColor.isValid();
        }>(u"author.no_color"_s),
        makeAccessor<[](const RunContext &ctx) {
            return std::ranges::any_of(
                ctx.message.twitchBadges, [](const auto &it) {
                    return it.key_ == u"subscriber" || it.key_ == u"founder";
                });
        }>(u"author.subbed"_s),
        makeAccessor<[](const RunContext &ctx) {
            auto it = ctx.message.twitchBadgeInfos.find(u"subscriber"_s);
            if (it == ctx.message.twitchBadgeInfos.end())
            {
                it = ctx.message.twitchBadgeInfos.find(u"founder"_s);
            }
            if (it == ctx.message.twitchBadgeInfos.end())
            {
                return 0;
            }
            return it->second.toInt();
        }>(u"author.sub_length"_s),

        // channel.*
        makeAccessor<[](const RunContext &ctx) {
            auto *tc = dynamic_cast<TwitchChannel *>(ctx.channel);
            if (tc)
            {
                return tc->isLive();
            }
            return false;
        }>(u"channel.live"_s),
        memberAccessor<&Message::channelName>(u"channel.name"_s),
        makeAccessor<[](const RunContext &ctx) {
            auto chan = getApp()->getTwitch()->getWatchingChannel().get();
            return !chan->getName().isEmpty() &&
                   chan->getName().compare(ctx.message.channelName,
                                           Qt::CaseInsensitive) == 0;
        }>(u"channel.watching"_s),

        // flags.*
        flagAccessor<MessageFlag::Action>(u"flags.action"_s),
        flagAccessor<MessageFlag::Highlighted>(u"flags.highlighted"_s),
        flagAccessor<MessageFlag::RedeemedHighlight>(
            u"flags.points_redeemed"_s),
        flagAccessor<MessageFlag::Subscription>(u"flags.sub_message"_s),
        flagAccessor<MessageFlag::System>(u"flags.system_message"_s),
        flagAccessor<MessageFlag::RedeemedChannelPointReward>(
            u"flags.reward_message"_s),
        flagAccessor<MessageFlag::FirstMessage>(u"flags.first_message"_s),
        flagAccessor<MessageFlag::ElevatedMessage>(u"flags.elevated_message"_s),
        flagAccessor<MessageFlag::ElevatedMessage>(u"flags.hype_chat"_s),
        flagAccessor<MessageFlag::CheerMessage>(u"flags.cheer_message"_s),
        flagAccessor<MessageFlag::Whisper>(u"flags.whisper"_s),
        flagAccessor<MessageFlag::ReplyMessage>(u"flags.reply"_s),
        flagAccessor<MessageFlag::AutoMod>(u"flags.automod"_s),
        flagAccessor<MessageFlag::RestrictedMessage>(u"flags.restricted"_s),
        flagAccessor<MessageFlag::MonitoredMessage>(u"flags.monitored"_s),
        flagAccessor<MessageFlag::SharedMessage>(u"flags.shared"_s),
        flagAccessor<MessageFlag::Similar>(u"flags.similar"_s),

        // message.*
        memberAccessor<&Message::messageText>(u"message.content"_s),
        makeAccessor<[](const RunContext &ctx) {
            return ctx.message.messageText.length();
        }>(u"message.length"_s),

        // reward.*
        makeAccessor<[](const RunContext &ctx) {
            auto r = ctx.message.reward;
            if (r)
            {
                return r->cost;
            }
            return -1;
        }>(u"reward.cost"_s),
        makeAccessor<[](const RunContext &ctx) {
            auto r = ctx.message.reward;
            if (r)
            {
                return r->id;
            }
            return QString{};
        }>(u"reward.id"_s),
        makeAccessor<[](const RunContext &ctx) {
            auto r = ctx.message.reward;
            if (r)
            {
                return r->title;
            }
            return QString{};
        }>(u"reward.title"_s),
    };
    static AccessorMap accessorMap = ([] {
        AccessorMap map;
        for (const auto &accessor : accessors)
        {
            map.emplace(accessor->name, accessor.get());
        }
        return map;
    })();
    return accessorMap;
}

struct ConstValueExpression final : public Expression {
    ConstValueExpression(QVariant value, Type type)
        : value(std::move(value))
        , type_(type)
    {
    }

    QString filterString() const override
    {
        switch (this->type_)
        {
            case Type::String:
                return '"' % this->value.toString().replace("\"", "\\\"") % '"';
            case Type::Int:
                return this->value.toString();
            case Type::RegularExpression: {
                auto re = this->value.toRegularExpression();
                QStringView prefix = u"r";
                if (re.patternOptions().testFlag(
                        QRegularExpression::CaseInsensitiveOption))
                {
                    prefix = u"ri";
                }
                return prefix % '"' % re.pattern().replace("\"", "\\\"") % '"';
            }
            default:
                assert(false);
                return {};
        }
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

    QVariant value;
    Type type_;  // NOLINT(readability-identifier-naming)
};

}  // namespace

namespace chatterino::filters {

CreateResult createValueExpression(QVariant value, TokenType tt)
{
    switch (tt)
    {
        case INT:
            return std::make_unique<ConstValueExpression>(std::move(value),
                                                          Type::Int);
        case STRING:
            return std::make_unique<ConstValueExpression>(std::move(value),
                                                          Type::String);
        case IDENTIFIER: {
            const auto &map = identifierMap();
            auto it = map.find(value.toString());
            if (it == map.end())
            {
                return makeUnexpected(value.toString() %
                                      u" is not a valid identifier");
            }
            return it->second->clone();
        }
        default:
            return makeUnexpected(tokenTypeToInfoString(tt) %
                                  " can't create a value expression");
    }
}

CreateResult createRegexExpression(const QString &regex, bool caseInsensitive)
{
    QRegularExpression::PatternOptions opts;
    opts.setFlag(QRegularExpression::CaseInsensitiveOption, caseInsensitive);
    return std::make_unique<ConstValueExpression>(
        QRegularExpression(regex, opts), Type::RegularExpression);
}

}  // namespace chatterino::filters
