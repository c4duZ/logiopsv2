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
#include "DaemonConnection.h"
#include "DeviceController.h"
#include "DeviceControllerFactory.h"
#include "DeviceModel.h"
#include "KeyNameMapper.h"

#include <QDBusConnection>
#include <QGuiApplication>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStringList>
#include <QUrl>
#include <QVariant>

namespace {
// Thin QObject facade exposing KeyNameMapper's static mapping to QML (the
// key-capture widget calls comboToEvdev to turn captured Qt key+modifiers into
// the evdev KEY_* names ButtonsModel.setKeypress expects). QML renders only —
// this is the C++ side of BTN-02.
class KeyNameBridge : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE QStringList comboToEvdev(const QVariantList& modifiers, int key) const {
        QList<int> mods;
        mods.reserve(modifiers.size());
        for (const QVariant& m : modifiers)
            mods.push_back(m.toInt());
        return logiops_gui::KeyNameMapper::comboToEvdev(mods, key);
    }
    Q_INVOKABLE QString toEvdevName(int key) const {
        return logiops_gui::KeyNameMapper::toEvdevName(key);
    }
};
} // namespace

// Entry point for the logiops-gui client. C++ owns ALL state: it constructs the
// DeviceModel (QAbstractListModel) and the DaemonConnection (live system-bus
// wiring + screen-state enum), exposes them to QML as context properties, and
// loads the QML shell. QML renders only — there is no business logic in JS
// (CONTEXT.md decision).
int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("logiops-gui"));
    QGuiApplication::setOrganizationName(QStringLiteral("logiops"));

    logiops_gui::DeviceModel model;
    // The connection drives the model from the daemon's live D-Bus signals.
    logiops_gui::DaemonConnection daemon(&model);
    // Owns the per-selected-device DeviceController; QML calls selectDevice(path)
    // on selection and binds the config shell to its `controller` property (the
    // factory builds + introspects a fresh controller per device). QML renders
    // only — capability discovery + marshalling stay in C++ (CONTEXT.md).
    logiops_gui::DeviceControllerFactory controllerFactory;
    // CONF-01 / CONF-02: the global persistence brain. Tracks unsaved changes
    // (every tab setter calls markDirty()), drives the async polkit-gated
    // .Config.Save (no UI freeze), and restores defaults via .Device.ClearProfile.
    // Same bus selection as the rest of the GUI (WR-01: user bus in a USE_USER_BUS
    // dev build, system bus otherwise).
#ifdef USE_USER_BUS
    logiops_gui::ConfigState configState(QDBusConnection::sessionBus());
#else
    logiops_gui::ConfigState configState(QDBusConnection::systemBus());
#endif
    // CONF-01 (WR-03): hand the dirty-tracker to the factory so every per-device
    // controller/model marks unsaved changes on a live-apply (the pill appears).
    controllerFactory.setConfigState(&configState);
    // BTN-02 key-capture -> evdev names bridge (static mapper exposed to QML).
    KeyNameBridge keyNames;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("deviceModel"), &model);
    engine.rootContext()->setContextProperty(QStringLiteral("daemon"), &daemon);
    engine.rootContext()->setContextProperty(QStringLiteral("deviceControllerFactory"),
                                             &controllerFactory);
    engine.rootContext()->setContextProperty(QStringLiteral("configState"), &configState);
    engine.rootContext()->setContextProperty(QStringLiteral("keyNames"), &keyNames);

    // Load the shell from the qt_add_qml_module resource. Qt 6.4.2 has no
    // QQmlApplicationEngine::loadFromModule (that is 6.5+), so load the module's
    // generated qrc URL directly (URI logiops.gui -> /qt/qml/logiops/gui).
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [](QObject* obj, const QUrl&) {
            if (obj == nullptr)
                QCoreApplication::exit(EXIT_FAILURE);
        },
        Qt::QueuedConnection);

    // qt_add_qml_module sets `prefer :/logiops/gui/` in the generated qmldir, so
    // the module's QML lives under that resource prefix (Qt 6.4.2 has no
    // loadFromModule helper to abstract this).
    engine.load(QUrl(QStringLiteral("qrc:/logiops/gui/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return EXIT_FAILURE;

    return QGuiApplication::exec();
}

// KeyNameBridge has Q_OBJECT in this TU; pull in its generated meta-object.
#include "main.moc"
