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
        "ToggleSmartShift", "ToggleHiresScroll", "None",
    };

    // Human one-liner for the binding list (BTN-04). Parametric summaries are
    // refined by applyCurrentAction when the param is known; this is the default.
    QString defaultSummary(const QString& type) {
        if (type == QLatin1String("None") || type.isEmpty())
            return QObject::tr("Disabled");
        if (type == QLatin1String("Keypress")) return QObject::tr("Keystroke");
        if (type == QLatin1String("ChangeDPI")) return QObject::tr("Change DPI");
        if (type == QLatin1String("CycleDPI")) return QObject::tr("Cycle DPI");
        if (type == QLatin1String("ChangeHost")) return QObject::tr("Switch host");
        if (type == QLatin1String("ChangeProfile")) return QObject::tr("Switch profile");
        if (type == QLatin1String("ToggleSmartShift")) return QObject::tr("Toggle SmartShift");
        if (type == QLatin1String("ToggleHiresScroll")) return QObject::tr("Toggle hi-res scroll");
        return type;
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
}

void ButtonsModel::setKeypress(int row, const QStringList& evdevNames) {
    if (!validRow(row) || !_rows.at(row).remappable || evdevNames.isEmpty())
        return;
    performSetAction(row, QStringLiteral("Keypress"));
    performParamCall(row, QStringLiteral("Keypress"), QStringLiteral("SetKeys"),
                     {QVariant::fromValue(evdevNames)});
    applyCurrentAction(row, QStringLiteral("Keypress"), evdevNames.join(QStringLiteral(" + ")));
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
}

void ButtonsModel::setCycleDpi(int row, const QList<int>& dpis) {
    if (!validRow(row) || !_rows.at(row).remappable)
        return;
    performSetAction(row, QStringLiteral("CycleDPI"));
    performParamCall(row, QStringLiteral("CycleDPI"), QStringLiteral("SetDPIs"),
                     {QVariant::fromValue(dpis)});
    applyCurrentAction(row, QStringLiteral("CycleDPI"), tr("Cycle DPI"));
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
    return true;
}

void ButtonsModel::setChangeProfile(int row, const QString& name) {
    if (!validRow(row) || !_rows.at(row).remappable || name.isEmpty())
        return;
    performSetAction(row, QStringLiteral("ChangeProfile"));
    performParamCall(row, QStringLiteral("ChangeProfile"), QStringLiteral("SetProfile"),
                     {QVariant::fromValue(name)});
    applyCurrentAction(row, QStringLiteral("ChangeProfile"), tr("Profile %1").arg(name));
}

void ButtonsModel::setToggleSmartShift(int row) {
    setAction(row, QStringLiteral("ToggleSmartShift"));
}

void ButtonsModel::setToggleHiresScroll(int row) {
    setAction(row, QStringLiteral("ToggleHiresScroll"));
}

void ButtonsModel::clearAction(int row) {
    if (!validRow(row) || !_rows.at(row).remappable)
        return;
    performSetAction(row, QStringLiteral("None"));
    applyCurrentAction(row, QStringLiteral("None"), tr("Disabled"));
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
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [btn](QDBusPendingCallWatcher* watcher) {
                watcher->deleteLater();
                btn->deleteLater();
                // SetAction reconfigures the hardware itself (RemapButton.cpp);
                // an error here means the button refused the type (e.g. raced a
                // non-remappable). The row's optimistic state already reflects the
                // intent; no further action needed on the happy path.
            });
}

void ButtonsModel::performParamCall(int row, const QString& type,
                                    const QString& method, const QVariantList& args) {
    if (!_live || !validRow(row))
        return;
    const QString path = _rows.at(row).path;
    const QString iface = QStringLiteral("pizza.pixl.LogiOps.Action.") + type;
    const QDBusConnection bus = _bus;

    // Sequence step 2 behind step 1: re-issue SetAction here is NOT needed (the
    // public setters already called performSetAction). Instead we must wait for
    // the .Action.<type> interface to exist. Since both calls are queued on the
    // same connection in order, a defensive 0ms event-loop hop guarantees the
    // SetAction reply (and thus the interface creation) is processed first. Use a
    // QDBusPendingCallWatcher on a cheap Introspect to chain after SetAction.
    auto* probe = new QDBusInterface(
        kService, path, QStringLiteral("org.freedesktop.DBus.Properties"), bus, this);
    auto pending = probe->asyncCall(
        QStringLiteral("GetAll"), iface);
    auto* w = new QDBusPendingCallWatcher(pending, this);
    connect(w, &QDBusPendingCallWatcher::finished, this,
            [this, path, iface, method, args, probe, bus](QDBusPendingCallWatcher* watcher) {
                watcher->deleteLater();
                probe->deleteLater();
                // Whether or not the probe succeeded, the SetAction that preceded
                // it on this ordered connection has now been processed, so the
                // .Action.<type> interface exists. Fire the param setter.
                auto* action = new QDBusInterface(kService, path, iface, bus, this);
                auto reply = action->asyncCallWithArgumentList(method, args);
                auto* pw = new QDBusPendingCallWatcher(reply, this);
                connect(pw, &QDBusPendingCallWatcher::finished, this,
                        [action](QDBusPendingCallWatcher* w2) {
                            w2->deleteLater();
                            action->deleteLater();
                        });
            });
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
