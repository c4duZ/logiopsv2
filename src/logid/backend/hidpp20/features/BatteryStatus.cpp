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
#include <backend/hidpp20/features/BatteryStatus.h>
#include <backend/hidpp20/Device.h>

using namespace logid::backend::hidpp20;

BatteryStatus::BatteryStatus(Device* dev) : Feature(dev, ID) {
}

BatteryStatus::Status BatteryStatus::getStatus() {
    std::vector<uint8_t> params(0);
    auto response = callFunction(GetBatteryLevelStatus, params);

    // callFunction returns the response *parameters* only. Re-wrap them in a
    // Long report carrying this feature's resolved index so the pure decoder
    // (and its bounds discipline) can run over a uniform Report shape.
    hidpp::Report report(hidpp::ReportType::Long,
                         _device->deviceIndex(),
                         featureIndex(),
                         GetBatteryLevelStatus,
                         /*sw_id=*/0x0f);
    report.setParams(response);
    return decodeStatus(report);
}
