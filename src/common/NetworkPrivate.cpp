#include "common/NetworkPrivate.hpp"

#include "common/NetworkManager.hpp"
#include "common/NetworkResult.hpp"
#include "common/Outcome.hpp"
#include "common/QLogging.hpp"
#include "debug/AssertInGuiThread.hpp"
#include "singletons/Paths.hpp"
#include "util/DebugCount.hpp"
#include "util/PostToThread.hpp"

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkReply>
#include <QtConcurrent>

#include <tuple>

namespace {

using namespace chatterino;

class NetworkRequester : public QObject
{
    Q_OBJECT

signals:
    void requestUrl();
};

class NetworkWorker : public QObject
{
    Q_OBJECT

public:
    NetworkWorker(std::shared_ptr<NetworkData> &&data)
        : data_(std::move(data))
    {
    }

public slots:
    void start();

private:
    std::shared_ptr<NetworkData> data_;
    QNetworkReply *reply_ = nullptr;

    QString httpVerb() const;

    QNetworkReply *sendRequest();

    void handleError();
    void handleSuccess();

    void emitOnError(NetworkResult &&result);
    void emitOnSuccess(NetworkResult &&result);
    void emitFinally();

    void logResponse() const;
    void writeToCache(const QByteArray &data) const;

    void tryRunConcurrent(auto &&fn)
    {
        if (this->data_->executeConcurrently_)
        {
            std::ignore = QtConcurrent::run(std::forward<decltype(fn)>(fn));
        }
        else
        {
            runInGuiThread(std::forward<decltype(fn)>(fn));
        }
    }

private slots:
    void finished();
    void timeout();
};

}  // namespace

namespace chatterino {

NetworkData::NetworkData()
    : lifetimeManager_(new QObject)
{
    DebugCount::increase("NetworkData");
}

NetworkData::~NetworkData()
{
    this->lifetimeManager_->deleteLater();

    DebugCount::decrease("NetworkData");
}

QString NetworkData::hash() const
{
    QByteArray bytes;

    bytes.append(this->request_.url().toString().toUtf8());

    for (const auto &header : this->request_.rawHeaderList())
    {
        bytes.append(header);
    }

    QByteArray hashBytes(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256));

    return hashBytes.toHex();
}

void loadUncached(std::shared_ptr<NetworkData> &&data)
{
    DebugCount::increase("http request started");

    NetworkRequester requester;
    auto *caller = data->caller_.get();
    auto *worker = new NetworkWorker(std::move(data));
    if (caller != nullptr)
    {
        worker->setParent(caller);
    }

    worker->moveToThread(&NetworkManager::workerThread);

    QObject::connect(&requester, &NetworkRequester::requestUrl, worker,
                     &NetworkWorker::start);

    emit requester.requestUrl();
}

// First tried to load cached, then uncached.
void loadCached(std::shared_ptr<NetworkData> &&data)
{
    QFile cachedFile(getPaths()->cacheDirectory() + "/" + data->hash());

    if (!cachedFile.exists() || !cachedFile.open(QIODevice::ReadOnly))
    {
        // File didn't exist OR File could not be opened
        loadUncached(std::move(data));
        return;
    }

    // XXX: check if bytes is empty?
    QByteArray bytes = cachedFile.readAll();
    NetworkResult result(NetworkResult::NetworkError::NoError, QVariant(200),
                         bytes);

    qCDebug(chatterinoHTTP)
        << QString("%1 [CACHED] 200 %2")
               .arg(networkRequestTypes.at(int(data->requestType_)),
                    data->request_.url().toString());
    if (data->onSuccess_)
    {
        if (data->executeConcurrently_ || isGuiThread())
        {
            // XXX: If outcome is Failure, we should invalidate the cache file
            // somehow/somewhere
            /*auto outcome =*/
            if (data->hasCaller_ && data->caller_.isNull())
            {
                return;
            }
            data->onSuccess_(result);
        }
        else
        {
            postToThread([data, result]() {
                if (data->hasCaller_ && data->caller_.isNull())
                {
                    return;
                }

                data->onSuccess_(result);
            });
        }
    }

    if (data->finally_)
    {
        if (data->executeConcurrently_ || isGuiThread())
        {
            if (data->hasCaller_ && data->caller_.isNull())
            {
                return;
            }

            data->finally_();
        }
        else
        {
            postToThread([data]() {
                if (data->hasCaller_ && data->caller_.isNull())
                {
                    return;
                }

                data->finally_();
            });
        }
    }
}

void load(std::shared_ptr<NetworkData> &&data)
{
    if (data->cache_)
    {
        std::ignore = QtConcurrent::run([data = std::move(data)]() mutable {
            loadCached(std::move(data));
        });
    }
    else
    {
        loadUncached(std::move(data));
    }
}

}  // namespace chatterino

