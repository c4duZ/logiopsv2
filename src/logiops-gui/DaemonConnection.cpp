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
#include "DaemonConnection.h"

#include "DeviceModel.h"
#include "logid_device_proxy.h"
#include "logid_devices_proxy.h"

#include <QClipboard>
#include <QDBusError>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QGuiApplication>
#include <QVariant>

// Reuse the daemon's identity constants rather than hardcoding (CLAUDE.md /
// CONTEXT.md: reuse ipc_defs.h values).
#include "ipc_defs.h"

namespace logiops_gui {

namespace {
// The daemon's root object path holds the .Devices manager interface.
const QString kService = QStringLiteral(SERVICE_ROOT_NAME);
const QString kRootPath = QStringLiteral("/pizza/pixl/logiops");
} // namespace

DaemonConnection::DaemonConnection(DeviceModel* model, QObject* parent)
    : QObject(parent), _model(model), _bus(QDBusConnection::systemBus()) {
    // VERIFIED A2: QDBusServiceWatcher + WatchForOwnerChange is the canonical,
    // race-free reconnect mechanism. Watch only the daemon's well-known name.
    _watcher = new QDBusServiceWatcher(kService, _bus,
                                       QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(_watcher, &QDBusServiceWatcher::serviceRegistered,
            this, &DaemonConnection::onDaemonUp);
    connect(_watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &DaemonConnection::onDaemonDown);

    // The .Devices manager proxy lives on the root node.
    _devices = new PizzaPixlLogiOpsDevicesInterface(kService, kRootPath, _bus, this);
    connect(_devices, &PizzaPixlLogiOpsDevicesInterface::DeviceAdded,
            this, &DaemonConnection::onDeviceAdded);
    connect(_devices, &PizzaPixlLogiOpsDevicesInterface::DeviceRemoved,
            this, &DaemonConnection::onDeviceRemoved);

    setScreenState(Loading);
    enumerate();
}

DaemonConnection::~DaemonConnection() = default;

void DaemonConnection::setScreenState(ScreenState state) {
    if (_screenState == state)
        return;
    _screenState = state;
    emit screenStateChanged();
}

void DaemonConnection::teardownProxies() {
    for (auto* proxy : std::as_const(_deviceProxies))
        proxy->deleteLater();
    _deviceProxies.clear();
    if (_model)
        _model->clear();
}

void DaemonConnection::recomputePopulation() {
    // Never override an error state — those are owned by enumerate()/onDaemonDown.
    if (_screenState == DaemonDown || _screenState == AccessDenied)
        return;
    setScreenState(_deviceProxies.isEmpty() ? Empty : Populated);
}

void DaemonConnection::enumerate() {
    if (!_devices)
        return;

    auto reply = _devices->Enumerate();
    auto* watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher* w) {
                QDBusPendingReply<QList<QDBusObjectPath>> reply = *w;
                w->deleteLater();

                if (reply.isError()) {
                    const QDBusError err = reply.error();
                    // VERIFIED A3 (against the daemon's group=logiops policy): a
                    // user outside the logiops group is denied send_destination,
                    // which D-Bus returns as AccessDenied. Distinguish that from
                    // a missing owner (daemon not running) so the UI shows the
                    // correct, separate onboarding vs. retry screen.
                    if (err.type() == QDBusError::AccessDenied ||
                        err.name() == QLatin1String("org.freedesktop.DBus.Error.AccessDenied")) {
                        setScreenState(AccessDenied);
                    } else {
                        setScreenState(DaemonDown);
                    }
                    return;
                }

                const QList<QDBusObjectPath> paths = reply.value();
                if (paths.isEmpty()) {
                    setScreenState(Empty);
                    return;
                }
                for (const QDBusObjectPath& path : paths)
                    addDevice(path);
                recomputePopulation();
            });
}

