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
#include "GestureActions.h"

#include <QStringList>
#include <QVariantMap>

namespace logiops_gui {

namespace {

// One predefined action: a human label + the ordered evdev KEY_* combo
// (modifiers... then the key). Every name is a real libevdev KEY_* token,
// verified against KeyNameMapper's vocabulary (the daemon resolves them via
// InputDevice::toKeyCode).
struct Action {
    const char* label;
    QStringList keys;
};

struct Category {
    const char* name;
    QList<Action> actions;
};

// The curated Options+-style action set. NOTE on DE-dependent defaults: Media is
// universal (standard hardware codes); Desktops & Windows / Brightness / Zoom /
// Screenshot use sensible GNOME/KDE common bindings and may vary per environment.
const QList<Category>& table() {
    static const QList<Category> cats = {
        {"Media",
         {
             {"Volume Up", {QStringLiteral("KEY_VOLUMEUP")}},
             {"Volume Down", {QStringLiteral("KEY_VOLUMEDOWN")}},
             {"Mute", {QStringLiteral("KEY_MUTE")}},
             {"Play/Pause", {QStringLiteral("KEY_PLAYPAUSE")}},
             {"Next Track", {QStringLiteral("KEY_NEXTSONG")}},
             {"Previous Track", {QStringLiteral("KEY_PREVIOUSSONG")}},
         }},
        {"Brightness",
         {
             {"Brightness Up", {QStringLiteral("KEY_BRIGHTNESSUP")}},
             {"Brightness Down", {QStringLiteral("KEY_BRIGHTNESSDOWN")}},
         }},
        {"Desktops & Windows",
         {
             {"Desktop Left",
              {QStringLiteral("KEY_LEFTCTRL"), QStringLiteral("KEY_LEFTALT"),
               QStringLiteral("KEY_LEFT")}},
             {"Desktop Right",
              {QStringLiteral("KEY_LEFTCTRL"), QStringLiteral("KEY_LEFTALT"),
               QStringLiteral("KEY_RIGHT")}},
             {"Show Desktop",
              {QStringLiteral("KEY_LEFTMETA"), QStringLiteral("KEY_D")}},
             {"Activities/Overview", {QStringLiteral("KEY_LEFTMETA")}},
             {"Maximize",
              {QStringLiteral("KEY_LEFTMETA"), QStringLiteral("KEY_UP")}},
             {"Snap Left",
              {QStringLiteral("KEY_LEFTMETA"), QStringLiteral("KEY_LEFT")}},
             {"Snap Right",
              {QStringLiteral("KEY_LEFTMETA"), QStringLiteral("KEY_RIGHT")}},
         }},
        {"Edit & Navigation",
         {
             {"Copy", {QStringLiteral("KEY_LEFTCTRL"), QStringLiteral("KEY_C")}},
             {"Paste", {QStringLiteral("KEY_LEFTCTRL"), QStringLiteral("KEY_V")}},
             {"Back", {QStringLiteral("KEY_LEFTALT"), QStringLiteral("KEY_LEFT")}},
             {"Forward",
              {QStringLiteral("KEY_LEFTALT"), QStringLiteral("KEY_RIGHT")}},
             {"Zoom In",
              {QStringLiteral("KEY_LEFTCTRL"), QStringLiteral("KEY_EQUAL")}},
             {"Zoom Out",
              {QStringLiteral("KEY_LEFTCTRL"), QStringLiteral("KEY_MINUS")}},
         }},
        {"System",
         {
             {"Screenshot", {QStringLiteral("KEY_PRINT")}},
         }},
    };
    return cats;
}

} // namespace

QVariantList GestureActions::categories() const {
    QVariantList out;
    for (const Category& cat : table()) {
        QVariantList actions;
        for (const Action& act : cat.actions) {
            QVariantMap a;
            a.insert(QStringLiteral("label"), QString::fromUtf8(act.label));
            a.insert(QStringLiteral("keys"), act.keys);
            actions.append(a);
        }
        QVariantMap c;
        c.insert(QStringLiteral("name"), QString::fromUtf8(cat.name));
        c.insert(QStringLiteral("actions"), actions);
        out.append(c);
    }
    return out;
}

} // namespace logiops_gui
