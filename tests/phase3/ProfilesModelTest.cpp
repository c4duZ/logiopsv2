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
 * ProfilesModel + ConfigState unit (Phase 3, Wave 4) — the Profiles-tab + global
 * persistence bridge (PROF-01 / CONF-01 / CONF-02). Driven through the test
 * constructors + seed seams with NO live bus (Phase 2 DeviceModelTest precedent).
 *
 * RecordingProfilesModel overrides the daemon-write hooks (performSetProfile /
 * performRemoveProfile) so the SetProfile/RemoveProfile call ORDER + args are
 * asserted with no bus. RecordingConfigState overrides performSave /
 * performClearProfile and lets the test feed simulated daemon replies into
 * onSaveReplied / onClearReplied, exercising the dirty/saving/lastError transitions
 * and the two UI-SPEC error-string mappings.
 *
 * Asserts:
 *   (a) createProfile issues SetProfile (creates-if-absent + switches),
 *   (b) markDirty sets dirty=true and a SUCCESSFUL save clears it + fires saved(),
 *   (c) an AccessDenied save reply maps to the auth-declined copy (dirty STAYS),
 *       a ServiceUnknown reply maps to the daemon-down copy,
 *   (d) restoreDefaults issues ClearProfile (and marks dirty on success).
 */

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QStringList>
#include <QVector>

#include "ConfigState.h"
#include "ProfilesModel.h"

using logiops_gui::ConfigState;
using logiops_gui::ProfilesModel;

namespace {

// Records the daemon-write calls so the test can assert create/switch/remove
// dispatch without a live bus.
class RecordingProfilesModel : public ProfilesModel {
public:
    using ProfilesModel::ProfilesModel;

    struct Call {
        QString method; // "SetProfile" or "RemoveProfile"
        QString arg;
    };
    QVector<Call> calls;

protected:
    void performSetProfile(const QString& name) override {
        calls.push_back({QStringLiteral("SetProfile"), name});
    }
    void performRemoveProfile(const QString& name) override {
        calls.push_back({QStringLiteral("RemoveProfile"), name});
    }
};

// Captures Save / ClearProfile dispatch and lets the test drive the simulated
// daemon reply into the protected reply sinks.
class RecordingConfigState : public ConfigState {
public:
    using ConfigState::ConfigState;

    int saveCalls = 0;
    QVector<QString> clearCalls;

    // Re-expose the protected reply sinks for the test to simulate the daemon.
    using ConfigState::onClearReplied;
    using ConfigState::onSaveReplied;

protected:
    void performSave() override { ++saveCalls; }
    void performClearProfile(const QString& profile) override {
        clearCalls.push_back(profile);
    }
};

} // namespace

class ProfilesModelTest : public QObject {
    Q_OBJECT

private slots:
    // ----- ProfilesModel (PROF-01) -----

    // Roles populate from the injected GetProfiles snapshot; active/default flagged.
    void test_roles_populate_from_seed() {
        ProfilesModel m;
        m.seedProfiles(QStringList{QStringLiteral("default"), QStringLiteral("Gaming")},
                       QStringLiteral("Gaming"), QStringLiteral("default"));
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(m.data(m.index(0), ProfilesModel::NameRole).toString(),
                 QStringLiteral("default"));
        QVERIFY(m.data(m.index(0), ProfilesModel::IsDefaultRole).toBool());
        QVERIFY(!m.data(m.index(0), ProfilesModel::IsActiveRole).toBool());
        QVERIFY(m.data(m.index(1), ProfilesModel::IsActiveRole).toBool());
        QCOMPARE(m.activeProfile(), QStringLiteral("Gaming"));
    }

    // (a) createProfile issues SetProfile (creates-if-absent + switches) and adds
    // the optimistic row + active flag.
    void test_create_profile_issues_setprofile() {
        RecordingProfilesModel m;
        m.seedProfiles(QStringList{QStringLiteral("default")},
                       QStringLiteral("default"), QStringLiteral("default"));
        m.calls.clear();

        QSignalSpy activeSpy(&m, &ProfilesModel::activeProfileChanged);
        m.createProfile(QStringLiteral("Work"));

        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(m.calls.size(), 1);
        QCOMPARE(m.calls.at(0).method, QStringLiteral("SetProfile"));
        QCOMPARE(m.calls.at(0).arg, QStringLiteral("Work"));
        QCOMPARE(m.activeProfile(), QStringLiteral("Work"));
        QVERIFY(activeSpy.count() >= 1);
    }

