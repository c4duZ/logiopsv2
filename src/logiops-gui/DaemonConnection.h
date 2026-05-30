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
#ifndef LOGIOPS_GUI_DAEMONCONNECTION_H
#define LOGIOPS_GUI_DAEMONCONNECTION_H

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QMap>
#include <QObject>
#include <QString>

class PizzaPixlLogiOpsDevicesInterface;
class PizzaPixlLogiOpsDeviceInterface;
class QDBusServiceWatcher;

namespace logiops_gui {

class DeviceModel;

/*
 * DaemonConnection — the live wiring between the daemon's D-Bus surface and the
 * DeviceModel (Phase 2, Plan 04).
 *
 * Owns the connection to pizza.pixl.LogiOps on the SYSTEM bus (CLAUDE.md:
 * USE_USER_BUS default OFF; never session), the .Devices manager proxy, a
 * per-device .Device proxy map, and a QDBusServiceWatcher that auto-reconnects
 * on NameOwnerChanged (DEV-03 / CONF-03 — no app restart). It exposes a C++
 * ScreenState enum as a Q_PROPERTY so QML renders the four distinct full-window
 * states (loading / empty / daemon-down / access-denied) without inferring any
 * of them in JS (CONTEXT.md: QML renders only).
 *
 * Flow (RESEARCH Pattern 1, "manager emits paths, client follows"):
 *   Enumerate() -> QList<QDBusObjectPath> -> addDevice(path) for each
 *   DeviceAdded(o) / DeviceRemoved(o) -> add / remove the row + proxy
 *   per-device StatusChanged(b) -> DeviceModel::onStatusChanged
 * All D-Bus calls are async (QDBusPendingCallWatcher) so a slow/asleep device
 * never freezes the UI thread (RESEARCH Pitfall 4).
 *
 * Screen-state distinction (RESEARCH A3, VERIFIED against the daemon policy):
 *   - Enumerate error QDBusError::AccessDenied  -> AccessDenied (not in group)
 *   - Enumerate error ServiceUnknown / no owner -> DaemonDown
 *   - success, 0 paths -> Empty ; >= 1 -> Populated
 */
class DaemonConnection final : public QObject {
    Q_OBJECT

public:
    enum ScreenState { Loading, Populated, Empty, DaemonDown, AccessDenied };
    Q_ENUM(ScreenState)

    Q_PROPERTY(ScreenState screenState READ screenState NOTIFY screenStateChanged)

    // The model is created/owned by main.cpp and injected so both can be
    // registered as QML context properties from one place.
    explicit DaemonConnection(DeviceModel* model, QObject* parent = nullptr);
    ~DaemonConnection() override;

    [[nodiscard]] ScreenState screenState() const { return _screenState; }

    // "Retry connection" CTA (daemon-down state): tear down stale proxies, clear
    // the model, and re-enumerate from scratch. Same path as an owner re-acquire.
    Q_INVOKABLE void retry();
    // "Copy command" CTA (access-denied onboarding): copy the usermod guidance to
    // the clipboard so the user can paste it into a terminal.
    Q_INVOKABLE void copyUsermodCommand();

signals:
    void screenStateChanged();

private slots:
    // QDBusServiceWatcher: the daemon acquired / lost the well-known name.
    void onDaemonUp(const QString& service);
    void onDaemonDown(const QString& service);
    // .Devices manager hotplug signals.
    void onDeviceAdded(const QDBusObjectPath& path);
    void onDeviceRemoved(const QDBusObjectPath& path);

private:
    void setScreenState(ScreenState state);
    // Async Enumerate(); maps the reply (or its error) to a ScreenState and
    // populates the model. Called on construction, on retry(), and on owner-up.
    void enumerate();
    // Construct a per-device proxy, async-read Name/ProductID/Active, push the
    // row into the model, and subscribe StatusChanged.
    void addDevice(const QDBusObjectPath& path);
    void removeDevice(const QString& path);
    // Drop every per-device proxy and clear the model (reconnect / daemon-down).
    void teardownProxies();
    // Recompute Empty vs Populated after an add/remove (never overrides the
    // DaemonDown / AccessDenied error states).
    void recomputePopulation();

    DeviceModel* _model;                              // injected, not owned.
    QDBusConnection _bus;                             // the system bus.
    PizzaPixlLogiOpsDevicesInterface* _devices = nullptr;
    QDBusServiceWatcher* _watcher = nullptr;
    QMap<QString, PizzaPixlLogiOpsDeviceInterface*> _deviceProxies;
    ScreenState _screenState = Loading;
};

} // namespace logiops_gui

#endif // LOGIOPS_GUI_DAEMONCONNECTION_H
