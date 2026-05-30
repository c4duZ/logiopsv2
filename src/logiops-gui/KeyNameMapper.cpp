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

#include "KeyNameMapper.h"

#include <QHash>
#include <Qt>

namespace logiops_gui {

namespace {

// The lookup table. Every value is a real libevdev KEY_* name (verified against
// /usr/include/linux/input-event-codes.h, the vocabulary the daemon's
// InputDevice::toKeyCode resolves). Modifier families map to their LEFT*
// variants (the daemon and Options+ both treat a bare modifier as the left key).
const QHash<int, QString>& table() {
    static const QHash<int, QString> map = [] {
        QHash<int, QString> m;

        // Modifiers -> LEFT* variants.
        m.insert(Qt::Key_Control, QStringLiteral("KEY_LEFTCTRL"));
        m.insert(Qt::Key_Alt, QStringLiteral("KEY_LEFTALT"));
        m.insert(Qt::Key_AltGr, QStringLiteral("KEY_RIGHTALT"));
        m.insert(Qt::Key_Shift, QStringLiteral("KEY_LEFTSHIFT"));
        m.insert(Qt::Key_Meta, QStringLiteral("KEY_LEFTMETA"));
        m.insert(Qt::Key_Super_L, QStringLiteral("KEY_LEFTMETA"));
        m.insert(Qt::Key_Super_R, QStringLiteral("KEY_RIGHTMETA"));

        // Whitespace / editing.
        m.insert(Qt::Key_Return, QStringLiteral("KEY_ENTER"));
        m.insert(Qt::Key_Enter, QStringLiteral("KEY_ENTER"));
        m.insert(Qt::Key_Space, QStringLiteral("KEY_SPACE"));
        m.insert(Qt::Key_Tab, QStringLiteral("KEY_TAB"));
        m.insert(Qt::Key_Backspace, QStringLiteral("KEY_BACKSPACE"));
        m.insert(Qt::Key_Delete, QStringLiteral("KEY_DELETE"));
        m.insert(Qt::Key_Insert, QStringLiteral("KEY_INSERT"));
        m.insert(Qt::Key_Home, QStringLiteral("KEY_HOME"));
        m.insert(Qt::Key_End, QStringLiteral("KEY_END"));
        m.insert(Qt::Key_PageUp, QStringLiteral("KEY_PAGEUP"));
        m.insert(Qt::Key_PageDown, QStringLiteral("KEY_PAGEDOWN"));
        m.insert(Qt::Key_CapsLock, QStringLiteral("KEY_CAPSLOCK"));
        m.insert(Qt::Key_Print, QStringLiteral("KEY_SYSRQ"));
        m.insert(Qt::Key_Menu, QStringLiteral("KEY_COMPOSE"));

        // Arrows.
        m.insert(Qt::Key_Up, QStringLiteral("KEY_UP"));
        m.insert(Qt::Key_Down, QStringLiteral("KEY_DOWN"));
        m.insert(Qt::Key_Left, QStringLiteral("KEY_LEFT"));
        m.insert(Qt::Key_Right, QStringLiteral("KEY_RIGHT"));

        // Media keys.
        m.insert(Qt::Key_VolumeUp, QStringLiteral("KEY_VOLUMEUP"));
        m.insert(Qt::Key_VolumeDown, QStringLiteral("KEY_VOLUMEDOWN"));
        m.insert(Qt::Key_VolumeMute, QStringLiteral("KEY_MUTE"));
        m.insert(Qt::Key_MediaTogglePlayPause, QStringLiteral("KEY_PLAYPAUSE"));
        m.insert(Qt::Key_MediaPlay, QStringLiteral("KEY_PLAYPAUSE"));
        m.insert(Qt::Key_MediaPause, QStringLiteral("KEY_PLAYPAUSE"));
        m.insert(Qt::Key_MediaStop, QStringLiteral("KEY_STOPCD"));
        m.insert(Qt::Key_MediaNext, QStringLiteral("KEY_NEXTSONG"));
        m.insert(Qt::Key_MediaPrevious, QStringLiteral("KEY_PREVIOUSSONG"));

        return m;
    }();
    return map;
}

} // namespace

QString KeyNameMapper::toEvdevName(int qtKey) {
    // Escape is the capture-cancel sentinel — never a key.
    if (qtKey == Qt::Key_Escape)
        return {};

    // Letters A-Z (Qt uses the uppercase ASCII code for letter keys).
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z)
        return QStringLiteral("KEY_") + QChar(static_cast<char>(qtKey));

    // Digits 0-9.
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
        return QStringLiteral("KEY_") + QChar(static_cast<char>(qtKey));

    // Function keys F1-F24 (evdev defines through KEY_F24).
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
        return QStringLiteral("KEY_F") + QString::number(qtKey - Qt::Key_F1 + 1);

    const auto it = table().constFind(qtKey);
    if (it != table().constEnd())
        return it.value();

    // Unmappable -> empty; the caller skips it rather than emit a guess.
    return {};
}

QStringList KeyNameMapper::comboToEvdev(const QList<int>& modifiers, int key) {
    QStringList out;
    out.reserve(modifiers.size() + 1);
    for (const int mod : modifiers) {
        const QString name = toEvdevName(mod);
        if (!name.isEmpty())
            out.append(name);
    }
    const QString keyName = toEvdevName(key);
    if (!keyName.isEmpty())
        out.append(keyName);
    return out;
}

} // namespace logiops_gui
