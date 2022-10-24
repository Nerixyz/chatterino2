#pragma once

#include "providers/liveupdates/BasicPubSubClient.hpp"
#include "providers/liveupdates/BasicPubSubWebsocket.hpp"
#include "util/ExponentialBackoff.hpp"

#include <QJsonObject>
#include <QString>
#include <pajlada/signals/signal.hpp>
#include <websocketpp/client.hpp>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace chatterino {

template <class Subscription>
class BasicPubSubManager
{
public:
    BasicPubSubManager(const QString &host);
    virtual ~BasicPubSubManager() = default;

    BasicPubSubManager(const BasicPubSubManager &) = delete;
    BasicPubSubManager(const BasicPubSubManager &&) = delete;
    BasicPubSubManager &operator=(const BasicPubSubManager &) = delete;
    BasicPubSubManager &operator=(const BasicPubSubManager &&) = delete;

    enum class State {
        Connected,
        Disconnected,
    };

    void start();
    void stop();

    bool isConnected() const
    {
        return this->state_ == State::Connected;
    }

protected:
    using WebsocketMessagePtr =
        websocketpp::config::asio_tls_client::message_type::ptr;
    using WebsocketContextPtr =
        websocketpp::lib::shared_ptr<boost::asio::ssl::context>;

    virtual std::shared_ptr<BasicPubSubClient<Subscription>> createClient(
        liveupdates::WebsocketClient &client, websocketpp::connection_hdl hdl);
    virtual void onMessage(websocketpp::connection_hdl hdl,
                           WebsocketMessagePtr msg) = 0;

    std::shared_ptr<BasicPubSubClient<Subscription>> findClient(
        websocketpp::connection_hdl hdl);
    void unsubscribe(const Subscription &subscription);
    void subscribe(const Subscription &subscription);

private:
    void onConnectionOpen(websocketpp::connection_hdl hdl);
    void onConnectionFail(websocketpp::connection_hdl hdl);
    void onConnectionClose(websocketpp::connection_hdl hdl);
    WebsocketContextPtr onTLSInit(websocketpp::connection_hdl hdl);

    void runThread();
    void addClient();

    bool trySubscribe(const Subscription &subscription);

    State state_ = State::Connected;

    std::map<liveupdates::WebsocketHandle,
             std::shared_ptr<BasicPubSubClient<Subscription>>,
             std::owner_less<liveupdates::WebsocketHandle>>
        clients_;

    std::vector<Subscription> pendingSubscriptions_;
    std::atomic<bool> addingClient_{false};
    ExponentialBackoff<5> connectBackoff_{std::chrono::milliseconds(1000)};

    std::shared_ptr<boost::asio::io_service::work> work_{nullptr};

    liveupdates::WebsocketClient websocketClient_;
    std::unique_ptr<std::thread> mainThread_;

    const QString host_;

    bool stopping_{false};
};

}  // namespace chatterino
