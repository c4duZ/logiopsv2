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
#include "DeviceModel.h"

namespace logiops_gui {

DeviceModel::DeviceModel(QObject* parent) : QAbstractListModel(parent) {}

int DeviceModel::rowCount(const QModelIndex& parent) const {
    // Flat list: children of a valid index don't exist.
    if (parent.isValid())
        return 0;
    return static_cast<int>(_rows.size());
}

QVariant DeviceModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= _rows.size())
        return {};

    const Row& row = _rows.at(index.row());
    switch (role) {
    case PathRole:
        return row.path;
    case NameRole:
        return row.name;
    case ModelRole:
        return row.model;
    case DeviceKindRole:
        return row.deviceKind;
    case BatteryPercentRole:
        return row.batteryPercent;
    case BatteryKnownRole:
        return row.batteryKnown;
    case ChargingRole:
        return row.charging;
    case ConnectionStateRole:
        return row.connectionState;
    default:
        return {};
    }
}

QHash<int, QByteArray> DeviceModel::roleNames() const {
    return {
        {PathRole, "path"},
        {NameRole, "name"},
        {ModelRole, "model"},
        {DeviceKindRole, "deviceKind"},
        {BatteryPercentRole, "batteryPercent"},
        {BatteryKnownRole, "batteryKnown"},
        {ChargingRole, "charging"},
        {ConnectionStateRole, "connectionState"},
    };
}

int DeviceModel::kindFromPid(quint16 pid) {
    // Minimal PID -> kind table for v1 (refined in Fase 3, RESEARCH Open Q3).
    // Logitech keyboard PIDs cluster in the 0x40xx-0x42xx ranges alongside mice,
    // so an exhaustive table is deferred; this covers the tested mice and the
    // handful of keyboards known on the bus, defaulting to Mouse for the common
    // mouse-heavy case. Unknown stays available for genuinely unclassified PIDs.
    switch (pid) {
    // Keyboards.
    case 0xb350: // MX Keys
    case 0xb35b: // MX Keys (variant)
    case 0x408a: // MX Keys (receiver)
    case 0x920a: // MX Mechanical
        return Keyboard;
    default:
        // Mice (and the common mouse-dominated default): MX Master 4 (0x4082),
        // MX Master 3/3S, MX Anywhere, etc. — pick the mouse icon.
        return Mouse;
    }
}

int DeviceModel::nicknameOf(const QString& path) {
    // Stable sort key = the trailing integer after the last '/'. The daemon names
    // device nodes /pizza/pixl/logiops/devices/N with N a plain integer string.
    const int slash = path.lastIndexOf(QLatin1Char('/'));
    if (slash < 0 || slash + 1 >= path.size())
        return 0;
    bool ok = false;
    const int n = path.mid(slash + 1).toInt(&ok);
    return ok ? n : 0;
}

int DeviceModel::rowForPath(const QString& path) const {
    for (int i = 0; i < _rows.size(); ++i) {
        if (_rows.at(i).path == path)
            return i;
    }
    return -1; // T-02-09: callers must guard against -1 before indexing _rows.
}

void DeviceModel::onDeviceAdded(const QString& path, const QString& name,
                                quint16 pid, bool active) {
    // Ignore duplicate adds (T-02-09: a re-announced path must not double a row).
    if (rowForPath(path) >= 0)
        return;

    // Stable insertion index: keep rows ordered by the nickname integer so a
    // later status/battery tick never has to reorder (no-flicker contract).
    const int nick = nicknameOf(path);
    int insertAt = static_cast<int>(_rows.size());
    for (int i = 0; i < _rows.size(); ++i) {
        if (nick < nicknameOf(_rows.at(i).path)) {
            insertAt = i;
            break;
        }
    }

    Row row;
    row.path = path;
    row.name = name;
    row.model = name; // sub-line falls back to the name until Fase 3 lookup.
    row.deviceKind = kindFromPid(pid);
    row.batteryPercent = 0;
    row.batteryKnown = false; // unwired here; Plan 05 fills via onBatteryChanged.
    row.charging = false;
    row.connectionState = active ? Online : Sleeping;

    beginInsertRows(QModelIndex(), insertAt, insertAt);
    _rows.insert(insertAt, row);
    endInsertRows();
}

void DeviceModel::onDeviceRemoved(const QString& path) {
    const int row = rowForPath(path);
    if (row < 0)
        return; // genuinely gone or never present — nothing to remove.

    beginRemoveRows(QModelIndex(), row, row);
    _rows.removeAt(row);
    endRemoveRows();
}

void DeviceModel::onStatusChanged(const QString& path, bool active) {
    // StatusChanged carries the daemon's wake/sleep bool (Active): awake -> Online,
    // asleep -> Sleeping (dimmed, stays in list). True disconnect/receiver-lost is
    // a separate Offline state driven via onConnectionStateChanged (UI-SPEC).
    onConnectionStateChanged(path, active ? Online : Sleeping);
}

void DeviceModel::onConnectionStateChanged(const QString& path, int state) {
    const int row = rowForPath(path);
    if (row < 0)
        return;

    if (_rows[row].connectionState == state)
        return; // no change -> no spurious repaint.

    _rows[row].connectionState = state;
    const QModelIndex idx = index(row);
    // No-flicker: ONLY ConnectionStateRole, on the one row. Never reset/resort.
    emit dataChanged(idx, idx, {ConnectionStateRole});
}

void DeviceModel::onBatteryChanged(const QString& path, int percent,
                                   bool charging, bool known) {
    const int row = rowForPath(path);
    if (row < 0)
        return;

    _rows[row].batteryPercent = percent;
    _rows[row].charging = charging;
    _rows[row].batteryKnown = known;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx,
                     {BatteryPercentRole, ChargingRole, BatteryKnownRole});
}

void DeviceModel::clear() {
    // Reconnect path ONLY (CONF-03): the daemon owns the truth, so on a fresh
    // connection we drop every row and re-enumerate from scratch. This is the one
    // place a full reset is allowed — value changes use targeted dataChanged.
    beginResetModel();
    _rows.clear();
    endResetModel();
}

} // namespace logiops_gui
