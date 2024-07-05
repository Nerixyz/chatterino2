#include "widgets/buttons/MessageButtons.hpp"

#include "messages/layouts/MessageLayout.hpp"
#include "messages/Message.hpp"
#include "singletons/Resources.hpp"

#include <QHBoxLayout>

namespace chatterino {

MessageButtons::MessageButtons(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setAlignment(Qt::AlignRight);

    auto initButton = [&](PixmapButton *btn, const QPixmap &pm) {
        layout->addWidget(btn);
        btn->setScaleIndependantSize({20, 20});
        btn->setPixmap(pm);
    };
    initButton(&this->replyAllButton_, getResources().buttons.replyThreadDark);
    initButton(&this->replyButton_, getResources().buttons.replyDark);
    initButton(&this->copyButton_, getResources().buttons.copyLight);

    QObject::connect(&this->replyAllButton_, &PixmapButton::leftClicked, this,
                     &MessageButtons::replyAllRequested);
    QObject::connect(&this->replyButton_, &PixmapButton::leftClicked, this,
                     &MessageButtons::replyRequested);
    QObject::connect(&this->copyButton_, &PixmapButton::leftClicked, this,
                     &MessageButtons::copyRequested);
}

void MessageButtons::setMessageLayout(const MessageLayoutPtr &layout)
{
    if (this->currentLayout_.get() == layout.get())
    {
        return;
    }

    this->currentLayout_ = layout;
    if (!this->currentLayout_)
    {
        this->hide();
        return;
    }

    bool canReply = this->canReply_ && this->currentLayout_->isReplyable();
    bool canReplyAll =
        canReply && this->currentLayout_->getMessage()->replyThread != nullptr;

    bool anyChange = this->replyAllButton_.isVisible() != canReplyAll ||
                     this->replyButton_.isVisible() != canReply;

    this->replyAllButton_.setVisible(canReplyAll);
    this->replyButton_.setVisible(canReply);

    if (anyChange)
    {
        this->adjustSize();
    }
}

const Message *MessageButtons::message() const
{
    if (!this->currentLayout_)
    {
        return nullptr;
    }
    return this->currentLayout_->getMessage();
}

MessagePtr MessageButtons::messagePtr() const
{
    if (!this->currentLayout_)
    {
        return nullptr;
    }
    return this->currentLayout_->getMessagePtr();
}

const MessageLayout *MessageButtons::messageLayout() const
{
    return this->currentLayout_.get();
}

void MessageButtons::setCanReply(bool canReply)
{
    this->canReply_ = canReply;
}

}  // namespace chatterino
