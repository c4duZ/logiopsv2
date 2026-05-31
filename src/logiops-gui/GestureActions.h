/*
 * Copyright 2024 c4duZ - https://github.com/c4duZ/logiopsv2
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
#ifndef LOGIOPS_GUI_GESTUREACTIONS_H
#define LOGIOPS_GUI_GESTUREACTIONS_H

#include <QObject>
#include <QVariantList>

namespace logiops_gui {

/*
 * GestureActions — the static C++ provider of curated, Options+-style predefined
 * gesture actions exposed to QML as the `gestureActions` context property. Each
 * predefined action is a KEYSTROKE combo dispatched through the EXISTING
 * GestureModel::setGestureKeypress (SetAction("Keypress") + SetKeys on the
 * gesture node), so the user never needs the physical key (e.g. a keyboard with
 * no Volume-Up key can still bind a Volume-Up gesture).
 *
 * QML renders only: this object carries no business logic beyond the static data
 * table. The picker UI (GestureBuilder/ReassignPanel) reads categories(), shows a
 * categorized list, and on selection calls gestureModel.setGestureKeypress(dir,
 * action.keys). The `keys` lists are real libevdev KEY_* names (the exact
 * vocabulary KeyNameMapper emits and the daemon's SetKeys resolves via
 * InputDevice::toKeyCode) — never invented tokens.
 *
 * DE-DEPENDENT DEFAULTS: the Media keys are hardware/standard codes and universal.
 * The Desktops & Windows, Brightness, Zoom, and Screenshot entries use sensible
 * GNOME/KDE common bindings (e.g. Super+D show desktop, Ctrl+Alt+Arrow desktop
 * switch, Super = Activities/Overview) and MAY need per-environment tweaking —
 * they are good defaults, not guaranteed universal across every desktop.
 */
class GestureActions : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    // The categorized predefined action set for QML. Returns an ordered list of
    // { name: <category>, actions: [ { label, keys: [KEY_* ...] }, ... ] }.
    // Suitable for a QML Repeater/ListView (a category header + per-action rows).
    Q_INVOKABLE QVariantList categories() const;
};

} // namespace logiops_gui

#endif // LOGIOPS_GUI_GESTUREACTIONS_H
