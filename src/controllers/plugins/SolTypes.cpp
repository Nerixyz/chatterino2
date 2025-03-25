#ifdef CHATTERINO_HAVE_PLUGINS
#    include "controllers/plugins/SolTypes.hpp"

#    include "controllers/plugins/PluginController.hpp"

#    include <QObject>
#    include <sol/thread.hpp>

#    include <string>

using namespace std::string_view_literals;

namespace chatterino::lua {

Plugin *ThisPluginState::plugin()
{
    if (this->plugptr_ != nullptr)
    {
        return this->plugptr_;
    }
    auto *pl = getApp()->getPlugins()->getPluginByStatePtr(this->state_);
    if (pl == nullptr)
    {
        throw std::runtime_error("internal error: missing plugin");
    }
    this->plugptr_ = pl;
    return pl;
}

}  // namespace chatterino::lua

// NOLINTBEGIN(readability-named-parameter)
// QString
bool sol_lua_check(sol::types<QString>, lua_State *L, int index,
                   std::function<sol::check_handler_type> handler,
                   sol::stack::record &tracking)
{
    return sol::stack::check<const char *>(L, index, handler, tracking);
}

QString sol_lua_get(sol::types<QString>, lua_State *L, int index,
                    sol::stack::record &tracking)
{
    auto str = sol::stack::get<std::string_view>(L, index, tracking);
    return QString::fromUtf8(str.data(), static_cast<qsizetype>(str.length()));
}

int sol_lua_push(sol::types<QString>, lua_State *L, const QString &value)
{
    return sol::stack::push(L, value.toUtf8().data());
}

// QStringList
bool sol_lua_check(sol::types<QStringList>, lua_State *L, int index,
                   std::function<sol::check_handler_type> handler,
                   sol::stack::record &tracking)
{
    return sol::stack::check<sol::table>(L, index, handler, tracking);
}

QStringList sol_lua_get(sol::types<QStringList>, lua_State *L, int index,
                        sol::stack::record &tracking)
{
    sol::table table = sol::stack::get<sol::table>(L, index, tracking);
    QStringList result;
    result.reserve(static_cast<qsizetype>(table.size()));
    for (size_t i = 1; i < table.size() + 1; i++)
    {
        result.append(table.get<QString>(i));
    }
    return result;
}

int sol_lua_push(sol::types<QStringList>, lua_State *L,
                 const QStringList &value)
{
    sol::table table = sol::table::create(L, static_cast<int>(value.size()));
    for (const QString &str : value)
    {
        table.add(str);
    }
    return sol::stack::push(L, table);
}

// QByteArray
bool sol_lua_check(sol::types<QByteArray>, lua_State *L, int index,
                   std::function<sol::check_handler_type> handler,
                   sol::stack::record &tracking)
{
    return sol::stack::check<const char *>(L, index, handler, tracking);
}

QByteArray sol_lua_get(sol::types<QByteArray>, lua_State *L, int index,
                       sol::stack::record &tracking)
{
    auto str = sol::stack::get<std::string_view>(L, index, tracking);
    return QByteArray::fromRawData(str.data(), str.length());
}

int sol_lua_push(sol::types<QByteArray>, lua_State *L, const QByteArray &value)
{
    return sol::stack::push(L,
                            std::string_view(value.constData(), value.size()));
}

// QRect
bool sol_lua_check(sol::types<QRect>, lua_State *L, int index,
                   std::function<sol::check_handler_type> handler,
                   sol::stack::record &tracking)
{
    return sol::stack::check<sol::table>(L, index, handler, tracking);
}

QRect sol_lua_get(sol::types<QRect>, lua_State *L, int index,
                  sol::stack::record &tracking)
{
    sol::state_view lua(L);
    sol::table table = sol::stack::get<sol::table>(L, index, tracking);
    switch (table.size())
    {
        case 0:
            return {
                table.get<int>("x"sv),
                table.get<int>("y"sv),
                table.get<int>("width"sv),
                table.get<int>("height"sv),
            };
        case 2:
            return {
                table.get<QPoint>(1),
                table.get<QSize>(2),
            };
        case 4:
            return {
                table.get<int>(1),
                table.get<int>(2),
                table.get<int>(3),
                table.get<int>(4),
            };
        default:
            throw sol::error("QRect must be a table {x=, y=, width=, height=}, "
                             "{QPoint, QSize}, or {x, y, width, height}.");
    }
}

