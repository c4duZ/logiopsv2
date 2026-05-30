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

/*
 * UnifiedBattery decoder unit (Phase 2 follow-up) — DEV-02 (0x1004).
 *
 * Mirrors BatteryStatusTest for the modern HID++ 0x1004 UnifiedBattery feature
 * used by recent devices (MX Master 3/3S/4). Exercises the PURE static decoder
 * UnifiedBattery::decodeStatus(hidpp::Report) over a synthetic 0x1004 GetStatus
 * report. No device, no bus — just the wire-format decode contract:
 *   param[0] = stateOfCharge percentage (e.g. 0x50 == 80)
 *   param[1] = batteryLevel discrete enum
 *   param[2] = chargingStatus enum (0 = discharging; 1/2/3 = charging family;
 *              4 = charging error => NOT charging)
 *
 * It also pins the bounds discipline (V5 / ACCESS-04): a report carrying no
 * meaningful battery payload must yield known==false rather than over-reading.
 */

#include <QtTest/QtTest>

#include <cstdint>
#include <vector>

#include "backend/hidpp/Report.h"
#include "backend/hidpp20/features/UnifiedBattery.h"

using logid::backend::hidpp::Report;
using logid::backend::hidpp20::UnifiedBattery;

namespace {
    // Build a synthetic long 0x1004 GetStatus report on the wireless device
    // index (1); feature index is irrelevant to the pure decoder beyond being
    // non-zero (a resolved feature).
    Report makeUnifiedReport(uint8_t percentage, uint8_t chargingStatus) {
        Report report(Report::Type::Long,
                      logid::backend::hidpp::DeviceIndex::WirelessDevice1,
                      /*feature_index=*/0x06,
                      UnifiedBattery::GetStatus,
                      /*sw_id=*/0x0f);
        std::vector<uint8_t> params(report.paramEnd() - report.paramBegin(), 0);
        params[0] = percentage;       // stateOfCharge %
        params[1] = 0x00;             // batteryLevel enum
        params[2] = chargingStatus;   // chargingStatus enum
        report.setParams(params);
        return report;
    }
}

class UnifiedBatteryTest : public QObject {
    Q_OBJECT

private slots:
    // 0x50 -> 80%, chargingStatus 0 (discharging) -> charging == false, known.
    void test_decode_discharging() {
        Report report = makeUnifiedReport(0x50, /*chargingStatus=*/0x00);
        const UnifiedBattery::Status status = UnifiedBattery::decodeStatus(report);
        QCOMPARE(static_cast<int>(status.percentage), 80);
        QCOMPARE(status.charging, false);
        QCOMPARE(status.known, true);
    }

    // chargingStatus 1 (charging) -> charging == true.
    void test_decode_charging() {
        Report report = makeUnifiedReport(0x50, /*chargingStatus=*/0x01);
        const UnifiedBattery::Status status = UnifiedBattery::decodeStatus(report);
        QCOMPARE(static_cast<int>(status.percentage), 80);
        QCOMPARE(status.charging, true);
        QCOMPARE(status.known, true);
    }

    // chargingStatus 4 (charging error) -> NOT charging, but still known.
    void test_decode_charging_error() {
        Report report = makeUnifiedReport(0x50, /*chargingStatus=*/0x04);
        const UnifiedBattery::Status status = UnifiedBattery::decodeStatus(report);
        QCOMPARE(status.charging, false);
        QCOMPARE(status.known, true);
    }

    // V5 / ACCESS-04 bounds discipline: a report with no meaningful battery
    // payload must report known == false, NOT over-read past the buffer.
    void test_decode_bounds_guard() {
        std::vector<uint8_t> raw;
        raw.push_back(Report::Type::Long);
        raw.push_back(logid::backend::hidpp::DeviceIndex::WirelessDevice1);
        // No feature/function/params — the decoder must treat this as "unknown".
        Report report(raw);
        report.setParams(std::vector<uint8_t>(report.paramEnd() - report.paramBegin(), 0));
        const UnifiedBattery::Status status = UnifiedBattery::decodeStatus(report);
        QCOMPARE(status.known, false);
    }
};

QTEST_MAIN(UnifiedBatteryTest)
#include "UnifiedBatteryTest.moc"
