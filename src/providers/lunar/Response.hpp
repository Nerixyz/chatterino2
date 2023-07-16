#pragma once

#include <QJsonObject>

#include <cstdint>

namespace chatterino::lunar {

/// Responses are always JSON.
class Response
{
public:
    enum class Status : unsigned {
        Ok = 200,
        BadRequest = 400,
        Forbidden = 403,
        InternalServerError = 500,
    };

    Response(Status status, QJsonObject body)
        : status_(status)
        , body_(std::move(body)){};

    Status status() const
    {
        return this->status_;
    }

    const QJsonObject &body() const
    {
        return this->body_;
    }

private:
    Status status_;
    QJsonObject body_;
};

}  // namespace chatterino::lunar
