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
#include "ButtonsModel.h"

#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QRegularExpression>

#include <functional>
#include <memory>

#include "ConfigState.h"
#include "logid_button_proxy.h"
#include "logid_buttons_proxy.h"

namespace logiops_gui {

namespace {
    // The daemon's well-known bus name (mirrors ipc_defs SERVICE_ROOT_NAME).
    const QString kService = QStringLiteral("pizza.pixl.LogiOps");

    // The action interface names the daemon recognizes (each action's
    // interface_name; "None" -> NullAction/clear). Used to introspect the CURRENT
    // binding (BTN-04) and to gate setAction types.
    const char* kActionTypes[] = {
        "Keypress", "ChangeDPI", "CycleDPI", "ChangeHost", "ChangeProfile",
        "ToggleSmartShift", "ToggleHiresScroll", "Gesture", "None",
    };

    // Human one-liner for the binding list (BTN-04). Parametric summaries are
    // refined by applyCurrentAction when the param is known; this is the default.
    QString defaultSummary(const QString& type) {
        if (type == QLatin1String("None") || type.isEmpty())
            return QObject::tr("Disabled");
        // "Default" is the synthetic un-divert pseudo-type: the button regains its
        // native hardware function. The daemon never reports it as a present
        // .Action.<X> interface (it carries NO action), so an enumerate read-back of
        // a restored button comes back as "None"; the optimistic row uses this label.
        if (type == QLatin1String("Default")) return QObject::tr("Default (native)");
        if (type == QLatin1String("Keypress")) return QObject::tr("Keystroke");
        if (type == QLatin1String("ChangeDPI")) return QObject::tr("Change DPI");
        if (type == QLatin1String("CycleDPI")) return QObject::tr("Cycle DPI");
        if (type == QLatin1String("ChangeHost")) return QObject::tr("Switch host");
        if (type == QLatin1String("ChangeProfile")) return QObject::tr("Switch profile");
        if (type == QLatin1String("ToggleSmartShift")) return QObject::tr("Toggle SmartShift");
        if (type == QLatin1String("ToggleHiresScroll")) return QObject::tr("Toggle hi-res scroll");
        return type;
    }

    // Human label for a BTN_* mouse-button re-emit (routed through Keypress). When
    // the evdev name is not a known mouse button, fall back to the raw name.
    QString mouseButtonSummary(const QString& btnName) {
        if (btnName == QLatin1String("BTN_MIDDLE")) return QObject::tr("Middle click");
        if (btnName == QLatin1String("BTN_LEFT")) return QObject::tr("Left click");
        if (btnName == QLatin1String("BTN_RIGHT")) return QObject::tr("Right click");
        if (btnName == QLatin1String("BTN_BACK")) return QObject::tr("Back");
        if (btnName == QLatin1String("BTN_FORWARD")) return QObject::tr("Forward");
        if (btnName == QLatin1String("BTN_SIDE")) return QObject::tr("Side");
        if (btnName == QLatin1String("BTN_EXTRA")) return QObject::tr("Extra");
        return btnName;
    }
}

ButtonsModel::ButtonsModel(const QString& devicePath, const QDBusConnection& bus,
                           QObject* parent)
    : QAbstractListModel(parent), _devicePath(devicePath), _bus(bus), _live(true) {
    enumerate();
}

ButtonsModel::ButtonsModel(QObject* parent)
    : QAbstractListModel(parent), _bus(QDBusConnection::sessionBus()), _live(false) {}

ButtonsModel::~ButtonsModel() = default;

int ButtonsModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return _rows.size();
}

QVariant ButtonsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= _rows.size())
        return {};
    const ButtonInfo& b = _rows.at(index.row());
    switch (role) {
        case ControlIDRole:           return b.controlId;
        case TaskIDRole:              return b.taskId;
        case RemappableRole:          return b.remappable;
        case GestureSupportRole:      return b.gestureSupport;
        case CurrentActionTypeRole:   return b.currentActionType;
        case CurrentActionSummaryRole:return b.currentActionSummary;
        case ButtonNameRole:          return b.name;
        case ButtonPathRole:          return b.path;
        default:                      return {};
    }
}

