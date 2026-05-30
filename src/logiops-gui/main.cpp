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

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>

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

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("deviceModel"), &model);
    engine.rootContext()->setContextProperty(QStringLiteral("daemon"), &daemon);

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
