#include "controllers/tracing/TracingController.hpp"

#include "common/QLogging.hpp"
#include "debug/AssertInGuiThread.hpp"

#include <QThread>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <mutex>

namespace {

using namespace chatterino;
class QFileStream
{
public:
    using Ch = char;

    QFileStream(QFile &os)
        : file(os)
    {
    }
    ~QFileStream() = default;
    QFileStream(const QFileStream &) = delete;
    QFileStream &operator=(const QFileStream &) = delete;
    QFileStream(QFileStream &&) = delete;
    QFileStream &operator=(QFileStream &&) = delete;

    // NOLINTBEGIN(readability-identifier-naming,readability-convert-member-functions-to-static)
    Ch Peek() const
    {
        assert(false);
        return '\0';
    }
    Ch Take()
    {
        assert(false);
        return '\0';
    }
    size_t Tell() const
    {
        return 0;
    }

    Ch *PutBegin()
    {
        assert(false);
        return nullptr;
    }
    void Put(Ch c)
    {
        file.putChar(c);
    }
    void Flush()
    {
        file.flush();
    }
    size_t PutEnd(Ch * /*begin*/)
    {
        assert(false);
        return 0;
    }
    // NOLINTEND(readability-identifier-naming,readability-convert-member-functions-to-static)

private:
    QFile &file;
};

template <typename T>
constexpr bool IS_TIME_POINT = false;

template <typename Clock, typename Duration>
constexpr bool IS_TIME_POINT<std::chrono::time_point<Clock, Duration>> = true;

template <typename T>
constexpr bool IS_DURATION = false;

template <typename Rep, typename Period>
constexpr bool IS_DURATION<std::chrono::duration<Rep, Period>> = true;

auto toMicros(auto tp)
    requires IS_TIME_POINT<decltype(tp)>
{
    return std::chrono::time_point_cast<std::chrono::microseconds>(tp);
}

auto toMicros(auto dur)
    requires IS_DURATION<decltype(dur)>
{
    return std::chrono::duration_cast<std::chrono::microseconds>(dur);
}

rapidjson::Value baseEvent(std::string_view type, auto &alloc)
{
    rapidjson::Value value(rapidjson::kObjectType);

    value.AddMember("ph", rapidjson::StringRef(type.data(), type.size()),
                    alloc);

    return value;
}

rapidjson::Value namedEvent(std::string_view name, std::string_view type,
                            auto &alloc)
{
    auto value = baseEvent(type, alloc);

    rapidjson::Value nameValue;  // make sure this is copied
    nameValue.SetString(name.data(), name.size(), alloc);

    value.AddMember("name", nameValue, alloc);

    return value;
}

rapidjson::Value threadEvent(std::string_view name, std::string_view type,
                             auto &alloc)
{
    auto value = namedEvent(name, type, alloc);
    value.AddMember("tid", std::bit_cast<size_t>(QThread::currentThreadId()),
                    alloc);
    return value;
}

}  // namespace

namespace chatterino {

class TracingControllerState
{
public:
    TracingControllerState(QString outPath)
        : outPath(std::move(outPath))
        , doc(rapidjson::kObjectType)
        , root(rapidjson::kArrayType) {};

    QString outPath;
    rapidjson::Document doc;
    rapidjson::Value root;
    std::mutex mutex;
};

TracingController::TracingController(bool enabled, QString outPath)
    : state(enabled
                ? std::make_unique<TracingControllerState>(std::move(outPath))
                : nullptr)
{
}

TracingController::~TracingController() = default;

void TracingController::durationBegin(std::string_view name)
{
    if (!this->state)
    {
        return;
    }
    std::lock_guard guard(this->state->mutex);

    auto &alloc = this->state->doc.GetAllocator();
    auto event = threadEvent(name, "B", alloc);
    event.AddMember("ts", toMicros(Clock::now()).time_since_epoch().count(),
                    alloc);

    this->state->root.PushBack(event, alloc);
}

void TracingController::durationEnd(std::string_view name)
{
    if (!this->state)
    {
        return;
    }
    std::lock_guard guard(this->state->mutex);

    auto &alloc = this->state->doc.GetAllocator();
    auto event = threadEvent(name, "E", alloc);
    event.AddMember("ts", toMicros(Clock::now()).time_since_epoch().count(),
                    alloc);

    this->state->root.PushBack(event, alloc);
}

void TracingController::asyncBegin(std::string_view name, void *id)
{
    if (!this->state)
    {
        return;
    }
    std::lock_guard guard(this->state->mutex);

    auto &alloc = this->state->doc.GetAllocator();
    auto event = namedEvent(name, "b", alloc);
    event.AddMember("ts", toMicros(Clock::now()).time_since_epoch().count(),
                    alloc);
    event.AddMember("id", std::bit_cast<size_t>(id), alloc);

    this->state->root.PushBack(event, alloc);
}

void TracingController::asyncEnd(std::string_view name, void *id)
{
    if (!this->state)
    {
        return;
    }
    std::lock_guard guard(this->state->mutex);

    auto &alloc = this->state->doc.GetAllocator();
    auto event = namedEvent(name, "e", alloc);
    event.AddMember("ts", toMicros(Clock::now()).time_since_epoch().count(),
                    alloc);
    event.AddMember("id", std::bit_cast<size_t>(id), alloc);

    this->state->root.PushBack(event, alloc);
}

void TracingController::completeEvent(std::string_view name,
                                      Clock::time_point start,
                                      Clock::duration duration)
{
    if (!this->state)
    {
        return;
    }
    std::lock_guard guard(this->state->mutex);

    auto &alloc = this->state->doc.GetAllocator();
    auto event = threadEvent(name, "X", alloc);
    event.AddMember("ts", toMicros(start).time_since_epoch().count(), alloc);
    event.AddMember("dur", toMicros(duration).count(), alloc);

    this->state->root.PushBack(event, alloc);
}

void TracingController::finish()
{
    assertInGuiThread();
    if (!this->state)
    {
        return;
    }

    {
        QFile outFile(this->state->outPath);
        if (!outFile.open(QFile::WriteOnly))
        {
            qCWarning(chatterinoApp)
                << "Failed to open" << this->state->outPath;
            return;
        }

        this->state->doc.AddMember("traceEvents", this->state->root,
                                   this->state->doc.GetAllocator());

        QFileStream fs(outFile);
        rapidjson::Writer writer(fs);
        this->state->doc.Accept(writer);
    }
    this->state = nullptr;
}

}  // namespace chatterino