QHash<int, QByteArray> ButtonsModel::roleNames() const {
    return {
        {ControlIDRole,            "controlId"},
        {TaskIDRole,               "taskId"},
        {RemappableRole,           "remappable"},
        {GestureSupportRole,       "gestureSupport"},
        {CurrentActionTypeRole,    "currentActionType"},
        {CurrentActionSummaryRole, "currentActionSummary"},
        {ButtonNameRole,           "buttonName"},
        {ButtonPathRole,           "buttonPath"},
    };
}

void ButtonsModel::setHostCount(int count) {
    if (count == _hostCount)
        return;
    _hostCount = count;
    emit hostCountChanged();
}

void ButtonsModel::seedButtons(const QVector<ButtonInfo>& buttons) {
    beginResetModel();
    _rows = buttons;
    for (ButtonInfo& b : _rows) {
        if (b.currentActionSummary.isEmpty())
            b.currentActionSummary = defaultSummary(b.currentActionType);
        if (b.name.isEmpty())
            b.name = tr("Button (CID 0x%1)").arg(b.controlId, 0, 16);
    }
    endResetModel();
    emit countChanged();
}

// ---------------------------------------------------------------------------
// Reassignment (two-step). Step 1 = SetAction(type) for ALL assignments. Param
// setters chain step 2 in the live override of performParamCall (after SetAction
// resolves). T-3-02-03: never SetAction a non-remappable button.
// ---------------------------------------------------------------------------

void ButtonsModel::setAction(int row, const QString& type) {
    if (!validRow(row) || !_rows.at(row).remappable)
        return;
    performSetAction(row, type);
    applyCurrentAction(row, type, defaultSummary(type));
    reconcileFromDaemon(row);
}

void ButtonsModel::setKeypress(int row, const QStringList& evdevNames) {
    if (!validRow(row) || !_rows.at(row).remappable || evdevNames.isEmpty())
        return;
    performSetAction(row, QStringLiteral("Keypress"));
    performParamCall(row, QStringLiteral("Keypress"), QStringLiteral("SetKeys"),
                     {QVariant::fromValue(evdevNames)});
    applyCurrentAction(row, QStringLiteral("Keypress"), evdevNames.join(QStringLiteral(" + ")));
    // performParamCall reconciles after step 2 lands; no direct reconcile here.
}

void ButtonsModel::setMouseButton(int row, const QString& btnName) {
    if (!validRow(row) || !_rows.at(row).remappable || btnName.isEmpty())
        return;
    // BTN_* are EV_KEY codes the daemon's InputDevice already registers, so a
    // mouse-button re-emit is just a single-key Keypress. Re-uses the proven
    // two-step path; the summary renders the human button label.
    performSetAction(row, QStringLiteral("Keypress"));
    performParamCall(row, QStringLiteral("Keypress"), QStringLiteral("SetKeys"),
                     {QVariant::fromValue(QStringList{btnName})});
    applyCurrentAction(row, QStringLiteral("Keypress"), mouseButtonSummary(btnName));
    // performParamCall reconciles after step 2 lands; no direct reconcile here.
}

void ButtonsModel::setChangeDpi(int row, int change) {
    if (!validRow(row) || !_rows.at(row).remappable)
        return;
    performSetAction(row, QStringLiteral("ChangeDPI"));
    performParamCall(row, QStringLiteral("ChangeDPI"), QStringLiteral("SetChange"),
                     {QVariant::fromValue(change)});
    applyCurrentAction(row, QStringLiteral("ChangeDPI"),
                       tr("Change DPI %1%2").arg(change >= 0 ? QStringLiteral("+") : QString())
                           .arg(change));
    // performParamCall reconciles after step 2 lands; no direct reconcile here.
}

