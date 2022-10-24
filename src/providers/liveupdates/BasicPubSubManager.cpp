#include "providers/liveupdates/BasicPubSubManager.hpp"

#include "common/QLogging.hpp"
#include "providers/seventv/eventapi/SeventvEventApiSubscription.hpp"
#include "providers/twitch/PubSubHelpers.hpp"
#include "util/DebugCount.hpp"
#include "util/Helpers.hpp"

#include <algorithm>
#include <exception>
#include <thread>

namespace chatterino {

template <class Subscription>
BasicPubSubManager<Subscription>::BasicPubSubManager(const QString &host)
    : host_(host)
{
    this->websocketClient_.set_access_channels(websocketpp::log::alevel::all);
    this->websocketClient_.clear_access_channels(
        websocketpp::log::alevel::frame_payload |
        websocketpp::log::alevel::frame_header);

    this->websocketClient_.init_asio();

    // SSL Handshake
    this->websocketClient_.set_tls_init_handler([this](auto hdl) {
        return this->onTLSInit(hdl);
    });

    this->websocketClient_.set_message_handler([this](auto hdl, auto msg) {
        this->onMessage(hdl, msg);
    });
    this->websocketClient_.set_open_handler([this](auto hdl) {
        this->onConnectionOpen(hdl);
    });
    this->websocketClient_.set_close_handler([this](auto hdl) {
        this->onConnectionClose(hdl);
    });
    this->websocketClient_.set_fail_handler([this](auto hdl) {
        this->onConnectionFail(hdl);
    });
}

template <class Subscription>
void BasicPubSubManager<Subscription>::start()
{
    this->work_ = std::make_shared<boost::asio::io_service::work>(
        this->websocketClient_.get_io_service());
    this->mainThread_.reset(
        new std::thread(std::bind(&BasicPubSubManager::runThread, this)));
}

template <class Subscription>
void BasicPubSubManager<Subscription>::stop()
{
    this->stopping_ = true;

    for (const auto &client : this->clients_)
    {
        client.second->close("Shutting down");
    }

    this->work_.reset();

    if (this->mainThread_->joinable())
    {
        this->mainThread_->join();
    }

    assert(this->clients_.empty());
}

template <class Subscription>
typename BasicPubSubManager<Subscription>::WebsocketContextPtr
    BasicPubSubManager<Subscription>::onTLSInit(websocketpp::connection_hdl hdl)
{
    WebsocketContextPtr ctx(
        new boost::asio::ssl::context(boost::asio::ssl::context::tlsv12));

    try
    {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::single_dh_use);
    }
    catch (const std::exception &e)
    {
        qCDebug(chatterinoLiveupdates)
            << "Exception caught in OnTLSInit:" << e.what();
    }

    return ctx;
}

template <class Subscription>
void BasicPubSubManager<Subscription>::onConnectionOpen(
    websocketpp::connection_hdl hdl)
{
    DebugCount::increase("LiveUpdates connections");
    this->addingClient_ = false;

    this->connectBackoff_.reset();

    auto client = this->createClient(this->websocketClient_, hdl);

    // We separate the starting from the constructor because we will want to use
    // shared_from_this
    client->start();

    this->clients_.emplace(hdl, client);

    auto pendingSubsToTake =
        (std::min)(this->pendingSubscriptions_.size(),
                   BasicPubSubClient<Subscription>::MAX_SUBSCRIPTIONS);

    qCDebug(chatterinoLiveupdates)
        << "LiveUpdate connection opened, subscribing to" << pendingSubsToTake
        << "subscriptions!";

    while (pendingSubsToTake > 0 && !this->pendingSubscriptions_.empty())
    {
        const auto last = std::move(this->pendingSubscriptions_.back());
        this->pendingSubscriptions_.pop_back();
        if (!client->subscribe(last))
        {
            qCDebug(chatterinoLiveupdates)
                << "Failed to subscribe to" << last << "on new client.";
            // TODO: should we try to add a new client here?
            return;
        }
        DebugCount::decrease("LiveUpdates subscription backlog");
        pendingSubsToTake--;
    }

    if (!this->pendingSubscriptions_.empty())
    {
        this->addClient();
    }
}