namespace {

QString NetworkWorker::httpVerb() const
{
    return networkRequestTypes.at(static_cast<int>(this->data_->requestType_));
}

void NetworkWorker::start()
{
    const auto &data = this->data_;
    if (data->hasTimeout_)
    {
        auto *timer = new QTimer(this);
        timer->setSingleShot(true);
        timer->start(data->timeoutMS_);
        QObject::connect(timer, &QTimer::timeout, this,
                         &NetworkWorker::timeout);
    }

    this->reply_ = this->sendRequest();

    if (this->reply_ == nullptr)
    {
        qCDebug(chatterinoCommon) << "Unhandled request type";
        this->deleteLater();
        return;
    }
    this->reply_->setParent(this);

    QObject::connect(this->reply_, &QNetworkReply::finished, this,
                     &NetworkWorker::finished);
}

QNetworkReply *NetworkWorker::sendRequest()
{
    const auto &data = this->data_;
    auto &accessManager = NetworkManager::accessManager;

    switch (data->requestType_)
    {
        case NetworkRequestType::Get:
            return accessManager.get(data->request_);

        case NetworkRequestType::Put:
            return accessManager.put(data->request_, data->payload_);

        case NetworkRequestType::Delete:
            return accessManager.deleteResource(data->request_);

        case NetworkRequestType::Post:
            if (data->multiPartPayload_)
            {
                assert(data->payload_.isNull());

                return accessManager.post(data->request_,
                                          data->multiPartPayload_);
            }
            else
            {
                return accessManager.post(data->request_, data->payload_);
            }
        case NetworkRequestType::Patch:
            if (data->multiPartPayload_)
            {
                assert(data->payload_.isNull());

                return accessManager.sendCustomRequest(data->request_, "PATCH",
                                                       data->multiPartPayload_);
            }
            else
            {
                return accessManager.sendCustomRequest(data->request_, "PATCH",
                                                       data->payload_);
            }
    }
    return nullptr;
}

void NetworkWorker::handleError()
{
    auto &data = this->data_;

    if (reply_->error() == QNetworkReply::NetworkError::OperationCanceledError)
    {
        // Operation cancelled, most likely timed out
        qCDebug(chatterinoHTTP)
            << this->httpVerb() << "[cancelled]" << data->request_.url();
        return;
    }

    this->logResponse();

    auto status =
        this->reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    this->emitOnError(
        NetworkResult(this->reply_->error(), status, this->reply_->readAll()));
    this->emitFinally();
}

void NetworkWorker::handleSuccess()
{
    QByteArray bytes = this->reply_->readAll();
    if (this->data_->cache_)
    {
        this->writeToCache(bytes);
    }

    this->logResponse();

    auto status =
        this->reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute);

    DebugCount::increase("http request success");

    this->emitOnSuccess(NetworkResult(this->reply_->error(), status, bytes));
    this->emitFinally();
}

void NetworkWorker::finished()
{
    auto &data = this->data_;

    if (data->hasCaller_ && data->caller_.isNull())
    {
        this->deleteLater();
        return;
    }

    // TODO(pajlada): A reply was received, kill the timeout timer
    if (this->reply_->error() == QNetworkReply::NetworkError::NoError)
    {
        this->handleSuccess();
    }
    else
    {
        this->handleError();
    }

    this->deleteLater();
}

void NetworkWorker::timeout()
{
    auto &data = this->data_;

    this->reply_->abort();
    qCDebug(chatterinoHTTP)
        << QString("%1 [timed out] %2")
               .arg(networkRequestTypes.at(int(data->requestType_)),
                    data->request_.url().toString());

    this->emitOnError(
        NetworkResult(NetworkResult::NetworkError::TimeoutError, {}, {}));
    this->emitFinally();
    this->deleteLater();
}

void NetworkWorker::logResponse() const
{
    auto dbg =
        QMessageLogger(QT_MESSAGELOG_FILE, QT_MESSAGELOG_LINE,
                       QT_MESSAGELOG_FUNC, chatterinoHTTP().categoryName())
            .debug();
    dbg.noquote();
    dbg << this->httpVerb();

    auto status =
        this->reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (status.isValid())
    {
        dbg << status.toInt();
    }
    else
    {
        dbg << this->reply_->errorString();
    }

    dbg << this->data_->request_.url().toString();

    if (!this->data_->payload_.isEmpty())
    {
        if (this->data_->payload_.length() < 256)
        {
            dbg << QString::fromUtf8(this->data_->payload_);
        }
        else
        {
            dbg << "(paylod:" << this->data_->payload_.length() << "bytes)";
        }
    }
}

void NetworkWorker::writeToCache(const QByteArray &data) const
{
    auto path = getPaths()->cacheDirectory() + "/" + this->data_->hash();
    std::ignore = QtConcurrent::run([path, data] {
        QFile cachedFile(path);

        if (cachedFile.open(QIODevice::WriteOnly))
        {
            cachedFile.write(data);
        }
    });
}

void NetworkWorker::emitOnError(NetworkResult &&result)
{
    if (!this->data_->onError_)
    {
        return;
    }

    auto fn = [cb = std::move(this->data_->onError_),
               result = std::move(result)] {
        cb(result);
    };

    this->tryRunConcurrent(std::move(fn));
}

void NetworkWorker::emitOnSuccess(NetworkResult &&result)
{
    if (!this->data_->onSuccess_)
    {
        return;
    }

    auto fn = [cb = std::move(this->data_->onSuccess_),
               result = std::move(result)] {
        cb(result);
    };

    this->tryRunConcurrent(std::move(fn));
}

void NetworkWorker::emitFinally()
{
    if (!this->data_->finally_)
    {
        return;
    }

    auto fn = [cb = std::move(this->data_->finally_)] {
        cb();
    };

    this->tryRunConcurrent(std::move(fn));
}

}  // namespace

#include "NetworkPrivate.moc"