void ButtonsModel::setCycleDpi(int row, const QList<int>& dpis) {
    if (!validRow(row) || !_rows.at(row).remappable)
        return;
    performSetAction(row, QStringLiteral("CycleDPI"));
    performParamCall(row, QStringLiteral("CycleDPI"), QStringLiteral("SetDPIs"),
                     {QVariant::fromValue(dpis)});
    applyCurrentAction(row, QStringLiteral("CycleDPI"), tr("Cycle DPI"));
    // performParamCall reconciles after step 2 lands; no direct reconcile here.
}

bool ButtonsModel::setChangeHost(int row, const QString& host) {
    if (!validRow(row) || !_rows.at(row).remappable)
        return false;

    // T-3-02-02: validate the host string BEFORE any D-Bus call. The daemon's
    // ChangeHostAction::setHost runs std::stoi on a non-"next"/"prev" string,
    // which THROWS on a non-numeric or empty value (unhandled -> DoS). Accept
    // only "next" / "prev" / "previous" (case-insensitive) or a 1-based integer
    // in [1, hostCount].
    const QString h = host.trimmed().toLower();
    bool allowed = false;
    if (h == QLatin1String("next") || h == QLatin1String("prev") ||
        h == QLatin1String("previous")) {
        allowed = true;
    } else {
        bool ok = false;
        const int n = h.toInt(&ok);
        if (ok && n >= 1 && n <= _hostCount)
            allowed = true;
    }
    if (!allowed) {
        emit hostRejected(row, host);
        return false;
    }

    performSetAction(row, QStringLiteral("ChangeHost"));
    performParamCall(row, QStringLiteral("ChangeHost"), QStringLiteral("SetHost"),
                     {QVariant::fromValue(h)});
    applyCurrentAction(row, QStringLiteral("ChangeHost"),
                       (h == QLatin1String("next") || h == QLatin1String("prev") ||
                        h == QLatin1String("previous"))
                           ? tr("Host %1").arg(h)
                           : tr("Host %1").arg(h.toInt()));
    // performParamCall reconciles after step 2 lands; no direct reconcile here.
    return true;
}

void ButtonsModel::setChangeProfile(int row, const QString& name) {
    if (!validRow(row) || !_rows.at(row).remappable || name.isEmpty())
        return;
    performSetAction(row, QStringLiteral("ChangeProfile"));
    performParamCall(row, QStringLiteral("ChangeProfile"), QStringLiteral("SetProfile"),
                     {QVariant::fromValue(name)});
    applyCurrentAction(row, QStringLiteral("ChangeProfile"), tr("Profile %1").arg(name));
    // performParamCall reconciles after step 2 lands; no direct reconcile here.
}

void ButtonsModel::setToggleSmartShift(int row) {
    setAction(row, QStringLiteral("ToggleSmartShift"));
}

void ButtonsModel::setToggleHiresScroll(int row) {
    setAction(row, QStringLiteral("ToggleHiresScroll"));
}

void ButtonsModel::restoreDefault(int row) {
    if (!validRow(row) || !_rows.at(row).remappable)
        return;
    // SetAction("Default") -> the daemon resets the config (no action) and
    // RemapButton drops the TemporaryDiverted bit, so the device handles the
    // control natively again. DISTINCT from clearAction("None"): "None" keeps the
    // button diverted-but-inert (dead); "Default" gives the button back its real
    // hardware function. No param step (Default carries no parameters).
    performSetAction(row, QStringLiteral("Default"));
    applyCurrentAction(row, QStringLiteral("Default"), tr("Default (native)"));
    reconcileFromDaemon(row);
}

void ButtonsModel::clearAction(int row) {
    if (!validRow(row) || !_rows.at(row).remappable)
        return;
    performSetAction(row, QStringLiteral("None"));
    applyCurrentAction(row, QStringLiteral("None"), tr("Disabled"));
    reconcileFromDaemon(row);
}

void ButtonsModel::applyCurrentAction(int row, const QString& type,
                                      const QString& summary) {
    if (!validRow(row))
        return;
    _rows[row].currentActionType = type;
    _rows[row].currentActionSummary = summary.isEmpty() ? defaultSummary(type) : summary;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {CurrentActionTypeRole, CurrentActionSummaryRole});
    // CONF-01 (WR-03): every reassign funnels through here, so mark the global
    // config dirty once the optimistic row update lands. No-op if unwired.
    if (_configState != nullptr)
        _configState->markDirty();
}

