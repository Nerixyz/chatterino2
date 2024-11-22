#include "util/RapidjsonHelpers.hpp"

#include <rapidjson/prettywriter.h>

namespace chatterino {
namespace rj {

    void addMember(rapidjson::Value &obj, const char *key,
                   rapidjson::Value &&value,
                   rapidjson::Document::AllocatorType &a)
    {
        obj.AddMember(rapidjson::Value(key, a).Move(), std::move(value), a);
    }

    void addMember(rapidjson::Value &obj, const char *key,
                   rapidjson::Value &value,
                   rapidjson::Document::AllocatorType &a)
    {
        obj.AddMember(rapidjson::Value(key, a).Move(), value.Move(), a);
    }

    QString stringify(const rapidjson::Value &value)
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        value.Accept(writer);

        return buffer.GetString();
    }

    bool getSafeObject(rapidjson::Value &obj, std::string_view key,
                       rapidjson::Value &out)
    {
        if (!obj.IsObject())
        {
            return false;
        }

        rapidjson::Value kv(key.data(), key.size());
        auto it = obj.FindMember(kv);
        if (it != obj.MemberEnd())
        {
            out = it->value.Move();
            return true;
        }
        return false;
    }

    bool checkJsonValue(const rapidjson::Value &obj,
                        const rapidjson::Value &key)
    {
        return obj.IsObject() && obj.HasMember(key);
    }

}  // namespace rj
}  // namespace chatterino
