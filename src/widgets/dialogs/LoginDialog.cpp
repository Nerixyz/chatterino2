#include "widgets/dialogs/LoginDialog.hpp"

#include "Application.hpp"
#include "common/Literals.hpp"
#include "common/QLogging.hpp"
#include "controllers/accounts/AccountController.hpp"
#include "providers/lunar/Response.hpp"
#include "providers/lunar/Router.hpp"
#include "providers/lunar/Server.hpp"
#include "providers/twitch/api/Helix.hpp"
#include "util/Clipboard.hpp"
#include "util/PostToThread.hpp"

#ifdef USEWINSDK
#    include <Windows.h>
#endif

#include <pajlada/settings/setting.hpp>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QMessageBox>
#include <QPointer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

namespace {

using namespace chatterino;
using namespace chatterino::literals;

const QStringList TOKEN_SCOPES = {
    u"channel:moderate"_s,  // for seeing automod & which moderator banned/unbanned a user (felanbird unbanned weeb123)
    u"channel:read:redemptions"_s,  // for getting the list of channel point redemptions (not currently used)
    u"chat:edit"_s,      // for sending messages in chat
    u"chat:read"_s,      // for viewing messages in chat
    u"whispers:read"_s,  // for viewing recieved whispers

    // https://dev.twitch.tv/docs/api/reference#start-commercial
    u"channel:edit:commercial"_s,  // for /commercial api

    // https://dev.twitch.tv/docs/api/reference#create-clip
    u"clips:edit"_s,  // for /clip creation

    // https://dev.twitch.tv/docs/api/reference#create-stream-marker
    // https://dev.twitch.tv/docs/api/reference#modify-channel-information
    u"channel:manage:broadcast"_s,  // for creating stream markers with /marker command, and for the /settitle and /setgame commands

    // https://dev.twitch.tv/docs/api/reference#get-user-block-list
    u"user:read:blocked_users"_s,  // for getting list of blocked users

    // https://dev.twitch.tv/docs/api/reference#block-user
    // https://dev.twitch.tv/docs/api/reference#unblock-user
    u"user:manage:blocked_users"_s,  // for blocking/unblocking other users

    // https://dev.twitch.tv/docs/api/reference#manage-held-automod-messages
    u"moderator:manage:automod"_s,  // for approving/denying automod messages

    // https://dev.twitch.tv/docs/api/reference#start-a-raid
    // https://dev.twitch.tv/docs/api/reference#cancel-a-raid
    u"channel:manage:raids"_s,  // for starting/canceling raids

    // https://dev.twitch.tv/docs/api/reference#create-poll
    // https://dev.twitch.tv/docs/api/reference#end-poll
    u"channel:manage:polls"_s,  // for creating & ending polls (not currently used)

    // https://dev.twitch.tv/docs/api/reference#get-polls
    u"channel:read:polls"_s,  // for reading broadcaster poll status (not currently used)

    // https://dev.twitch.tv/docs/api/reference#create-prediction
    // https://dev.twitch.tv/docs/api/reference#end-prediction
    u"channel:manage:predictions"_s,  // for creating & ending predictions (not currently used)

    // https://dev.twitch.tv/docs/api/reference#get-predictions
    u"channel:read:predictions"_s,  // for reading broadcaster prediction status (not currently used)

    // https://dev.twitch.tv/docs/api/reference#send-chat-announcement
    u"moderator:manage:announcements"_s,  // for /announce api

    // https://dev.twitch.tv/docs/api/reference#send-whisper
    u"user:manage:whispers"_s,  // for whispers api

    // https://dev.twitch.tv/docs/api/reference#ban-user
    // https://dev.twitch.tv/docs/api/reference#unban-user
    u"moderator:manage:banned_users"_s,  // for ban/unban/timeout/untimeout api

    // https://dev.twitch.tv/docs/api/reference#delete-chat-messages
    u"moderator:manage:chat_messages"_s,  // for delete message api (/delete, /clear)

    // https://dev.twitch.tv/docs/api/reference#update-user-chat-color
    u"user:manage:chat_color"_s,  // for update user color api (/color coral)

    // https://dev.twitch.tv/docs/api/reference#get-chat-settings
    u"moderator:manage:chat_settings"_s,  // for roomstate api (/followersonly, /uniquechat, /slow)

    // https://dev.twitch.tv/docs/api/reference#get-moderators
    // https://dev.twitch.tv/docs/api/reference#add-channel-moderator
    // https://dev.twitch.tv/docs/api/reference#remove-channel-vip
    u"channel:manage:moderators"_s,  // for add/remove/view mod api

    // https://dev.twitch.tv/docs/api/reference#add-channel-vip
    // https://dev.twitch.tv/docs/api/reference#remove-channel-vip
    // https://dev.twitch.tv/docs/api/reference#get-vips
    u"channel:manage:vips"_s,  // for add/remove/view vip api

    // https://dev.twitch.tv/docs/api/reference#get-chatters
    u"moderator:read:chatters"_s,  // for get chatters api

    // https://dev.twitch.tv/docs/api/reference#get-shield-mode-status
    // https://dev.twitch.tv/docs/api/reference#update-shield-mode-status
    u"moderator:manage:shield_mode"_s,  // for reading/managing the channel's shield-mode status

    // https://dev.twitch.tv/docs/api/reference/#send-a-shoutout
    u"moderator:manage:shoutouts"_s,  // for reading/managing the channel's shoutouts (not currently used)
};

constexpr const uint16_t SERVER_PORT = 39534;
const QString LOCAL_CLIENT_ID = u"k3swk9byj8n6a50on6a5qim7re6pty"_s;
const QString LOCAL_REDIRECT_ENDPOINT = u"/callback"_s;

QString buildLocalUrl(const QString &state)
{
    // TODO(Qt ^5.13): use initializer list
    QUrlQuery query;
    query.addQueryItem(u"response_type"_s, u"token"_s);
    query.addQueryItem(u"client_id"_s, LOCAL_CLIENT_ID);
    query.addQueryItem(u"redirect_uri"_s, u"http://localhost:%1%2"_s.arg(
                                              QString::number(SERVER_PORT),
                                              LOCAL_REDIRECT_ENDPOINT));
    query.addQueryItem(u"state"_s, state);
    query.addQueryItem(u"scope"_s, TOKEN_SCOPES.join(u' '));

    return u"https://id.twitch.tv/oauth2/authorize?%1"_s.arg(
        query.toString(QUrl::FullyEncoded));
}

bool logInWithCredentials(QWidget *parent, const QString &userID,
                          const QString &username, const QString &clientID,
                          const QString &oauthToken)
{
    QStringList errors;

    if (userID.isEmpty())
    {
        errors.append("Missing user ID");
    }
    if (username.isEmpty())
    {
        errors.append("Missing username");
    }
    if (clientID.isEmpty())
    {
        errors.append("Missing Client ID");
    }
    if (oauthToken.isEmpty())
    {
        errors.append("Missing OAuth Token");
    }

    if (errors.length() > 0)
    {
        QMessageBox messageBox(parent);
        messageBox.setWindowTitle("Invalid account credentials");
        messageBox.setIcon(QMessageBox::Critical);
        messageBox.setText(errors.join("<br>"));
        messageBox.exec();
        return false;
    }

    std::string basePath = "/accounts/uid" + userID.toStdString();
    pajlada::Settings::Setting<QString>::set(basePath + "/username", username);
    pajlada::Settings::Setting<QString>::set(basePath + "/userID", userID);
    pajlada::Settings::Setting<QString>::set(basePath + "/clientID", clientID);
    pajlada::Settings::Setting<QString>::set(basePath + "/oauthToken",
                                             oauthToken);

    getApp()->accounts->twitch.reloadUsers();
    getApp()->accounts->twitch.currentUsername = username;
    return true;
}

}  // namespace