int sol_lua_push(sol::types<QRect>, lua_State *L, const QRect &value)
{
    sol::state_view lua(L);
    sol::table table =
        lua.create_table_with("x"sv, value.x(), "y"sv, value.y(), "width"sv,
                              value.width(), "height"sv, value.height());
    return sol::stack::push(L, table);
}

// QMargins
bool sol_lua_check(sol::types<QMargins>, lua_State *L, int index,
                   std::function<sol::check_handler_type> handler,
                   sol::stack::record &tracking)
{
    return sol::stack::check<sol::table>(L, index, handler, tracking);
}

QMargins sol_lua_get(sol::types<QMargins>, lua_State *L, int index,
                     sol::stack::record &tracking)
{
    sol::state_view lua(L);
    sol::table table = sol::stack::get<sol::table>(L, index, tracking);
    switch (table.size())
    {
        case 0:
            return {
                table.get<int>("left"sv),
                table.get<int>("top"sv),
                table.get<int>("right"sv),
                table.get<int>("bottom"sv),
            };
        case 4:
            return {
                table.get<int>(1),
                table.get<int>(2),
                table.get<int>(3),
                table.get<int>(4),
            };
        default:
            throw sol::error("QMargins must be a table {left=, top=, right=, "
                             "bottom=} or {left, top, right, bottom}.");
    }
}

int sol_lua_push(sol::types<QMargins>, lua_State *L, const QMargins &value)
{
    sol::state_view lua(L);
    sol::table table = lua.create_table_with(
        "left"sv, value.left(), "top"sv, value.top(), "right"sv, value.right(),
        "bottom"sv, value.bottom());
    return sol::stack::push(L, table);
}

// QSize
bool sol_lua_check(sol::types<QSize>, lua_State *L, int index,
                   std::function<sol::check_handler_type> handler,
                   sol::stack::record &tracking)
{
    return sol::stack::check<sol::lua_table>(L, index, handler, tracking);
}

QSize sol_lua_get(sol::types<QSize>, lua_State *L, int index,
                  sol::stack::record &tracking)
{
    sol::state_view lua(L);
    sol::table table = sol::stack::get<sol::table>(L, index, tracking);
    switch (table.size())
    {
        case 0:
            return {
                table.get<int>("width"sv),
                table.get<int>("height"sv),
            };
        case 2:
            return {table.get<int>(1), table.get<int>(2)};
        default:
            throw sol::error(
                "QSize must be a table {width=, height=} or {width, height}.");
    }
}

int sol_lua_push(sol::types<QSize>, lua_State *L, const QSize &value)
{
    sol::state_view lua(L);
    sol::table table = lua.create_table_with("width"sv, value.width(),
                                             "height"sv, value.height());
    return sol::stack::push(L, table);
}

// QPoint
bool sol_lua_check(sol::types<QPoint>, lua_State *L, int index,
                   std::function<sol::check_handler_type> handler,
                   sol::stack::record &tracking)
{
    return sol::stack::check<sol::table>(L, index, handler, tracking);
}

QPoint sol_lua_get(sol::types<QPoint>, lua_State *L, int index,
                   sol::stack::record &tracking)
{
    sol::state_view lua(L);
    sol::table table = sol::stack::get<sol::table>(L, index, tracking);
    switch (table.size())
    {
        case 0:
            return {table.get<int>("x"sv), table.get<int>("y"sv)};
        case 2:
            return {table.get<int>(1), table.get<int>(2)};
        default:
            throw sol::error("QPoint must be a table {x=, y=} or {x, y}");
    }
}

int sol_lua_push(sol::types<QPoint>, lua_State *L, const QPoint &value)
{
    sol::state_view lua(L);
    sol::table table =
        lua.create_table_with("x"sv, value.x(), "y"sv, value.y());
    return sol::stack::push(L, table);
}

namespace chatterino::lua {

// ThisPluginState

bool sol_lua_check(sol::types<chatterino::lua::ThisPluginState>,
                   lua_State * /*L*/, int /* index*/,
                   std::function<sol::check_handler_type> /* handler*/,
                   sol::stack::record & /*tracking*/)
{
    return true;
}

chatterino::lua::ThisPluginState sol_lua_get(
    sol::types<chatterino::lua::ThisPluginState>, lua_State *L, int /*index*/,
    sol::stack::record &tracking)
{
    tracking.use(0);
    return {L};
}

int sol_lua_push(sol::types<chatterino::lua::ThisPluginState>, lua_State *L,
                 const chatterino::lua::ThisPluginState &value)
{
    return sol::stack::push(L, sol::thread(L, value));
}

}  // namespace chatterino::lua

// NOLINTEND(readability-named-parameter)

#endif
