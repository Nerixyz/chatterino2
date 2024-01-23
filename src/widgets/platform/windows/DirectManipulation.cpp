#include "widgets/platform/windows/DirectManipulation.hpp"

#include "common/QLogging.hpp"

#include <boost/signals2.hpp>

#include <array>
#include <cassert>

namespace {

const auto LOG = chatterinoDirectManipulation;

}  // namespace

namespace chatterino::windows {

using Event = DirectManipulation::Event;
using EventType = DirectManipulation::Event::Type;

class DirectManipulation::EventHandler
    : public winrt::implements<DirectManipulation::EventHandler,
                               IDirectManipulationViewportEventHandler,
                               IDirectManipulationInteractionEventHandler>
{
public:
    void setViewportSize(QSize size)
    {
        this->viewportSize = size;
    }

    // from IDirectManipulationViewportEventHandler
    HRESULT OnViewportStatusChanged(
        IDirectManipulationViewport *viewport,
        DIRECTMANIPULATION_STATUS current,
        DIRECTMANIPULATION_STATUS previous) override;

    HRESULT OnViewportUpdated(IDirectManipulationViewport *viewport) override;

    HRESULT OnContentUpdated(IDirectManipulationViewport *viewport,
                             IDirectManipulationContent *content) override;

    // from IDirectManipulationInteractionEventHandler
    HRESULT OnInteraction(
        IDirectManipulationViewport2 *viewport,
        DIRECTMANIPULATION_INTERACTION_TYPE interaction) override;

    boost::signals2::signal<void(bool)> interactingChanged;
    boost::signals2::signal<void(Event)> manipulationEvent;

private:
    enum class State {
        None,
        Scrolling,
        Moving,
        Pinching,
    };

    void nextState(State next);

    State state = State::None;
    QSize viewportSize;
    float viewportOffsetX = 0.0F;
    float viewportOffsetY = 0.0F;
    float scale = 1.0F;
    bool pendingScrollStart = false;
};

HRESULT DirectManipulation::EventHandler::OnViewportStatusChanged(
    IDirectManipulationViewport *viewport, DIRECTMANIPULATION_STATUS current,
    DIRECTMANIPULATION_STATUS previous)
{
    if (!viewport)
    {
        qCWarning(LOG) << "OnViewportStatusChanged: no viewport";
        return S_OK;
    }

    if (current == DIRECTMANIPULATION_INERTIA)
    {
        if (previous != DIRECTMANIPULATION_RUNNING ||
            this->state != State::Scrolling)
        {
            return S_OK;
        }

        this->nextState(State::Moving);
        return S_OK;
    }

    if (current == DIRECTMANIPULATION_RUNNING &&
        previous == DIRECTMANIPULATION_INERTIA)
    {
        this->nextState(State::None);
        return S_OK;
    }

    if (current != DIRECTMANIPULATION_READY)
    {
        return S_OK;
    }

    if (this->scale != 1.0F || this->viewportOffsetX != 0.0F ||
        this->viewportOffsetY != 0.0F)
    {
        auto hr = viewport->ZoomToRect(
            0, 0, static_cast<float>(this->viewportSize.width()),
            static_cast<float>(this->viewportSize.height()), FALSE);
        if (FAILED(hr))
        {
            return hr;
        }

        this->scale = 1.0F;
        this->viewportOffsetX = 0.0F;
        this->viewportOffsetY = 0.0F;
    }

    this->nextState(State::None);

    return S_OK;
}

HRESULT DirectManipulation::EventHandler::OnViewportUpdated(
    IDirectManipulationViewport * /*viewport*/)
{
    return S_OK;
}

HRESULT DirectManipulation::EventHandler::OnContentUpdated(
    IDirectManipulationViewport *viewport, IDirectManipulationContent *content)
{
    if (!viewport || !content)
    {
        qCWarning(LOG) << "OnContentUpdated: viewport or content was null";
        return S_OK;
    }

    std::array<float, 6> matrix{};
    auto hr = content->GetContentTransform(matrix.data(),
                                           static_cast<DWORD>(matrix.size()));
    if (FAILED(hr))
    {
        return hr;
    }
    float scale = matrix[0];
    float xOffset = matrix[4];
    float yOffset = matrix[5];

    if (scale == 0.0F)
    {
        return S_OK;
    }

    if (qFuzzyCompare(this->scale, scale) && this->viewportOffsetX == xOffset &&
        this->viewportOffsetY == yOffset)
    {
        return S_OK;
    }

    if (qFuzzyCompare(scale, 1.0F))
    {
        if (this->state == State::None)
        {
            this->nextState(State::Scrolling);
        }
    }
    else
    {
        this->nextState(State::Pinching);
    }

    xOffset = std::floor(xOffset);
    yOffset = std::floor(yOffset);
    QPoint delta{
        static_cast<int>(std::abs(this->viewportOffsetX - xOffset)),
        static_cast<int>(std::abs(this->viewportOffsetY - yOffset)),
    };
    this->viewportOffsetX = xOffset;
    this->viewportOffsetY = yOffset;

    this->scale = scale;

    bool scrolling = this->state == State::Scrolling;
    bool moving = this->state == State::Moving;
    if (!scrolling || !moving)
    {
        return S_OK;
    }

    if (delta.isNull())
    {
        return S_OK;
    }

    if (scrolling)
    {
        auto ty = EventType::Scroll;
        if (this->pendingScrollStart)
        {
            ty = EventType::ScrollBegin;
            this->pendingScrollStart = false;
        }

        this->manipulationEvent({
            .delta = delta,
            .type = ty,
        });

        return S_OK;
    }

    // we're moving
    this->manipulationEvent({
        .delta = delta,
        .type = EventType::Move,
    });

    return S_OK;
}

void DirectManipulation::EventHandler::nextState(State next)
{
    if (this->state == next)
    {
        return;
    }

    auto prev = this->state;
    this->state = next;

    switch (prev)
    {
        case State::Scrolling: {
            if (next != State::Moving)
            {
                this->manipulationEvent({.type = EventType::ScrollEnd});
            }
        }
        break;
        case State::Moving: {
            this->manipulationEvent({.type = EventType::ScrollEnd});
        }
        break;
        case State::Pinching:  // ignored
            [[fallthrough]];
        case State::None:
            break;
    }

    switch (next)
    {
        case State::Scrolling: {
            // TODO
            this->pendingScrollStart = true;
        }
        break;
        case State::Moving: {
            assert(prev == State::Scrolling);
            this->manipulationEvent({.type = EventType::MoveBegin});
        }
        break;
        case State::Pinching:
            [[fallthrough]];
        case State::None:
            break;
    }
}

HRESULT DirectManipulation::EventHandler::OnInteraction(
    IDirectManipulationViewport2 * /*viewport*/,
    DIRECTMANIPULATION_INTERACTION_TYPE interaction)
{
    if (interaction == DIRECTMANIPULATION_INTERACTION_BEGIN)
    {
        this->interactingChanged(true);
    }
    else if (interaction == DIRECTMANIPULATION_INTERACTION_END)
    {
        this->interactingChanged(false);
    }

    return S_OK;
}

DirectManipulation::DirectManipulation(QWidget *parent)
    : QObject(parent)
    , window(reinterpret_cast<HWND>(parent->winId()))
{
    auto init = [&] {
        this->manager = winrt::create_instance<IDirectManipulationManager>(
            CLSID_DirectManipulationManager);
        if (!this->manager)
        {
            return false;
        }

        auto hr = this->manager->GetUpdateManager(
            IID_PPV_ARGS(this->updateManager.put()));
        if (FAILED(hr))
        {
            return false;
        }
        assert(this->updateManager);

        hr = this->manager->CreateViewport(nullptr, this->window,
                                           IID_PPV_ARGS(this->viewport.put()));
        if (FAILED(hr))
        {
            return false;
        }
        assert(this->manager);

        // everything except scaling
        auto config = DIRECTMANIPULATION_CONFIGURATION_INTERACTION |
                      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_X |
                      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_Y |
                      DIRECTMANIPULATION_CONFIGURATION_TRANSLATION_INERTIA |
                      DIRECTMANIPULATION_CONFIGURATION_RAILS_X |
                      DIRECTMANIPULATION_CONFIGURATION_RAILS_Y;

        hr = this->viewport->AddConfiguration(config);
        if (FAILED(hr))
        {
            return false;
        }

        hr = this->viewport->SetViewportOptions(
            DIRECTMANIPULATION_VIEWPORT_OPTIONS_MANUALUPDATE);
        if (FAILED(hr))
        {
            return false;
        }

        this->eventHandler =
            winrt::make_self<DirectManipulation::EventHandler>();
        hr = this->viewport->AddEventHandler(this->window,
                                             this->eventHandler.get(),
                                             &this->viewportEventHandlerCookie);
        if (FAILED(hr))
        {
            return false;
        }

        this->manipulationEventConn =
            this->eventHandler->manipulationEvent.connect(
                [this](const auto &event) {
                    this->manipulationEvent(event);
                });
        this->interactingConn = this->eventHandler->interactingChanged.connect(
            [this](bool interacting) {
                if (!this->updateManager)
                {
                    return;
                }

                if (interacting)
                {
                    this->updateTimer.start();
                }
                else
                {
                    this->updateTimer.stop();
                }
            });
        this->updateTimer.setTimerType(Qt::PreciseTimer);
        this->updateTimer.setInterval(1000 / 60);
        QObject::connect(&this->updateTimer, &QTimer::timeout, this, [this] {
            if (this->updateManager)
            {
                this->updateManager->Update(nullptr);
            }
        });

        RECT initialRect{0, 0, 1024, 1024};
        hr = this->viewport->SetViewportRect(&initialRect);
        if (FAILED(hr))
        {
            return false;
        }

        hr = this->manager->Activate(this->window);
        if (FAILED(hr))
        {
            return false;
        }

        hr = this->viewport->Enable();
        if (FAILED(hr))
        {
            return false;
        }

        hr = this->updateManager->Update(nullptr);
        return SUCCEEDED(hr);
    };

    if (!init())
    {
        this->shutdown();
    }
}

DirectManipulation::~DirectManipulation()
{
    this->shutdown();
}

void DirectManipulation::updateWindowSize(QSize size)
{
    if (!this->eventHandler || !this->viewport)
    {
        return;
    }
    this->eventHandler->setViewportSize(size);
    RECT viewportRect{0, 0, size.width(), size.height()};
    auto hr = this->viewport->SetViewportRect(&viewportRect);
    if (FAILED(hr))
    {
        qCWarning(LOG) << "Failed to set viewport rect" << size << "hr:" << hr;
    }
}

void DirectManipulation::onPointerHitTest(WPARAM wParam)
{
    auto pointerId = static_cast<UINT32>(GET_POINTERID_WPARAM(wParam));
    POINTER_INPUT_TYPE ty{};
    if (GetPointerType(pointerId, &ty) != TRUE)
    {
        qCWarning(LOG) << "Failed to get pointer type - wParam:" << wParam;
        return;
    }
    if (ty == PT_TOUCHPAD && this->viewport)
    {
        this->viewport->SetContact(pointerId);
    }
}

void DirectManipulation::shutdown()
{
    this->interactingConn.disconnect();
    this->manipulationEventConn.disconnect();

    if (this->eventHandler)
    {
        this->eventHandler = {};
    }

    if (this->viewport)
    {
        this->viewport->Stop();

        if (this->viewportEventHandlerCookie != 0)
        {
            this->viewport->RemoveEventHandler(
                this->viewportEventHandlerCookie);
            this->viewportEventHandlerCookie = 0;
        }

        this->viewport->Abandon();
        this->viewport = {};
    }

    if (this->updateManager)
    {
        this->updateManager = {};
    }

    if (this->manager)
    {
        this->manager->Deactivate(this->window);
        this->manager = {};
    }
}

}  // namespace chatterino::windows
