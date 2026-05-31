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
#include <actions/gesture/ReleaseGesture.h>
#include <Configuration.h>
#include <stdexcept>

using namespace logid::actions;

const char* ReleaseGesture::interface_name = "OnRelease";

ReleaseGesture::ReleaseGesture(Device* device, config::ReleaseGesture& config,
                               const std::shared_ptr<ipcgull::node>& parent) :
        Gesture(device, parent, interface_name, {
                {
                        {"GetThreshold", {this, &ReleaseGesture::getThreshold, {"threshold"}}},
                        {"SetThreshold", {this, &ReleaseGesture::setThreshold, {"threshold"}}},
                        {"SetAction", {this, &ReleaseGesture::setAction, {"type"}}}
                },
                {},
                {}
        }), _config(config) {
    if (_config.action.has_value())
        _action = Action::makeAction(device, _config.action.value(), _node);
}

void ReleaseGesture::press(bool init_threshold) {
    std::shared_lock lock(_config_mutex);
    if (init_threshold) {
        _axis = (int32_t) (_config.threshold.value_or(defaults::gesture_threshold));
    } else {
        _axis = 0;
    }
}

void ReleaseGesture::release(bool primary) {
    if (metThreshold() && primary) {
        if (_action) {
            _action->press();
            _action->release();
        }
    }
}

void ReleaseGesture::move(int16_t axis) {
    _axis += axis;
}

bool ReleaseGesture::wheelCompatibility() const {
    return false;
}

bool ReleaseGesture::metThreshold() const {
    std::shared_lock lock(_config_mutex);
    return _axis >= _config.threshold.value_or(defaults::gesture_threshold);
}


int ReleaseGesture::getThreshold() const {
    std::shared_lock lock(_config_mutex);
    return _config.threshold.value_or(0);
}

void ReleaseGesture::setThreshold(int threshold) {
    std::unique_lock lock(_config_mutex);
    if (threshold == 0)
        _config.threshold.reset();
    else
        _config.threshold = threshold;
}

void ReleaseGesture::setAction(const std::string& type) {
    std::unique_lock lock(_config_mutex);
    // `type` is an untrusted D-Bus string. makeAction throws InvalidAction on an
    // unrecognized type and may have partially mutated _config.action. Mirror
    // GestureAction::setGesture: snapshot the prior config, and on failure restore
    // a VALID action (from the saved config, or reset) and rethrow a clean D-Bus
    // error so the node never aborts the root daemon (WR-05, T-04-01-03).
    auto prev_config = _config.action;
    _action.reset();
    try {
        _action = Action::makeAction(_device, type, _config.action, _node);
    } catch (InvalidAction& e) {
        _config.action = prev_config;
        if (_config.action)
            _action = Action::makeAction(_device, _config.action.value(), _node);
        throw std::invalid_argument("Invalid action type");
    }
}
