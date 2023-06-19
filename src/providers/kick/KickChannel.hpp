#pragma once

#include "common/Channel.hpp"

namespace chatterino {

class KickChannel : public Channel
{
public:
    KickChannel(size_t id);

    bool canSendMessage() const override;
    size_t id() const
    {
        return this->id_;
    }

private:
    size_t id_;
};

}  // namespace chatterino
