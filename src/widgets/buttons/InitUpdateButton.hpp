#pragma once

#include <memory>

namespace pajlada {
namespace Signals {
    class SignalHolder;
}
}  // namespace pajlada

namespace chatterino {

class PixmapButton;
class UpdateDialog;

void initUpdateButton(PixmapButton &button,
                      pajlada::Signals::SignalHolder &signalHolder);

}  // namespace chatterino
