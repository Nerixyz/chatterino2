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

}  // namespace chatterino::lua

SOL_STACK_FUNCTIONS(QString)
SOL_STACK_FUNCTIONS(QStringList)
SOL_STACK_FUNCTIONS(QByteArray)
SOL_STACK_FUNCTIONS(QRect)
SOL_STACK_FUNCTIONS(QMargins)
SOL_STACK_FUNCTIONS(QSize)
SOL_STACK_FUNCTIONS(QPoint)

#    undef SOL_STACK_FUNCTIONS

SOL_BASE_CLASSES(QWidget, QObject);
SOL_BASE_CLASSES(QLabel, QWidget);
SOL_BASE_CLASSES(QPushButton, QWidget);
SOL_BASE_CLASSES(QLayout, QObject);
SOL_BASE_CLASSES(QBoxLayout, QLayout);
SOL_BASE_CLASSES(QVBoxLayout, QBoxLayout);
SOL_BASE_CLASSES(QHBoxLayout, QBoxLayout);

SOL_DERIVED_CLASSES(QBoxLayout, QVBoxLayout, QHBoxLayout);
SOL_DERIVED_CLASSES(QLayout, QBoxLayout);
SOL_DERIVED_CLASSES(QWidget, QLabel, QPushButton);
SOL_DERIVED_CLASSES(QObject, QWidget, QLayout);

template <typename T>
struct OwnedQPointer {
    OwnedQPointer(T *ptr)
        : ptr(ptr)
    {
    }
    OwnedQPointer(const OwnedQPointer &) = delete;
    OwnedQPointer &operator=(const OwnedQPointer &) = delete;
    OwnedQPointer(OwnedQPointer &&) = default;
    OwnedQPointer &operator=(OwnedQPointer &&) = default;
    ~OwnedQPointer()
    {
        auto *p = this->ptr.get();
        if (p)
        {
            p->deleteLater();
        }
    }

    QPointer<T> ptr;
};

namespace sol {

template <typename T>
struct unique_usertype_traits<OwnedQPointer<T>> {
    using type = T;
    using actual_type = OwnedQPointer<T>;
    static const bool value = true;

    static bool is_null(const actual_type &ptr)
    {
        return ptr.ptr.isNull();
    }

    static type *get(const actual_type &ptr)
    {
        return ptr.ptr.get();
    }
};

template <typename T>
struct unique_usertype_traits<QPointer<T>> {
    using type = T;
    using actual_type = QPointer<T>;
    static const bool value = true;

    static bool is_null(const actual_type &ptr)
    {
        return ptr.ptr.isNull();
    }

    static type *get(const actual_type &ptr)
    {
        return ptr.ptr.get();
    }
};

}  // namespace sol

template <std::derived_from<QObject> T>
void sol_lua_check_access(sol::types<T> /* tag */, lua_State *L, int index,
                          sol::stack::record &tracking)
{
    sol::optional<OwnedQPointer<T> &> owned =
        sol::stack::check_get<OwnedQPointer<T> &>(L, index, sol::no_panic,
                                                  tracking);

    if (!owned.has_value())
    {
        sol::optional<QPointer<T> &> owned =
            sol::stack::check_get<QPointer<T> &>(L, index, sol::no_panic,
                                                 tracking);
        if (!owned.has_value())
        {
            return;
        }
        if (owned->isNull())
        {
            throw sol::error("QObject has been deleted");
        }
        return;
    }

    if (owned->ptr.isNull())
    {
        throw sol::error("QObject has been deleted");
    }
}

#endif
