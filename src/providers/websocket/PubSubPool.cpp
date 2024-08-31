#include "providers/websocket/PubSubPool.hpp"

#include "common/Literals.hpp"
#include "common/QLogging.hpp"
#include "util/Helpers.hpp"
#include "util/RapidjsonHelpers.hpp"

#include <rapidjson/rapidjson.h>

namespace {
using namespace std::chrono_literals;

constexpr std::chrono::steady_clock::duration PING_INTERVAL = 1min;
constexpr std::chrono::steady_clock::duration PING_TIMEOUT = 3 * PING_INTERVAL;
constexpr size_t TOPICS_PER_CONNECTION = 50;

}  // namespace

namespace chatterino::pubsub {

ConnectionData::ConnectionData()
    : lastPong(std::chrono::steady_clock::now())
{
}

}  // namespace chatterino::pubsub

namespace chatterino {

using namespace literals;

PubSubPool::PubSubPool(ws::Endpoint endpoint)
    : pingTimer_(this->executor())
    , endpoint(std::move(endpoint))
{
}

PubSubPool::~PubSubPool()
{
    this->shutdown();
}

void PubSubPool::run()
{
    ws::Pool<PubSubPool, pubsub::ConnectionData>::run();

    this->post([this] {
        this->doPing();
    });
}

void PubSubPool::subscribe(const QString &topic)
{
    this->subscribe(std::vector{topic});
}

void PubSubPool::subscribe(std::vector<QString> topics)
{
    {
        std::lock_guard guard(this->topicMutex);
        std::erase_if(topics, [&](const auto &topic) {
            return this->allTopics.contains(topic);
        });
    }

    if (topics.size() > TOPICS_PER_CONNECTION)
    {
        std::span remaining = topics;
        while (!remaining.empty())
        {
            std::span batch = remaining.subspan(
                0, std::min(TOPICS_PER_CONNECTION, remaining.size()));
            this->post([this, topics{std::vector(batch.begin(),
                                                 batch.end())}]() mutable {
                this->subscribeUnique(std::move(topics));
            });
        }
        return;
    }

    this->post([this, topics{std::move(topics)}]() mutable {
        this->subscribeUnique(std::move(topics));
    });
}

void PubSubPool::unsubscribe(const QString &topic)
{
    this->post([this, topic{topic}] {
        ConnectionHandle *conn{};
        {
            std::lock_guard guard(this->topicMutex);
            auto it = this->allTopics.find(topic);
            if (it == this->allTopics.end())
            {
                qCWarning(chatterinoPubSub)
                    << "Tried to unsubscribe from unknown topic" << topic;
                return;
            }

            conn = this->findConnection(it->second);
        }
        if (!conn)
        {
            qCWarning(chatterinoPubSub) << "Dangling topic detected:" << topic;
            return;
        }

        this->rawUnsubscribe(*conn, {topic});
    });
}

void PubSubPool::unsubscribePrefix(const QString &prefix)
{
    this->post([this, prefix{prefix}] {
        boost::unordered_flat_map<size_t, std::vector<QString>> reverseMap;

        {
            std::lock_guard guard(this->topicMutex);
            for (const auto &[topic, id] : this->allTopics)
            {
                if (topic.startsWith(prefix))
                {
                    auto it = reverseMap.find(id);
                    if (it == reverseMap.end())
                    {
                        reverseMap.emplace(id, std::vector{topic});
                    }
                    else
                    {
                        it->second.emplace_back(topic);
                    }
                }
            }
        }

        for (auto &&[id, topics] : reverseMap)
        {
            auto *conn = this->findConnection(id);
            if (!conn)
            {
                qCWarning(chatterinoPubSub) << "Dangling topic(s)" << topics;
                continue;
            }
            this->rawUnsubscribe(*conn, std::move(topics));
        }
    });
}

bool PubSubPool::isSubscribed(const QString &topic) const
{
    std::lock_guard guard(this->topicMutex);
    return this->allTopics.contains(topic);
}

void PubSubPool::setTokenProvider(TokenProvider fn)
{
    this->post([this, fn{std::move(fn)}]() mutable {
        this->tokenProvider = std::move(fn);
    });
}

void PubSubPool::setMessageHandler(MessageHandler fn)
{
    this->post([this, fn{std::move(fn)}]() mutable {
        this->messageHandler = std::move(fn);
    });
}

void PubSubPool::onConnectionReceive(ConnectionHandle &conn,
                                     QByteArrayView data)
{
    this->expectIoThread();

    rapidjson::Document doc;
    doc.Parse(data.data(), data.size());
    if (doc.HasParseError())
    {
        return;
    }

    if (!doc.IsObject())
    {
        return;
    }

    std::string_view ty;
    rj::getSafe(doc, "type", ty);

    if (ty == "PONG")
    {
        conn.data->lastPong = std::chrono::steady_clock::now();
        return;
    }

    if (ty == "RESPONSE")
    {
        QString nonce;
        rj::getSafe(doc, "nonce", nonce);
        std::string_view error;
        rj::getSafe(doc, "error", error);
        if (nonce.isEmpty())
        {
            return;
        }
        this->handleResponse(conn, nonce, error);
        return;
    }

    if (ty == "RECONNECT")
    {
        this->reconnectConnection(conn);
        return;
    }

    if (ty == "MESSAGE")
    {
        this->handleMessage(doc);
        return;
    }
}

void PubSubPool::onConnectionClose(ConnectionHandle &conn)
{
    this->expectIoThread();

    if (conn.data->alive)
    {
        this->reconnectConnection(conn);
    }
}

void PubSubPool::onConnectionOpen(ConnectionHandle &conn)
{
    this->expectIoThread();

    // subscribe to pending topics
    conn.data->connected = true;
    for (auto &req : conn.data->pendingSubscriptions)
    {
        this->rawSubscribe(conn, std::move(req.topics), req.auth);
    }
    conn.data->pendingSubscriptions = {};
}

void PubSubPool::handleResponse(ConnectionHandle &conn, const QString &nonce,
                                std::string_view error)
{
    this->expectIoThread();

    auto it = conn.data->unconfirmed.find(nonce);
    if (it == conn.data->unconfirmed.end())
    {
        qCWarning(chatterinoPubSub) << "Failed to find nonce" << nonce;
        return;
    }

    bool hasError = !error.empty();
    bool isSub = it->second.type == pubsub::UnconfirmedAction::Type::Subscribe;

    if (hasError)
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
        auto err = QString::fromUtf8(error.data(), error.size());
#else
        auto err = error;
#endif

        if (isSub)
        {
            qCWarning(chatterinoPubSub)
                << "Failed to listen to" << it->second.topics << "-" << err;
            conn.data->totalSubs -= it->second.topics.size();

            std::lock_guard guard(this->topicMutex);
            for (const auto &topic : it->second.topics)
            {
                this->allTopics.erase(topic);
            }
        }
        else
        {
            qCWarning(chatterinoPubSub)
                << "Failed to unlisten from" << it->second.topics << "-" << err;
        }
    }
    else if (isSub)
    {
        for (const auto &topic : it->second.topics)
        {
            conn.data->topics.emplace(topic);
        }
    }

