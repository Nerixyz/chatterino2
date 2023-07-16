#pragma once

#include <QByteArray>
#include <QJsonObject>

#include <functional>
#include <string>
#include <unordered_map>
#include <variant>

namespace chatterino::lunar {

class Response;

/// This is a really simple router.
/// It supports two types of routes:
/// 1) Static files (GET/HEAD)
/// 2) Dynamic JSON handlers (POST only)
class Router
{
public:
    struct StaticFile {
        std::string contentType;
        QByteArray data;
    };
    /// This function can be invoked from any thread.
    using JsonRoute = std::function<Response(QJsonObject)>;

    using Routes =
        std::unordered_map<std::string, std::variant<JsonRoute, StaticFile>>;

    Router(Routes &&init)
        : routes_(std::move(init)){};

    const Routes &routes() const
    {
        return this->routes_;
    }

private:
    Routes routes_;
};

}  // namespace chatterino::lunar
