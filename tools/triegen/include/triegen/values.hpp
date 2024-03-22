#pragma once

#include <triegen/detail/size_types.hpp>

#include <string_view>

/// Customization point for values
namespace triegen::value {

/// An invalid value returned when nothing matched (e.g. -1)
template <typename T>
T invalidValue();

/// The name of the type (e.g. 'int')
template <typename T>
std::string_view valueName();

/// Any additional includes for the type (must end in a newline if specified)
template <typename T>
std::string_view additionalIncludes()
{
    return {};
}

// Specializations for qsizetype (ssize_t)

template <>
inline qsizetype invalidValue()
{
    return -1;
}

template <>
inline std::string_view valueName<qsizetype>()
{
    return "qsizetype";
}

// Specializations for int

template <>
inline int invalidValue()
{
    return -1;
}

template <>
inline std::string_view valueName<int>()
{
    return "int";
}

}  // namespace triegen::value
