/*
 * Copyright 2024 c4duZ - https://github.com/c4duZ/logiopsv2
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
 * DpiCycleTest (Phase 3, Wave 2 / Plan 03) — the DPI-cycle preset model + the
 * EXACT persistence path (DPI-02 / DPI-03).
 *
 * Option-a (Task 0) makes the Pointer-tab cycle list device/profile-scoped: the
 * DeviceController preset mutators (addPreset/removePreset/setPresetLabel/
 * setPresetValue) push the FULL {value,label} list to the daemon via
 * .DPI.SetPresets. This test drives the controller with NO live bus by overriding
 * the virtual pushPresets() hook to RECORD the exact (values,labels) payload, then
 * asserts:
 *   - add/remove/relabel/setValue update the {value,label} model,
 *   - DPI values clamp to [dpiMin,dpiMax] before they are emitted/pushed
 *     (T-3-03-01),
 *   - every mutation pushes the FULL preset list to the .DPI.SetPresets payload
 *     (named node — NOT a generic "round-trip through CycleDPI.SetDPIs"),
 *   - seedPresets (the GetPresets reply path) populates the model from parallel
 *     value/label arrays.
 */

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "DeviceController.h"

using logiops_gui::DeviceController;

namespace {

// Records the exact .DPI.SetPresets payload pushed by the controller's preset
// mutators, with no live bus. This is the ACTUAL device-scoped persistence path
// (option-a), so asserting against it proves the chosen node — not a hand-wave.
class RecordingController : public DeviceController {
public:
    explicit RecordingController(const QString& path) : DeviceController(path) {}

    QList<uint> lastValues;
    QStringList lastLabels;
    int pushCount = 0;

protected:
    void pushPresets(const QList<uint>& values, const QStringList& labels) override {
        lastValues = values;
        lastLabels = labels;
        ++pushCount;
    }
};

QSet<QString> dpiIface() {
    QSet<QString> s;
    s.insert(QStringLiteral("pizza.pixl.LogiOps.DPI"));
    return s;
}

int presetValue(const QVariantList& presets, int i) {
    return presets.at(i).toMap().value(QStringLiteral("value")).toInt();
}
QString presetLabel(const QVariantList& presets, int i) {
    return presets.at(i).toMap().value(QStringLiteral("label")).toString();
}

} // namespace

class DpiCycleTest : public QObject {
    Q_OBJECT

private slots:
    // seedPresets (the GetPresets reply path) builds the {value,label} model from
    // the two parallel arrays.
    void test_seed_presets_builds_model() {
        RecordingController dc(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        dc.applyIntrospection(dpiIface());
        dc.seedPresets(QList<uint>{800u, 1600u, 3200u},
                       QStringList{QStringLiteral("Low"), QStringLiteral("Med"),
                                   QStringLiteral("High")});
        const QVariantList p = dc.dpiPresets();
        QCOMPARE(p.size(), 3);
        QCOMPARE(presetValue(p, 0), 800);
        QCOMPARE(presetLabel(p, 0), QStringLiteral("Low"));
        QCOMPARE(presetValue(p, 2), 3200);
        QCOMPARE(presetLabel(p, 2), QStringLiteral("High"));
        // Seeding does NOT push (it reflects the daemon's current state).
        QCOMPARE(dc.pushCount, 0);
    }

    // addPreset updates the model AND pushes the FULL {value,label} list to the
    // .DPI.SetPresets payload (the actual persistence node).
    void test_add_preset_pushes_full_list() {
        RecordingController dc(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        dc.applyIntrospection(dpiIface());
        QSignalSpy spy(&dc, &DeviceController::dpiPresetsChanged);

        dc.addPreset(800, QStringLiteral("Low"));
        dc.addPreset(1600, QStringLiteral("Medium"));

        const QVariantList p = dc.dpiPresets();
        QCOMPARE(p.size(), 2);
        QCOMPARE(presetValue(p, 1), 1600);
        QCOMPARE(presetLabel(p, 1), QStringLiteral("Medium"));
        QVERIFY(spy.count() >= 2);

        // The LAST push carries the full two-entry list as parallel arrays.
        QCOMPARE(dc.lastValues, (QList<uint>{800u, 1600u}));
        QCOMPARE(dc.lastLabels, (QStringList{QStringLiteral("Low"),
                                             QStringLiteral("Medium")}));
        QCOMPARE(dc.pushCount, 2);
    }

    // removePreset drops the row from both the model and the pushed payload.
    void test_remove_preset_pushes_shrunk_list() {
        RecordingController dc(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        dc.applyIntrospection(dpiIface());
        dc.seedPresets(QList<uint>{800u, 1600u, 3200u},
                       QStringList{QStringLiteral("Low"), QStringLiteral("Med"),
                                   QStringLiteral("High")});
        dc.removePreset(1); // drop "Med" / 1600

        const QVariantList p = dc.dpiPresets();
        QCOMPARE(p.size(), 2);
        QCOMPARE(presetValue(p, 0), 800);
        QCOMPARE(presetValue(p, 1), 3200);
        QCOMPARE(dc.lastValues, (QList<uint>{800u, 3200u}));
        QCOMPARE(dc.lastLabels, (QStringList{QStringLiteral("Low"),
                                             QStringLiteral("High")}));
    }

    // setPresetLabel relabels a row and re-pushes the list with the new label
    // (proves LABELS are carried in the persistence payload, DPI-03).
    void test_relabel_pushes_new_label() {
        RecordingController dc(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        dc.applyIntrospection(dpiIface());
        dc.seedPresets(QList<uint>{800u, 1600u},
                       QStringList{QStringLiteral("Low"), QStringLiteral("Med")});
        dc.setPresetLabel(1, QStringLiteral("Office"));

        QCOMPARE(presetLabel(dc.dpiPresets(), 1), QStringLiteral("Office"));
        QCOMPARE(dc.lastValues, (QList<uint>{800u, 1600u}));
        QCOMPARE(dc.lastLabels, (QStringList{QStringLiteral("Low"),
                                             QStringLiteral("Office")}));
    }

    // DPI values clamp to [dpiMin,dpiMax] before they reach the model/payload
    // (T-3-03-01 defense in depth) for BOTH addPreset and setPresetValue.
    void test_preset_values_clamp_to_bounds() {
        RecordingController dc(QStringLiteral("/pizza/pixl/logiops/devices/0"));
        dc.applyIntrospection(dpiIface());
        dc.seedDpiBounds(QList<int>{400, 8000}, 50, /*isRange=*/true, /*sensorCount=*/1);

        dc.addPreset(100, QStringLiteral("TooLow"));   // -> clamp to 400
        dc.addPreset(99999, QStringLiteral("TooHigh")); // -> clamp to 8000

        QCOMPARE(presetValue(dc.dpiPresets(), 0), 400);
        QCOMPARE(presetValue(dc.dpiPresets(), 1), 8000);
        QCOMPARE(dc.lastValues, (QList<uint>{400u, 8000u}));

        dc.setPresetValue(0, 50);       // below min -> 400
        QCOMPARE(presetValue(dc.dpiPresets(), 0), 400);
        dc.setPresetValue(1, 1000000);  // above max -> 8000
        QCOMPARE(presetValue(dc.dpiPresets(), 1), 8000);
        QCOMPARE(dc.lastValues, (QList<uint>{400u, 8000u}));
    }
};

QTEST_MAIN(DpiCycleTest)
#include "DpiCycleTest.moc"
