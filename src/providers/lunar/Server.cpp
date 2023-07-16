#include "providers/lunar/Server.hpp"

#include "common/QLogging.hpp"
#include "providers/lunar/Response.hpp"
#include "providers/lunar/Router.hpp"

#ifdef Q_OS_WIN
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#endif

#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <QJsonDocument>

#include <memory>
#include <variant>

namespace beast = boost::beast;    // from <boost/beast.hpp>
namespace http = beast::http;      // from <boost/beast/http.hpp>
namespace net = boost::asio;       // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;  // from <boost/asio/ip/tcp.hpp>

using namespace std::literals;

namespace {

using namespace chatterino;

template <typename Body = http::string_body>
http::response<Body> basicResponse(
    const auto &req, http::status status, auto setBody,
    beast::string_view contentType = "text/plain"sv)
{
    http::response<Body> res{status, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, contentType);
    res.keep_alive(req.keep_alive());
    setBody(res.body());
    res.prepare_payload();
    return res;
}

http::response<http::string_body> badRequest(const auto &req,
                                             beast::string_view why)
{
    return basicResponse(req, http::status::bad_request, [why](auto &body) {
        body = why;
    });
}

http::response<http::string_body> notFound(const auto &req,
                                           beast::string_view why)
{
    return basicResponse(req, http::status::not_found, [why](auto &body) {
        body = why;
    });
}

http::message_generator handleRoute(const http::request<http::string_body> &req,
                                    const lunar::Router::JsonRoute &route)
{
    static_assert(sizeof(lunar::Response::Status) == sizeof(http::status));

    if (req.method() != http::verb::post)
    {
        return badRequest(req, "This endpoint only allows POST requests");
    }

    // we have to make a copy here :/
    QByteArray body(req.body().data(),
                    static_cast<qsizetype>(req.body().size()));
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError)
    {
        return badRequest(req, "Invalid JSON");
    }
    if (!doc.isObject())
    {
        return badRequest(req, "Expected JSON root to be an object");
    }

    // run the actual handler
    auto routeRes = route(doc.object());

    http::response<http::string_body> res{
        static_cast<http::status>(routeRes.status()), req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    res.body() = QJsonDocument(routeRes.body())
                     .toJson(QJsonDocument::Compact)
                     .toStdString();
    res.prepare_payload();
    return res;
}

http::message_generator handleRoute(const http::request<http::string_body> &req,
                                    const lunar::Router::StaticFile &file)
{
    // Respond to HEAD request
    if (req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, file.contentType);
        res.content_length(static_cast<size_t>(file.data.size()));
        res.keep_alive(req.keep_alive());
        return res;
    }
    if (req.method() != http::verb::get)
    {
        return badRequest(req,
                          "This endpoint only allows GET and HEAD requests");
    }

    http::response<http::buffer_body> res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, file.contentType);
    res.keep_alive(req.keep_alive());
    // Safety: The router stores the buffer data for as long as the server runs
    res.body() = http::buffer_body::value_type{
        .data = (void *)file.data.data(),
        .size = static_cast<size_t>(file.data.size()),
        .more = false,
    };
    res.prepare_payload();
    return res;
}

template <class Body, class Allocator>
http::message_generator handleRequest(
    const lunar::Router &router,
    http::request<Body, http::basic_fields<Allocator>> &&req)
{
    if (req.method() != http::verb::get && req.method() != http::verb::head &&
        req.method() != http::verb::post)
    {
        return badRequest(req, "Unexpected HTTP-method");
    }

    auto it = router.routes().find(req.target());
    if (it == router.routes().end())
    {
        return notFound(req, "This route doesn't exist");
    }

    return std::visit(
        [&](const auto &route) {
            return handleRoute(req, route);
        },
        it->second);
}

//------------------------------------------------------------------------------
// Report a failure
void fail(beast::error_code ec, char const *what)
{
    qCWarning(chatterinoLunar) << what << ": " << ec.message();
}

// NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)

