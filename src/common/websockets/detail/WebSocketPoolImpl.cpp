#include "common/websockets/detail/WebSocketPoolImpl.hpp"

#include "Application.hpp"
#include "common/websockets/detail/WebSocketConnection.hpp"
#include "util/RenameThread.hpp"

#include <boost/certify/https_verification.hpp>

namespace chatterino::ws::detail {

WebSocketPoolImpl::WebSocketPoolImpl()
    : ioc(1)
    , ssl(boost::asio::ssl::context::tls_client)
    , work(this->ioc.get_executor())
{
#ifdef CHATTERINO_WITH_TESTS
    if (!getApp()->isTest())
#endif
    {
        this->ssl.set_verify_mode(
            boost::asio::ssl::verify_peer |
            boost::asio::ssl::verify_fail_if_no_peer_cert);
        this->ssl.set_default_verify_paths();

        boost::certify::enable_native_https_server_verification(this->ssl);
    }

    this->ioThread = std::make_unique<std::thread>([this] {
        this->ioc.run();
    });
    renameThread(*this->ioThread, "WebSocketPool");
}

WebSocketPoolImpl::~WebSocketPoolImpl()
{
    this->closing = true;
    this->work.reset();
    {
        std::lock_guard g(this->connectionMutex);
        for (const auto &conn : this->connections)
        {
            conn->close();
        }
    }
    if (!this->ioThread)
    {
        return;
    }
    if (this->ioThread->joinable())
    {
        this->ioThread->join();
    }
}

void WebSocketPoolImpl::removeConnection(WebSocketConnection *conn)
{
    std::lock_guard g(this->connectionMutex);
    std::erase_if(this->connections, [conn](const auto &v) {
        return v.get() == conn;
    });
}

}  // namespace chatterino::ws::detail
