#include "providers/websocket/WebSocketPool.hpp"

#include "common/QLogging.hpp"
#include "common/Version.hpp"

#include <chrono>

namespace chatterino::ws {

Endpoint Endpoint::fromUrl(const QUrl &url)
{
    auto path = url.path(QUrl::FullyEncoded).toStdString();
    if (path.empty())
    {
        path = "";
    }

    return {
        .host = url.host(QUrl::FullyEncoded).toStdString(),
        .port = std::to_string(url.port(443)),
        .path = path,
        // prefer TLS even if the scheme isn't specified
        .useTls = url.scheme() != u"ws",
    };
}

// MARK: Connection

Connection::Connection(PoolBase &parent, size_t id, Endpoint endpoint)
    : endpoint_(std::move(endpoint))
    , id_(id)
    , parent_(parent)
{
}

Connection::~Connection()
{
    this->parent_.onFullClose();
}

std::shared_ptr<Connection> Connection::forEndpoint(PoolBase &parent, size_t id,
                                                    Endpoint endpoint,
                                                    asio::io_context &ioc,
                                                    asio::ssl::context &ssl)
{
    if (endpoint.useTls)
    {
        return std::make_shared<WssConnection>(parent, id, std::move(endpoint),
                                               ioc, ssl);
    }
    return std::make_shared<WsConnection>(parent, id, std::move(endpoint), ioc);
}

void Connection::close(beast::error_code ec, QStringView op)
{
    if (ec)
    {
        qCDebug(chatterinoWebsocket).noquote().nospace()
            << *this << ' ' << op << " failed with \"" << ec.message() << '"';
    }
    else
    {
        qCDebug(chatterinoWebsocket).noquote().nospace()
            << *this << " Regular shutdown (" << op << ")";
    }

    this->parent_.onCloseRaw(this->id_);

    this->closeSocket();
}

void Connection::onClose(beast::error_code ec) const
{
    if (ec)
    {
        qCDebug(chatterinoWebsocket)
            << *this << "Closing failed with" << ec.message();
    }
    else
    {
        qCDebug(chatterinoWebsocket) << *this << "Connection closed";
    }
}

QDebug operator<<(QDebug dbg, const Connection &conn)
{
    QDebugStateSaver s(dbg);
    dbg.noquote().nospace();

    // operator<< with std::string was added in 6.5
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    dbg << '[' << conn.protocol() << conn.endpoint_.host << ":"
        << conn.endpoint_.port << conn.endpoint_.path << " id=" << conn.id_
        << ']';
#else
    dbg << "[id=" << conn.id_ << ']';
#endif

    return dbg;
}

// MARK: ConnectionHelper

template <typename Derived>
void ConnectionHelper<Derived>::write(QByteArray data)
{
    this->post([this, data{std::move(data)}] {
        this->writeQueue_.emplace_back(data);
        if (this->writeQueue_.size() > 1 || !derived()->ws_.is_open())
        {
            // Either there's an ongoing write or the socket is not yet open
            qCDebug(chatterinoWebsocket) << *this << "Queueing write";
            return;
        }

        qCDebug(chatterinoWebsocket) << *this << "Starting write";
        derived()->ws_.async_write(
            this->writeQueue_.front(),
            beast::bind_front_handler(&Derived::onWrite, this->sharedSelf()));
    });
}

template <typename Derived>
void ConnectionHelper<Derived>::onWSHandshake(beast::error_code ec)
{
    qCDebug(chatterinoWebsocket) << *this << "onWSHandshake";
    if (ec)
    {
        this->close(ec, u"WS handshake");
        return;
    }

    this->parent_.onOpenRaw(this->id_);
    derived()->ws_.text(true);
    qCDebug(chatterinoWebsocket) << *this << "Starting initial read";
    derived()->ws_.async_read(
        this->buffer_,
        beast::bind_front_handler(&Derived::onRead, this->sharedSelf()));

    if (!this->writeQueue_.empty())
    {
        qCDebug(chatterinoWebsocket) << *this << "Starting initial write";
        derived()->ws_.async_write(
            this->writeQueue_.front(),
            beast::bind_front_handler(&Derived::onWrite, this->sharedSelf()));
    }
}

template <typename Derived>
void ConnectionHelper<Derived>::onRead(beast::error_code ec,
                                       size_t bytesTransferred)
{
    if (ec)
    {
        if (ec == asio::error::operation_aborted)
        {
            return;
        }

        this->close(ec, u"Read");
        return;
    }

    QByteArrayView data{
        static_cast<const char *>(this->buffer_.cdata().data()),
        static_cast<QByteArrayView::size_type>(bytesTransferred),
    };
    this->parent_.onReceiveRaw(this->id_, data);
    this->buffer_.consume(bytesTransferred);

    derived()->ws_.async_read(
        this->buffer_,
        beast::bind_front_handler(&Derived::onRead, this->sharedSelf()));
}

template <typename Derived>
void ConnectionHelper<Derived>::onWrite(beast::error_code ec,
                                        size_t bytesTransferred)
{
    if (ec)
    {
        if (ec == asio::error::operation_aborted)
        {
            return;
        }

        this->close(ec, u"Write");
        return;
    }

    if (this->writeQueue_.empty())
    {
        qCWarning(chatterinoWebsocket) << *this << "Write queue empty";
        assert(false);
        return;
    }

    if (static_cast<size_t>(this->writeQueue_.front().data().size()) !=
        bytesTransferred)
    {
        qCWarning(chatterinoWebsocket)
            << *this << bytesTransferred << "B transferred but expected "
            << this->writeQueue_.front().data().size() << 'B';
    }
    this->writeQueue_.pop_front();

    if (this->writeQueue_.empty())
    {
        return;
    }

    derived()->ws_.async_write(
        this->writeQueue_.front(),
        beast::bind_front_handler(&Derived::onWrite, this->sharedSelf()));
}

// MARK: WsConnection

WsConnection::WsConnection(PoolBase &parent, size_t id, Endpoint endpoint,
                           asio::io_context &ioc)
    : ConnectionHelper(parent, id, std::move(endpoint))
    , ws_(asio::make_strand(ioc))
    , resolver_(this->ws_.get_executor())
{
    qCDebug(chatterinoWebsocket) << *this << "created";
}

WsConnection::~WsConnection()
{
    qCDebug(chatterinoWebsocket) << *this << "destroyed";
}

void WsConnection::start()
{
    this->post([this] {
        this->resolver_.async_resolve(
            this->endpoint_.host, this->endpoint_.port,
            beast::bind_front_handler(&WsConnection::onResolve,
                                      this->sharedSelf()));
    });
}

void WsConnection::shutdown()
{
    this->post([this] {
        this->close({}, u"shutdown() called");
    });
}

void WsConnection::onResolve(
    beast::error_code ec, const asio::ip::tcp::resolver::results_type &results)
{
    qCDebug(chatterinoWebsocket) << *this << "onResolve";
    if (ec)
    {
        this->close(ec, u"Resolving host");
        return;
    }

    beast::get_lowest_layer(this->ws_).expires_after(
        std::chrono::milliseconds(30));

    beast::get_lowest_layer(this->ws_).async_connect(
        results, beast::bind_front_handler(&WsConnection::onTCPHandshake,
                                           this->sharedSelf()));
}

void WsConnection::onTCPHandshake(
    beast::error_code ec,
    const asio::ip::tcp::resolver::results_type::endpoint_type &ep)
{
    qCDebug(chatterinoWebsocket) << *this << "onTCPHandshake";
    if (ec)
    {
        this->close(ec, u"TCP handshake");
        return;
    }

    this->targetHost_ = this->endpoint_.host + ':' + std::to_string(ep.port());

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(this->ws_).expires_never();

    // Set suggested timeout settings for the websocket
    this->ws_.set_option(beast::websocket::stream_base::timeout::suggested(
        beast::role_type::client));

    // Change the User-Agent of the handshake
    this->ws_.set_option(beast::websocket::stream_base::decorator(
        [](beast::websocket::request_type &req) {
            req.set(beast::http::field::user_agent,
                    (u"Chatterino/" % Version::instance().version())
                        .toUtf8()
                        .toStdString());
        }));

    this->ws_.async_handshake(
        this->targetHost_, this->endpoint_.path,
        beast::bind_front_handler(&WsConnection::onWSHandshake,
                                  this->sharedSelf()));
}

void WsConnection::closeSocket()
{
    if (!this->ws_.is_open())
    {
        return;
    }

    this->ws_.async_close(
        beast::websocket::close_code::normal,
        beast::bind_front_handler(&WsConnection::onClose, this->sharedSelf()));
}

// MARK: WssConnection

WssConnection::WssConnection(PoolBase &parent, size_t id, Endpoint endpoint,
                             asio::io_context &ioc, asio::ssl::context &ssl)
    : ConnectionHelper(parent, id, std::move(endpoint))
    , ws_(asio::make_strand(ioc), ssl)
    , resolver_(this->ws_.get_executor())
{
    qCDebug(chatterinoWebsocket) << *this << "created";
}

WssConnection::~WssConnection()
{
    qCDebug(chatterinoWebsocket) << *this << "destroyed";
}

void WssConnection::start()
{
    this->post([this] {
        this->resolver_.async_resolve(
            this->endpoint_.host, this->endpoint_.port,
            beast::bind_front_handler(&WssConnection::onResolve,
                                      this->sharedSelf()));
    });
}

void WssConnection::shutdown()
{
    this->post([this] {
        this->close({}, u"shutdown() called");
    });
}

void WssConnection::onResolve(
    beast::error_code ec, const asio::ip::tcp::resolver::results_type &results)
{
    qCDebug(chatterinoWebsocket) << *this << "onResolve";
    if (ec)
    {
        this->close(ec, u"Resolving host");
        return;
    }

    beast::get_lowest_layer(this->ws_).expires_after(
        std::chrono::milliseconds(30));

    beast::get_lowest_layer(this->ws_).async_connect(
        results, beast::bind_front_handler(&WssConnection::onTCPHandshake,
                                           this->sharedSelf()));
}

void WssConnection::onTCPHandshake(
    beast::error_code ec,
    const asio::ip::tcp::resolver::results_type::endpoint_type &ep)
{
    qCDebug(chatterinoWebsocket) << *this << "onTCPHandshake";
    if (ec)
    {
        this->close(ec, u"TCP handshake");
        return;
    }

    // Set SNI Hostname
    if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(),
                                  this->endpoint_.host.c_str()))
    {
        ec = beast::error_code(static_cast<int>(::ERR_get_error()),
                               asio::error::get_ssl_category());
        this->close(ec, u"Setting SNI hostname");
        return;
    }

    this->targetHost_ = this->endpoint_.host + ':' + std::to_string(ep.port());

    this->ws_.next_layer().async_handshake(
        asio::ssl::stream_base::client,
        beast::bind_front_handler(&WssConnection::onTLSHandshake,
                                  this->sharedSelf()));
}

