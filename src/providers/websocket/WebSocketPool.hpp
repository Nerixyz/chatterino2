#pragma once

#include "common/UniqueAccess.hpp"
#include "util/QByteArrayBuffer.hpp"
#include "util/QStringHash.hpp"

#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <QByteArray>
#include <QString>
#include <QUrl>

#include <atomic>
#include <concepts>
#include <memory>
#include <string>

// In the current implementation, all components expect to run in a
// single-threaded io-context.
namespace chatterino::ws {

namespace beast = boost::beast;  // from <boost/beast.hpp>
namespace asio = boost::asio;    // from <boost/asio.hpp>

class PoolBase;

struct Endpoint {
    std::string host;
    std::string port;
    std::string path;
    bool useTls = true;

    static Endpoint fromUrl(const QUrl &url);
};

class Connection : public std::enable_shared_from_this<Connection>
{
public:
    Connection(PoolBase &parent, size_t id, Endpoint endpoint);
    virtual ~Connection();

    static std::shared_ptr<Connection> forEndpoint(PoolBase &parent, size_t id,
                                                   Endpoint endpoint,
                                                   asio::io_context &ioc,
                                                   asio::ssl::context &ssl);

    virtual void start() = 0;

    /// Write @a data on this connection
    virtual void write(QByteArray data) = 0;

    /// Stops this connection by sending a close message
    virtual void shutdown() = 0;

    virtual asio::strand<asio::io_context::executor_type>
        executor() noexcept = 0;

    size_t id() const
    {
        return this->id_;
    }

    void post(std::invocable auto cb)
    {
        asio::post(this->executor(),
                   [lifetime{this->shared_from_this()}, cb]() {
                       cb();
                   });
    }

protected:
    Endpoint endpoint_;
    size_t id_;
    PoolBase &parent_;

    // This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    std::string targetHost_;

    beast::flat_buffer buffer_;
    std::deque<QByteArrayBuffer> writeQueue_;

    /// Close this client (calls \ref closeSocket)
    void close(beast::error_code ec, QStringView op);

    /// The protocol this connection uses ("ws://" or "wss://")
    virtual QStringView protocol() const noexcept = 0;

    /// Start closing the underlying socket
    virtual void closeSocket() = 0;

    template <typename Derived>
    void onWSHandshake(beast::error_code ec);

    template <typename Derived>
    void onRead(beast::error_code ec, size_t bytesTransferred);

    template <typename Derived>
    void onWrite(beast::error_code ec, size_t bytesTransferred);

    void onClose(beast::error_code ec) const;

    friend QDebug operator<<(QDebug dbg, const Connection &conn);
};

template <typename Derived>
// NOLINTNEXTLINE(bugprone-crtp-constructor-accessibility)
class ConnectionHelper : public Connection
{
public:
    void write(QByteArray data) override;

protected:
    using Connection::Connection;

    beast::flat_buffer buffer_;
    std::deque<QByteArrayBuffer> writeQueue_;

    void onWSHandshake(beast::error_code ec);
    void onRead(beast::error_code ec, size_t bytesTransferred);
    void onWrite(beast::error_code ec, size_t bytesTransferred);

    std::shared_ptr<Derived> sharedSelf()
    {
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

private:
    Derived *derived() noexcept
    {
        return static_cast<Derived *>(this);
    }
};

class WsConnection : public ConnectionHelper<WsConnection>
{
public:
    WsConnection(PoolBase &parent, size_t id, Endpoint endpoint,
                 asio::io_context &ioc);
    ~WsConnection() override;

    void start() override;
    void shutdown() override;

    asio::strand<asio::io_context::executor_type> executor() noexcept override
    {
        return this->ws_.get_executor();
    }

    size_t id() const
    {
        return this->id_;
    }

protected:
    QStringView protocol() const noexcept override
    {
        return u"ws://";
    }

    void closeSocket() override;

private:
    void onResolve(beast::error_code ec,
                   const asio::ip::tcp::resolver::results_type &results);
    void onTCPHandshake(
        beast::error_code ec,
        const asio::ip::tcp::resolver::results_type::endpoint_type &ep);

    // Ws<Tcp>
    beast::websocket::stream<beast::basic_stream<
        asio::ip::tcp, asio::strand<asio::io_context::executor_type>>>
        ws_;

    asio::ip::tcp::resolver resolver_;

    friend ConnectionHelper<WsConnection>;
};

/// A websocket connection over TLS (wss://)
class WssConnection : public ConnectionHelper<WssConnection>
{
public:
    WssConnection(PoolBase &parent, size_t id, Endpoint endpoint,
                  asio::io_context &ioc, asio::ssl::context &ssl);
    ~WssConnection() override;

    void start() override;
    void shutdown() override;

    asio::strand<asio::io_context::executor_type> executor() noexcept override
    {
        return this->ws_.get_executor();
    }

protected:
    QStringView protocol() const noexcept override
    {
        return u"wss://";
    }

    void closeSocket() override;

private:
    void onResolve(beast::error_code ec,
                   const asio::ip::tcp::resolver::results_type &results);
    void onTCPHandshake(
        beast::error_code ec,
        const asio::ip::tcp::resolver::results_type::endpoint_type &ep);
    void onTLSHandshake(beast::error_code ec);

    // Ws<Tls<Tcp>>
    beast::websocket::stream<asio::ssl::stream<beast::basic_stream<
        asio::ip::tcp, asio::strand<asio::io_context::executor_type>>>>
        ws_;

    asio::ip::tcp::resolver resolver_;

    friend ConnectionHelper<WssConnection>;
};

class PoolBase : public std::enable_shared_from_this<PoolBase>
{
public:
    PoolBase();
    virtual ~PoolBase() = default;

    void run();
    virtual void shutdown();

