#pragma once

#include "util/RapidJsonSerializeQString.hpp"

#include <pajlada/serialize.hpp>
#include <rapidjson/document.h>

#include <cassert>
#include <string>

namespace chatterino {
namespace rj {

    void addMember(rapidjson::Value &obj, const char *key,
                   rapidjson::Value &&value,
                   rapidjson::Document::AllocatorType &a);
    void addMember(rapidjson::Value &obj, const char *key,
                   rapidjson::Value &value,
                   rapidjson::Document::AllocatorType &a);

    template <typename Type>
    void set(rapidjson::Value &obj, const char *key, const Type &value,
             rapidjson::Document::AllocatorType &a)
    {
        assert(obj.IsObject());

        addMember(obj, key, pajlada::Serialize<Type>::get(value, a), a);
    }

    template <>
    inline void set(rapidjson::Value &obj, const char *key,
                    const rapidjson::Value &value,
                    rapidjson::Document::AllocatorType &a)
    {
        assert(obj.IsObject());

        addMember(obj, key, const_cast<rapidjson::Value &>(value), a);
    }

    template <typename Type>
    void set(rapidjson::Document &obj, const char *key, const Type &value)
    {
        assert(obj.IsObject());

        auto &a = obj.GetAllocator();

        addMember(obj, key, pajlada::Serialize<Type>::get(value, a), a);
    }

    template <>
    inline void set(rapidjson::Document &obj, const char *key,
                    const rapidjson::Value &value)
    {
        assert(obj.IsObject());

        auto &a = obj.GetAllocator();

        addMember(obj, key, const_cast<rapidjson::Value &>(value), a);
    }

    template <typename Type>
    void add(rapidjson::Value &arr, const Type &value,
             rapidjson::Document::AllocatorType &a)
    {
        assert(arr.IsArray());

        arr.PushBack(pajlada::Serialize<Type>::get(value, a), a);
    }

    bool checkJsonValue(const rapidjson::Value &obj,
                        const rapidjson::Value &key);

    template <typename Type>
    bool getSafe(const rapidjson::Value &obj, std::string_view key, Type &out)
    {
        rapidjson::Value kv(key.data(), key.size());
        if (!checkJsonValue(obj, kv))
        {
            return false;
        }

        bool error = false;
        out = pajlada::Deserialize<Type>::get(obj[kv], &error);

        return !error;
    }

    inline bool getSafe(const rapidjson::Value &obj, std::string_view key,
                        std::string_view &out)
    {
        if (!obj.IsObject())
        {
            return false;
        }

        rapidjson::Value kv(key.data(), key.size());
        auto it = obj.FindMember(kv);
        if (it != obj.MemberEnd() && it->value.IsString())
        {
            out = {it->value.GetString(), it->value.GetStringLength()};
            return true;
        }

        return false;
    }

    template <typename Type>
    bool getSafe(const rapidjson::Value &value, Type &out)
    {
        bool error = false;
        out = pajlada::Deserialize<Type>::get(value, &error);

        return !error;
    }

    bool getSafeObject(rapidjson::Value &obj, std::string_view key,
                       rapidjson::Value &out);

    QString stringify(const rapidjson::Value &value);

}  // namespace rj
}  // namespace chatterino