// ---------------------------------------------------------------------------
// Live two-step dispatch. performSetAction issues Button.SetAction(type); the
// param call is sequenced to fire only AFTER that reply lands (T-3-02-04: the
// .Action.<type> interface does not exist until SetAction creates it).
// ---------------------------------------------------------------------------

void ButtonsModel::performSetAction(int row, const QString& type) {
    if (!_live || !validRow(row))
        return;
    const QString path = _rows.at(row).path;
    auto* btn = new QDBusInterface(
        kService, path,
        QString::fromUtf8(PizzaPixlLogiOpsButtonInterface::staticInterfaceName()),
        _bus, this);
    auto pending = btn->asyncCall(QStringLiteral("SetAction"), type);
    auto* w = new QDBusPendingCallWatcher(pending, this);
    // Record this watcher as the pending step-1 for this path, so a subsequent
    // performParamCall (issued back-to-back by the public setter) can chain its
    // per-type setter off the ACTUAL SetAction reply rather than a separate
    // GetAll-probe that merely ASSUMES ordering and fires even on SetAction error.
    _pendingSetAction.insert(path, w);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, btn, path](QDBusPendingCallWatcher* watcher) {
                // Drop the pending registration once resolved; performParamCall
                // either already attached or there was no param step.
                if (_pendingSetAction.value(path) == watcher)
                    _pendingSetAction.remove(path);
                watcher->deleteLater();
                btn->deleteLater();
                // SetAction reconfigures the hardware itself (RemapButton.cpp);
                // an error here means the button refused the type. The optimistic
                // row already reflects the intent; reconcileFromDaemon (queued by
                // the setter) corrects it against the daemon's real applied action.
            });
}

void ButtonsModel::performParamCall(int row, const QString& type,
                                    const QString& method, const QVariantList& args) {
    if (!_live || !validRow(row))
        return;
    const QString path = _rows.at(row).path;
    const QString iface = QStringLiteral("pizza.pixl.LogiOps.Action.") + type;
    const QDBusConnection bus = _bus;

    // Step 2 must fire ONLY after step 1's SetAction reply lands (the
    // .Action.<type> interface does not exist until then) and ONLY on success (on a
    // SetAction error the interface is absent, so the param call would hit a
    // nonexistent interface and silently no-op while the row lies). Chain directly
    // off the SetAction watcher registered by performSetAction for this path.
    QDBusPendingCallWatcher* setActionWatcher = _pendingSetAction.value(path, nullptr);

    auto fireParam = [this, path, iface, method, args, bus]() {
        auto* action = new QDBusInterface(kService, path, iface, bus, this);
        auto reply = action->asyncCallWithArgumentList(method, args);
        auto* pw = new QDBusPendingCallWatcher(reply, this);
        connect(pw, &QDBusPendingCallWatcher::finished, this,
                [this, action, path](QDBusPendingCallWatcher* w2) {
                    w2->deleteLater();
                    action->deleteLater();
                    // Param landed (or errored) — reconcile the row against the
                    // daemon's real present action, replacing the optimistic guess.
                    // The param reply lands strictly after SetAction, so probe
                    // directly (no need to re-chain through the SetAction watcher).
                    const int r = rowForPath(path);
                    if (r >= 0)
                        doReconcile(r);
                });
    };

    if (setActionWatcher != nullptr) {
        connect(setActionWatcher, &QDBusPendingCallWatcher::finished, this,
                [fireParam](QDBusPendingCallWatcher* w) {
                    QDBusPendingReply<> reply = *w;
                    // Only fire the param setter when SetAction SUCCEEDED — on error
                    // the .Action.<type> interface was never created.
                    if (!reply.isError())
                        fireParam();
                });
    } else {
        // No pending SetAction (e.g. the interface already exists): fire directly.
        fireParam();
    }
}

