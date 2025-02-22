#include "controllers/commands/builtin/chatterino/Debugging.hpp"

#include "common/Channel.hpp"
#include "common/Env.hpp"
#include "common/Literals.hpp"
#include "controllers/commands/CommandContext.hpp"
#include "messages/Image.hpp"
#include "messages/MessageBuilder.hpp"
#include "messages/MessageElement.hpp"
#include "singletons/Theme.hpp"
#include "util/PostToThread.hpp"

#include <QApplication>
#include <QLoggingCategory>
#include <QString>

namespace chatterino::commands {

using namespace literals;

QString setLoggingRules(const CommandContext &ctx)
{
    if (ctx.words.size() < 2)
    {
        ctx.channel->addSystemMessage(
            "Usage: /c2-set-logging-rules <rules...>. To enable debug logging "
            "for all categories from chatterino, use "
            "'chatterino.*.debug=true'. For the format on the rules, see "
            "https://doc.qt.io/qt-6/"
            "qloggingcategory.html#configuring-categories");
        return {};
    }

    auto filterRules = ctx.words.mid(1).join('\n');

    QLoggingCategory::setFilterRules(filterRules);

    auto message =
        QStringLiteral("Updated filter rules to '%1'.").arg(filterRules);

    if (!qgetenv("QT_LOGGING_RULES").isEmpty())
    {
        message += QStringLiteral(
            " Warning: Logging rules were previously set by the "
            "QT_LOGGING_RULES environment variable. This might cause "
            "interference - see: "
            "https://doc.qt.io/qt-6/qloggingcategory.html#setFilterRules");
    }

    ctx.channel->addSystemMessage(message);
    return {};
}

QString toggleThemeReload(const CommandContext &ctx)
{
    if (getTheme()->isAutoReloading())
    {
        getTheme()->setAutoReload(false);
        ctx.channel->addSystemMessage(u"Disabled theme auto reloading."_s);
        return {};
    }

    getTheme()->setAutoReload(true);
    ctx.channel->addSystemMessage(u"Auto reloading theme every %1 ms."_s.arg(
        Theme::AUTO_RELOAD_INTERVAL_MS));
    return {};
}

QString listEnvironmentVariables(const CommandContext &ctx)
{
    const auto &channel = ctx.channel;
    if (channel == nullptr)
    {
        return "";
    }

    auto env = Env::get();

    QStringList debugMessages{
        "recentMessagesApiUrl: " + env.recentMessagesApiUrl,
        "linkResolverUrl: " + env.linkResolverUrl,
        "proxyUrl: " + env.proxyUrl.value_or("N/A"),
        "twitchServerHost: " + env.twitchServerHost,
        "twitchServerPort: " + QString::number(env.twitchServerPort),
        "twitchServerSecure: " + QString::number(env.twitchServerSecure),
    };

    for (QString &str : debugMessages)
    {
        MessageBuilder builder;
        builder.emplace<TimestampElement>(QTime::currentTime());
        builder.emplace<TextElement>(str, MessageElementFlag::Text,
                                     MessageColor::System);
        channel->addMessage(builder.release(), MessageContext::Original);
    }
    return "";
}

QString listArgs(const CommandContext &ctx)
{
    const auto &channel = ctx.channel;
    if (channel == nullptr)
    {
        return "";
    }

    QString msg = QApplication::instance()->arguments().join(' ');

    channel->addSystemMessage(msg);

    return "";
}

QString forceImageGarbageCollection(const CommandContext &ctx)
{
    (void)ctx;

    runInGuiThread([] {
        auto &iep = ImageExpirationPool::instance();
        iep.freeOld();
    });
    return "";
}

QString forceImageUnload(const CommandContext &ctx)
{
    (void)ctx;

    runInGuiThread([] {
        auto &iep = ImageExpirationPool::instance();
        iep.freeAll();
    });
    return "";
}

QString debugTest(const CommandContext &ctx)
{
    if (!ctx.channel)
    {
        return "";
    }

    ctx.channel->addSystemMessage("debug-test called");

    return "";
}

}  // namespace chatterino::commands
