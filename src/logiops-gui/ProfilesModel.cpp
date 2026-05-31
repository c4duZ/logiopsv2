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
#include "ProfilesModel.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusVariant>

#include "ConfigState.h"
#include "logid_device_proxy.h"

namespace logiops_gui {

namespace {
    // The daemon's well-known bus name (mirrors ipc_defs SERVICE_ROOT_NAME).
    const QString kService = QStringLiteral("pizza.pixl.LogiOps");
}

ProfilesModel::ProfilesModel(const QString& devicePath, const QDBusConnection& bus,
                             QObject* parent)
    : QAbstractListModel(parent), _devicePath(devicePath), _bus(bus), _live(true) {
    refresh();
}

ProfilesModel::ProfilesModel(QObject* parent)
    : QAbstractListModel(parent), _bus(QDBusConnection(QString())), _live(false) {}

ProfilesModel::~ProfilesModel() = default;

int ProfilesModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return _names.size();
}

QVariant ProfilesModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= _names.size())
        return {};
    const QString& name = _names.at(index.row());
    switch (role) {
        case NameRole:      return name;
        case IsActiveRole:  return name == _active;
        case IsDefaultRole: return name == _default;
        default:            return {};
    }
}

QHash<int, QByteArray> ProfilesModel::roleNames() const {
    return {
        {NameRole,      "name"},
        {IsActiveRole,  "isActive"},
        {IsDefaultRole, "isDefault"},
    };
}

void ProfilesModel::seedProfiles(const QStringList& names, const QString& active,
                                 const QString& def) {
    beginResetModel();
    _names = names;
    _default = def;
    endResetModel();
    setActive(active);
}

void ProfilesModel::setActive(const QString& name) {
    if (_active == name)
        return;
    _active = name;
    emit activeProfileChanged();
    // The IsActiveRole of every row may have moved; cheapest correct notify is a
    // whole-column dataChanged (the list is tiny).
    if (!_names.isEmpty())
        emit dataChanged(index(0), index(_names.size() - 1), {IsActiveRole});
}

// ---------------------------------------------------------------------------
// Mutators. createProfile / switchProfile both map to SetProfile (creates-if-
// absent + switches); rename is create-new + switch + drop-old (no daemon rename).
// ---------------------------------------------------------------------------

void ProfilesModel::markDirty() const {
    // CONF-01 (WR-03): a profile mutation is an applied-but-unsaved change.
    if (_configState != nullptr)
        _configState->markDirty();
}

void ProfilesModel::createProfile(const QString& name) {
    if (name.isEmpty())
        return;
    // Optimistic: add the row if new and mark it active immediately (live-apply).
    if (!_names.contains(name)) {
        beginInsertRows(QModelIndex(), _names.size(), _names.size());
        _names.append(name);
        endInsertRows();
    }
    setActive(name);
    performSetProfile(name);
    markDirty();
}

void ProfilesModel::switchProfile(const QString& name) {
    if (name.isEmpty() || !_names.contains(name))
        return;
    setActive(name);
    performSetProfile(name);
    markDirty();
}

void ProfilesModel::removeProfile(const QString& name) {
    // Refuse to remove the last remaining profile (a device must keep at least
    // one — the daemon's default profile).
    const int row = _names.indexOf(name);
    if (row < 0 || _names.size() <= 1)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    _names.removeAt(row);
    endRemoveRows();
    performRemoveProfile(name);
    // If we just removed the active profile, fall back to the first remaining one
    // (and switch the hardware to it).
    if (_active == name && !_names.isEmpty()) {
        const QString next = _names.first();
        setActive(next);
        performSetProfile(next);
    }
    markDirty();
}

void ProfilesModel::renameProfile(const QString& oldName, const QString& newName) {
    if (oldName.isEmpty() || newName.isEmpty() || oldName == newName)
        return;
    if (!_names.contains(oldName) || _names.contains(newName))
        return;
    const int row = _names.indexOf(oldName);
    // Create-new + switch, then drop-old (the daemon has no rename primitive).
    _names[row] = newName;
    emit dataChanged(index(row), index(row), {NameRole});
    const bool wasActive = (_active == oldName);
    if (wasActive)
        setActive(newName);
    // SEQUENCED create-then-remove (WR-05): the live override only fires
    // RemoveProfile(oldName) after SetProfile(newName) succeeds, so a failed
    // create can never leave the device with neither the old nor the new profile.
    performRenameProfile(oldName, newName);
    markDirty();
}

