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
#ifndef LOGIOPS_GUI_KEYNAMEMAPPER_H
#define LOGIOPS_GUI_KEYNAMEMAPPER_H

#include <QString>
#include <QStringList>
#include <QList>

namespace logiops_gui {

// Maps Qt key / modifier codes to the evdev KEY_* name strings the daemon's
// KeypressAction::SetKeys expects (the daemon resolves them via
// InputDevice::toKeyCode, i.e. libevdev event-code names). The mapping is built
// by cross-referencing the libevdev KEY_* vocabulary (linux/input-event-codes.h)
// the daemon accepts — never invented tokens.
//
// Contract (BTN-02, T-3-00-03):
//   - A mapped key returns its exact KEY_* name (e.g. "KEY_A", "KEY_LEFTCTRL").
//   - Escape returns an EMPTY string — Esc is the capture-cancel sentinel and is
//     never emitted as a key.
//   - An unmappable / unknown Qt key returns an EMPTY string so the caller skips
//     it, never a guessed garbage name the daemon would mishandle.
class KeyNameMapper {
public:
    // Qt key code -> evdev KEY_* name, or empty string when unmapped/cancel.
    [[nodiscard]] static QString toEvdevName(int qtKey);

    // A modifier list + a final key -> ordered {modifiers..., key} KEY_* names.
    // Unmappable components are dropped (skipped), never emitted as garbage.
    [[nodiscard]] static QStringList comboToEvdev(const QList<int>& modifiers,
                                                  int key);
};

} // namespace logiops_gui

#endif // LOGIOPS_GUI_KEYNAMEMAPPER_H
