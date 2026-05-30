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
#ifndef LOGID_FEATURE_DEVICEBATTERY_H
#define LOGID_FEATURE_DEVICEBATTERY_H

#include <features/DeviceFeature.h>
#include <Device.h>
#include <backend/hidpp20/features/BatteryStatus.h>
#include <backend/hidpp20/features/UnifiedBattery.h>

namespace logid::features {
    /* Not `final`: the framework's _featureWrapper<T> derives from this type
     * (DeviceFeature::make), so the leaf class must remain open. Leaf-ness is
     * instead expressed via `final` on each override (mirrors DeviceStatus).
     *
     * Battery source preference: modern devices (MX Master 3/3S/4) report via
     * HID++ 0x1004 UnifiedBattery; older devices via 0x1000 BatteryStatus. The
     * ctor tries 0x1004 first and falls back to 0x1000. Whichever resolves,
     * both feed the SAME Device::setBattery() / BatteryChanged path — the
     * .Device interface shape is unchanged. */
    class DeviceBattery : public DeviceFeature {
    public:
        void configure() final;

        void listen() final;

        void setProfile(config::Profile& profile) final;

    protected:
        explicit DeviceBattery(Device* dev);

    private:
        EventHandlerLock<backend::hidpp::Device> _ev_handler;
        // Exactly one of these is non-null. _unified_battery (0x1004) is
        // preferred; _battery_status (0x1000) is the fallback.
        std::shared_ptr<backend::hidpp20::UnifiedBattery> _unified_battery;
        std::shared_ptr<backend::hidpp20::BatteryStatus> _battery_status;
    };
}

#endif //LOGID_FEATURE_DEVICEBATTERY_H