    // switchProfile on an existing name issues SetProfile + moves the active pill.
    void test_switch_profile_issues_setprofile() {
        RecordingProfilesModel m;
        m.seedProfiles(QStringList{QStringLiteral("default"), QStringLiteral("Gaming")},
                       QStringLiteral("default"), QStringLiteral("default"));
        m.calls.clear();

        m.switchProfile(QStringLiteral("Gaming"));
        QCOMPARE(m.activeProfile(), QStringLiteral("Gaming"));
        QCOMPARE(m.calls.size(), 1);
        QCOMPARE(m.calls.at(0).method, QStringLiteral("SetProfile"));
        QCOMPARE(m.calls.at(0).arg, QStringLiteral("Gaming"));
    }

    // removeProfile issues RemoveProfile, but the LAST remaining profile is kept.
    void test_remove_profile() {
        RecordingProfilesModel m;
        m.seedProfiles(QStringList{QStringLiteral("default"), QStringLiteral("Gaming")},
                       QStringLiteral("default"), QStringLiteral("default"));
        m.calls.clear();

        m.removeProfile(QStringLiteral("Gaming"));
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.calls.size(), 1);
        QCOMPARE(m.calls.at(0).method, QStringLiteral("RemoveProfile"));
        QCOMPARE(m.calls.at(0).arg, QStringLiteral("Gaming"));

