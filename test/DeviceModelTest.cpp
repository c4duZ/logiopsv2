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
 * DeviceModel unit (Phase 2, Wave 0) — DEV-01/02/03/04 + CONF-03.
 *
 * A Qt Test that drives the GUI device model directly through its public slots
 * (no live D-Bus needed): onDeviceAdded / onDeviceRemoved / onStatusChanged /
 * onBatteryChanged / clear(). It asserts row counts, role values, and — most
 * importantly — the no-flicker contract (a status flip emits exactly one
 * dataChanged for only ConnectionStateRole on one row; never a modelReset, never
 * a reorder).
 *
 * RED-BY-DESIGN: at this plan's point in time src/logiops-gui/DeviceModel.{h,cpp}
 * do NOT exist, so this translation unit FAILS TO COMPILE/LINK. That is the
 * intentional Wave 0 RED baseline — it turns GREEN once Plan 03 lands the model
 * against the <interfaces> contract recorded in 02-01-PLAN.md. Do NOT stub the
 * model here; the model is Plan 03's deliverable, this plan owns only the test.
 */

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "DeviceModel.h"   // Plan 03 deliverable (src/logiops-gui/DeviceModel.h)

using logiops_gui::DeviceModel;

namespace {
    // Known mouse PID used to assert DeviceKindRole == Mouse. The model derives
    // kind from ProductID; this value must map to a mouse in Plan 03's table.
    constexpr quint16 kMousePid = 0x4082; // MX Master 4 (per TESTED.md)
}

class DeviceModelTest : public QObject {
    Q_OBJECT

private slots:
    // DEV-01: enumerate (three direct onDeviceAdded calls) populates three rows.
    void test_enumerate_populates_rows() {
        DeviceModel model;
        model.onDeviceAdded("/pizza/pixl/logiops/devices/0", "Device A", kMousePid, true);
        model.onDeviceAdded("/pizza/pixl/logiops/devices/1", "Device B", kMousePid, true);
        model.onDeviceAdded("/pizza/pixl/logiops/devices/2", "Device C", kMousePid, true);

        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.data(model.index(0), DeviceModel::NameRole).toString(), QStringLiteral("Device A"));
        QCOMPARE(model.data(model.index(1), DeviceModel::NameRole).toString(), QStringLiteral("Device B"));
        QCOMPARE(model.data(model.index(2), DeviceModel::NameRole).toString(), QStringLiteral("Device C"));
    }

    // DEV-03: a single add then remove produces EXACTLY one insert + one remove,
    // and never a full model reset.
    void test_add_remove_single_row() {
        DeviceModel model;
        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

        model.onDeviceAdded("/pizza/pixl/logiops/devices/0", "Solo", kMousePid, true);
        QCOMPARE(model.rowCount(), 1);
        model.onDeviceRemoved("/pizza/pixl/logiops/devices/0");
        QCOMPARE(model.rowCount(), 0);

        QCOMPARE(inserted.count(), 1);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(reset.count(), 0);
    }

    // DEV-03 / CONF-03: a status flip emits ONE dataChanged for ConnectionStateRole
    // only, no reset, and the row order is unchanged (no flicker / no re-sort).
    void test_status_noflicker() {
        DeviceModel model;
        const QString path = QStringLiteral("/pizza/pixl/logiops/devices/0");
        model.onDeviceAdded(path, "Flicker Check", kMousePid, true);

        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

        model.onStatusChanged(path, false);

        QCOMPARE(changed.count(), 1);
        const QList<QVariant> args = changed.takeFirst();
        // dataChanged(topLeft, bottomRight, roles): roles is the 3rd argument.
        const QList<int> roles = args.at(2).value<QList<int>>();
        QCOMPARE(roles, QList<int>{DeviceModel::ConnectionStateRole});
        QCOMPARE(reset.count(), 0);
        // Row order unchanged — path still at row 0.
        QCOMPARE(model.data(model.index(0), DeviceModel::PathRole).toString(), path);
    }

    // DEV-04: name + device-kind roles populate from name / known mouse PID.
    void test_name_kind_roles() {
        DeviceModel model;
        model.onDeviceAdded("/pizza/pixl/logiops/devices/0", "MX Master 4", kMousePid, true);
        QCOMPARE(model.data(model.index(0), DeviceModel::NameRole).toString(), QStringLiteral("MX Master 4"));
        QCOMPARE(model.data(model.index(0), DeviceModel::DeviceKindRole).toInt(), 0 /* Mouse */);
    }

    // DEV-02: unknown battery renders "—" (BatteryKnownRole==false); a later known
    // update populates percent/charging/known.
    void test_battery_unknown_renders_dash() {
        DeviceModel model;
        const QString path = QStringLiteral("/pizza/pixl/logiops/devices/0");
        model.onDeviceAdded(path, "Battery Dev", kMousePid, true);

        model.onBatteryChanged(path, 0, /*charging=*/false, /*known=*/false);
        QCOMPARE(model.data(model.index(0), DeviceModel::BatteryKnownRole).toBool(), false);

        model.onBatteryChanged(path, 80, /*charging=*/true, /*known=*/true);
        QCOMPARE(model.data(model.index(0), DeviceModel::BatteryPercentRole).toInt(), 80);
        QCOMPARE(model.data(model.index(0), DeviceModel::ChargingRole).toBool(), true);
        QCOMPARE(model.data(model.index(0), DeviceModel::BatteryKnownRole).toBool(), true);
    }

    // CONF-03: the model holds no rows except those from enumerate + signals.
    // clear() (reconnect path) drops every row — there is no independent cache.
    void test_clear_reenumerate() {
        DeviceModel model;
        model.onDeviceAdded("/pizza/pixl/logiops/devices/0", "A", kMousePid, true);
        model.onDeviceAdded("/pizza/pixl/logiops/devices/1", "B", kMousePid, true);
        QCOMPARE(model.rowCount(), 2);

        model.clear();
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_MAIN(DeviceModelTest)
#include "DeviceModelTest.moc"