void ButtonsModel::reconcileFromDaemon(int row) {
    if (!_live || !validRow(row))
        return;
    const QString path = _rows.at(row).path;
    // Chain the read-back off the in-flight SetAction reply (step 1) when present,
    // so the probe runs AFTER the daemon created/cleared the .Action.<X> interface
    // — not before it lands (which would race and wrongly read the OLD binding). For
    // param-less actions (setAction / restoreDefault / clearAction) this is the only
    // reconcile; for parametric actions performParamCall reconciles after step 2 and
    // passes through doReconcile directly, so it never double-probes prematurely.
    QDBusPendingCallWatcher* setActionWatcher = _pendingSetAction.value(path, nullptr);
    if (setActionWatcher != nullptr) {
        connect(setActionWatcher, &QDBusPendingCallWatcher::finished, this,
                [this, path](QDBusPendingCallWatcher*) {
                    const int r = rowForPath(path);
                    if (r >= 0)
                        doReconcile(r);
                });
    } else {
        doReconcile(row);
    }
}

int ButtonsModel::rowForPath(const QString& path) const {
    for (int i = 0; i < _rows.size(); ++i)
        if (_rows.at(i).path == path)
            return i;
    return -1;
}

void ButtonsModel::doReconcile(int row) {
    if (!_live || !validRow(row))
        return;
    const QString path = _rows.at(row).path;
    // Re-probe which .Action.<X> interface is present at the button node (the same
    // discovery enumerate() uses) and reconcile the row to it, so the binding list
    // reflects the daemon's REAL applied action rather than the optimistic guess.
    // Done async so it does not block; the optimistic update stays for snappiness.
    auto* props = new QDBusInterface(
        kService, path, QStringLiteral("org.freedesktop.DBus.Properties"), _bus, this);
    // Probe each candidate action interface in turn via GetAll; the first that
    // resolves is the present binding. We chain sequentially to keep ordering
    // deterministic and avoid a burst of parallel calls.
    auto* index = new int(0);
    auto* self = this;
    auto runner = std::make_shared<std::function<void()>>();
    *runner = [self, props, path, index, runner]() {
        // Skip "None" (it is the absence of any .Action.<X>); reaching the end
        // means no action interface is present -> the button is "None"/default.
        while (*index < static_cast<int>(sizeof(kActionTypes) / sizeof(kActionTypes[0])) &&
               qstrcmp(kActionTypes[*index], "None") == 0) {
            ++(*index);
        }
        if (*index >= static_cast<int>(sizeof(kActionTypes) / sizeof(kActionTypes[0]))) {
            // No present action interface: daemon reports no action (un-diverted
            // default OR disabled). Only correct the row if it optimistically
            // claimed a concrete action that did NOT take. Preserve the user-facing
            // "Default (native)" vs "Disabled" distinction the optimistic update set
            // (the daemon cannot tell them apart over D-Bus — both carry no action).
            const int r = self->rowForPath(path);
            if (r >= 0) {
                const QString cur = self->_rows.at(r).currentActionType;
                if (cur != QLatin1String("None") && cur != QLatin1String("Default")) {
                    self->applyCurrentAction(r, QStringLiteral("None"),
                                             QObject::tr("Disabled"));
                }
            }
            props->deleteLater();
            delete index;
            return;
        }
        const QString type = QString::fromUtf8(kActionTypes[*index]);
        const QString iface = QStringLiteral("pizza.pixl.LogiOps.Action.") + type;
        auto pending = props->asyncCall(QStringLiteral("GetAll"), iface);
        auto* w = new QDBusPendingCallWatcher(pending, self);
        QObject::connect(w, &QDBusPendingCallWatcher::finished, self,
                [self, props, path, index, iface, type, runner](QDBusPendingCallWatcher* watcher) {
                    QDBusPendingReply<QVariantMap> reply = *watcher;
                    watcher->deleteLater();
                    if (!reply.isError()) {
                        // This interface is present -> the real applied action.
                        const int r = self->rowForPath(path);
                        if (r >= 0 &&
                            self->_rows.at(r).currentActionType != type) {
                            self->applyCurrentAction(r, type,
                                                     defaultSummary(type));
                        }
                        props->deleteLater();
                        delete index;
                        return;
                    }
                    ++(*index);
                    (*runner)();
                });
    };
    (*runner)();
}

