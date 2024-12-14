#pragma once

#include <QString>

#include <chrono>
#include <memory>

namespace chatterino {

class TracingControllerState;
/// See https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview
/// for the format
class TracingController
{
public:
    using Clock = std::chrono::high_resolution_clock;

    TracingController(bool enabled, QString outPath);
    ~TracingController();

    void durationBegin(std::string_view name);
    void durationEnd(std::string_view name);

    void asyncBegin(std::string_view name, void *id);
    void asyncEnd(std::string_view name, void *id);

    void completeEvent(std::string_view name, Clock::time_point start,
                       Clock::duration duration);

    void finish();

private:
    std::unique_ptr<TracingControllerState> state;
};

void initTracing(QString outPath);
TracingController *getTracer();

}  // namespace chatterino
