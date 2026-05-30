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
#ifndef LOGID_BACKEND_HIDPP20_FEATURE_BATTERYSTATUS_H
#define LOGID_BACKEND_HIDPP20_FEATURE_BATTERYSTATUS_H

#include <backend/hidpp20/Feature.h>
#include <backend/hidpp20/feature_defs.h>
#include <backend/hidpp/Report.h>

namespace logid::backend::hidpp20 {
    class BatteryStatus : public Feature {
    public:
        static constexpr uint16_t ID = FeatureID::BATTERY_STATUS; // 0x1000

        [[nodiscard]] uint16_t getID() final { return ID; }

        enum Function : uint8_t {
            GetBatteryLevelStatus = 0
        };

        enum Event : uint8_t {
            BatteryStatusBroadcast = 0
        };

        struct Status {
            uint8_t percentage;
            bool charging;
            bool known;
        };

        explicit BatteryStatus(Device* dev);

        // Calls function 0 (GetBatteryLevelStatus) and decodes the response.
        Status getStatus();

        /* PURE decoder of a HID++ 0x1000 GetBatteryLevelStatus report.
         *
         * Wire format (HID++ 2.0 0x1000, function 0 response / broadcast):
         *   param[0] = battery discharge level (percentage 0..100)
         *   param[1] = next discharge level
         *   param[2] = battery status enum
         *              (0 = discharging; 1 = recharging; 2 = almost full;
         *               3 = full/charge complete; 4 = slow recharge;
         *               5 = invalid battery; 6 = thermal error)
         *   status in {1,2,3,4} => charging.
         *
         * Untrusted-HID bounds discipline (V5 / ACCESS-04): a report that does
         * not carry a resolved battery payload (no feature index, or too few
         * params) yields {0, false, false} — known == false, NEVER an
         * out-of-bounds read past the report buffer. Defined inline so the
         * dependency-free `battery_feature` unit can exercise it without
         * linking the full daemon.
         */
        [[nodiscard]] static Status decodeStatus(const hidpp::Report& report) {
            Status status = {0, false, false};

            // Bounds guard: only Long reports carry a 0x1000 battery payload,
            // and a resolved feature report has a non-zero feature index.
            // A short/empty/unresolved report (feature index 0 == ROOT) is
            // treated as "unknown" rather than over-read.
            if (report.type() != hidpp::ReportType::Long)
                return status;
            if (report.feature() == 0)
                return status;

            const auto begin = report.paramBegin();
            const auto end = report.paramEnd();
            // Need at least 3 parameter bytes [pct, next, status].
            if (end - begin < 3)
                return status;

            const uint8_t pct = static_cast<uint8_t>(begin[0]);
            const uint8_t st = static_cast<uint8_t>(begin[2]);

            status.percentage = pct;
            status.charging = (st >= 1 && st <= 4);
            status.known = true;
            return status;
        }
    };
}

#endif //LOGID_BACKEND_HIDPP20_FEATURE_BATTERYSTATUS_H
