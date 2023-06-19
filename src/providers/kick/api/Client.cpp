#include "providers/kick/api/Client.hpp"

#include "common/NetworkCommon.hpp"
#include "common/NetworkManager.hpp"
#include "common/NetworkRequest.hpp"
#include "common/Outcome.hpp"

namespace chatterino {

KickClient::KickClient()
{
    this->networkManager_.moveToThread(&NetworkManager::workerThread);
    this->networkManager_.setCookieJar(&this->cookieJar_);
}

void KickClient::userByName(const QString &user,
                            SuccessFn<kick::User> onSuccess, ErrorFn<> onError)
{
    this->makeGet(QStringLiteral("channels/") + user)
        .onSuccess([](const auto &data) -> Outcome {
            return Success;
        });
}

NetworkRequest KickClient::makeRequest(const QString &url,
                                       NetworkRequestType type)
{
    Q_ASSERT(!url.startsWith("/"));

    auto baseUrl = QStringLiteral("https://kick.com/api/v1/");

    QUrl fullUrl(baseUrl + url);

    return NetworkRequest(fullUrl, type)
        .timeout(5 * 1000)
        .accessManager(&this->networkManager_);
}

NetworkRequest KickClient::makeGet(const QString &url)
{
    return this->makeRequest(url, NetworkRequestType::Get);
}
}  // namespace chatterino