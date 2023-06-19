#include "providers/kick/KickChannel.hpp"

namespace chatterino {

KickChannel::KickChannel(size_t id)
    : Channel(QString::number(id), Channel::Type::Kick)
    , id_(id)
{
}

bool KickChannel::canSendMessage() const
{
    return false;
}

}  // namespace chatterino
