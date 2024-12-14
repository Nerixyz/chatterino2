#pragma once

#include <string_view>

namespace chatterino::detail {

template <typename T>
consteval auto fullTypeName()
{
    std::string_view name, prefix, suffix;
#ifdef __clang__
    name = __PRETTY_FUNCTION__;
    prefix = "auto chatterino::detail::fullTypeName() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name = __PRETTY_FUNCTION__;
    prefix = "consteval auto chatterino::detail::fullTypeName() [with T = ";
    suffix = "]";
#elif defined(_MSC_VER)
    name = __FUNCSIG__;
    prefix = "auto __cdecl chatterino::detail::fullTypeName<";
    suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());

    if (name.starts_with("class "))
    {
        name.remove_prefix(6);
    }
    if (name.starts_with("struct "))
    {
        name.remove_prefix(7);
    }

    return name;
}

consteval std::string_view repoPath()
{
    auto lambda = []() {};
    std::string_view path = fullTypeName<decltype(lambda)>();
    path.remove_prefix(sizeof("(lambda at ") - 1);
#ifdef Q_OS_WIN
    size_t pathEnd = path.rfind("util\\TypeName.hpp");
#else
    size_t pathEnd = path.rfind("util/TypeName.hpp");
#endif
    if (pathEnd == std::string_view::npos)
    {
        return "";
    }
    path.remove_suffix(path.size() - pathEnd);

    return path;
}

}  // namespace chatterino::detail

namespace chatterino {

// Adapted from: https://stackoverflow.com/a/56766138.
// NOTE: Relies on the "magic" prefixes and suffixes. There are implementations
// that attempt to manually detect these (linked in the SO answer above) but
// they seemed too complex for the scope we have here.
template <typename T>
consteval auto type_name()
{
    auto name = detail::fullTypeName<T>();

#ifdef __clang__
    if (name.starts_with("(lambda at"))
    {
        name.remove_prefix(sizeof("(lambda at ") - 1);
        name.remove_suffix(1);
        name.remove_prefix(detail::repoPath().size());
    }
#endif
    // TODO: other compilers

    return name;
}

}  // namespace chatterino
