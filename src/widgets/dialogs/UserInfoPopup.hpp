#pragma once

#include "singletons/Paths.hpp"
#include "widgets/BaseWindow.hpp"
#include "widgets/DraggablePopup.hpp"
#include "widgets/helper/EffectLabel.hpp"

#include <pajlada/signals/scoped-connection.hpp>
#include <pajlada/signals/signal.hpp>
#include <QPixmap>
#include <QPointer>

#include <chrono>

class QCheckBox;
class QMovie;

namespace chatterino {

inline static const QString SEVENTV_USER_API =
    "https://7tv.io/v3/users/twitch/%1";

class Channel;
using ChannelPtr = std::shared_ptr<Channel>;
class Label;
class EditUserNotesDialog;
class ChannelView;
class Split;
struct HelixUser;

class UserInfoPopup final : public DraggablePopup
{
    Q_OBJECT

public:
    /**
     * @param closeAutomatically Decides whether the popup should close when it loses focus
     * @param split Will be used as the popup's parent. Must not be null
     */
    UserInfoPopup(bool closeAutomatically, Split *split);

    void setData(const QString &name, const ChannelPtr &channel);
    void setData(const QString &name, const ChannelPtr &contextChannel,
                 const ChannelPtr &openingChannel);

protected:
    void themeChangedEvent() override;
    void scaleChangedEvent(float scale) override;
    void windowDeactivationEvent() override;

private:
    void installEvents();
    void updateUserData();
    void updateLatestMessages();
    void updateNotes();

    void loadAvatar(const HelixUser &user);

    void loadSevenTVAvatar(const HelixUser &user);
    void setSevenTVAvatar(const QString &filename, const QByteArray &format);

    void saveCacheAvatar(const QByteArray &avatar,
                         const QString &filename) const;

    void updateAvatarUrl();

    bool isMod_{};
    bool isBroadcaster_{};

    Split *split_;

    QString userName_;
    QString userId_;
    QString avatarUrl_;
    QString helixAvatarUrl_;
    QString seventvAvatarUrl_;

    // The channel the popup was opened from (e.g. /mentions or #forsen). Can be a special channel.
    ChannelPtr channel_;

    // The channel the messages are rendered from (e.g. #forsen). Can be a special channel, but will try to not be where possible.
    ChannelPtr underlyingChannel_;

    pajlada::Signals::NoArgSignal userStateChanged_;

    std::unique_ptr<pajlada::Signals::ScopedConnection> refreshConnection_;
    std::unique_ptr<pajlada::Signals::ScopedConnection>
        userDataUpdatedConnection_;

    // If we should close the dialog automatically if the user clicks out
    // Set based on the "Automatically close usercard when it loses focus" setting
    // Pinned status is tracked in DraggablePopup::isPinned_.
    const bool closeAutomatically_;

    struct {
        Button *avatarButton = nullptr;
        Button *localizedNameCopyButton = nullptr;

        Label *nameLabel = nullptr;
        Label *localizedNameLabel = nullptr;
        Label *pronounsLabel = nullptr;
        Label *followerCountLabel = nullptr;
        Label *createdDateLabel = nullptr;
        Label *userIDLabel = nullptr;
        Label *followageLabel = nullptr;
        Label *subageLabel = nullptr;

        QCheckBox *block = nullptr;
        QCheckBox *ignoreHighlights = nullptr;
        Label *notesPreview = nullptr;
        EffectLabel2 *notesAdd = nullptr;

        Label *noMessagesLabel = nullptr;
        ChannelView *latestMessages = nullptr;

        EffectLabel2 *usercardLabel = nullptr;
        EffectLabel2 *switchAvatars = nullptr;
    } ui_;

    QMovie *seventvAvatar_ = nullptr;
    bool isTwitchAvatarShown_ = true;
    QPixmap avatarPixmap_;
    QPointer<EditUserNotesDialog> editUserNotesDialog_;

    class TimeoutWidget : public BaseWidget
    {
    public:
        enum Action { Ban, Unban, Timeout };

        TimeoutWidget();

        pajlada::Signals::Signal<std::pair<Action, int>> buttonClicked;

    protected:
        void paintEvent(QPaintEvent *event) override;
    };
};

}  // namespace chatterino