void DaemonConnection::addDevice(const QDBusObjectPath& path) {
    const QString key = path.path();
    if (key.isEmpty() || _deviceProxies.contains(key))
        return; // guard empty/duplicate paths (T-02-09).

    auto* dev = new PizzaPixlLogiOpsDeviceInterface(kService, key, _bus, this);
    _deviceProxies.insert(key, dev);

    // Live sleep/wake -> model. Capture the path string by value (stable key).
    connect(dev, &PizzaPixlLogiOpsDeviceInterface::StatusChanged, this,
            [this, key](bool active) {
                if (_model)
                    _model->onStatusChanged(key, active);
            });

    // Live battery -> model (CONF-03: signal-driven, no polling). The daemon's
    // BatteryChanged(percentage y, charging b, known b) maps to a uchar first
    // arg in the generated proxy; clamp 0..100 in the delegate (T-02-15). This
    // is the LIVE no-poll path; the initial value is read below via GetAll.
    connect(dev, &PizzaPixlLogiOpsDeviceInterface::BatteryChanged, this,
            [this, key](uchar percentage, bool charging, bool known) {
                if (_model)
                    _model->onBatteryChanged(key, static_cast<int>(percentage),
                                             charging, known);
            });

    // Async-read Name / ProductID / Active via Properties.GetAll so a sleeping
    // device never blocks the UI thread (Pitfall 4). The generated property
    // accessors are synchronous, so go through the standard Properties iface.
    auto* propsIface = new QDBusInterface(
        kService, key, QStringLiteral("org.freedesktop.DBus.Properties"), _bus, this);
    auto pending = propsIface->asyncCall(
        QStringLiteral("GetAll"),
        QString::fromLatin1(PizzaPixlLogiOpsDeviceInterface::staticInterfaceName()));
    auto* watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, key, propsIface](QDBusPendingCallWatcher* w) {
                QDBusPendingReply<QVariantMap> reply = *w;
                w->deleteLater();
                propsIface->deleteLater();

                if (reply.isError()) {
                    // A property read failed (device vanished mid-enumerate, or
                    // policy edge): still register the row with best-effort empty
                    // values so it is not silently dropped — the model owns truth.
                    if (_model && _deviceProxies.contains(key)) {
                        _model->onDeviceAdded(key, QString(), 0, true);
                        // No battery info available -> "—" (known=false).
                        _model->onBatteryChanged(key, 0, false, false);
                    }
                    recomputePopulation();
                    return;
                }
                if (!_deviceProxies.contains(key))
                    return; // removed before the reply landed.

                const QVariantMap props = reply.value();
                const QString name = props.value(QStringLiteral("Name")).toString();
                const quint16 pid =
                    static_cast<quint16>(props.value(QStringLiteral("ProductID")).toUInt());
                const bool active = props.value(QStringLiteral("Active"), true).toBool();

                if (_model)
                    _model->onDeviceAdded(key, name, pid, active);

                // Initial battery value from the same GetAll snapshot (no extra
                // round-trip). An older daemon / unsupported device omits the
                // Battery property entirely -> known stays false -> the row shows
                // "—" gracefully rather than a fake 0%. The 'y' uint8 arrives as
                // an unsigned int in the variant map.
                if (_model) {
                    const QVariant battV = props.value(QStringLiteral("Battery"));
                    const bool hasBattery = battV.isValid() &&
                        props.contains(QStringLiteral("BatteryKnown"));
                    if (hasBattery) {
                        const int percent = static_cast<int>(battV.toUInt());
                        const bool charging =
                            props.value(QStringLiteral("Charging")).toBool();
                        const bool known =
                            props.value(QStringLiteral("BatteryKnown")).toBool();
                        _model->onBatteryChanged(key, percent, charging, known);
                    } else {
                        _model->onBatteryChanged(key, 0, false, false);
                    }
                }
                recomputePopulation();
            });
}

void DaemonConnection::removeDevice(const QString& path) {
    if (auto* proxy = _deviceProxies.take(path))
        proxy->deleteLater();
    if (_model)
        _model->onDeviceRemoved(path);
    recomputePopulation();
}

void DaemonConnection::onDeviceAdded(const QDBusObjectPath& path) {
    addDevice(path);
}

void DaemonConnection::onDeviceRemoved(const QDBusObjectPath& path) {
    removeDevice(path.path());
}

void DaemonConnection::onDaemonDown(const QString& service) {
    Q_UNUSED(service)
    // Owner lost: drop everything and flip to the daemon-down screen. The watcher
    // keeps watching, so a later re-acquire fires onDaemonUp automatically.
    teardownProxies();
    setScreenState(DaemonDown);
}

void DaemonConnection::onDaemonUp(const QString& service) {
    Q_UNUSED(service)
    // Owner re-acquired: rebuild from scratch — no app restart (DEV-03/CONF-03).
    teardownProxies();
    setScreenState(Loading);
    enumerate();
}

void DaemonConnection::retry() {
    // "Retry connection" CTA: same as a fresh owner-up. Clear any error state so
    // recomputePopulation/enumerate can resolve it cleanly.
    teardownProxies();
    setScreenState(Loading);
    enumerate();
}

void DaemonConnection::copyUsermodCommand() {
    const QString user = qEnvironmentVariable("USER");
    const QString command =
        QStringLiteral("sudo usermod -aG logiops ") + (user.isEmpty() ? QStringLiteral("$USER") : user);
    if (auto* clipboard = QGuiApplication::clipboard())
        clipboard->setText(command);
}

} // namespace logiops_gui
