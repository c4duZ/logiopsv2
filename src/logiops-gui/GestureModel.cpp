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
#include "GestureModel.h"

#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusMessage>
#include <QVariantMap>

#include "ConfigState.h"
#include "logid_action_gesture_proxy.h"

namespace logiops_gui {

namespace {
    // The daemon's well-known bus name (mirrors ipc_defs SERVICE_ROOT_NAME).
    const QString kService = QStringLiteral("pizza.pixl.LogiOps");

    // The four cardinal directions the UI surfaces. The daemon also exposes
    // "none", but the builder only edits the 4 (RESEARCH / CONTEXT lock).
    bool isCardinal(const QString& d) {
        return d == QLatin1String("up") || d == QLatin1String("down") ||
               d == QLatin1String("left") || d == QLatin1String("right");
    }
}

GestureModel::GestureModel(const QString& buttonPath, const QDBusConnection& bus,
                           QObject* parent)
    : QObject(parent), _buttonPath(buttonPath), _bus(bus), _live(true) {
    // WR-03: open reflecting the button's existing gesture config, not blank.
    seedFromDaemon();
}

GestureModel::GestureModel(QObject* parent)
    : QObject(parent), _bus(QDBusConnection::sessionBus()), _live(false) {}

GestureModel::~GestureModel() = default;

namespace {
    // Reverse of mapMode: a present daemon .Gesture.<type> -> plain-language mode.
    // Only the four canonical types are ever produced by the daemon's allowlist.
    QString plainModeForType(const QString& type) {
        if (type == QLatin1String("OnInterval"))
            return QStringLiteral("Repeat while moving");
        if (type == QLatin1String("OnRelease"))
            return QStringLiteral("Do once when moved far enough");
        if (type == QLatin1String("Axis"))
            return QStringLiteral("Adjust proportionally");
        if (type == QLatin1String("None"))
            return QStringLiteral("Nothing");
        return {};
    }
}

// ---------------------------------------------------------------------------
// WR-03: seed per-direction state from the daemon's CURRENT gesture config so the
// builder opens reflecting existing bindings. Mirrors ButtonsModel::enumerate's
// BTN-04 present-interface readback: for each cardinal direction, find which
// .Gesture.<type> interface is present at .../gestures/{dir} (the FIRST present
// one wins) and read its granularity param. Bounded (4 directions x small probe
// set) and only runs once, lazily, when a button's Gesture category opens.
// ---------------------------------------------------------------------------
void GestureModel::seedFromDaemon() {
    if (!_live)
        return;

    static const char* kDirs[] = {"up", "down", "left", "right"};
    // Probe order matches the daemon allowlist; "None" is meaningful too (an
    // explicitly-cleared direction reads back as "Nothing", not unconfigured).
    static const char* kTypes[] = {"OnInterval", "OnRelease", "Axis", "None"};

    bool seededAny = false;
    for (const char* d : kDirs) {
        const QString direction = QString::fromUtf8(d);
        const QString childPath =
            _buttonPath + QStringLiteral("/gestures/") + direction;
        QDBusInterface props(
            kService, childPath,
            QStringLiteral("org.freedesktop.DBus.Properties"), _bus);

        for (const char* t : kTypes) {
            const QString type = QString::fromUtf8(t);
            const QString iface = QStringLiteral("pizza.pixl.LogiOps.Gesture.") + type;
            QDBusReply<QVariantMap> got = props.call(QStringLiteral("GetAll"), iface);
            if (!got.isValid())
                continue;

            DirectionState& st = stateFor(direction);
            st.mode = type;
            st.plainMode = plainModeForType(type);

            // Read the granularity param via the per-mode getter (best-effort; a
            // missing/zeroed value just leaves the default granularity phrase).
            QDBusInterface gesture(kService, childPath, iface, _bus);
            if (type == QLatin1String("OnInterval")) {
                QDBusMessage reply = gesture.call(QStringLiteral("GetConfig"));
                if (reply.type() == QDBusMessage::ReplyMessage &&
                    !reply.arguments().isEmpty()) {
                    st.granularity = reply.arguments().at(0).toInt();
                }
            } else if (type == QLatin1String("OnRelease")) {
                QDBusReply<int> thr = gesture.call(QStringLiteral("GetThreshold"));
                if (thr.isValid())
                    st.granularity = thr.value();
            } else if (type == QLatin1String("Axis")) {
                QDBusMessage reply = gesture.call(QStringLiteral("GetConfig"));
                if (reply.type() == QDBusMessage::ReplyMessage &&
                    reply.arguments().size() >= 2) {
                    // GetConfig -> (axis, multiplier, threshold); granularity = mult.
                    st.granularity =
                        qRound(reply.arguments().at(1).toDouble());
                }
            }
            seededAny = true;
            emit configuredChanged(direction);
            break; // first present interface wins
        }
    }

    if (seededAny)
        emit previewChanged();
}

// ---------------------------------------------------------------------------
// Mode allowlist — the SINGLE source of truth for plain-language -> daemon type.
// Only OnInterval / OnRelease / Axis / None are ever produced. Mode 2 is
// canonically "OnRelease" (NOT "OnThreshold" — interface-name collision ->
// InvalidGesture). "Nothing" -> "None" (NOT "NoPress"). T-04-02-02.
// ---------------------------------------------------------------------------
bool GestureModel::mapMode(const QString& plainMode, QString& typeOut) {
    if (plainMode == QLatin1String("Repeat while moving")) {
        typeOut = QStringLiteral("OnInterval");
        return true;
    }
    if (plainMode == QLatin1String("Do once when moved far enough")) {
        typeOut = QStringLiteral("OnRelease");
        return true;
    }
    if (plainMode == QLatin1String("Adjust proportionally")) {
        typeOut = QStringLiteral("Axis");
        return true;
    }
    if (plainMode == QLatin1String("Nothing")) {
        typeOut = QStringLiteral("None");
        return true;
    }
    return false;
}

bool GestureModel::validDirection(const QString& direction) {
    return isCardinal(direction);
}

QStringList GestureModel::plainModes() {
    return {
        QStringLiteral("Repeat while moving"),
        QStringLiteral("Do once when moved far enough"),
        QStringLiteral("Adjust proportionally"),
        QStringLiteral("Nothing"),
    };
}

GestureModel::DirectionState& GestureModel::stateFor(const QString& direction) {
    return _dirs[direction];
}

// ---------------------------------------------------------------------------
// setMode — validate-before-dispatch (T-04-02-01), then the two-step:
// performSetGesture(direction, type) FIRST; the per-mode param call (if any)
// is sequenced SECOND by the public param setters / live override.
// ---------------------------------------------------------------------------
bool GestureModel::setMode(const QString& direction, const QString& plainMode) {
    if (!validDirection(direction)) {
        emit editRejected(direction, QStringLiteral("invalid direction"));
        return false;
    }
    QString type;
    if (!mapMode(plainMode, type)) {
        emit editRejected(direction, QStringLiteral("invalid mode"));
        return false;
    }

    // Step 1: the mode switch on the button node. Rebuilds the .../gestures/{dir}
    // node so the .Gesture.<type> param interface exists for any later param call.
    performSetGesture(direction, type);

    DirectionState& st = stateFor(direction);
    st.mode = type;
    st.plainMode = plainMode;
    // Switching to a mode without a discrete action clears any stale action.
    if (type == QLatin1String("None") || type == QLatin1String("Axis"))
        st.actionType.clear();

    if (_configState != nullptr)
        _configState->markDirty();

    emit configuredChanged(direction);
    if (direction == _active) {
        emit previewChanged();
    }
    return true;
}

// ---------------------------------------------------------------------------
// setGranularity — clamp to sane positive bounds (T-04-02-03), then route to
// the correct per-mode setter behind the two-step. The setter only fires after
// the mode's SetGesture has landed (live override sequences performParamCall).
// ---------------------------------------------------------------------------
bool GestureModel::setGranularity(const QString& direction, int rawValue) {
    if (!validDirection(direction)) {
        emit editRejected(direction, QStringLiteral("invalid direction"));
        return false;
    }
    DirectionState& st = stateFor(direction);
    if (st.mode.isEmpty() || st.mode == QLatin1String("None")) {
        emit editRejected(direction, QStringLiteral("no granularity for this mode"));
        return false;
    }
    // Reject negative; 0 is the documented threshold-reset sentinel, allowed.
    if (rawValue < 0) {
        emit editRejected(direction, QStringLiteral("granularity out of range"));
        return false;
    }

    QString method;
    QVariantList args;
    if (st.mode == QLatin1String("OnInterval")) {
        // Repeat-while-moving granularity drives the interval (one fire per step).
        method = QStringLiteral("SetInterval");
        args = {QVariant::fromValue(rawValue)};
    } else if (st.mode == QLatin1String("OnRelease")) {
        method = QStringLiteral("SetThreshold");
        args = {QVariant::fromValue(rawValue)};
    } else if (st.mode == QLatin1String("Axis")) {
        // Axis multiplier is a double; the slider value maps to "notches per move".
        method = QStringLiteral("SetMultiplier");
        args = {QVariant::fromValue(static_cast<double>(rawValue))};
    } else {
        emit editRejected(direction, QStringLiteral("no granularity for this mode"));
        return false;
    }

    performParamCall(direction, st.mode, method, args);
    st.granularity = rawValue;

    if (_configState != nullptr)
        _configState->markDirty();

    if (direction == _active)
        emit previewChanged();
    return true;
}

bool GestureModel::setGestureAction(const QString& direction, const QString& actionType) {
    if (!validDirection(direction)) {
        emit editRejected(direction, QStringLiteral("invalid direction"));
        return false;
    }
    DirectionState& st = stateFor(direction);
    // SetAction only exists on the discrete modes (OnInterval / OnRelease).
    if (st.mode != QLatin1String("OnInterval") && st.mode != QLatin1String("OnRelease")) {
        emit editRejected(direction, QStringLiteral("mode has no action"));
        return false;
    }
    if (actionType.isEmpty()) {
        emit editRejected(direction, QStringLiteral("empty action"));
        return false;
    }

    performParamCall(direction, st.mode, QStringLiteral("SetAction"),
                     {QVariant::fromValue(actionType)});
    st.actionType = actionType;

    if (_configState != nullptr)
        _configState->markDirty();

    if (direction == _active)
        emit previewChanged();
    return true;
}

void GestureModel::setActiveDirection(const QString& direction) {
    if (!validDirection(direction) || direction == _active)
        return;
    _active = direction;
    emit activeDirectionChanged();
    emit previewChanged();
}

bool GestureModel::isConfigured(const QString& direction) const {
    auto it = _dirs.constFind(direction);
    if (it == _dirs.constEnd())
        return false;
    return !it->mode.isEmpty() && it->mode != QLatin1String("None");
}

QString GestureModel::modeOf(const QString& direction) const {
    auto it = _dirs.constFind(direction);
    if (it == _dirs.constEnd())
        return {};
    return it->plainMode;
}

QString GestureModel::previewSentence() const {
    auto it = _dirs.constFind(_active);
    DirectionState st = (it == _dirs.constEnd()) ? DirectionState{} : *it;
    return composePreview(_active, st);
}

QString GestureModel::granularityReadout() const {
    auto it = _dirs.constFind(_active);
    const int raw = (it == _dirs.constEnd()) ? 0 : it->granularity;
    return tr("~1 step per %1 of movement").arg(granularityPhrase(raw));
}

// ---------------------------------------------------------------------------
// Preview composition (GEST-04) — UI-SPEC §Copywriting templates, composed in
// C++ so QML only binds the finished string. Raw daemon mode names and HID++
// units NEVER appear.
// ---------------------------------------------------------------------------
QString GestureModel::composePreview(const QString& direction, const DirectionState& st) {
    const QString dir = direction;
    const QString amount = granularityPhrase(st.granularity);
    const QString action = actionVerbPhrase(st.actionType);

    if (st.mode == QLatin1String("OnInterval")) {
        return tr("Moving %1 keeps %2 as long as you keep moving "
                  "— about one step per %3.")
            .arg(dir, action, amount);
    }
    if (st.mode == QLatin1String("OnRelease")) {
        return tr("Moving %1 far enough %2 once. A single flick does one step.")
            .arg(dir, action);
    }
    if (st.mode == QLatin1String("Axis")) {
        return tr("Moving %1 %2 in proportion to how far you move.")
            .arg(dir, action);
    }
    // "Nothing" / unset -> the empty-direction sentence.
    return tr("Moving %1 does nothing.").arg(dir);
}

// Map a raw granularity value to a human {amount} phrase, leftmost stop ->
// "small flick", rightmost -> "long swipe". Tabular-stable set of phrases so
// the QML readout never reflows mid-drag.
QString GestureModel::granularityPhrase(int rawValue) {
    if (rawValue <= 0)
        return tr("small flick");
    if (rawValue < 30)
        return tr("small flick");
    if (rawValue < 80)
        return tr("flick");
    if (rawValue < 160)
        return tr("swipe");
    return tr("long swipe");
}

// Plain verb phrase for a gesture action type (NEVER raw daemon internals).
QString GestureModel::actionVerbPhrase(const QString& actionType) {
    if (actionType == QLatin1String("ChangeHost"))
        return tr("switching hosts");
    if (actionType == QLatin1String("ChangeProfile"))
        return tr("switching a profile");
    if (actionType == QLatin1String("ChangeDPI") || actionType == QLatin1String("CycleDPI"))
        return tr("changing DPI");
    if (actionType == QLatin1String("Keypress"))
        return tr("pressing the keystroke");
    if (actionType == QLatin1String("ToggleSmartShift"))
        return tr("toggling SmartShift");
    if (actionType == QLatin1String("ToggleHiresScroll"))
        return tr("toggling hi-res scroll");
    if (actionType.isEmpty())
        return tr("doing what you choose");
    return tr("running the action");
}

// ---------------------------------------------------------------------------
// Live two-step dispatch. performSetGesture issues Action.Gesture.SetGesture;
// performParamCall is sequenced to fire only AFTER that reply lands (the
// .Gesture.<type> interface does not exist until SetGesture rebuilds the node).
// The recording test subclass overrides both to capture order/args (no bus).
// ---------------------------------------------------------------------------
void GestureModel::performSetGesture(const QString& direction, const QString& type) {
    if (!_live)
        return;
    auto* gesture = new PizzaPixlLogiOpsActionGestureInterface(
        kService, _buttonPath, _bus, this);
    auto pending = gesture->SetGesture(direction, type);
    auto* w = new QDBusPendingCallWatcher(pending, this);
    // WR-01: record the in-flight SetGesture for this direction so a param call
    // can chain strictly behind THIS reply (the .Gesture.<type> child node does
    // not exist until SetGesture rebuilds it). The watcher stays alive (not
    // deleteLater'd here) so a param call arriving mid-flight can connect to its
    // finished signal; it is cleared and deleted when the reply lands.
    _pendingSetGesture.insert(direction, w);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, gesture, direction](QDBusPendingCallWatcher* watcher) {
                gesture->deleteLater();
                if (_pendingSetGesture.value(direction) == watcher)
                    _pendingSetGesture.remove(direction);
                watcher->deleteLater();
            });
}

