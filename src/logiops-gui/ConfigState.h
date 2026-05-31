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
#ifndef LOGIOPS_GUI_CONFIGSTATE_H
#define LOGIOPS_GUI_CONFIGSTATE_H

#include <QDBusConnection>
#include <QObject>
#include <QString>

namespace logiops_gui {

/*
 * ConfigState — the global persistence + dirty-tracking home (CONF-01 / CONF-02).
 *
 * This single QObject is the phase's "unsaved changes" brain. It is registered
 * once as a QML context property and:
 *
 *   - Tracks DIRTY. Every feature setter across the Buttons / Pointer / Scroll
 *     tabs (and the Profiles tab) calls markDirty() after applying a change live;
 *     `dirty` flips true and the warning pill appears (CONF-01). A successful
 *     Save() clears it.
 *
 *   - Drives the polkit-gated Save() ASYNC. save() fires .Config.Save over D-Bus
 *     via QDBusPendingCallWatcher. The daemon holds its server_lock through the
 *     polkit prompt (RESEARCH Pitfall 4 / T-3-04-02), so the GUI MUST NOT block:
 *     while pending, `saving` is true (the toolbar shows a BusyIndicator + disables
 *     the button). On success: dirty=false, saved() emitted (the transient "Saved"
 *     tick). On error, `lastError` is set to the mapped UI-SPEC copy:
 *       AccessDenied / "Not authorized" -> the auth-declined string (T-3-04-03:
 *         dirty STAYS true so the user knows the change is applied-live but unsaved)
 *       ServiceUnknown / NoReply        -> the daemon-down string.
 *
 *   - Restores defaults (CONF-02). restoreDefaults(profile) calls
 *     .Device.ClearProfile async on the currently-targeted device (T-3-04-04: the
 *     QML gates this behind a confirmation Dialog) and marks dirty (the reset is an
 *     applied-but-unsaved change until the user Saves).
 *
 * The active device path (for ClearProfile) is set by the QML shell on selection
 * via setDevicePath(); Save() targets the daemon's single root Config node.
 *
 * Test seam (no live bus): save()/restoreDefaults route through the virtual
 * performSave() / performClearProfile() hooks; a test subclass simulates the daemon
 * reply by calling the protected onSaveReplied(error,message) / onClearReplied()
 * so the dirty/saving/lastError transitions + the two error-string mappings are
 * asserted with no D-Bus. The live overrides issue the async calls and feed their
 * replies into the same onSaveReplied/onClearReplied sinks.
 */
class ConfigState : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    Q_PROPERTY(bool saving READ saving NOTIFY savingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    // The two UI-SPEC Save error strings (Copywriting table). Exposed as static
    // so the QML SaveToolbar and the unit test reference the SAME canonical copy.
    [[nodiscard]] static QString authDeclinedError();
    [[nodiscard]] static QString daemonDownError();

    // Live constructor: the bus the GUI is on (root Config node is well-known).
    explicit ConfigState(const QDBusConnection& bus, QObject* parent = nullptr);
    // Test constructor: no bus, no async — drive via save()/restoreDefaults +
    // onSaveReplied/onClearReplied.
    explicit ConfigState(QObject* parent = nullptr);
    ~ConfigState() override;

    [[nodiscard]] bool dirty() const { return _dirty; }
    [[nodiscard]] bool saving() const { return _saving; }
    [[nodiscard]] QString lastError() const { return _lastError; }

    // The device the Restore-defaults call targets (.Device.ClearProfile). Set by
    // the QML shell on selection; empty disables restore.
    Q_INVOKABLE void setDevicePath(const QString& path) { _devicePath = path; }
    [[nodiscard]] QString devicePath() const { return _devicePath; }

    // Called by EVERY feature setter across the tabs after a live-apply (CONF-01).
    Q_INVOKABLE void markDirty();

    // Persist to disk via the polkit-gated .Config.Save (async). No-op while a Save
    // is already pending (no retry-storm, T-3-04-02).
    Q_INVOKABLE void save();

    // Restore the named profile to defaults via .Device.ClearProfile (CONF-02).
    // The QML gates this behind a confirmation Dialog (T-3-04-04). Marks dirty.
    Q_INVOKABLE void restoreDefaults(const QString& profile);

signals:
    void dirtyChanged();
    void savingChanged();
    void lastErrorChanged();
    // The transient "Saved" tick driver (UI-SPEC): fired once on a successful Save.
    void saved();

protected:
    // Live D-Bus hooks (overridable for the no-bus test). performSave issues
    // .Config.Save async and routes its reply into onSaveReplied; performClearProfile
    // issues .Device.ClearProfile async and routes into onClearReplied.
    virtual void performSave();
    virtual void performClearProfile(const QString& profile);

    // Reply sinks (the test calls these directly to simulate the daemon). isError +
    // the D-Bus error name/message drive the success/clear-dirty vs error-string
    // mapping.
    void onSaveReplied(bool isError, const QString& errorName, const QString& errorMessage);
    void onClearReplied(bool isError);

    void setSaving(bool saving);
    void setLastError(const QString& error);

private:
    bool _dirty = false;
    bool _saving = false;
    QString _lastError;
    QString _devicePath;

    QDBusConnection _bus;
    bool _live = false;
};

} // namespace logiops_gui

#endif // LOGIOPS_GUI_CONFIGSTATE_H
