/*
 * Copyright 2019-2023 PixlOne
 * Adapted by c4duZ - https://github.com/c4duZ/logiopsv2
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <actions/ChangeHostAction.h>
#include <Device.h>
#include <backend/hidpp20/features/ReprogControls.h>
#include <algorithm>
#include <util/task.h>
#include <util/log.h>

using namespace logid::actions;
using namespace logid::backend;

const char* ChangeHostAction::interface_name = "ChangeHost";

ChangeHostAction::ChangeHostAction(
        Device* device, config::ChangeHost& config,
        [[maybe_unused]] const std::shared_ptr<ipcgull::node>& parent)
        : Action(device, interface_name, {
        {
                {"GetHost", {this, &ChangeHostAction::getHost, {"host"}}},
                {"SetHost", {this, &ChangeHostAction::setHost, {"host"}}},
                {"GetHostCount", {this, &ChangeHostAction::getHostCount, {"count"}}}
        },
        {},
        {}
}), _config(config) {
    if (_config.host.has_value()) {
        if (std::holds_alternative<std::string>(_config.host.value())) {
            auto& host = std::get<std::string>(_config.host.value());
            std::transform(host.begin(), host.end(),
                           host.begin(), ::tolower);
        }
    }
    try {
        _change_host = std::make_shared<hidpp20::ChangeHost>(&device->hidpp20());
    } catch (hidpp20::UnsupportedFeature& e) {
        logPrintf(WARN, "%s:%d: ChangeHost feature not supported, "
                        "ChangeHostAction will not work.", device->hidpp20()
                          .devicePath().c_str(), device->hidpp20().deviceIndex());
    }
}

std::string ChangeHostAction::getHost() const {
    std::shared_lock lock(_config_mutex);
    if (_config.host.has_value()) {
        if (std::holds_alternative<std::string>(_config.host.value()))
            return std::get<std::string>(_config.host.value());
        else
            return std::to_string(std::get<int>(_config.host.value()));
    } else {
        return "";
    }
}

uint8_t ChangeHostAction::getHostCount() const {
    if (!_change_host)
        return 0;
    return _change_host->getHostInfo().hostCount;
}

void ChangeHostAction::setHost(std::string host) {
    std::transform(host.begin(), host.end(),
                   host.begin(), ::tolower);
    std::unique_lock lock(_config_mutex);
    if (host == "next" || host == "prev" || host == "previous") {
        _config.host = std::move(host);
        return;
    }
    // SetHost is reachable over D-Bus by a non-root caller (CR-01). Never run
    // std::stoi on the untrusted string directly: it throws std::invalid_argument
    // on a non-numeric value and std::out_of_range on an oversized one, and an
    // uncaught throw out of the ipcgull dispatch destabilizes the root daemon.
    // Parse defensively and surface a marshalled D-Bus error instead.
    int parsed;
    try {
        std::size_t consumed = 0;
        parsed = std::stoi(host, &consumed);
        if (consumed != host.size()) // reject trailing junk ("3a", "3 ", ...)
            throw std::invalid_argument(host);
    } catch (const std::exception&) {
        throw std::invalid_argument(
                "ChangeHost: host must be a number or next/prev/previous");
    }
    _config.host = parsed;
}

void ChangeHostAction::press() {
    // Do nothing, wait until release
}

void ChangeHostAction::release() {
    std::shared_lock lock(_config_mutex);
    if (_change_host && _config.host.has_value()) {
        run_task([self_weak = self<ChangeHostAction>(), host = _config.host.value()] {
            if (auto self = self_weak.lock()) {
                auto host_info = self->_change_host->getHostInfo();
                // hostCount is a device-reported uint8_t; a value of 0 (odd or
                // transient device read) would make the modulo below an integer
                // division by zero -> SIGFPE in the root daemon (CR-02).
                if (host_info.hostCount == 0)
                    return;
                int next_host;
                if (std::holds_alternative<std::string>(host)) {
                    const auto& host_str = std::get<std::string>(host);
                    if (host_str == "next")
                        next_host = host_info.currentHost + 1;
                    else if (host_str == "prev" || host_str == "previous")
                        next_host = host_info.currentHost - 1;
                    else
                        next_host = host_info.currentHost;
                } else {
                    next_host = std::get<int>(host) - 1;
                }
                // C++ % follows the dividend's sign, so "prev" from host 0 yields
                // -1 and would send host 255. Use a non-negative modulo so the
                // intended wrap-around (prev from 0 -> last host) happens.
                const int count = host_info.hostCount;
                next_host = ((next_host % count) + count) % count;
                if (next_host != host_info.currentHost)
                    self->_change_host->setHost(static_cast<uint8_t>(next_host));
            }
        });
    }
}

uint8_t ChangeHostAction::reprogFlags() const {
    return hidpp20::ReprogControls::TemporaryDiverted;
}