void GestureModel::performParamCall(const QString& direction, const QString& type,
                                    const QString& method, const QVariantList& args) {
    if (!_live)
        return;
    const QString childPath = _buttonPath + QStringLiteral("/gestures/") + direction;
    const QString iface = QStringLiteral("pizza.pixl.LogiOps.Gesture.") + type;
    const QDBusConnection bus = _bus;

    // The actual param setter, fired only once the .Gesture.<type> child node is
    // known to exist. Mirrors ButtonsModel::performParamCall's final hop.
    auto fire = [this, childPath, iface, method, args, bus]() {
        auto* gesture = new QDBusInterface(kService, childPath, iface, bus, this);
        auto reply = gesture->asyncCallWithArgumentList(method, args);
        auto* pw = new QDBusPendingCallWatcher(reply, this);
        connect(pw, &QDBusPendingCallWatcher::finished, this,
                [gesture](QDBusPendingCallWatcher* w2) {
                    w2->deleteLater();
                    gesture->deleteLater();
                });
    };

    // WR-01: if a SetGesture for this direction is still in flight, chain the
    // param call strictly behind ITS reply instead of probing a child node that
    // may not exist yet. Only fire the setter when SetGesture did NOT error — a
    // failed mode switch means the .Gesture.<type> interface was never created,
    // so a blind param call would hit a non-existent interface and be lost.
    QDBusPendingCallWatcher* pending = _pendingSetGesture.value(direction, nullptr);
    if (pending != nullptr) {
        connect(pending, &QDBusPendingCallWatcher::finished, this,
                [fire](QDBusPendingCallWatcher* watcher) {
                    if (!watcher->isError())
                        fire();
                });
        return;
    }

    // No SetGesture in flight (e.g. adjusting a param on an already-set mode).
    // Probe the child node's interface; only fire the setter if it is present, so
    // a param call to a missing child node is never issued blind.
    auto* probe = new QDBusInterface(
        kService, childPath, QStringLiteral("org.freedesktop.DBus.Properties"), bus, this);
    auto probePending = probe->asyncCall(QStringLiteral("GetAll"), iface);
    auto* w = new QDBusPendingCallWatcher(probePending, this);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [fire, probe](QDBusPendingCallWatcher* watcher) {
                watcher->deleteLater();
                probe->deleteLater();
                if (!watcher->isError())
                    fire();
            });
}

} // namespace logiops_gui
