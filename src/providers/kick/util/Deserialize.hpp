#pragma once

#include <QStringView>
#include <rapidjson/document.h>
#include <rapidjson/encodings.h>

#include <optional>

namespace chatterino::de {

namespace detail {
    using Utf16 = rapidjson::UTF16<char16_t>;
    using Utf16Val = rapidjson::GenericValue<Utf16>;
}  // namespace detail

template <typename Target, typename Enc = detail::Utf16>
struct De {
    using CharType = typename rapidjson::GenericValue<Enc>::Ch;
    static bool get(const rapidjson::GenericValue<Enc> &val,
                    const CharType *key, Target &target);
};

template <typename Enc>
const rapidjson::GenericValue<Enc> *child(
    const rapidjson::GenericValue<Enc> &src,
    const typename rapidjson::GenericValue<Enc>::Ch *key)
{
    if (!src.IsObject())
    {
        return nullptr;
    }
    auto member = src.FindMember(key);
    if (member != src.MemberEnd())
    {
        return &member->value;
    }
    return nullptr;
}

template <>
struct De<QStringView, detail::Utf16> {
    static bool get(const detail::Utf16Val &val, const char16_t *key,
                    QStringView &target)
    {
        const auto *value = child(val, key);
        if (value == nullptr)
        {
            return false;
        }

        if (!value->IsString())
        {
            return false;
        }
        target = QStringView(value->GetString(), value->GetStringLength());
        return true;
    }
};

template <typename Enc>
struct De<size_t, Enc> {
    using CharType = typename rapidjson::GenericValue<Enc>::Ch;
    static bool get(const rapidjson::GenericValue<Enc> &val,
                    const CharType *key, size_t &target)
    {
        const auto *value = child(val, key);
        if (value == nullptr)
        {
            return false;
        }

        if (value->IsUint())
        {
            target = static_cast<size_t>(value->GetUint());
            return true;
        }
        if constexpr (sizeof(size_t) >= 64 / 8)
        {
            if (value->IsUint64())
            {
                target = static_cast<size_t>(value->GetUint64());
                return true;
            }
        }
        return false;
    }
};

template <typename Enc>
struct De<bool, Enc> {
    using CharType = typename rapidjson::GenericValue<Enc>::Ch;
    static bool get(const rapidjson::GenericValue<Enc> &val,
                    const CharType *key, bool &target)
    {
        const auto *value = child(val, key);
        if (value == nullptr || !value->IsBool())
        {
            return false;
        }
        target = value->GetBool();
        return true;
    }
};

template <typename Target, typename Enc>
bool get(const rapidjson::GenericValue<Enc> &val,
         const typename rapidjson::GenericValue<Enc>::Ch *key, Target &target)
{
    return De<Target, Enc>::get(val, key, target);
}

}  // namespace chatterino::de
