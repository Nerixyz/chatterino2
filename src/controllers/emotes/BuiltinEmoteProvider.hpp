#pragma once

#include "common/ChatterinoSetting.hpp"
#include "controllers/emotes/EmoteProvider.hpp"
#include "util/FunctionRef.hpp"

#include <memory>

class QJsonValue;

namespace chatterino {

class TwitchChannel;

class EmoteHolder;

class BuiltinEmoteProvider
    : public EmoteProvider,
      public std::enable_shared_from_this<BuiltinEmoteProvider>
{
public:
    static constexpr uint32_t FFZ_PRIORITY = 10;
    static constexpr uint32_t BTTV_PRIORITY = 20;
    static constexpr uint32_t SEVENTV_PRIORITY = 30;

    BuiltinEmoteProvider(BoolSetting *globalSetting, QString globalUrl,
                         BoolSetting *channelSetting, QString name, QString id,
                         uint32_t priority);

    void initialize() override;

    void reloadGlobalEmotes(
        std::function<void(ExpectedStr<void>)> onDone) override;

    void loadChannelEmotes(
        const std::shared_ptr<Channel> &channel,
        std::function<void(ExpectedStr<EmoteLoadResult>)> onDone,
        LoadChannelArgs args) override;

    bool supportsChannel(Channel *channel) override;

    bool hasChannelEmotes() const override;
    bool hasGlobalEmotes() const override;

protected:
    virtual std::optional<EmoteMap> parseChannelEmotes(
        TwitchChannel &twitch, const QJsonValue &json) = 0;
    virtual std::optional<EmoteMap> parseGlobalEmotes(
        const QJsonValue &json) = 0;

    virtual QString channelEmotesUrl(const TwitchChannel &twitch) const = 0;

private:
    BoolSetting *globalSetting;
    QString globalUrl;

    BoolSetting *channelSetting;

    pajlada::Signals::SignalHolder signals;
};

}  // namespace chatterino