    conn.data->unconfirmed.erase(it);
}

void PubSubPool::reconnectConnection(ConnectionHandle &conn)
{
    this->expectIoThread();

    if (this->isShuttingDown())
    {
        return;
    }

    // All topics this connection was subscribed to and all open subscriptions
    std::vector<QString> subs;

    std::ranges::move(conn.data->topics, std::back_inserter(subs));
    conn.data->topics = {};

    for (auto &it : conn.data->unconfirmed)
    {
        if (it.second.type == pubsub::UnconfirmedAction::Type::Subscribe)
        {
            std::ranges::move(it.second.topics, std::back_inserter(subs));
        }
    }
    conn.data->unconfirmed = {};

    for (auto &it : conn.data->pendingSubscriptions)
    {
        std::ranges::move(it.topics, std::back_inserter(subs));
    }
    conn.data->pendingSubscriptions = {};

    conn.data->totalSubs = 0;
    conn.data->alive = false;

    auto [id, next] =
        this->createConnection(this->endpoint, new pubsub::ConnectionData);

    {
        std::lock_guard guard(this->topicMutex);
        for (const auto &sub : subs)
        {
            this->allTopics[sub] = id;
        }
    }

    // re-authenticate the topics with the current token
    next.data->pendingSubscriptions.emplace_back(std::move(subs),
                                                 this->currentToken());
}

