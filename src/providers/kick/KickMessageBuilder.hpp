#pragma once

#include "messages/MessageBuilder.hpp"

namespace chatterino {

class Channel;
class KickMessage;

class KickMessageBuilder : public MessageBuilder
{
public:
    KickMessageBuilder(Channel *channel, const KickMessage &message);

    MessagePtr build();

private:
    void appendChannelName();
    void appendTimestamp();
    void appendUsername();
    void appendContent();

    const KickMessage &msg_;
    Channel *channel_;
};

}  // namespace chatterino
