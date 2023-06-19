#include "providers/kick/KickMessageBuilder.hpp"

#include "Application.hpp"
#include "common/Channel.hpp"
#include "messages/Link.hpp"
#include "messages/Message.hpp"
#include "messages/MessageElement.hpp"
#include "messages/SharedMessageBuilder.hpp"
#include "providers/kick/KickMessage.hpp"
#include "singletons/Emotes.hpp"

namespace chatterino {

KickMessageBuilder::KickMessageBuilder(Channel *channel,
                                       const KickMessage &message)
    : msg_(message)
    , channel_(channel)
{
}

MessagePtr KickMessageBuilder::build()
{
    this->appendChannelName();
    this->appendTimestamp();
    this->appendUsername();

    this->message().messageText = this->msg_.content().toString();
    this->message().searchText += this->msg_.content();

    this->appendContent();

    return this->release();
}

void KickMessageBuilder::appendChannelName()
{
    QString channelName(u'#' + QString::number(this->msg_.chatroomID()));
    Link link(Link::JumpToChannel, this->channel_->getName());

    this->emplace<TextElement>(channelName, MessageElementFlag::ChannelName,
                               MessageColor::System)
        ->setLink(link);
}

void KickMessageBuilder::appendTimestamp()
{
    this->message().serverReceivedTime = this->msg_.createdAt();
    this->emplace<TimestampElement>(this->message().serverReceivedTime.time());
}

void KickMessageBuilder::appendUsername()
{
    this->message().loginName = this->msg_.sender().slug.toString();

    if (this->msg_.sender().username.compare(this->msg_.sender().slug,
                                             Qt::CaseInsensitive) != 0)
    {
        this->message().localizedName = this->msg_.sender().slug.toString();
    }

    QString usernameText = SharedMessageBuilder::stylizeUsername(
        this->message().loginName, this->message());

    this->message().searchText +=
        usernameText + u' ' + this->message().localizedName + u' ' +
        this->message().loginName + QStringLiteral(u": ");

    usernameText += u':';

    this->emplace<TextElement>(usernameText, MessageElementFlag::Username,
                               QColor(this->msg_.sender().color),
                               FontStyle::ChatMediumBold)
        ->setLink({Link::UserInfo, this->message().displayName});
}

void KickMessageBuilder::appendContent()
{
    for (auto word : this->msg_.content().tokenize(u' '))
    {
        if (word.isEmpty())
        {
            continue;
        }

        // split words
        for (auto &variant : getApp()->emotes->emojis.parse(
                 word.toString() /* THIS IS SO SAD! */))
        {
            boost::apply_visitor(
                [&](auto &&arg) {
                    this->addTextOrEmoji(arg);
                },
                variant);
        }
    }
}

}  // namespace chatterino