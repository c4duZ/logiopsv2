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
#ifndef LOGIOPS_GUI_DEVICECONTROLLERFACTORY_H
#define LOGIOPS_GUI_DEVICECONTROLLERFACTORY_H

#include <QDBusConnection>
#include <QObject>
#include <QString>

// Full definitions required: the Q_PROPERTY(T*) metatypes the moc emits for this
// class need the complete types (a forward declaration suffices for the pointer
// member but not for Q_DECLARE_METATYPE, which moc instantiates here).
#include "ButtonsModel.h"
#include "DeviceController.h"
#include "ProfilesModel.h"

namespace logiops_gui {

class ConfigState;

/*
 * DeviceControllerFactory — owns the single "currently selected device"
 * DeviceController and swaps it when the sidebar selection changes.
 *
 * Registered once as the `deviceControllerFactory` QML context property; QML
 * calls selectDevice(path) on selection and binds the tab shell to the
 * `controller` property (which re-points + re-introspects per device). This
 * keeps main.cpp's registration a single context property while still creating
 * a fresh capability-discovering controller per device (the plan's
 * "created/swapped when the sidebar selection changes" model). C++ owns the bus
 * and all marshalling; QML renders only.
 */
class DeviceControllerFactory final : public QObject {
    Q_OBJECT

    Q_PROPERTY(logiops_gui::DeviceController* controller READ controller NOTIFY controllerChanged)
    // The per-device Buttons model for the Buttons tab (BTN-01..04), swapped in
    // lock-step with the controller. Its hostCount is seeded from the controller
    // (HOST-01: the daemon's ChangeHost.GetHostCount, via the controller).
    Q_PROPERTY(logiops_gui::ButtonsModel* buttonsModel READ buttonsModel NOTIFY controllerChanged)
    // The per-device Profiles model for the Profiles tab (PROF-01), swapped in
    // lock-step with the controller. Manual profile create/switch/remove over
    // .Device.GetProfiles/SetProfile/RemoveProfile.
    Q_PROPERTY(logiops_gui::ProfilesModel* profilesModel READ profilesModel NOTIFY controllerChanged)

public:
    explicit DeviceControllerFactory(QObject* parent = nullptr);
    ~DeviceControllerFactory() override;

    // CONF-01 (WR-03): the global dirty-tracker injected into each per-device
    // controller/model so config-mutating setters flip the "Unsaved changes" pill.
    // Set once at startup before any device is selected.
    void setConfigState(ConfigState* configState) { _configState = configState; }

    [[nodiscard]] DeviceController* controller() const { return _controller; }
    [[nodiscard]] ButtonsModel* buttonsModel() const { return _buttonsModel; }
    [[nodiscard]] ProfilesModel* profilesModel() const { return _profilesModel; }

    // Build (or rebuild) the controller for the given device object-path. An
    // empty path clears the selection. No-op when the path is unchanged.
    Q_INVOKABLE void selectDevice(const QString& devicePath);

signals:
    void controllerChanged();

private:
    QDBusConnection _bus;
    DeviceController* _controller = nullptr;
    ButtonsModel* _buttonsModel = nullptr;
    ProfilesModel* _profilesModel = nullptr;
    QString _currentPath;
    // CONF-01 dirty-tracker (WR-03), injected into each per-device object. Not owned.
    ConfigState* _configState = nullptr;
};

} // namespace logiops_gui

#endif // LOGIOPS_GUI_DEVICECONTROLLERFACTORY_H
