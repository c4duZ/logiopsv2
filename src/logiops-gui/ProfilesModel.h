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
#ifndef LOGIOPS_GUI_PROFILESMODEL_H
#define LOGIOPS_GUI_PROFILESMODEL_H

#include <QAbstractListModel>
#include <QDBusConnection>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace logiops_gui {

/*
 * ProfilesModel — the C++<->QML bridge for the Profiles tab (PROF-01).
 *
 * A QAbstractListModel over the daemon's `.Device.GetProfiles()` surface. Each row
 * is one manual profile: its name, whether it is the ACTIVE profile (the one
 * currently applied to the hardware, .Device.ActiveProfile) and whether it is the
 * DEFAULT profile (.Device.DefaultProfile). The Profiles tab renders the rows as
 * pills (active = accentTint + accent text, UI-SPEC).
 *
 * Mutations map directly to the daemon (RESEARCH "Don't Hand-Roll"):
 *   createProfile(name) -> SetProfile(name)   (creates-if-absent + switches live)
 *   switchProfile(name) -> SetProfile(name)   (switches live)
 *   removeProfile(name) -> RemoveProfile(name)
 *   renameProfile(old,new) -> SetProfile(new) (create+switch) then RemoveProfile(old)
 *                              — the daemon has no first-class rename (VERIFIED:
 *                              Device.cpp exposes only Get/Set/Remove/Clear), so a
 *                              rename is create-new + switch + drop-old.
 * All calls are async (QDBusPendingCallWatcher); on each reply the model re-reads
 * GetProfiles + ActiveProfile so the list + active pill stay in sync (there is no
 * per-config changed signal — RESEARCH anti-pattern: do not poll, refresh on our
 * own writes).
 *
 * Test seam (mirrors DeviceModel / ButtonsModel "drive via public methods, no live
 * bus"): the list is injected via seedProfiles(); every daemon write is funnelled
 * through the virtual performSetProfile / performRemoveProfile hooks so a test
 * subclass records the call + args and can simulate the daemon by re-seeding. The
 * live path overrides them to issue the actual async D-Bus calls.
 */
class ProfilesModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(QString activeProfile READ activeProfile NOTIFY activeProfileChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        IsActiveRole,
        IsDefaultRole,
    };
    Q_ENUM(Roles)

    // Live constructor: the device object-path + the bus the GUI is on. Kicks off
    // the async GetProfiles + ActiveProfile/DefaultProfile reads.
    explicit ProfilesModel(const QString& devicePath, const QDBusConnection& bus,
                           QObject* parent = nullptr);
    // Test constructor: no bus, no async — drive rows via seedProfiles().
    explicit ProfilesModel(QObject* parent = nullptr);
    ~ProfilesModel() override;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString activeProfile() const { return _active; }

    // --- Mutators (live-apply). All async; refresh the list on reply. ---
    // Creates the profile if absent and switches to it (single SetProfile call).
    Q_INVOKABLE void createProfile(const QString& name);
    // Switches the active profile live (SetProfile on an existing name).
    Q_INVOKABLE void switchProfile(const QString& name);
    // Removes a profile (RemoveProfile). Refuses to remove the last profile.
    Q_INVOKABLE void removeProfile(const QString& name);
    // Rename = create-new + switch + drop-old (no daemon rename primitive).
    Q_INVOKABLE void renameProfile(const QString& oldName, const QString& newName);

    // --- Test / construction seam. ---
    // Replace all rows + the active/default names with an injected snapshot. Emits a
    // model reset (and activeProfileChanged if the active name moved).
    void seedProfiles(const QStringList& names, const QString& active,
                      const QString& def);
    [[nodiscard]] const QStringList& profiles() const { return _names; }

signals:
    void activeProfileChanged();

protected:
    // Daemon-write hooks. The live path issues the async D-Bus call + refreshes;
    // the test subclass records (method, arg) and simulates the daemon by
    // re-seeding. Kept separate so ordering/args are assertable with no bus.
    virtual void performSetProfile(const QString& name);
    virtual void performRemoveProfile(const QString& name);

    // Optimistically set the active profile + emit changed (so the active pill
    // moves immediately, live-apply model).
    void setActive(const QString& name);

    QStringList _names;
    QString _active;
    QString _default;

private:
    // Live: async re-read GetProfiles + ActiveProfile after a write so the model
    // reflects the daemon's post-mutation truth.
    void refresh();

    QString _devicePath;
    QDBusConnection _bus;
    bool _live = false;
};

} // namespace logiops_gui

#endif // LOGIOPS_GUI_PROFILESMODEL_H
