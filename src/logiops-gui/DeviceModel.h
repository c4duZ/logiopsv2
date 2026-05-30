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
#ifndef LOGIOPS_GUI_DEVICEMODEL_H
#define LOGIOPS_GUI_DEVICEMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

namespace logiops_gui {

/*
 * DeviceModel — the C++<->QML bridge for the device sidebar (Phase 2).
 *
 * A QAbstractListModel whose rows come ONLY from the daemon's Enumerate +
 * DeviceAdded / DeviceRemoved / StatusChanged (and the future BatteryChanged)
 * signals (CONF-03: the daemon is the single source of truth; this model never
 * caches or persists a list independent of those signals). QML binds each role
 * independently so a single field repaints without touching the whole row.
 *
 * No-flicker contract (UI-SPEC): a status / battery change emits a targeted
 * dataChanged for ONLY the affected role(s) on the one changed row — never
 * beginResetModel, never a re-sort. Rows are sorted once on insert by a stable
 * key (the trailing /devices/N nickname integer), so changes never reorder.
 *
 * The live D-Bus subscription (Enumerate + connecting the generated proxy
 * signals to these slots) is finalized in Plan 04's DaemonConnection; this class
 * is driven directly through its public slots, which is exactly what the
 * DeviceModelTest unit exercises (no live bus required).
 */
class DeviceModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        NameRole,
        ModelRole,
        DeviceKindRole,
        BatteryPercentRole,
        BatteryKnownRole,
        ChargingRole,
        ConnectionStateRole,
    };
    Q_ENUM(Roles)

    // DeviceKindRole values (icon selection).
    enum DeviceKind { Mouse = 0, Keyboard = 1, UnknownKind = 2 };
    Q_ENUM(DeviceKind)

    // ConnectionStateRole values (DEV-03).
    enum ConnectionState { Online = 0, Sleeping = 1, Offline = 2, UnknownState = 3 };
    Q_ENUM(ConnectionState)

    explicit DeviceModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    // PID -> device kind heuristic. Minimal table + name-less fallback for v1
    // (refined in Fase 3 per RESEARCH Open Question 3).
    [[nodiscard]] static int kindFromPid(quint16 pid);

public slots:
    // Insert a device row (from Enumerate or DeviceAdded). Duplicate paths are
    // ignored. Insertion keeps a stable sort by the trailing nickname integer.
    void onDeviceAdded(const QString& path, const QString& name, quint16 pid, bool active);
    // Remove the one row matching path (from DeviceRemoved). No-op if absent.
    void onDeviceRemoved(const QString& path);
    // Sleep/wake (StatusChanged). Targeted dataChanged for ConnectionStateRole
    // only — no reset, no reorder.
    void onStatusChanged(const QString& path, bool active);
    // Connection-loss / dim-in-place (UI-SPEC: connection loss = Offline, NOT
    // removal; only a genuine unpair/unplug drops the row via onDeviceRemoved).
    // Sets an explicit ConnectionState and emits the same targeted, no-flicker
    // dataChanged({ConnectionStateRole}) on the one row. No-op if absent.
    void onConnectionStateChanged(const QString& path, int state);
    // Battery update (BatteryChanged — wired to the daemon signal in Plan 05).
    // Targeted dataChanged for the battery roles only.
    void onBatteryChanged(const QString& path, int percent, bool charging, bool known);
    // Reconnect path: drop every row (the only place a full reset is allowed).
    void clear();

private:
    struct Row {
        QString path;
        QString name;
        QString model;
        int deviceKind;
        int batteryPercent;
        bool batteryKnown;
        bool charging;
        int connectionState;
    };

    // Stable lookup; returns -1 when the path is absent (guards malformed/empty
    // paths from the bus — T-02-09: never index _rows without a found row).
    [[nodiscard]] int rowForPath(const QString& path) const;
    // Parse the trailing /devices/N nickname as the stable sort key.
    [[nodiscard]] static int nicknameOf(const QString& path);

    // These rows ARE the only state — never persisted, only mutated by the slots.
    QVector<Row> _rows;
};

} // namespace logiops_gui

#endif // LOGIOPS_GUI_DEVICEMODEL_H
