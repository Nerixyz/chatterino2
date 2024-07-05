#pragma once

#include "widgets/buttons/PixmapButton.hpp"

#include <QWidget>

#include <memory>

namespace chatterino {

struct Message;
using MessagePtr = std::shared_ptr<const Message>;

class MessageLayout;
using MessageLayoutPtr = std::shared_ptr<MessageLayout>;

class MessageButtons : public QWidget
{
    Q_OBJECT

public:
    MessageButtons(QWidget *parent);

    void setMessageLayout(const MessageLayoutPtr &layout);

    const Message *message() const;
    MessagePtr messagePtr() const;
    const MessageLayout *messageLayout() const;

    void setCanReply(bool canReply);

signals:
    void replyAllRequested();
    void replyRequested();
    void copyRequested();

private:
    PixmapButton copyButton_;
    PixmapButton replyButton_;
    PixmapButton replyAllButton_;
    MessageLayoutPtr currentLayout_;
    bool canReply_ = true;
};

}  // namespace chatterino
