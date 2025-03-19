#pragma once
#ifdef CHATTERINO_HAVE_PLUGINS
#    include "util/QMagicEnum.hpp"
#    include "util/TypeName.hpp"

#    include <nonstd/expected.hpp>
#    include <QObject>
#    include <QString>
#    include <QStringBuilder>
#    include <QStringList>
#    include <sol/sol.hpp>

class QObject;
class QWidget;

namespace chatterino::detail {

// NOLINTBEGIN(readability-identifier-naming)
template <typename T>
constexpr bool IsOptional = false;
template <typename T>
constexpr bool IsOptional<std::optional<T>> = true;
// NOLINTEND(readability-identifier-naming)

}  // namespace chatterino::detail

namespace chatterino {

class Plugin;
class BaseWidget;
class BaseWindow;
class Window;
class SplitNotebook;
class Notebook;

}  // namespace chatterino

namespace chatterino::lua {

class ThisPluginState
{
public:
    ThisPluginState(lua_State *Ls)
        : plugptr_(nullptr)
        , state_(Ls)
    {
    }

    operator lua_State *() const noexcept
    {
        return this->state_;
    }

    lua_State *operator->() const noexcept
    {
        return this->state_;
    }
    lua_State *state() const noexcept
    {
        return this->state_;
    }

    Plugin *plugin();

private:
    Plugin *plugptr_;
    lua_State *state_;
};

/// @brief Attempts to call @a function with @a args
///
/// @a T is expected to be returned.
/// If `void` is specified, the returned values
/// are ignored.
/// `std::optional<T>` means nil|LuaEquiv<T> (or zero returns)
/// A return type that doesn't match returns an error
template <typename T, typename... Args>
inline nonstd::expected_lite::expected<T, QString> tryCall(
    const sol::protected_function &function, Args &&...args)
{
    sol::protected_function_result result =
        function(std::forward<Args>(args)...);
    if (!result.valid())
    {
        sol::error err = result;
        return nonstd::expected_lite::make_unexpected(
            QString::fromUtf8(err.what()));
    }

    if constexpr (std::is_same_v<T, void>)
    {
        return {};
    }
    else
    {
        if constexpr (detail::IsOptional<T>)
        {
            if (result.return_count() == 0)
            {
                return {};
            }
        }
        if (result.return_count() > 1)
        {
            return nonstd::expected_lite::make_unexpected(
                u"Expected one value to be returned but " %
                QString::number(result.return_count()) %
                u" values were returned");
        }

        try
        {
            if constexpr (detail::IsOptional<T>)
            {
                // we want to error on anything that is not nil|T,
                // std::optional<T> in sol means "give me a T or if it does not match nullopt"
                if (result.get_type() == sol::type::nil)
                {
                    return {};
                }
                auto ret = result.get<T>();

                if (!ret)
                {
                    auto t = type_name<T>();
                    return nonstd::expected_lite::make_unexpected(
                        u"Expected " % QLatin1String(t.data(), t.size()) %
                        u" to be returned but " %
                        qmagicenum::enumName(result.get_type()) %
                        u" was returned");
                }
                return *ret;
            }
            else
            {
                auto ret = result.get<std::optional<T>>();

                if (!ret)
                {
                    auto t = type_name<T>();
                    return nonstd::expected_lite::make_unexpected(
                        u"Expected " % QLatin1String(t.data(), t.size()) %
                        u" to be returned but " %
                        qmagicenum::enumName(result.get_type()) %
                        u" was returned");
                }
                return *ret;
            }
        }
        catch (std::runtime_error &e)
        {
            return nonstd::expected_lite::make_unexpected(
                QString::fromUtf8(e.what()));
        }
        // non other exceptions we let it explode
    }
}

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#    define SOL_STACK_FUNCTIONS(TYPE)                                      \
        bool sol_lua_check(sol::types<TYPE>, lua_State *L, int index,      \
                           std::function<sol::check_handler_type> handler, \
                           sol::stack::record &tracking);                  \
        TYPE sol_lua_get(sol::types<TYPE>, lua_State *L, int index,        \
                         sol::stack::record &tracking);                    \
        int sol_lua_push(sol::types<TYPE>, lua_State *L, const TYPE &value);

SOL_STACK_FUNCTIONS(chatterino::lua::ThisPluginState)

template <typename Target, typename Owner>
struct QGuardedPtr {
    QPointer<Owner> guard;
    Target *target;

    Target *get()
    {
        if (guard.isNull())  // XXX: is this necessary?
        {
            return nullptr;
        }
        return this->target;
    }
};

}  // namespace chatterino::lua

SOL_STACK_FUNCTIONS(QString)
SOL_STACK_FUNCTIONS(QStringList)
SOL_STACK_FUNCTIONS(QByteArray)

#    undef SOL_STACK_FUNCTIONS

namespace sol {

// NOLINTBEGIN(readability-identifier-naming)
template <typename T>
struct unique_usertype_traits<QPointer<T>> {
    using type = T;
    using actual_type = QPointer<T>;
    static const bool value = true;

    static bool is_null(const actual_type &ptr)
    {
        return ptr == nullptr;
    }

    static type *get(const actual_type &ptr)
    {
        return ptr.get();
    }
};

template <typename Target, typename Owner>
struct unique_usertype_traits<chatterino::lua::QGuardedPtr<Target, Owner>> {
    using type = Target;
    using actual_type = chatterino::lua::QGuardedPtr<Target, Owner>;
    static const bool value = true;

    static bool is_null(const actual_type &ptr)
    {
        return ptr.guard == nullptr || ptr.target == nullptr;
    }

    static type *get(const actual_type &ptr)
    {
        return ptr.target;
    }
};
// NOLINTEND(readability-identifier-naming)

}  // namespace sol

SOL_BASE_CLASSES(chatterino::SplitNotebook, chatterino::Notebook);
SOL_DERIVED_CLASSES(chatterino::Notebook, chatterino::SplitNotebook);

SOL_BASE_CLASSES(QWidget, QObject);
SOL_BASE_CLASSES(chatterino::BaseWidget, QWidget);
SOL_BASE_CLASSES(chatterino::BaseWindow, chatterino::BaseWidget);
SOL_BASE_CLASSES(chatterino::Window, chatterino::BaseWindow);

SOL_DERIVED_CLASSES(QObject, QWidget);
SOL_DERIVED_CLASSES(QWidget, chatterino::BaseWidget);
SOL_DERIVED_CLASSES(chatterino::BaseWidget, chatterino::BaseWindow);
SOL_DERIVED_CLASSES(chatterino::BaseWindow, chatterino::Window);

#endif
