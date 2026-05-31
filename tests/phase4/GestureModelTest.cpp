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

// RED until Plan 02 lands the GestureModel skeleton (GEST-01/04).
//
// This is the GUI-side recording-subclass scaffold for the gesture builder,
// mirroring tests/phase3/ButtonsModelTest.cpp's no-bus pattern. It will NOT LINK
// until Plan 02 creates src/logiops-gui/GestureModel.{h,cpp}; that is the intended
// Wave-0 state. The tests/phase4/CMakeLists.txt `EXISTS GestureModelTest.cpp` +
// `TARGET logiops-gui-lib` guard keeps the daemon-side phase4_gesture_math unit
// buildable regardless of this file's RED status.
//
// Plan 02 symbols this scaffold expects (the contract Plan 02 must satisfy):
//   class logiops_gui::GestureModel : QObject
//     - Q_PROPERTY(QString previewSentence READ previewSentence NOTIFY previewChanged)
//     - Q_INVOKABLE bool setMode(const QString& direction, const QString& mode)
//         plain-language `mode` allowlist -> daemon gesture type:
//           "Repeat while moving"        -> "OnInterval"
//           "Do once when moved far enough" -> "OnRelease"
//           "Adjust proportionally"      -> "Axis"
//           "Nothing"                    -> "None"
//         an UNKNOWN mode or direction is rejected with NO recorded dispatch.
//     - virtual void performSetGesture(const QString& direction, const QString& type)
//     - virtual void performParamCall(const QString& direction, const QString& type,
//                                     const QString& method, const QVariantList& args)
//         (the two-step seam: SetGesture FIRST, the per-mode .Gesture.<type>
//          param setter SECOND — mirrors ButtonsModel performSetAction/performParamCall)
//     - QString previewSentence() const  // GEST-04, recomposed per UI-SPEC templates

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QString>
#include <QVariantList>
#include <QVector>

#include "GestureModel.h"

using logiops_gui::GestureModel;

namespace {

// Records the two-step dispatch (direction, type, method, args) so the test can
// assert SetGesture precedes the per-mode param setter without a live bus.
class RecordingGestureModel : public GestureModel {
public:
    using GestureModel::GestureModel;

    struct Call {
        QString kind;       // "SetGesture" or the param method ("SetInterval", ...)
        QString direction;  // up/down/left/right
        QString type;       // daemon gesture type: OnInterval/OnRelease/Axis/None
        QVariantList args;
    };
    QVector<Call> calls;

protected:
    void performSetGesture(const QString& direction, const QString& type) override {
        calls.push_back({QStringLiteral("SetGesture"), direction, type, {}});
    }
    void performParamCall(const QString& direction, const QString& type,
                          const QString& method, const QVariantList& args) override {
        calls.push_back({method, direction, type, args});
    }
};

} // namespace

class GestureModelTest : public QObject {
    Q_OBJECT

private slots:
    // (1) TWO-STEP ORDER: a mode set records SetGesture(direction, type) FIRST,
    // then any per-mode param call SECOND (the .Gesture.<type> setter does not
    // exist until SetGesture has rebuilt the node — RESEARCH Pattern 1).
    void test_two_step_order() {
        RecordingGestureModel m;
        m.setMode(QStringLiteral("up"), QStringLiteral("Repeat while moving"));

        QVERIFY(m.calls.size() >= 1);
        QCOMPARE(m.calls.at(0).kind, QStringLiteral("SetGesture"));
        QCOMPARE(m.calls.at(0).direction, QStringLiteral("up"));
        QCOMPARE(m.calls.at(0).type, QStringLiteral("OnInterval"));
        // If a per-mode param call follows, it comes strictly AFTER SetGesture.
        if (m.calls.size() >= 2)
            QVERIFY(m.calls.at(1).kind != QStringLiteral("SetGesture"));
    }

    // (2) MODE ALLOWLIST: the four plain-language modes map to exactly
    // OnInterval / OnRelease / Axis / None; an unknown mode or direction is
    // rejected BEFORE any dispatch (no recorded call) — V5 input validation.
    void test_mode_allowlist() {
        struct { const char* mode; const char* type; } cases[] = {
            {"Repeat while moving",          "OnInterval"},
            {"Do once when moved far enough", "OnRelease"},
            {"Adjust proportionally",         "Axis"},
            {"Nothing",                       "None"},
        };
        for (const auto& c : cases) {
            RecordingGestureModel m;
            QVERIFY(m.setMode(QStringLiteral("down"), QString::fromUtf8(c.mode)));
            QVERIFY(m.calls.size() >= 1);
            QCOMPARE(m.calls.at(0).kind, QStringLiteral("SetGesture"));
            QCOMPARE(m.calls.at(0).type, QString::fromUtf8(c.type));
        }

        // Unknown mode -> rejected, zero dispatch.
        RecordingGestureModel bad;
        QCOMPARE(bad.setMode(QStringLiteral("up"),
                             QStringLiteral("Teleport to Mars")), false);
        QCOMPARE(bad.calls.size(), 0);

        // Unknown direction -> rejected, zero dispatch (the daemon toDirection
        // would throw std::invalid_argument; the GUI must never send garbage).
        RecordingGestureModel baddir;
        QCOMPARE(baddir.setMode(QStringLiteral("diagonal"),
                                QStringLiteral("Repeat while moving")), false);
        QCOMPARE(baddir.calls.size(), 0);
    }

    // (3) PREVIEW SENTENCE (GEST-04): previewSentence recomposes per the UI-SPEC
    // copywriting templates and previewChanged NOTIFY fires on change.
    void test_preview_sentence() {
        RecordingGestureModel m;
        QSignalSpy spy(&m, &GestureModel::previewChanged);

        m.setMode(QStringLiteral("up"), QStringLiteral("Nothing"));
        QVERIFY(spy.count() >= 1);
        // "Nothing" + up -> "Moving up does nothing."
        QCOMPARE(m.previewSentence(), QStringLiteral("Moving up does nothing."));
    }
};

QTEST_MAIN(GestureModelTest)
#include "GestureModelTest.moc"
