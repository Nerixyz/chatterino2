#ifdef CHATTERINO_HAVE_PLUGINS
#    include "controllers/plugins/SolTypes.hpp"

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
    QStringList result(static_cast<qsizetype>(table.size()));
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
// NOLINTEND(readability-named-parameter)

#endif