    auto executor() const
    {
        return this->strand.executor_;
    }

    asio::io_context &ioContext()
    {
        return this->ioc;
    }

    asio::ssl::context &sslContext()
    {
        return this->ssl;
    }

    bool isShuttingDown() const
    {
        return this->shuttingDown;
    }

#ifdef NDEBUG
    void expectIoThread() const {};
#else
    void expectIoThread() const;
#endif

protected:
    // events from Connection
    virtual void onCloseRaw(size_t id) = 0;
    virtual void onOpenRaw(size_t id) = 0;
    virtual void onReceiveRaw(size_t id, QByteArrayView data) = 0;
    virtual size_t openConnections() const = 0;
    void onFullClose();

private:
    asio::io_context ioc;
    asio::strand<asio::io_context::executor_type> strand;
    asio::ssl::context ssl;

    std::thread ioThread;
    std::atomic<bool> shuttingDown = false;

    std::mutex threadDoneMutex;
    std::condition_variable threadDoneCv;
    bool threadDone = false;

    UniqueAccess<std::function<void()>> fullCloseHandler;

    friend Connection;
    friend ConnectionHelper<WsConnection>;
    friend ConnectionHelper<WssConnection>;
};

template <typename Derived, typename ConnectionData>
class Pool : public PoolBase
{
public:
    struct ConnectionHandle {
        ConnectionHandle(std::shared_ptr<Connection> connection,
                         std::unique_ptr<ConnectionData> data)
            : connection(std::move(connection))
            , data(std::move(data))
        {
        }
        ~ConnectionHandle() = default;
        ConnectionHandle(const ConnectionHandle &) = delete;
        ConnectionHandle(ConnectionHandle &&) = default;
        ConnectionHandle &operator=(const ConnectionHandle &) = delete;
        ConnectionHandle &operator=(ConnectionHandle &&) = default;

        std::shared_ptr<Connection> connection;
        std::unique_ptr<ConnectionData> data;
    };

    Pool() = default;

    ConnectionHandle *findConnection(
        std::predicate<const ConnectionHandle &> auto cb)
    {
        for (auto &[_, conn] : this->connections_)
        {
            if (cb(conn))
            {
                return &conn;
            }
        }
        return nullptr;
    }

    ConnectionHandle *findConnection(size_t id)
    {
        auto it = this->connections_.find(id);
        if (it == this->connections_.end())
        {
            return nullptr;
        }
        return &it->second;
    }

    template <typename T>
    std::pair<size_t, ConnectionHandle &> createConnection(Endpoint endpoint,
                                                           T factory);

    template <>
    std::pair<size_t, ConnectionHandle &> createConnection(Endpoint endpoint,
                                                           ConnectionData *ptr)
    {
        size_t id = this->nextID_++;

        ConnectionHandle hdl{
            Connection::forEndpoint(*this, id, std::move(endpoint),
                                    this->ioContext(), this->sslContext()),
            std::unique_ptr<ConnectionData>{ptr},
        };
        return this->createConnection(id, std::move(hdl));
    }

    template <std::invocable<size_t> T>
    std::pair<size_t, ConnectionHandle &> createConnection(Endpoint endpoint,
                                                           T factory)
    {
        size_t id = this->nextID_++;
        std::unique_ptr<typename Derived::ConnectionData> data;
        if constexpr (std::is_same_v<decltype(factory(0)), ConnectionData *>)
        {
            data = factory(id);
        }
        else
        {
            data = std::make_unique<ConnectionData>(factory(id));
        }

        ConnectionHandle hdl{
            Connection::forEndpoint(*this, id, std::move(endpoint),
                                    this->ioContext(), this->sslContext()),
            std::move(data),
        };
        return this->createConnection(id, std::move(hdl));
    }

    /// @brief Invokes @a cb for each connection
    ///
    /// @a cb must not add or remove any connection.
    void forEachConnection(std::invocable<size_t, ConnectionHandle &> auto cb)
    {
        for (auto &[id, conn] : this->connections_)
        {
            cb(id, conn);
        }
    }

    void post(std::invocable auto cb)
    {
        asio::post(this->executor(),
                   [lifetime{this->shared_from_this()}, cb]() mutable {
                       cb();
                   });
    }

    void shutdown() override
    {
        for (const auto &[_id, conn] : this->connections_)
        {
            conn.connection->shutdown();
        }
        PoolBase::shutdown();
    }

protected:
    void onCloseRaw(size_t id) final
    {
        auto *conn = this->findConnection(id);
        if (!conn)
        {
            return;
        }
        static_cast<Derived *>(this)->onConnectionClose(*conn);
        this->connections_.erase(id);
        this->nConnections_.fetch_sub(1);
    }

    void onOpenRaw(size_t id) final
    {
        auto *conn = this->findConnection(id);
        if (!conn)
        {
            return;
        }
        static_cast<Derived *>(this)->onConnectionOpen(*conn);
    }

    void onReceiveRaw(size_t id, QByteArrayView data) final
    {
        auto *conn = this->findConnection(id);
        if (!conn)
        {
            return;
        }
        static_cast<Derived *>(this)->onConnectionReceive(*conn, data);
    }

    size_t openConnections() const final
    {
        return this->nConnections_.load();
    }

private:
    std::pair<size_t, ConnectionHandle &> createConnection(
        size_t id, ConnectionHandle &&hdl)
    {
        auto [it, inserted] = this->connections_.emplace(id, std::move(hdl));
        this->nConnections_.fetch_add(1);

        return {id, it->second};
    }

    boost::unordered_flat_map<size_t, ConnectionHandle> connections_;
    size_t nextID_ = 1;
    std::atomic<size_t> nConnections_ = 0;
};

}  // namespace chatterino::ws
