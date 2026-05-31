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
#include "ConfigState.h"

#include <QDBusError>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include "logid_config_proxy.h"
#include "logid_device_proxy.h"

namespace logiops_gui {

namespace {
    const QString kService = QStringLiteral("pizza.pixl.LogiOps");
    // The daemon's root object node hosts the .Config interface (Save). ipcgull
    // derives the object path from the bus name (SERVICE_ROOT_NAME ->
    // /pizza/pixl/logiops); VERIFIED in 03-RESEARCH (.Config @ /pizza/pixl/logiops)
    // and the dbus/logid_config.xml header.
    const QString kRootPath = QStringLiteral("/pizza/pixl/logiops");

    // Map a D-Bus reply error to one of the two UI-SPEC Save error strings. polkit
    // denial surfaces as AccessDenied / a "Not authorized" message; a missing or
    // unresponsive daemon surfaces as ServiceUnknown / NoReply.
    bool isAuthDenied(const QString& name, const QString& message) {
        return name.contains(QLatin1String("AccessDenied"), Qt::CaseInsensitive) ||
               name.contains(QLatin1String("AuthFailed"), Qt::CaseInsensitive) ||
               message.contains(QLatin1String("not authorized"), Qt::CaseInsensitive) ||
               message.contains(QLatin1String("authoriz"), Qt::CaseInsensitive);
    }
}

QString ConfigState::authDeclinedError() {
    // UI-SPEC Copywriting: Save error (polkit denied).
    return tr("Couldn't save — authorization was declined. Your changes are still "
              "applied live; click Save to try again.");
}

QString ConfigState::daemonDownError() {
    // UI-SPEC Copywriting: Save error (daemon/D-Bus failure).
    return tr("Couldn't save to disk. The daemon may be unavailable — check that "
              "logid is running, then try again.");
}

ConfigState::ConfigState(const QDBusConnection& bus, QObject* parent)
    : QObject(parent), _bus(bus), _live(true) {}

ConfigState::ConfigState(QObject* parent)
    : QObject(parent), _bus(QDBusConnection(QString())), _live(false) {}

ConfigState::~ConfigState() = default;

void ConfigState::markDirty() {
    setLastError(QString()); // a fresh change clears any stale error banner.
    if (_dirty)
        return;
    _dirty = true;
    emit dirtyChanged();
}

void ConfigState::setSaving(bool saving) {
    if (_saving == saving)
        return;
    _saving = saving;
    emit savingChanged();
}

void ConfigState::setLastError(const QString& error) {
    if (_lastError == error)
        return;
    _lastError = error;
    emit lastErrorChanged();
}

void ConfigState::save() {
    // No retry-storm: ignore a Save while one is already pending (T-3-04-02).
    if (_saving)
        return;
    setLastError(QString());
    setSaving(true);
    performSave();
}

void ConfigState::onSaveReplied(bool isError, const QString& errorName,
                                const QString& errorMessage) {
    setSaving(false);
    if (!isError) {
        // Success: clear dirty + fire the transient "Saved" tick (UI-SPEC).
        setLastError(QString());
        if (_dirty) {
            _dirty = false;
            emit dirtyChanged();
        }
        emit saved();
        return;
    }
    // Error: map to one of the two UI-SPEC strings. dirty STAYS set (T-3-04-03 —
    // the change is applied live but not persisted; do not pretend it saved).
    setLastError(isAuthDenied(errorName, errorMessage) ? authDeclinedError()
                                                       : daemonDownError());
}

void ConfigState::restoreDefaults(const QString& profile) {
    // CONF-02: reset the named profile to defaults (.Device.ClearProfile). The QML
    // gates this behind a confirmation Dialog (T-3-04-04).
    if (_devicePath.isEmpty())
        return;
    performClearProfile(profile);
}

void ConfigState::onClearReplied(bool isError) {
    // The reset is an applied-but-unsaved change: mark dirty so the user can Save
    // it (or restart to revert). A failed clear leaves state untouched.
    if (!isError)
        markDirty();
}

// ---------------------------------------------------------------------------
// Live D-Bus hooks. Both async (QDBusPendingCallWatcher) so the UI never blocks
// through the daemon's polkit prompt (Pitfall 4).
// ---------------------------------------------------------------------------

void ConfigState::performSave() {
    if (!_live) {
        // Headless guard: with no bus, treat as an immediate daemon-down error so
        // the test/no-bus path is deterministic. (Tests normally override this.)
        onSaveReplied(true, QStringLiteral("org.freedesktop.DBus.Error.ServiceUnknown"),
                      QString());
        return;
    }
    auto* cfg = new PizzaPixlLogiOpsConfigInterface(kService, kRootPath, _bus, this);
    auto* w = new QDBusPendingCallWatcher(cfg->Save(), this);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, cfg](QDBusPendingCallWatcher* watcher) {
                QDBusPendingReply<> r = *watcher;
                const QDBusError err = r.error();
                onSaveReplied(r.isError(), err.name(), err.message());
                watcher->deleteLater();
                cfg->deleteLater();
            });
}

void ConfigState::performClearProfile(const QString& profile) {
    if (!_live)
        return;
    auto* dev = new PizzaPixlLogiOpsDeviceInterface(kService, _devicePath, _bus, this);
    auto* w = new QDBusPendingCallWatcher(dev->ClearProfile(profile), this);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, dev](QDBusPendingCallWatcher* watcher) {
                QDBusPendingReply<> r = *watcher;
                onClearReplied(r.isError());
                watcher->deleteLater();
                dev->deleteLater();
            });
}

} // namespace logiops_gui
