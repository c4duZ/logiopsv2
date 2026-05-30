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
 * DeviceController unit (Phase 3, Wave 1) — the capability-gating + DPI-bounds
 * spine that Wave 2's tab plans bind to (UI-01 / SCR-01..03 / DPI-01).
 *
 * Drives the controller through its construction seams (applyIntrospection +
 * the seed* entry points) with NO live bus — exactly the Phase 2 DeviceModelTest
 * precedent. It asserts:
 *   (a) an ABSENT interface -> hasX == false (T-3-01-03 gating),
 *   (b) a PRESENT DPI with isRange dpis {400, 8000} -> dpiMin == 400 / dpiMax == 8000,
 *   (c) hasTorque reflects the injected TorqueSupport read,
 *   plus the XML parse and the optimistic-setter change-signal contract.
 */

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "DeviceController.h"

using logiops_gui::DeviceController;

namespace {
    QSet<QString> ifaceSet(std::initializer_list<const char*> names) {
        QSet<QString> s;
        for (const char* n : names)
            s.insert(QString::fromUtf8(n));
        return s;
    }
}

class DeviceControllerTest : public QObject {
    Q_OBJECT

private slots:
    // (a) Absent interfaces gate their capability flags to false; present ones true.
    void test_absent_interface_caps_false() {
        DeviceController dc(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        // Only DPI + SmartShift present; HiresScroll / ThumbWheel / Buttons absent.
        dc.applyIntrospection(ifaceSet({"pizza.pixl.LogiOps.DPI",
                                        "pizza.pixl.LogiOps.SmartShift"}));
        QVERIFY(dc.hasDpi());
        QVERIFY(dc.hasSmartShift());
        QVERIFY(!dc.hasHires());
        QVERIFY(!dc.hasThumbwheel());
        QVERIFY(!dc.hasButtons());
    }

    // (b) Present DPI with isRange dpis {400, 8000} -> dpiMin 400 / dpiMax 8000.
    void test_dpi_bounds_from_range() {
        DeviceController dc(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        dc.applyIntrospection(ifaceSet({"pizza.pixl.LogiOps.DPI"}));
        dc.seedDpiBounds(QList<int>{400, 8000}, 50, /*isRange=*/true, /*sensorCount=*/1);
        QCOMPARE(dc.dpiMin(), 400);
        QCOMPARE(dc.dpiMax(), 8000);
        QCOMPARE(dc.dpiStep(), 50);
        QCOMPARE(dc.sensorCount(), 1);
    }

    // (c) hasTorque reflects the injected TorqueSupport read.
    void test_torque_support_reflected() {
        DeviceController on(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        on.applyIntrospection(ifaceSet({"pizza.pixl.LogiOps.SmartShift"}));
        on.seedTorqueSupport(true);
        QVERIFY(on.hasTorque());

        DeviceController off(QStringLiteral("/pizza/pixl/logiops/devices/1"));
        off.applyIntrospection(ifaceSet({"pizza.pixl.LogiOps.SmartShift"}));
        off.seedTorqueSupport(false);
        QVERIFY(!off.hasTorque());
    }

    // hostCount defaults to 3 (NOT exposed over D-Bus yet — Plan 02 wires real).
    void test_host_count_defaults_three() {
        DeviceController dc(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        QCOMPARE(dc.hostCount(), 3);
        dc.setHostCount(2);
        QCOMPARE(dc.hostCount(), 2);
    }

    // Introspect XML parses <interface name=...> into the present-set.
    void test_parse_interfaces_from_xml() {
        DeviceController dc(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        const QString xml = QStringLiteral(
            "<node>"
            "  <interface name=\"pizza.pixl.LogiOps.DPI\"/>"
            "  <interface name=\"pizza.pixl.LogiOps.HiresScroll\"/>"
            "  <interface name=\"org.freedesktop.DBus.Introspectable\"/>"
            "</node>");
        const QSet<QString> present = dc.parseInterfaces(xml);
        QVERIFY(present.contains(QStringLiteral("pizza.pixl.LogiOps.DPI")));
        QVERIFY(present.contains(QStringLiteral("pizza.pixl.LogiOps.HiresScroll")));
        QVERIFY(!present.contains(QStringLiteral("pizza.pixl.LogiOps.SmartShift")));
        // Applying it lights up exactly those caps.
        dc.applyIntrospection(present);
        QVERIFY(dc.hasDpi());
        QVERIFY(dc.hasHires());
        QVERIFY(!dc.hasSmartShift());
    }

    // Optimistic setter emits the change signal immediately (no bus needed).
    void test_optimistic_setter_emits() {
        DeviceController dc(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        dc.applyIntrospection(ifaceSet({"pizza.pixl.LogiOps.DPI"}));
        QSignalSpy spy(&dc, &DeviceController::dpiChanged);
        dc.setDpi(1600);
        QCOMPARE(dc.dpi(), 1600);
        QCOMPARE(spy.count(), 1);
        // Idempotent: same value does not re-emit.
        dc.setDpi(1600);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(DeviceControllerTest)
#include "DeviceControllerTest.moc"
