#pragma once

#include "common/NetworkCommon.hpp"
#include "providers/kick/api/Types.hpp"

#include <QNetworkAccessManager>
#include <QNetworkCookieJar>

namespace chatterino {

class NetworkRequest;

class KickClient
{
    template <typename... T>
    using SuccessFn = std::function<void(T...)>;
    template <typename... T>
    using ErrorFn = std::function<void(T...)>;

public:
    KickClient();

    void userByName(const QString &name, SuccessFn<kick::User> onSuccess,
                    ErrorFn<> onError);

private:
    NetworkRequest makeRequest(const QString &url, NetworkRequestType type);
    NetworkRequest makeGet(const QString &url);

    QNetworkAccessManager networkManager_;
    QNetworkCookieJar cookieJar_;
};

}  // namespace chatterino
