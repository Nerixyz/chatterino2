// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#ifdef CHATTERINO_HAVE_PLUGINS

#    include <sol/forward.hpp>

namespace chatterino::lua::api {

/// Loads the 'ui' module as a table.
sol::object loadUi(sol::state_view lua);

}  // namespace chatterino::lua::api

#endif
