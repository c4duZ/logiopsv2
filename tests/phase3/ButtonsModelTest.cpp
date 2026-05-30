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
 * ButtonsModel unit (Phase 3, Wave 2) — the .Buttons enumerate + two-step
 * setAction + binding-readback bridge the Buttons tab binds to (BTN-01..04,
 * HOST-01). Driven through the test constructor + seedButtons() with NO live bus
 * (Phase 2 DeviceModelTest precedent). The two-step dispatch is captured via a
 * RecordingButtonsModel subclass overriding the performSetAction /
 * performParamCall hooks, so the call ORDER and arguments are asserted without a
 * bus. It asserts:
 *   (a) roles populate from injected enumerate data,
 *   (b) a non-remappable button reports RemappableRole == false (and refuses
 *       SetAction — T-3-02-03),
 *   (c) setChangeHost("4") on a 3-host device is rejected BEFORE any D-Bus call
 *       (T-3-02-02 std::stoi DoS guard); valid hosts pass,
 *   (d) setKeypress issues SetAction BEFORE the SetKeys param setter (ordering,
 *       T-3-02-04).
 */

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include "ButtonsModel.h"

using logiops_gui::ButtonsModel;

namespace {

// Records the two-step dispatch order so the test can assert SetAction precedes
// the param setter without a live bus.
class RecordingButtonsModel : public ButtonsModel {
public:
    using ButtonsModel::ButtonsModel;

    struct Call {
        QString kind;   // "SetAction" or the param method ("SetKeys", "SetHost", ...)
        QString type;   // action type
        QVariantList args;
    };
    QVector<Call> calls;

protected:
    void performSetAction(int row, const QString& type) override {
        Q_UNUSED(row);
        calls.push_back({QStringLiteral("SetAction"), type, {}});
    }
    void performParamCall(int row, const QString& type, const QString& method,
                          const QVariantList& args) override {
        Q_UNUSED(row);
        calls.push_back({method, type, args});
    }
};

QVector<ButtonsModel::ButtonInfo> fixture() {
    QVector<ButtonsModel::ButtonInfo> v;
    {
        ButtonsModel::ButtonInfo b;
        b.path = QStringLiteral("/pizza/pixl/logiops/devices/0/buttons/0");
        b.controlId = 0x52;
        b.taskId = 0x52;
        b.remappable = true;
        b.gestureSupport = false;
        b.currentActionType = QStringLiteral("None");
        v.push_back(b);
    }
    {
        ButtonsModel::ButtonInfo b;
        b.path = QStringLiteral("/pizza/pixl/logiops/devices/0/buttons/1");
        b.controlId = 0x50; // left click — typically NOT remappable
        b.taskId = 0x50;
        b.remappable = false;
        b.gestureSupport = false;
        b.currentActionType = QStringLiteral("None");
        v.push_back(b);
    }
    return v;
}

} // namespace

class ButtonsModelTest : public QObject {
    Q_OBJECT

private slots:
    // (a) Roles populate from the injected enumerate data, CID-keyed.
    void test_roles_populate_from_seed() {
        ButtonsModel m;
        m.seedButtons(fixture());
        QCOMPARE(m.rowCount(), 2);

        const QModelIndex i0 = m.index(0);
        QCOMPARE(m.data(i0, ButtonsModel::ControlIDRole).toUInt(), 0x52u);
        QCOMPARE(m.data(i0, ButtonsModel::TaskIDRole).toUInt(), 0x52u);
        QVERIFY(m.data(i0, ButtonsModel::RemappableRole).toBool());
        QVERIFY(!m.data(i0, ButtonsModel::GestureSupportRole).toBool());
        QCOMPARE(m.data(i0, ButtonsModel::CurrentActionTypeRole).toString(),
                 QStringLiteral("None"));
        // "None" summarizes as the muted "Disabled" copy (UI-SPEC).
        QCOMPARE(m.data(i0, ButtonsModel::CurrentActionSummaryRole).toString(),
                 QStringLiteral("Disabled"));
        // Name is CID-derived when not provided.
        QVERIFY(m.data(i0, ButtonsModel::ButtonNameRole).toString().contains(
                    QStringLiteral("52")));
    }

    // (b) A non-remappable button reports RemappableRole == false AND refuses
    // SetAction (T-3-02-03: never reassign a non-remappable button).
    void test_non_remappable_gated() {
        RecordingButtonsModel m;
        m.seedButtons(fixture());
        QVERIFY(!m.data(m.index(1), ButtonsModel::RemappableRole).toBool());

        m.setAction(1, QStringLiteral("Keypress"));
        QCOMPARE(m.calls.size(), 0); // no D-Bus dispatch for a non-remappable row.
    }