namespace chatterino {

BasicLoginWidget::BasicLoginWidget()
{
    const QString logInLink = "https://chatterino.com/client_login";
    this->setLayout(&this->ui_.layout);

    this->ui_.loginButton.setText("Log in (Opens in browser)");
    this->ui_.pasteCodeButton.setText("Paste login info");
    this->ui_.unableToOpenBrowserHelper.setWindowTitle(
        "Chatterino - unable to open in browser");
    this->ui_.unableToOpenBrowserHelper.setWordWrap(true);
    this->ui_.unableToOpenBrowserHelper.hide();
    this->ui_.unableToOpenBrowserHelper.setText(
        QString("An error occurred while attempting to open <a href=\"%1\">the "
                "log in link (%1)</a> - open it manually in your browser and "
                "proceed from there.")
            .arg(logInLink));
    this->ui_.unableToOpenBrowserHelper.setOpenExternalLinks(true);

    this->ui_.horizontalLayout.addWidget(&this->ui_.loginButton);
    this->ui_.horizontalLayout.addWidget(&this->ui_.pasteCodeButton);

    this->ui_.layout.addLayout(&this->ui_.horizontalLayout);
    this->ui_.layout.addWidget(&this->ui_.unableToOpenBrowserHelper);

    connect(&this->ui_.loginButton, &QPushButton::clicked, [this, logInLink]() {
        qCDebug(chatterinoWidget) << "open login in browser";
        if (!QDesktopServices::openUrl(QUrl(logInLink)))
        {
            qCWarning(chatterinoWidget) << "open login in browser failed";
            this->ui_.unableToOpenBrowserHelper.show();
        }
    });

    connect(&this->ui_.pasteCodeButton, &QPushButton::clicked, [this]() {
        QStringList parameters = getClipboardText().split(";");
        QString oauthToken, clientID, username, userID;

        // Removing clipboard content to prevent accidental paste of credentials into somewhere
        crossPlatformCopy("");

        for (const auto &param : parameters)
        {
            QStringList kvParameters = param.split('=');
            if (kvParameters.size() != 2)
            {
                continue;
            }
            QString key = kvParameters[0];
            QString value = kvParameters[1];

            if (key == "oauth_token")
            {
                oauthToken = value;
            }
            else if (key == "client_id")
            {
                clientID = value;
            }
            else if (key == "username")
            {
                username = value;
            }
            else if (key == "user_id")
            {
                userID = value;
            }
            else
            {
                qCWarning(chatterinoWidget) << "Unknown key in code: " << key;
            }
        }

        if (logInWithCredentials(this, userID, username, clientID, oauthToken))
        {
            this->window()->close();
        }
    });
}

AdvancedLoginWidget::AdvancedLoginWidget()
{
    this->setLayout(&this->ui_.layout);

    this->ui_.instructionsLabel.setText("1. Fill in your username"
                                        "\n2. Fill in your user ID"
                                        "\n3. Fill in your client ID"
                                        "\n4. Fill in your OAuth token"
                                        "\n5. Press Add user");
    this->ui_.instructionsLabel.setWordWrap(true);

    this->ui_.layout.addWidget(&this->ui_.instructionsLabel);
    this->ui_.layout.addLayout(&this->ui_.formLayout);
    this->ui_.layout.addLayout(&this->ui_.buttonUpperRow.layout);

    this->refreshButtons();

    /// Form
    this->ui_.formLayout.addRow("Username", &this->ui_.usernameInput);
    this->ui_.formLayout.addRow("User ID", &this->ui_.userIDInput);
    this->ui_.formLayout.addRow("Client ID", &this->ui_.clientIDInput);
    this->ui_.formLayout.addRow("OAuth token", &this->ui_.oauthTokenInput);

    this->ui_.oauthTokenInput.setEchoMode(QLineEdit::Password);

    connect(&this->ui_.userIDInput, &QLineEdit::textChanged, [this]() {
        this->refreshButtons();
    });
    connect(&this->ui_.usernameInput, &QLineEdit::textChanged, [this]() {
        this->refreshButtons();
    });
    connect(&this->ui_.clientIDInput, &QLineEdit::textChanged, [this]() {
        this->refreshButtons();
    });
    connect(&this->ui_.oauthTokenInput, &QLineEdit::textChanged, [this]() {
        this->refreshButtons();
    });

    /// Upper button row

    this->ui_.buttonUpperRow.addUserButton.setText("Add user");
    this->ui_.buttonUpperRow.clearFieldsButton.setText("Clear fields");

    this->ui_.buttonUpperRow.layout.addWidget(
        &this->ui_.buttonUpperRow.addUserButton);
    this->ui_.buttonUpperRow.layout.addWidget(
        &this->ui_.buttonUpperRow.clearFieldsButton);

    connect(&this->ui_.buttonUpperRow.clearFieldsButton, &QPushButton::clicked,
            [this]() {
                this->ui_.userIDInput.clear();
                this->ui_.usernameInput.clear();
                this->ui_.clientIDInput.clear();
                this->ui_.oauthTokenInput.clear();
            });

    connect(&this->ui_.buttonUpperRow.addUserButton, &QPushButton::clicked,
            [this]() {
                QString userID = this->ui_.userIDInput.text();
                QString username = this->ui_.usernameInput.text();
                QString clientID = this->ui_.clientIDInput.text();
                QString oauthToken = this->ui_.oauthTokenInput.text();

                logInWithCredentials(this, userID, username, clientID,
                                     oauthToken);
            });
}

void AdvancedLoginWidget::refreshButtons()
{
    if (this->ui_.userIDInput.text().isEmpty() ||
        this->ui_.usernameInput.text().isEmpty() ||
        this->ui_.clientIDInput.text().isEmpty() ||
        this->ui_.oauthTokenInput.text().isEmpty())
    {
        this->ui_.buttonUpperRow.addUserButton.setEnabled(false);
    }
    else
    {
        this->ui_.buttonUpperRow.addUserButton.setEnabled(true);
    }
}

AutomaticLoginWidget::AutomaticLoginWidget()
    : state_(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    this->setLayout(&this->ui_.layout);
    this->ui_.status.setText(u"Waiting..."_s);

    this->ui_.layout.addWidget(&this->ui_.status, 1, Qt::AlignCenter);
    this->ui_.layout.addLayout(&this->ui_.buttons.layout);

    this->ui_.buttons.layout.addWidget(&this->ui_.buttons.openInBrowser);
    this->ui_.buttons.layout.addWidget(&this->ui_.buttons.copyURL);

    this->ui_.buttons.openInBrowser.setText(u"Open in Browser"_s);
    this->ui_.buttons.copyURL.setText(u"Copy URL"_s);

    connect(&this->ui_.buttons.openInBrowser, &QPushButton::clicked, [this]() {
        QDesktopServices::openUrl(QUrl(buildLocalUrl(this->state())));
    });
    connect(&this->ui_.buttons.copyURL, &QPushButton::clicked, [this]() {
        crossPlatformCopy(buildLocalUrl(this->state()));
    });
}

const QString &AutomaticLoginWidget::state() const
{
    return this->state_;
}

void AutomaticLoginWidget::setStatus(QString status)
{
    runInGuiThread([this, status = std::move(status)]() {
        this->ui_.status.setText(u"Waiting...<br>%1"_s.arg(status));
    });
}

void AutomaticLoginWidget::tryLogin(QString accessToken)
{
    QPointer<AutomaticLoginWidget> self(this);
    runInGuiThread([self, accessToken = std::move(accessToken)] {
        auto helix = std::make_shared<Helix>();
        helix->update(LOCAL_CLIENT_ID, accessToken);
        helix->getCurrentUser(
            [helix, self, accessToken](const auto &user) {
                if (self.isNull())
                {
                    return;
                }
                auto res = QMessageBox::question(
                    self.data(), u"Login"_s, u"Log in as %1"_s.arg(user.login),
                    QMessageBox::Yes | QMessageBox::No);
                if (res == QMessageBox::Yes)
                {
                    logInWithCredentials(self.data(), user.id, user.login,
                                         LOCAL_CLIENT_ID, accessToken);
                    self->window()->close();
                }
            },
            [helix, self]() {
                if (self.isNull())
                {
                    return;
                }
                self->setStatus(u"(Warning: Failed to fetch current user)"_s);
            });
    });
}

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    this->setMinimumWidth(300);
    this->setWindowFlags(
        (this->windowFlags() & ~(Qt::WindowContextHelpButtonHint)) |
        Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    this->setWindowTitle("Add new account");

    this->setLayout(&this->ui_.mainLayout);
    this->ui_.mainLayout.addWidget(&this->ui_.tabWidget);

    this->ui_.tabWidget.addTab(&this->ui_.basic, "Basic");
    this->ui_.tabWidget.addTab(&this->ui_.advanced, "Advanced");
    this->ui_.tabWidget.addTab(&this->ui_.automatic, "Automatic");

    this->ui_.buttonBox.setStandardButtons(QDialogButtonBox::Close);

    QObject::connect(&this->ui_.buttonBox, &QDialogButtonBox::rejected,
                     [this]() {
                         this->close();
                     });

    this->ui_.mainLayout.addWidget(&this->ui_.buttonBox);

    (new lunar::Server(
         lunar::Router{{
             {
                 "/callback",
                 lunar::Router::StaticFile{
                     "text/html", ([]() -> QByteArray {
                         QFile file(u":/pages/login-callback.html"_s);
                         if (!file.open(QFile::ReadOnly))
                         {
                             qCWarning(chatterinoApp) << "Resource not found";
                             Q_ASSERT_X(false, "LoginDialog",
                                        "Resource not found");
                             return QByteArrayLiteral("resource not found");
                         }
                         auto data = file.readAll();
                         file.close();
                         return data;
                     })()},
             },
             {
                 "/data",
                 [this](const QJsonObject &json) -> lunar::Response {
                     using Status = lunar::Response::Status;
                     if (json["state"_L1].toString() !=
                         this->ui_.automatic.state())
                     {
                         this->ui_.automatic.setStatus(
                             u"(Warning: received request with invalid state)"_s);
                         return {Status::Forbidden,
                                 {{"error"_L1, u"Invaild state"_s}}};
                     }
                     auto error = json["error"].toString();
                     if (!error.isEmpty())
                     {
                         this->ui_.automatic.setStatus(
                             u"(Error: %1 - %2)"_s.arg(
                                 error.toHtmlEscaped(),
                                 json["error_description"]
                                     .toString(u"(no info)"_s)
                                     .toHtmlEscaped()));
                         return {Status::Ok, {}};
                     }

                     auto token = json["access_token"].toString();
                     if (token.isEmpty())
                     {
                         this->ui_.automatic.setStatus(
                             u"(Warning: empty or no token)"_s);
                         return {
                             Status::BadRequest,
                             {{"error"_L1, u"Received empty or no token"_s}}};
                     }

                     this->ui_.automatic.tryLogin(token);

                     return {Status::Ok, {}};
                 },
             },
         }},
         this))
        ->listen(SERVER_PORT);
}

}  // namespace chatterino
