#pragma once

#include <cstdint>

namespace triegen::detail {

template <size_t Size>
struct IntForSize;

template <>
struct IntForSize<4> {
    using Signed = int32_t;
};

template <>
struct IntForSize<8> {
    using Signed = int64_t;
};

}  // namespace triegen::detail

using qsizetype = triegen::detail::IntForSize<sizeof(size_t)>::Signed;
