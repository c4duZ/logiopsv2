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
#ifndef LOGID_BACKEND_HIDPP20_FEATURE_UNIFIEDBATTERY_H
#define LOGID_BACKEND_HIDPP20_FEATURE_UNIFIEDBATTERY_H

#include <backend/hidpp20/Feature.h>
#include <backend/hidpp20/feature_defs.h>
#include <backend/hidpp/Report.h>

namespace logid::backend::hidpp20 {
    /* HID++ 2.0 UnifiedBattery (0x1004).
     *
     * The modern battery feature used by recent devices (e.g. MX Master 3/3S/4)
     * in preference to the legacy 0x1000 BatteryStatus. It exposes the same
     * {percentage, charging, known} shape so it can feed the identical
     * Device::setBattery() / BatteryChanged path with no .Device interface
     * change.
     *
     * Wire format (HID++ 2.0 0x1004) — confidence: MEDIUM (verified against
     * cvuchener/hidpp + libratbag references, not live hardware this session):
     *   function 0x00 getCapabilities -> supportedLevels bitmask + flags
     *                 (flag bit0 == stateOfChargeSupported / rechargeable);
     *                 only used to decide whether percentage is meaningful.
     *   function 0x01 getStatus response / broadcast:
     *     param[0] = stateOfCharge (battery percentage 0..100)
     *     param[1] = batteryLevel  (discrete level enum: critical/low/good/full)
     *     param[2] = chargingStatus enum
     *                (0 = discharging; 1 = charging; 2 = charging slow / nearly
     *                 full; 3 = charge complete; 4 = charging error)
     *     param[3] = externalPowerStatus flags
     *   chargingStatus in {1,2,3} => charging. (4 == error => not charging.)
     */
    class UnifiedBattery : public Feature {
    public:
        static constexpr uint16_t ID = FeatureID::UNIFIED_BATTERY; // 0x1004

        [[nodiscard]] uint16_t getID() final { return ID; }

        enum Function : uint8_t {
            GetCapabilities = 0,
            GetStatus = 1
        };

        enum Event : uint8_t {
            StatusBroadcast = 1
        };

        struct Status {
            uint8_t percentage;
            bool charging;
            bool known;
        };

        explicit UnifiedBattery(Device* dev);

        // Calls function 1 (GetStatus) and decodes the response.
        Status getStatus();

        /* PURE decoder of a HID++ 0x1004 GetStatus report.
         *
         * Untrusted-HID bounds discipline (V5 / ACCESS-04): a report that does
         * not carry a resolved battery payload (non-Long, unresolved feature
         * index, or too few params) yields {0, false, false} — known == false,
         * NEVER an out-of-bounds read past the report buffer. Defined inline so
         * a dependency-free unit can exercise it without linking the daemon.
         */
        [[nodiscard]] static Status decodeStatus(const hidpp::Report& report) {
            Status status = {0, false, false};

            // Only Long reports carry a 0x1004 battery payload; a resolved
            // feature report has a non-zero feature index. A short/empty/
            // unresolved report (feature index 0 == ROOT) is treated as
            // "unknown" rather than over-read.
            if (report.type() != hidpp::ReportType::Long)
                return status;
            if (report.feature() == 0)
                return status;

            const auto begin = report.paramBegin();
            const auto end = report.paramEnd();
            // Need at least 3 parameter bytes [stateOfCharge, level, charging].
            if (end - begin < 3)
                return status;

            const uint8_t pct = static_cast<uint8_t>(begin[0]);
            const uint8_t charging_status = static_cast<uint8_t>(begin[2]);

            status.percentage = pct;
            // 1 = charging, 2 = slow/nearly-full, 3 = complete => charging;
            // 4 = error and 0 = discharging => not charging.
            status.charging = (charging_status >= 1 && charging_status <= 3);
            status.known = true;
            return status;
        }
    };
}

#endif //LOGID_BACKEND_HIDPP20_FEATURE_UNIFIEDBATTERY_H
