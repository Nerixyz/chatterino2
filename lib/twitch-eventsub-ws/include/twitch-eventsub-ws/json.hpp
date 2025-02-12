#pragma once

#include "twitch-eventsub-ws/string.hpp"

#include <boost/json/value.hpp>
#include <boost/system/error_code.hpp>

#include <concepts>
#include <string>
#include <vector>

namespace chatterino::eventsub::lib::json {

template <typename T>
struct FromJsonTag {
};

template <typename T>
bool fromJson(T &target, boost::system::error_code &ec,
              const boost::json::value &jvRoot)
{
    tag_invoke(FromJsonTag<T>{}, target, ec, jvRoot);
    return !ec.failed();
}

template <>
inline bool fromJson<std::string>(std::string &target,
                                  boost::system::error_code &ec,
                                  const boost::json::value &jvRoot)
{
    auto res = jvRoot.try_as_string();
    if (!res)
    {
        ec = res.error();
        return false;
    }
    target = *res;
    return true;
}

template <>
inline bool fromJson<String>(String &target, boost::system::error_code &ec,
                             const boost::json::value &jvRoot)
{
    auto res = jvRoot.try_as_string();
    if (!res)
    {
        ec = res.error();
        return false;
    }
    target = std::string(*res);
    return true;
}

template <>
inline bool fromJson<bool>(bool &target, boost::system::error_code &ec,
                           const boost::json::value &jvRoot)
{
    auto res = jvRoot.try_as_bool();
    if (!res)
    {
        ec = res.error();
        return false;
    }
    target = *res;
    return true;
}

template <std::unsigned_integral T>
inline bool fromJson(T &target, boost::system::error_code &ec,
                     const boost::json::value &jvRoot)
    requires(!std::is_same_v<T, bool>)
{
    auto res = jvRoot.try_as_uint64();
    if (!res)
    {
        ec = res.error();
        return false;
    }
    target = static_cast<T>(*res);
    return true;
}

template <std::signed_integral T>
inline bool fromJson(T &target, boost::system::error_code &ec,
                     const boost::json::value &jvRoot)
{
    auto res = jvRoot.try_as_int64();
    if (!res)
    {
        ec = res.error();
        return false;
    }
    target = static_cast<T>(*res);
    return true;
}

template <typename T>
inline bool fromJson(std::vector<T> &target, boost::system::error_code &ec,
                     const boost::json::value &jvRoot)
{
    auto res = jvRoot.try_as_array();
    if (!res)
    {
        ec = res.error();
        return false;
    }
    const auto &arr = *res;

    target.resize(res->size());  // construct the elements
    size_t idx = 0;
    for (const auto &v : arr)
    {
        if (!fromJson(target[idx], ec, v))
        {
            return false;
        }
        idx++;
    }
    return true;
}

}  // namespace chatterino::eventsub::lib::json