void ButtonsModel::enumerate() {
    if (!_live)
        return;
    auto* buttons = new PizzaPixlLogiOpsButtonsInterface(kService, _devicePath, _bus, this);
    auto reply = buttons->Enumerate();
    auto* w = new QDBusPendingCallWatcher(reply, this);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, buttons](QDBusPendingCallWatcher* watcher) {
                QDBusPendingReply<QList<QDBusObjectPath>> r = *watcher;
                watcher->deleteLater();
                buttons->deleteLater();
                if (r.isError())
                    return;
                QVector<ButtonInfo> rows;
                const QList<QDBusObjectPath> paths = r.value();
                rows.reserve(paths.size());
                for (const QDBusObjectPath& op : paths) {
                    ButtonInfo b;
                    b.path = op.path();
                    // Read the .Button identity/gating props synchronously via the
                    // generated typed accessors over a short-lived proxy. These are
                    // cheap cached daemon properties (no hardware I/O), so this does
                    // not block on the wire meaningfully; the heavy enumerate call
                    // above was already async.
                    auto* btn = new QDBusInterface(
                        kService, b.path,
                        QString::fromUtf8(
                            PizzaPixlLogiOpsButtonInterface::staticInterfaceName()),
                        _bus, this);
                    b.controlId = static_cast<quint16>(
                        btn->property("ControlID").toUInt());
                    b.taskId = static_cast<quint16>(btn->property("TaskID").toUInt());
                    b.remappable = btn->property("Remappable").toBool();
                    b.gestureSupport = btn->property("GestureSupport").toBool();
                    btn->deleteLater();

                    // BTN-04: read back the CURRENT binding by probing which
                    // .Action.<X> interface is present at the button node. The
                    // FIRST present one wins; none present -> "None"/Disabled.
                    b.currentActionType = QStringLiteral("None");
                    QDBusInterface props(
                        kService, b.path,
                        QStringLiteral("org.freedesktop.DBus.Properties"), _bus);
                    for (const char* t : kActionTypes) {
                        if (qstrcmp(t, "None") == 0)
                            continue;
                        const QString iface =
                            QStringLiteral("pizza.pixl.LogiOps.Action.") +
                            QString::fromUtf8(t);
                        // A successful GetAll on the .Action.<X> interface means it
                        // is the current binding; an absent interface errors out.
                        QDBusReply<QVariantMap> got =
                            props.call(QStringLiteral("GetAll"), iface);
                        if (got.isValid()) {
                            b.currentActionType = QString::fromUtf8(t);
                            break;
                        }
                    }
                    // HOST-01: if this button currently carries a ChangeHost
                    // action, the daemon exposes the device's paired-host count on
                    // that action interface (ChangeHost.GetHostCount, added in this
                    // plan). Read it best-effort to render accurate host slots; a
                    // 0/absent count leaves the default (next/prev always cover it).
                    if (b.currentActionType == QLatin1String("ChangeHost")) {
                        QDBusInterface ch(
                            kService, b.path,
                            QStringLiteral("pizza.pixl.LogiOps.Action.ChangeHost"), _bus);
                        QDBusReply<uchar> hc = ch.call(QStringLiteral("GetHostCount"));
                        if (hc.isValid() && static_cast<int>(hc.value()) > 0)
                            setHostCount(static_cast<int>(hc.value()));
                    }

                    b.currentActionSummary = defaultSummary(b.currentActionType);
                    b.name = tr("Button (CID 0x%1)").arg(b.controlId, 0, 16);
                    rows.push_back(b);
                }
                beginResetModel();
                _rows = rows;
                endResetModel();
                emit countChanged();
            });
}

} // namespace logiops_gui
