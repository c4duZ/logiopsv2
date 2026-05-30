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
 * BatteryStatus decoder unit (Phase 2, Wave 0) — DEV-02.
 *
 * Exercises the PURE static decoder BatteryStatus::decodeStatus(hidpp::Report)
 * over a synthetic HID++ 0x1000 (BATTERY_STATUS) GetBatteryLevelStatus report.
 * No device, no bus — just the wire-format decode contract. Per HID++ 0x1000:
 *   param[0] = battery level percentage (e.g. 0x50 == 80)
 *   param[1] = next level
 *   param[2] = battery status enum (0 = discharging; 1/2/3 = charging family)
 *
 * It also pins the bounds discipline (V5 / ACCESS-04): a report carrying no
 * meaningful battery payload must yield known==false rather than over-reading.
 *
 * RED-BY-DESIGN: at this plan's point in time
 * src/logid/backend/hidpp20/features/BatteryStatus.h does NOT exist, so this
 * translation unit FAILS TO COMPILE. That is the intentional Wave 0 RED baseline
 * — it turns GREEN once Plan 02 lands BatteryStatus against the <interfaces>
 * contract in 02-01-PLAN.md. Do NOT stub the feature here; the decoder is Plan
 * 02's deliverable, this plan owns only the test.
 */

#include <QtTest/QtTest>

#include <cstdint>
#include <vector>

#include "backend/hidpp/Report.h"
#include "backend/hidpp20/features/BatteryStatus.h"   // Plan 02 deliverable

using logid::backend::hidpp::Report;
using logid::backend::hidpp20::BatteryStatus;

namespace {
    // Build a synthetic long 0x1000 GetBatteryLevelStatus report on the wireless
    // device index (1), feature index is irrelevant to the pure decoder.
    Report makeBatteryReport(uint8_t percentage, uint8_t statusByte) {
        Report report(Report::Type::Long,
                      logid::backend::hidpp::DeviceIndex::WirelessDevice1,
                      /*feature_index=*/0x05,
                      BatteryStatus::GetBatteryLevelStatus,
                      /*sw_id=*/0x0f);
        std::vector<uint8_t> params(report.paramEnd() - report.paramBegin(), 0);
        params[0] = percentage;     // battery level %
        params[1] = 0x00;           // next level
        params[2] = statusByte;     // battery status enum
        report.setParams(params);
        return report;
    }
}

class BatteryStatusTest : public QObject {
    Q_OBJECT

private slots:
    // DEV-02: 0x50 -> 80%, status 0 (discharging) -> charging == false, known == true.
    void test_decode_discharging() {
        Report report = makeBatteryReport(0x50, /*statusByte=*/0x00);
        const BatteryStatus::Status status = BatteryStatus::decodeStatus(report);
        QCOMPARE(static_cast<int>(status.percentage), 80);
        QCOMPARE(status.charging, false);
        QCOMPARE(status.known, true);
    }

    // DEV-02: a charging status code (>=1) -> charging == true.
    void test_decode_charging() {
        Report report = makeBatteryReport(0x50, /*statusByte=*/0x01);
        const BatteryStatus::Status status = BatteryStatus::decodeStatus(report);
        QCOMPARE(static_cast<int>(status.percentage), 80);
        QCOMPARE(status.charging, true);
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
        const BatteryStatus::Status status = BatteryStatus::decodeStatus(report);
        QCOMPARE(status.known, false);
    }
};

QTEST_MAIN(BatteryStatusTest)
#include "BatteryStatusTest.moc"