    // (c) setChangeHost validation: "4" on a 3-host device is rejected BEFORE any
    // D-Bus call (T-3-02-02); "2", "next", "prev" pass.
    void test_change_host_validation() {
        RecordingButtonsModel m;
        m.seedButtons(fixture());
        m.setHostCount(3);

        QSignalSpy rejected(&m, &ButtonsModel::hostRejected);

        // Out-of-range host -> rejected, zero dispatch.
        QCOMPARE(m.setChangeHost(0, QStringLiteral("4")), false);
        QCOMPARE(m.calls.size(), 0);
        QCOMPARE(rejected.count(), 1);

        // Non-numeric garbage -> rejected (the daemon stoi DoS vector).
        QCOMPARE(m.setChangeHost(0, QStringLiteral("'; rm -rf /")), false);
        QCOMPARE(m.calls.size(), 0);
        QCOMPARE(rejected.count(), 2);

        // Empty -> rejected.
        QCOMPARE(m.setChangeHost(0, QString()), false);
        QCOMPARE(m.calls.size(), 0);

        // Valid in-range slot -> accepted, dispatches SetAction then SetHost.
        QCOMPARE(m.setChangeHost(0, QStringLiteral("2")), true);
        QCOMPARE(m.calls.size(), 2);
        QCOMPARE(m.calls.at(0).kind, QStringLiteral("SetAction"));
        QCOMPARE(m.calls.at(0).type, QStringLiteral("ChangeHost"));
        QCOMPARE(m.calls.at(1).kind, QStringLiteral("SetHost"));
        QCOMPARE(m.calls.at(1).args.at(0).toString(), QStringLiteral("2"));

        // next / prev are always valid regardless of host count.
        m.calls.clear();
        QCOMPARE(m.setChangeHost(0, QStringLiteral("next")), true);
        QCOMPARE(m.calls.size(), 2);
        QCOMPARE(m.calls.at(1).args.at(0).toString(), QStringLiteral("next"));
        QCOMPARE(m.setChangeHost(0, QStringLiteral("PREV")), true); // case-insensitive
    }

    // (d) setKeypress issues SetAction BEFORE the SetKeys param setter (ordering,
    // T-3-02-04 — the .Action.<type> interface does not exist until SetAction).
    void test_two_step_ordering() {
        RecordingButtonsModel m;
        m.seedButtons(fixture());

        m.setKeypress(0, QStringList{QStringLiteral("KEY_LEFTCTRL"),
                                     QStringLiteral("KEY_C")});
        QCOMPARE(m.calls.size(), 2);
        QCOMPARE(m.calls.at(0).kind, QStringLiteral("SetAction"));
        QCOMPARE(m.calls.at(0).type, QStringLiteral("Keypress"));
        QCOMPARE(m.calls.at(1).kind, QStringLiteral("SetKeys"));
        // The evdev names flow through verbatim.
        const QStringList sent = m.calls.at(1).args.at(0).toStringList();
        QCOMPARE(sent, (QStringList{QStringLiteral("KEY_LEFTCTRL"),
                                    QStringLiteral("KEY_C")}));

        // The row's optimistic summary reflects the captured combo immediately.
        QCOMPARE(m.data(m.index(0), ButtonsModel::CurrentActionTypeRole).toString(),
                 QStringLiteral("Keypress"));
        QCOMPARE(m.data(m.index(0), ButtonsModel::CurrentActionSummaryRole).toString(),
                 QStringLiteral("KEY_LEFTCTRL + KEY_C"));
    }

    // clearAction maps to SetAction("None") and the row reads back "Disabled".
    void test_clear_action_disables() {
        RecordingButtonsModel m;
        m.seedButtons(fixture());
        m.setKeypress(0, QStringList{QStringLiteral("KEY_A")});
        m.calls.clear();

        m.clearAction(0);
        QCOMPARE(m.calls.size(), 1);
        QCOMPARE(m.calls.at(0).kind, QStringLiteral("SetAction"));
        QCOMPARE(m.calls.at(0).type, QStringLiteral("None"));
        QCOMPARE(m.data(m.index(0), ButtonsModel::CurrentActionSummaryRole).toString(),
                 QStringLiteral("Disabled"));
    }

    // hostCount is settable (wired from DeviceController, HOST-01) and notifies.
    void test_host_count_property() {
        ButtonsModel m;
        QSignalSpy spy(&m, &ButtonsModel::hostCountChanged);
        QCOMPARE(m.hostCount(), 3); // default until wired.
        m.setHostCount(2);
        QCOMPARE(m.hostCount(), 2);
        QCOMPARE(spy.count(), 1);
        m.setHostCount(2); // idempotent.
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(ButtonsModelTest)
#include "ButtonsModelTest.moc"