void WssConnection::onTLSHandshake(beast::error_code ec)
{
    qCDebug(chatterinoWebsocket) << *this << "onTLSHandshake";
    if (ec)
    {
        this->close(ec, u"TLS handshake");
        return;
    }

    // Turn off the timeout on the tcp_stream, because
    // the websocket stream has its own timeout system.
    beast::get_lowest_layer(this->ws_).expires_never();

    // Set suggested timeout settings for the websocket
    this->ws_.set_option(beast::websocket::stream_base::timeout::suggested(
        beast::role_type::client));

    // Change the User-Agent of the handshake
    this->ws_.set_option(beast::websocket::stream_base::decorator(
        [](beast::websocket::request_type &req) {
            req.set(beast::http::field::user_agent,
                    (u"Chatterino/" % Version::instance().version())
                        .toUtf8()
                        .toStdString());
        }));

    this->ws_.async_handshake(
        this->targetHost_, this->endpoint_.path,
        beast::bind_front_handler(&WssConnection::onWSHandshake,
                                  this->sharedSelf()));
}

void WssConnection::closeSocket()
{
    if (!this->ws_.is_open())
    {
        return;
    }

    this->ws_.async_close(
        beast::websocket::close_code::normal,
        beast::bind_front_handler(&WssConnection::onClose, this->sharedSelf()));
}

