#pragma once

#include <QObject>
#include <QScopedPointer>

namespace chatterino::lunar {

class Response;
class Request;
class Router;
class ServerPrivate;
class Server : QObject
{
    Q_OBJECT
public:
    Server(Router &&router, QObject *parent = nullptr);
    Server(const Server &) = delete;
    Server(Server &&) = delete;
    ~Server() override;

    void listen(uint16_t port);

private:
    // Could use d_ptr from QObject and extend from QObjectPrivate
    QScopedPointer<ServerPrivate> u_ptr;
    Q_DECLARE_PRIVATE_D(u_ptr, Server)
};

}  // namespace chatterino::lunar