template <class Subscription>
void BasicPubSubManager<Subscription>::onConnectionFail(
    websocketpp::connection_hdl hdl)
{
    DebugCount::increase("LiveUpdates failed connections");
    if (auto conn = this->websocketClient_.get_con_from_hdl(std::move(hdl)))
    {
        qCDebug(chatterinoLiveupdates)
            << "LiveUpdates connection attempt failed (error: "
            << conn->get_ec().message().c_str() << ")";
    }
    else
    {
        qCDebug(chatterinoLiveupdates)
            << "LiveUpdates connection attempt failed but we can't get the "
               "connection from a handle.";
    }
    this->addingClient_ = false;
    if (!this->pendingSubscriptions_.empty())
    {
        runAfter(this->websocketClient_.get_io_service(),
                 this->connectBackoff_.next(), [this](auto timer) {
                     this->addClient();
                 });
    }
}

template <class Subscription>
void BasicPubSubManager<Subscription>::onConnectionClose(
    websocketpp::connection_hdl hdl)
{
    qCDebug(chatterinoLiveupdates) << "Connection closed";

    DebugCount::decrease("LiveUpdates connections");
    auto clientIt = this->clients_.find(hdl);

    // If this assert goes off, there's something wrong with the connection
    // creation/preserving code KKona
    assert(clientIt != this->clients_.end());

    auto client = clientIt->second;

    this->clients_.erase(clientIt);

    client->stop();

    if (!this->stopping_)
    {
        auto subs = client->getSubscriptions();
        for (const auto &sub : subs)
        {
            this->subscribe(sub);
        }
    }
}

template <class Subscription>
void BasicPubSubManager<Subscription>::runThread()
{
    qCDebug(chatterinoLiveupdates) << "Start LiveUpdates manager thread";
    this->websocketClient_.run();
    qCDebug(chatterinoLiveupdates) << "Done with LiveUpdates manager thread";
}

template <class Subscription>
void BasicPubSubManager<Subscription>::addClient()
{
    if (this->addingClient_)
    {
        return;
    }

    qCDebug(chatterinoLiveupdates) << "Adding an additional client";

    this->addingClient_ = true;

    websocketpp::lib::error_code ec;
    auto con =
        this->websocketClient_.get_connection(this->host_.toStdString(), ec);

    if (ec)
    {
        qCDebug(chatterinoLiveupdates)
            << "Unable to establish connection:" << ec.message().c_str();
        return;
    }

    this->websocketClient_.connect(con);
}

template <class Subscription>
void BasicPubSubManager<Subscription>::subscribe(
    const Subscription &subscription)
{
    if (this->trySubscribe(subscription))
    {
        return;
    }

    this->addClient();
    this->pendingSubscriptions_.emplace_back(subscription);
    DebugCount::increase("LiveUpdates subscription backlog");
}

template <class Subscription>
bool BasicPubSubManager<Subscription>::trySubscribe(
    const Subscription &subscription)
{
    for (auto &client : this->clients_)
    {
        if (client.second->subscribe(subscription))
        {
            return true;
        }
    }
    return false;
}

template <class Subscription>
void BasicPubSubManager<Subscription>::unsubscribe(
    const Subscription &subscription)
{
    for (auto &client : this->clients_)
    {
        if (client.second->unsubscribe(subscription))
        {
            return;
        }
    }
}
template <class Subscription>
std::shared_ptr<BasicPubSubClient<Subscription>>
    BasicPubSubManager<Subscription>::createClient(
        liveupdates::WebsocketClient &client, websocketpp::connection_hdl hdl)
{
    return std::make_shared<BasicPubSubClient<Subscription>>(client, hdl);
}

template <class Subscription>
std::shared_ptr<BasicPubSubClient<Subscription>>
    BasicPubSubManager<Subscription>::findClient(
        websocketpp::connection_hdl hdl)
{
    auto clientIt = this->clients_.find(hdl);

    if (clientIt == this->clients_.end())
    {
        return {};
    }
    else
    {
        return clientIt->second;
    }
}

template class BasicPubSubManager<SeventvEventApiSubscription>;

}  // namespace chatterino
