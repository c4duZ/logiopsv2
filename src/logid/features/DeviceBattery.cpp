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
#include <features/DeviceBattery.h>
#include <util/task.h>
#include <util/log.h>

using namespace logid::features;
using namespace logid::backend;

DeviceBattery::DeviceBattery(logid::Device* dev) : DeviceFeature(dev) {
    try {
        _battery_status = std::make_shared<hidpp20::BatteryStatus>(&dev->hidpp20());
    } catch (hidpp20::UnsupportedFeature& e) {
        throw UnsupportedFeature();
    }
}

void DeviceBattery::configure() {
    /* One-shot initial read (NOT polling). Live updates arrive via the
     * BatteryStatusBroadcast event handler registered in listen(). A device
     * that is momentarily unreachable simply keeps its last/unknown value. */
    try {
        auto status = _battery_status->getStatus();
        _device->setBattery(status.percentage, status.charging, status.known);
    } catch (std::exception& e) {
        logPrintf(DEBUG, "Could not read initial battery status: %s", e.what());
    }
}

void DeviceBattery::listen() {
    if (_ev_handler.empty()) {
        _ev_handler = _device->hidpp20().addEventHandler(
                {[index = _battery_status->featureIndex()](
                        const hidpp::Report& report) -> bool {
                    return report.feature() == index &&
                           report.function() ==
                           hidpp20::BatteryStatus::BatteryStatusBroadcast;
                },
                 [self_weak = self<DeviceBattery>()](
                         const hidpp::Report& report) {
                     auto status = hidpp20::BatteryStatus::decodeStatus(report);
                     run_task([self_weak, status]() {
                         if (auto self = self_weak.lock())
                             self->_device->setBattery(status.percentage,
                                                       status.charging,
                                                       status.known);
                     });
                 }
                });
    }
}

void DeviceBattery::setProfile(config::Profile&) {
}
