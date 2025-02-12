#include "twitch-eventsub-ws/chrono.hpp"

#include "twitch-eventsub-ws/date.h"

#include <sstream>

namespace chatterino::eventsub::lib::json {

template <>
bool fromJson<std::chrono::system_clock::time_point>(
    std::chrono::system_clock::time_point &target,
    boost::system::error_code &ec, const boost::json::value &jvRoot)
{
    const auto raw = boost::json::try_value_to<std::string>(jvRoot);
    if (raw.has_error())
    {
        ec = raw.error();
        return false;
    }

    std::istringstream in{*raw};
    in >> date::parse("%FT%TZ", target);

    return true;
}

}  // namespace chatterino::eventsub::lib::json
