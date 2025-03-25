#pragma once

#include <sol/forward.hpp>

class QObject;

namespace chatterino::lua::api::ui {

void registerTypes(sol::table &c2, QObject *guard);

}  // namespace chatterino::lua::api::ui
