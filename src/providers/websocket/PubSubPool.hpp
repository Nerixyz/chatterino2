#pragma once

#include "providers/websocket/WebSocketPool.hpp"

#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_flat_set.hpp>
#include <rapidjson/rapidjson.h>

#include <chrono>
#include <vector>

namespace chatterino::pubsub {

struct UnconfirmedAction {
    enum class Type : uint8_t {
        Subscribe,
        Unsubscribe,
    };

    std::vector<QString> topics;
    Type type;
};

struct PendingSubscription {
    std::vector<QString> topics;
    QString auth;
};

struct ConnectionData {
    ConnectionData();

    /// Confirmed topics
    boost::unordered_flat_set<QString> topics;

    /// @brief Subscriptions that have not yet been confirmed by Twitch
    ///
    /// nonce -> action
    boost::unordered_flat_map<QString, UnconfirmedAction> unconfirmed;

    /// Subscriptions not yet sent as the connection isn't connected yet
    std::vector<PendingSubscription> pendingSubscriptions;

    size_t totalSubs = 0;
    bool connected = false;
    bool alive = true;

    std::chrono::steady_clock::time_point lastPong;
};

}  // namespace chatterino::pubsub

namespace chatterino {

class PubSubPool : public ws::Pool<PubSubPool, pubsub::ConnectionData>
{
public:
    using MessageHandler =
        std::function<void(const QString &, const QJsonObject &)>;
    using TokenProvider = std::function<QString()>;

    PubSubPool(ws::Endpoint endpoint);
    ~PubSubPool() override;

    void run();

    void subscribe(const QString &topic);
    void subscribe(std::vector<QString> topics);
    void unsubscribe(const QString &topic);
    void unsubscribePrefix(const QString &prefix);

    bool isSubscribed(const QString &topic) const;

    void setTokenProvider(TokenProvider fn);
    void setMessageHandler(MessageHandler fn);

protected:
    void onConnectionReceive(ConnectionHandle &conn, QByteArrayView data);
    void onConnectionClose(ConnectionHandle &conn);
    void onConnectionOpen(ConnectionHandle &conn);

private:
    void handleResponse(ConnectionHandle &conn, const QString &nonce,
                        std::string_view error);
    void reconnectConnection(ConnectionHandle &conn);
    void handleMessage(rapidjson::Document &doc);

    void subscribeUnique(std::vector<QString> topics);

    void rawSubscribe(ConnectionHandle &conn, std::vector<QString> &&topics,
                      const QString &auth);

    void rawUnsubscribe(ConnectionHandle &conn, std::vector<QString> &&topics);

    void doPing();

    QString currentToken() const;

    // topic -> connection-id
    boost::unordered_flat_map<QString, size_t> allTopics;
    std::mutex topicMutex;

    TokenProvider tokenProvider;
    MessageHandler messageHandler;
    asio::steady_timer pingTimer_;
    ws::Endpoint endpoint;

    friend ws::Pool<PubSubPool, pubsub::ConnectionData>;
};

}  // namespace chatterino