// ---------------------------------------------------------------------------
// Live daemon-write hooks + refresh. The test subclass overrides these.
// ---------------------------------------------------------------------------

void ProfilesModel::performSetProfile(const QString& name) {
    if (!_live)
        return;
    auto* dev = new PizzaPixlLogiOpsDeviceInterface(kService, _devicePath, _bus, this);
    auto* w = new QDBusPendingCallWatcher(dev->SetProfile(name), this);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, dev](QDBusPendingCallWatcher* watcher) {
                watcher->deleteLater();
                dev->deleteLater();
                refresh();
            });
}

void ProfilesModel::performRemoveProfile(const QString& name) {
    if (!_live)
        return;
    auto* dev = new PizzaPixlLogiOpsDeviceInterface(kService, _devicePath, _bus, this);
    auto* w = new QDBusPendingCallWatcher(dev->RemoveProfile(name), this);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, dev](QDBusPendingCallWatcher* watcher) {
                watcher->deleteLater();
                dev->deleteLater();
                refresh();
            });
}

void ProfilesModel::performRenameProfile(const QString& oldName, const QString& newName) {
    if (!_live) {
        // No-bus default (tests override this). Keep the dispatch ORDER observable
        // for the recording subclass: create/switch first, then drop the old name.
        performSetProfile(newName);
        performRemoveProfile(oldName);
        return;
    }
    // SEQUENCED (WR-05): SetProfile(newName) first; only on a non-error reply that
    // confirms newName now exists do we RemoveProfile(oldName). A failed create
    // therefore never triggers the remove, so the old profile is preserved.
    auto* dev = new PizzaPixlLogiOpsDeviceInterface(kService, _devicePath, _bus, this);
    auto* w = new QDBusPendingCallWatcher(dev->SetProfile(newName), this);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, dev, oldName, newName](QDBusPendingCallWatcher* watcher) {
                QDBusPendingReply<> r = *watcher;
                watcher->deleteLater();
                const bool created = !r.isError();
                // Re-read GetProfiles to confirm the new name landed before any
                // destructive remove.
                auto* gw = new QDBusPendingCallWatcher(dev->GetProfiles(), this);
                connect(gw, &QDBusPendingCallWatcher::finished, this,
                        [this, dev, oldName, newName, created]
                        (QDBusPendingCallWatcher* gwatcher) {
                            QDBusPendingReply<QStringList> gr = *gwatcher;
                            gwatcher->deleteLater();
                            bool newExists = false;
                            if (!gr.isError()) {
                                const QStringList names = gr.value();
                                newExists = names.contains(newName);
                                const QString active = dev->activeProfile();
                                const QString def = dev->defaultProfile();
                                seedProfiles(names, active, def);
                            }
                            dev->deleteLater();
                            // Only drop the old profile when the new one is
                            // confirmed present (WR-05): never delete oldName if
                            // the create failed or newName is absent.
                            if (created && newExists)
                                performRemoveProfile(oldName);
                        });
            });
}

void ProfilesModel::refresh() {
    if (!_live)
        return;
    auto* dev = new PizzaPixlLogiOpsDeviceInterface(kService, _devicePath, _bus, this);
    auto* w = new QDBusPendingCallWatcher(dev->GetProfiles(), this);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, dev](QDBusPendingCallWatcher* watcher) {
                QDBusPendingReply<QStringList> r = *watcher;
                watcher->deleteLater();
                if (!r.isError()) {
                    const QStringList names = r.value();
                    // Re-read the active profile property (no per-config signal, so
                    // we refresh on our own writes — RESEARCH anti-pattern: no poll).
                    const QString active = dev->activeProfile();
                    const QString def = dev->defaultProfile();
                    seedProfiles(names, active, def);
                }
                dev->deleteLater();
            });
}

} // namespace logiops_gui
