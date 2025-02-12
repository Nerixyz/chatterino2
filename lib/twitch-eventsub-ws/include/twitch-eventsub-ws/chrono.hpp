#pragma once

#include "twitch-eventsub-ws/json.hpp"

#include <boost/json.hpp>

#include <chrono>

namespace chatterino::eventsub::lib::json {

template <>
bool fromJson<std::chrono::system_clock::time_point>(
    std::chrono::system_clock::time_point &target,
    boost::system::error_code &ec, const boost::json::value &jvRoot);

}  // namespace chatterino::eventsub::lib::json
