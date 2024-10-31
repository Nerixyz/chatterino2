#pragma once

#ifdef USEWINSDK

#    include "ForwardDecl.hpp"

#    include <QTimer>
#    include <QWidget>

#    include <memory>

namespace chatterino {

class Split;
class Channel;
using ChannelPtr = std::shared_ptr<Channel>;

class AttachedWindow : public QWidget
{
    AttachedWindow(HWND _target);

public:
    struct GetArgs {
        QString winId;
        int yOffset = -1;
        double x = -1;
        double pixelRatio = -1;
        int width = -1;
        int height = -1;
        bool fullscreen = false;
    };

    ~AttachedWindow() override;

    static AttachedWindow *get(HWND target_, const GetArgs &args);
    static AttachedWindow *getForeground(const GetArgs &args);
    static void detach(const QString &winId);

    void setChannel(ChannelPtr channel);

protected:
    void showEvent(QShowEvent *) override;

private:
    struct {
        Split *split;
    } ui_{};

    struct Item {
        HWND hwnd;
        AttachedWindow *window;
        QString winId;
    };

    static std::vector<Item> items;

    void attachToHwnd(HWND attached);
    void updateWindowRect(HWND attached);

    HWND target_;
    double x_ = -1;
    double pixelRatio_ = -1;
    int width_ = 360;
    int height_ = -1;
    bool fullscreen_ = false;

    bool validProcessName_ = false;
    bool attached_ = false;
    QTimer timer_;
    QTimer slowTimer_;

    QRect lastAttachedRect_;
    size_t lastRectEqCounter_ = 0;
};

}  // namespace chatterino

#endif
