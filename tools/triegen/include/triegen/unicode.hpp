#pragma once

#include <boost/locale.hpp>

#include <string>

namespace triegen {

template <typename T>
std::u16string toUtf16(const std::basic_string<T> &in)
{
    return boost::locale::conv::utf_to_utf<char16_t>(in);
}

template <typename T>
std::u16string toUtf16(std::basic_string_view<T> in)
{
    return boost::locale::conv::utf_to_utf<char16_t>(in.data(),
                                                     in.data() + in.length());
}

template <typename T>
std::u8string toUtf8(const std::basic_string<T> &in)
{
    return boost::locale::conv::utf_to_utf<char8_t>(in);
}

template <typename T>
std::string toRegular(const std::basic_string<T> &in)
{
    return boost::locale::conv::utf_to_utf<char>(in);
}

template <typename T>
std::string toRegular(std::basic_string_view<T> in)
{
    return boost::locale::conv::utf_to_utf<char>(in.data(),
                                                 in.data() + in.length());
}

}  // namespace triegen