void PubSubPool::handleMessage(rapidjson::Document &doc)
{
    this->expectIoThread();

    if (!this->messageHandler)
    {
        qCWarning(chatterinoPubSub)
            << "No message handler set upon receiving message (ignoring)";
        return;
    }
    rapidjson::Value data;
    if (!rj::getSafeObject(doc, "data", data))
    {
        qCWarning(chatterinoPubSub) << "No 'data' of MESSAGE";
        return;
    }

    QString topic;
    bool ok = rj::getSafe(doc, "topic", topic);
    std::string_view message;
    ok = ok && rj::getSafe(doc, "message", message);

    if (!ok)
    {
        qCWarning(chatterinoPubSub) << "MESSAGE.data without topic/message";
        return;
    }

    // `buf` is only used for parsing, Qt's JSON types copy data
    auto buf = QByteArray::fromRawData(
        message.data(), static_cast<QByteArray::size_type>(message.size()));
    QJsonParseError err;
    auto inner = QJsonDocument::fromJson(buf, &err);

    if (err.error != QJsonParseError::NoError)
    {
        qCWarning(chatterinoPubSub) << "Failed to parse inner message for"
                                    << topic << '-' << err.errorString();
        return;
    }

    if (!inner.isObject())
    {
        qCWarning(chatterinoPubSub) << "Message isn't en object for" << topic;
        return;
    }

    this->messageHandler(topic, inner.object());
}

void PubSubPool::subscribeUnique(std::vector<QString> topics)
{
    this->expectIoThread();

    auto *existing = this->findConnection([&](const auto &hdl) {
        return hdl.data->alive &&
               hdl.data->totalSubs + topics.size() <= TOPICS_PER_CONNECTION;
    });
    if (existing)
    {
        {
            std::lock_guard guard(this->topicMutex);
            for (const auto &topic : topics)
            {
                this->allTopics.emplace(topic, existing->connection->id());
            }
        }

        if (existing->data->connected)
        {
            this->rawSubscribe(*existing, std::move(topics),
                               this->currentToken());
        }
        else
        {
            existing->data->pendingSubscriptions.emplace_back(
                std::move(topics), this->currentToken());
        }
        return;
    }

    const auto &&[id, conn] =
        this->createConnection(this->endpoint, new pubsub::ConnectionData{});
    conn.data->pendingSubscriptions.emplace_back(std::move(topics),
                                                 this->currentToken());
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void PubSubPool::rawSubscribe(ConnectionHandle &conn,
                              std::vector<QString> &&topics,
                              const QString &auth)
{
    this->expectIoThread();

    auto nonce = generateUuid();
    QJsonArray topicArr;
    for (const auto &topic : topics)
    {
        topicArr.append(topic);
    }

    QJsonObject msg{
        {u"type"_s, "LISTEN"_L1},
        {u"nonce"_s, nonce},
        {u"data"_s,
         {{
             {u"topics"_s, topicArr},
             {u"auth_token"_s, auth},
         }}},
    };
    conn.data->unconfirmed.emplace(
        nonce, pubsub::UnconfirmedAction{
                   .topics = std::move(topics),
                   .type = pubsub::UnconfirmedAction::Type::Subscribe,
               });

    conn.connection->write(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void PubSubPool::rawUnsubscribe(ConnectionHandle &conn,
                                std::vector<QString> &&topics)
{
    this->expectIoThread();

    auto nonce = generateUuid();
    QJsonArray topicArr;
    for (const auto &topic : topics)
    {
        topicArr.append(topic);
    }

    QJsonObject msg{
        {u"type"_s, "UNLISTEN"_L1},
        {u"nonce"_s, nonce},
        {u"data"_s,
         QJsonObject{
             {u"topics"_s, topicArr},
         }},
    };
    conn.data->unconfirmed.emplace(
        nonce, pubsub::UnconfirmedAction{
                   .topics = std::move(topics),
                   .type = pubsub::UnconfirmedAction::Type::Unsubscribe,
               });

    conn.connection->write(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void PubSubPool::doPing()
{
    this->expectIoThread();

    std::vector<size_t> expired;
    auto now = std::chrono::steady_clock::now();
    this->forEachConnection([&](size_t id, ConnectionHandle &conn) {
        if (now - conn.data->lastPong > PING_TIMEOUT)
        {
            expired.push_back(id);
        }
    });

    for (auto id : expired)
    {
        auto *conn = this->findConnection(id);
        if (conn)
        {
            this->reconnectConnection(*conn);
        }
    }

    this->pingTimer_.expires_after(PING_INTERVAL);
    this->pingTimer_.async_wait([this](const boost::system::error_code &ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }
        this->doPing();
    });
}

QString PubSubPool::currentToken() const
{
    this->expectIoThread();

    if (this->tokenProvider)
    {
        return this->tokenProvider();
    }
    return {};
}

}  // namespace chatterino
