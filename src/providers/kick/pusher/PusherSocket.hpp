#pragma once

#include <QObject>
#include <QWebSocket>

namespace chatterino {

struct PusherMessage;

class PusherSocket : public QObject
{
    Q_OBJECT

public:
    PusherSocket(QUrl url, QObject *parent = nullptr);

    void subscribe(const QString &channel, const QString &auth = {});

signals:
    void connectionEstablished(const QString &socketID);
    void disconnected();
    void subscriptionSucceeded(const QString &channel);
    void messageReceived(PusherMessage &msg);

public slots:
    void reconnect();

private slots:
    void textMessageReceived(const QString &string);
    void wsConnected();
    void wsDisconnected();

private:
    void sendOrQueue(const QString &msg);

    QWebSocket ws_;
    QUrl url_;

    QSet<QString> confirmedChannels_;
    QSet<QString> unconfirmedChannels_;
    QList<QString> messageBacklog_;

    QString socketID_;
    size_t activityTimeout_ = 120;
};

}  // namespace chatterino
