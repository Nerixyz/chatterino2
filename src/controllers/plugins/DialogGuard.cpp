// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#ifdef CHATTERINO_HAVE_PLUGINS

#    include "controllers/plugins/DialogGuard.hpp"

#    include "controllers/plugins/Plugin.hpp"

namespace chatterino::lua {

DialogGuard::~DialogGuard()
{
    PluginRef ref = weak.strong();
    if (ref)
    {
        if (ref.plugin()->openDialogs > 0)
        {
            ref.plugin()->openDialogs -= 1;
        }
        else
        {
            assert(false && "No open dialog?");
        }
    }
}

}  // namespace chatterino::lua

#endif