// Handles an HTTP server connection
class Session : public std::enable_shared_from_this<Session>
{
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    const lunar::Router &router_;
    http::request<http::string_body> req_;

public:
    // Take ownership of the stream
    Session(tcp::socket &&socket, const lunar::Router &router)
        : stream_(std::move(socket))
        , router_(router)
    {
    }

    void run()
    {
        net::dispatch(stream_.get_executor(),
                      beast::bind_front_handler(&Session::doRead,
                                                this->shared_from_this()));
    }

    void doRead()
    {
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};
        stream_.expires_after(std::chrono::seconds(30));

        http::async_read(stream_, buffer_, req_,
                         beast::bind_front_handler(&Session::onRead,
                                                   this->shared_from_this()));
    }

    void onRead(beast::error_code ec, std::size_t /*bytes_transferred*/)
    {
        // This means they closed the connection
        if (ec == http::error::end_of_stream)
        {
            return doClose();
        }

        if (ec)
        {
            return fail(ec, "read");
        }

        this->sendResponse(handleRequest(this->router_, std::move(req_)));
    }

    void sendResponse(http::message_generator &&msg)
    {
        bool keepAlive = msg.keep_alive();

        beast::async_write(
            stream_, std::move(msg),
            beast::bind_front_handler(&Session::onWrite,
                                      this->shared_from_this(), keepAlive));
    }

    void onWrite(bool keep_alive, beast::error_code ec,
                 std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
        {
            return fail(ec, "write");
        }

        if (!keep_alive)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return doClose();
        }

        doRead();
    }

    void doClose()
    {
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }

private:
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class Listener : public std::enable_shared_from_this<Listener>
{
    net::io_context &ioc_;
    tcp::acceptor acceptor_;
    const lunar::Router &router_;

public:
    Listener(net::io_context &ioc, const tcp::endpoint &endpoint,
             const lunar::Router &router)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , router_(router)
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void run()
    {
        this->doAccept();
    }

private:
    void doAccept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(&Listener::onAccept,
                                      this->shared_from_this()));
    }

    void onAccept(beast::error_code ec, tcp::socket socket)
    {
        if (ec)
        {
            fail(ec, "accept");
            return;  // To avoid infinite loop
        }

        // Create the session and run it
        std::make_shared<Session>(std::move(socket), router_)->run();

        // Accept another connection
        this->doAccept();
    }
};

// NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

}  // namespace

namespace chatterino::lunar {

class ServerPrivate
{
public:
    ~ServerPrivate()
    {
        qCDebug(chatterinoLunar) << "Stopping HTTP server...";
        this->ioc_.stop();
        this->work_.reset();
        if (this->asioThread_ != nullptr && this->asioThread_->joinable())
        {
            this->asioThread_->join();
        }
        qCDebug(chatterinoLunar) << "Stopped HTTP server";
    }

private:
    ServerPrivate(Server *owner, Router &&router)
        : q_ptr(owner)
        , router_(std::move(router))
        , work_(boost::asio::make_work_guard(this->ioc_))
    {
    }

    Server *q_ptr;
    Q_DECLARE_PUBLIC(Server)

    Router router_;

    boost::asio::io_context ioc_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        work_;
    std::unique_ptr<std::thread> asioThread_;
};

// ===== Server =====

Server::Server(Router &&router, QObject *parent)
    : QObject(parent)
    , u_ptr(new ServerPrivate(this, std::move(router)))
{
}

Server::~Server() = default;

void Server::listen(uint16_t port)
{
    Q_D(Server);

    // Create and launch a listening port
    std::make_shared<Listener>(
        d->ioc_, tcp::endpoint{net::ip::make_address("127.0.0.1"), port},
        d->router_)
        ->run();

    // Run the I/O service
    d->asioThread_ = std::make_unique<std::thread>([d]() {
        d->ioc_.run();
    });

    qCDebug(chatterinoLunar) << "Started HTTP server on port" << port;
}

}  // namespace chatterino::lunar