// MARK: PoolBase

PoolBase::PoolBase()
    : ioc(1)
    , strand(this->ioc.get_executor())
    , ssl(boost::asio::ssl::context::tlsv13_client)
{
}

void PoolBase::run()
{
    if (this->ioThread.get_id() != std::thread::id())
    {
        qCWarning(chatterinoWebsocket) << "Attempted to start pool twice";
        assert(false && "pool is already running");
        return;
    }

    this->ioThread = std::thread{[this] {
        try
        {
            this->ioContext().run();
        }
        catch (std::exception &ex)
        {
            qCWarning(chatterinoWebsocket)
                << "Websocket thread crashed" << ex.what();
            assert(false);
        }

        // notify the shutdown procedure that we're ended
        {
            std::lock_guard guard(this->threadDoneMutex);
            this->threadDone = true;
        }
        this->threadDoneCv.notify_one();
    }};
}

void PoolBase::shutdown()
{
    if (std::this_thread::get_id() == this->ioThread.get_id())
    {
        qCCritical(chatterinoWebsocket)
            << "ws::PoolBase must never be destroyed on the ASIO thread.";
        return;
    }

    this->shuttingDown = true;

    // wait for all connections to be destroyed
    size_t pendingConnections = this->openConnections();
    std::condition_variable connectionsCv;
    std::mutex mutex;
    *this->fullCloseHandler.access() = [&] {
        {
            std::lock_guard guard(mutex);
            if (pendingConnections == 0)
            {
                return;
            }
            pendingConnections--;
            if (pendingConnections != 0)
            {
                return;
            }
        }
        connectionsCv.notify_all();
    };

    {
        std::unique_lock lock(mutex);
        connectionsCv.wait_for(lock, std::chrono::milliseconds{2000}, [&] {
            return pendingConnections == 0;
        });
    }

    *this->fullCloseHandler.access() = {};

    this->ioContext().stop();

    if (!this->ioThread.joinable())
    {
        return;
    }

    // wait for the io-context to stop
    {
        std::unique_lock lock(this->threadDoneMutex);
        bool isDone = this->threadDoneCv.wait_for(
            lock, std::chrono::milliseconds{400}, [&] {
                return this->threadDone;
            });

        if (isDone)
        {
            this->ioThread.join();
        }
        else
        {
            // we can't do anything about the thread anymore - it's probably deadlocked
            this->ioThread.detach();
        }
    }
}

#ifndef NDEBUG
void PoolBase::expectIoThread() const
{
    assert(std::this_thread::get_id() == this->ioThread.get_id() &&
           "expected to be on ASIO thread");
}
#endif

void PoolBase::onFullClose()
{
    auto cb = this->fullCloseHandler.access();
    if (*cb)
    {
        (*cb)();
    }
}

}  // namespace chatterino::ws
