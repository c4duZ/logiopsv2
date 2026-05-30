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

// Phase 3, Wave 0 — BTN-02. Proves the live key-capture widget maps Qt key /
// modifier events to the EXACT evdev KEY_* strings the daemon's
// KeypressAction::SetKeys expects (resolved via InputDevice::toKeyCode, i.e.
// libevdev event-code names). Unmappable keys -> empty (caller skips), never a
// guessed garbage token (T-3-00-03 mitigation). Escape -> empty (cancel).

#include "KeyNameMapper.h"

#include <QtTest>

using logiops_gui::KeyNameMapper;

class KeyNameMapperTest : public QObject {
    Q_OBJECT

private slots:
    void letters() {
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_A), QStringLiteral("KEY_A"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Z), QStringLiteral("KEY_Z"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_T), QStringLiteral("KEY_T"));
    }

    void digits() {
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_0), QStringLiteral("KEY_0"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_9), QStringLiteral("KEY_9"));
    }

    void functionKeys() {
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_F1), QStringLiteral("KEY_F1"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_F12), QStringLiteral("KEY_F12"));
    }

    void modifiers() {
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Control),
                 QStringLiteral("KEY_LEFTCTRL"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Alt),
                 QStringLiteral("KEY_LEFTALT"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Shift),
                 QStringLiteral("KEY_LEFTSHIFT"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Meta),
                 QStringLiteral("KEY_LEFTMETA"));
    }

    void mediaKeys() {
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_VolumeUp),
                 QStringLiteral("KEY_VOLUMEUP"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_VolumeDown),
                 QStringLiteral("KEY_VOLUMEDOWN"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_VolumeMute),
                 QStringLiteral("KEY_MUTE"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_MediaTogglePlayPause),
                 QStringLiteral("KEY_PLAYPAUSE"));
    }

    void navAndWhitespace() {
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Up), QStringLiteral("KEY_UP"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Down), QStringLiteral("KEY_DOWN"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Left), QStringLiteral("KEY_LEFT"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Right), QStringLiteral("KEY_RIGHT"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Return), QStringLiteral("KEY_ENTER"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Enter), QStringLiteral("KEY_ENTER"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Space), QStringLiteral("KEY_SPACE"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Tab), QStringLiteral("KEY_TAB"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Backspace),
                 QStringLiteral("KEY_BACKSPACE"));
        QCOMPARE(KeyNameMapper::toEvdevName(Qt::Key_Delete),
                 QStringLiteral("KEY_DELETE"));
    }

    void escapeIsCancelSentinel() {
        // Esc = cancel, never emitted as a key — returns empty.
        QVERIFY(KeyNameMapper::toEvdevName(Qt::Key_Escape).isEmpty());
    }

    void unknownReturnsEmpty() {
        // An unmappable Qt key returns empty (caller skips), never a guess.
        QVERIFY(KeyNameMapper::toEvdevName(Qt::Key_unknown).isEmpty());
        QVERIFY(KeyNameMapper::toEvdevName(0x01ffffff).isEmpty());
    }

    void comboModifiersThenKey() {
        // Ctrl+Alt+T -> {KEY_LEFTCTRL, KEY_LEFTALT, KEY_T} (modifiers first).
        const QStringList combo = KeyNameMapper::comboToEvdev(
                {Qt::Key_Control, Qt::Key_Alt}, Qt::Key_T);
        QCOMPARE(combo, QStringList({QStringLiteral("KEY_LEFTCTRL"),
                                     QStringLiteral("KEY_LEFTALT"),
                                     QStringLiteral("KEY_T")}));
    }

    void comboSkipsUnmappable() {
        // An unmappable component is dropped, not turned into garbage.
        const QStringList combo = KeyNameMapper::comboToEvdev(
                {Qt::Key_Control, Qt::Key_unknown}, Qt::Key_A);
        QCOMPARE(combo, QStringList({QStringLiteral("KEY_LEFTCTRL"),
                                     QStringLiteral("KEY_A")}));
    }
};

QTEST_APPLESS_MAIN(KeyNameMapperTest)

#include "KeyNameMapperTest.moc"
