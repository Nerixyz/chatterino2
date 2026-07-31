// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#ifdef CHATTERINO_HAVE_PLUGINS

#    include "controllers/plugins/PluginRef.hpp"

namespace chatterino::lua {

class DialogGuard
{
public:
    explicit DialogGuard(PluginWeakRef weak)
        : weak(std::move(weak))
    {
    }
    DialogGuard(const DialogGuard &) = delete;
    DialogGuard &operator=(const DialogGuard &) = delete;
    DialogGuard(DialogGuard &&other) noexcept
        : weak(std::exchange(other.weak, {}))
    {
    }
    DialogGuard &operator=(DialogGuard &&other) noexcept
    {
        this->weak = std::exchange(other.weak, {});
        return *this;
    }
    ~DialogGuard();

private:
    PluginWeakRef weak;
};

}  // namespace chatterino::lua

#endif
