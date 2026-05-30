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
#include "DeviceControllerFactory.h"
#include "DeviceController.h"

namespace logiops_gui {

namespace {
    // Mirror the daemon's bus selection (WR-01 / DaemonConnection): user bus in a
    // -DUSE_USER_BUS dev build, system bus otherwise.
    QDBusConnection controllerBus() {
#ifdef USE_USER_BUS
        return QDBusConnection::sessionBus();
#else
        return QDBusConnection::systemBus();
#endif
    }
}

DeviceControllerFactory::DeviceControllerFactory(QObject* parent)
    : QObject(parent), _bus(controllerBus()) {}

DeviceControllerFactory::~DeviceControllerFactory() = default;

void DeviceControllerFactory::selectDevice(const QString& devicePath) {
    if (devicePath == _currentPath)
        return;
    _currentPath = devicePath;

    // Tear down the previous controller (it owns its proxies as children).
    if (_controller != nullptr) {
        _controller->deleteLater();
        _controller = nullptr;
    }

    if (!devicePath.isEmpty())
        _controller = new DeviceController(devicePath, _bus, this);

    emit controllerChanged();
}

} // namespace logiops_gui