        // The last remaining profile is never removed.
        m.calls.clear();
        m.removeProfile(QStringLiteral("default"));
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.calls.size(), 0);
    }

    // rename = SetProfile(new) then RemoveProfile(old) (no daemon rename primitive).
    void test_rename_profile() {
        RecordingProfilesModel m;
        m.seedProfiles(QStringList{QStringLiteral("default"), QStringLiteral("Gaming")},
                       QStringLiteral("Gaming"), QStringLiteral("default"));
        m.calls.clear();

        m.renameProfile(QStringLiteral("Gaming"), QStringLiteral("Esports"));
        QVERIFY(m.profiles().contains(QStringLiteral("Esports")));
        QVERIFY(!m.profiles().contains(QStringLiteral("Gaming")));
        QCOMPARE(m.activeProfile(), QStringLiteral("Esports"));
        QCOMPARE(m.calls.size(), 2);
        QCOMPARE(m.calls.at(0).method, QStringLiteral("SetProfile"));
        QCOMPARE(m.calls.at(0).arg, QStringLiteral("Esports"));
        QCOMPARE(m.calls.at(1).method, QStringLiteral("RemoveProfile"));
        QCOMPARE(m.calls.at(1).arg, QStringLiteral("Gaming"));
    }

    // ----- ConfigState (CONF-01) -----

    // (b) markDirty sets dirty=true; a SUCCESSFUL save clears it + fires saved().
    void test_markdirty_then_successful_save_clears() {
        RecordingConfigState s;
        QSignalSpy dirtySpy(&s, &ConfigState::dirtyChanged);
        QSignalSpy savingSpy(&s, &ConfigState::savingChanged);
        QSignalSpy savedSpy(&s, &ConfigState::saved);

        QVERIFY(!s.dirty());
        s.markDirty();
        QVERIFY(s.dirty());
        QCOMPARE(dirtySpy.count(), 1);

        s.save();
        QVERIFY(s.saving());          // pending -> BusyIndicator
        QCOMPARE(s.saveCalls, 1);

        // Simulate the daemon's success reply.
        s.onSaveReplied(false, QString(), QString());
        QVERIFY(!s.saving());
        QVERIFY(!s.dirty());          // success clears dirty
        QVERIFY(s.lastError().isEmpty());
        QCOMPARE(savedSpy.count(), 1); // transient "Saved" tick
        QVERIFY(savingSpy.count() >= 2);
    }

    // A second save() while one is pending is ignored (no retry-storm, T-3-04-02).
    void test_save_no_retry_storm_while_pending() {
        RecordingConfigState s;
        s.markDirty();
        s.save();
        s.save(); // ignored while saving
        QCOMPARE(s.saveCalls, 1);
        s.onSaveReplied(false, QString(), QString());
    }

    // (c) AccessDenied -> auth-declined copy (dirty STAYS); ServiceUnknown ->
    // daemon-down copy. Both leave the change applied-live (dirty true).
    void test_save_error_mapping() {
        RecordingConfigState s;

        // polkit denied.
        s.markDirty();
        s.save();
        s.onSaveReplied(true, QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"),
                        QStringLiteral("Not authorized to perform operation"));
        QVERIFY(!s.saving());
        QVERIFY(s.dirty()); // T-3-04-03: a denied save must NOT pretend it persisted
        QCOMPARE(s.lastError(), ConfigState::authDeclinedError());

        // daemon down / no service.
        s.save();
        s.onSaveReplied(true, QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown"),
                        QStringLiteral("The name pizza.pixl.LogiOps was not provided"));
        QVERIFY(s.dirty());
        QCOMPARE(s.lastError(), ConfigState::daemonDownError());

        // The two strings are distinct (UI-SPEC's two error paths).
        QVERIFY(ConfigState::authDeclinedError() != ConfigState::daemonDownError());
    }

    // markDirty clears a stale error banner (a fresh change supersedes it).
    void test_markdirty_clears_stale_error() {
        RecordingConfigState s;
        s.save();
        s.onSaveReplied(true, QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown"),
                        QString());
        QVERIFY(!s.lastError().isEmpty());
        s.markDirty();
        QVERIFY(s.lastError().isEmpty());
    }

    // ----- ConfigState restore (CONF-02) -----

    // (d) restoreDefaults issues ClearProfile (on the active device) + marks dirty
    // on the simulated success reply.
    void test_restore_defaults_issues_clearprofile() {
        RecordingConfigState s;
        s.setDevicePath(QStringLiteral("/pizza/pixl/logiops/devices/0"));

        s.restoreDefaults(QStringLiteral("default"));
        QCOMPARE(s.clearCalls.size(), 1);
        QCOMPARE(s.clearCalls.at(0), QStringLiteral("default"));

        QVERIFY(!s.dirty());
        s.onClearReplied(false); // simulate the daemon's success.
        QVERIFY(s.dirty()); // the reset is an applied-but-unsaved change.
    }

    // restoreDefaults with no device selected dispatches nothing.
    void test_restore_defaults_no_device_noop() {
        RecordingConfigState s;
        s.restoreDefaults(QStringLiteral("default"));
        QCOMPARE(s.clearCalls.size(), 0);
    }

    // ----- WR-03: CONF-01 dirty-wiring (a profile mutation flips the pill) -----

    // A profile setter on a ConfigState-wired model marks the global config dirty,
    // and a successful Save clears it (the "Unsaved changes" pill lifecycle).
    void test_profile_mutation_marks_dirty() {
        RecordingConfigState s;
        QVERIFY(!s.dirty());

        RecordingProfilesModel m;
        m.setConfigState(&s);
        m.seedProfiles(QStringList{QStringLiteral("default")},
                       QStringLiteral("default"), QStringLiteral("default"));

        // create -> dirty.
        m.createProfile(QStringLiteral("Work"));
        QVERIFY(s.dirty());

        // A successful save clears it.
        s.save();
        s.onSaveReplied(false, QString(), QString());
        QVERIFY(!s.dirty());

        // switch -> dirty again.
        m.switchProfile(QStringLiteral("default"));
        QVERIFY(s.dirty());
    }

    // An unwired model (no ConfigState injected) never touches dirty — the test/
    // headless path stays a no-op (markDirty guards on the null pointer).
    void test_profile_mutation_no_configstate_is_noop() {
        RecordingProfilesModel m;
        m.seedProfiles(QStringList{QStringLiteral("default")},
                       QStringLiteral("default"), QStringLiteral("default"));
        // No setConfigState() — must not crash.
        m.createProfile(QStringLiteral("Work"));
        QCOMPARE(m.rowCount(), 2);
    }

    // ----- WR-05: rename dispatch ORDER (SetProfile(new) before RemoveProfile(old)) -----

    // Even routed through performRenameProfile, the no-bus default preserves the
    // create-then-remove order the recording subclass observes (the live override
    // additionally SEQUENCES the remove behind the create's confirmed reply).
    void test_rename_sequences_create_before_remove() {
        RecordingProfilesModel m;
        m.seedProfiles(QStringList{QStringLiteral("default"), QStringLiteral("Gaming")},
                       QStringLiteral("Gaming"), QStringLiteral("default"));
        m.calls.clear();

        m.renameProfile(QStringLiteral("Gaming"), QStringLiteral("Esports"));
        QCOMPARE(m.calls.size(), 2);
        // SetProfile(new) MUST come first; RemoveProfile(old) only after.
        QCOMPARE(m.calls.at(0).method, QStringLiteral("SetProfile"));
        QCOMPARE(m.calls.at(0).arg, QStringLiteral("Esports"));
        QCOMPARE(m.calls.at(1).method, QStringLiteral("RemoveProfile"));
        QCOMPARE(m.calls.at(1).arg, QStringLiteral("Gaming"));
    }
};

QTEST_MAIN(ProfilesModelTest)
#include "ProfilesModelTest.moc"
