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
#ifndef LOGIOPS_GUI_BUTTONSMODEL_H
#define LOGIOPS_GUI_BUTTONSMODEL_H

#include <QAbstractListModel>
#include <QDBusConnection>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

namespace logiops_gui {

class ConfigState;

/*
 * ButtonsModel — the C++<->QML bridge for the Buttons tab (BTN-01..04, HOST-01).
 *
 * A QAbstractListModel over the daemon's `.Buttons.Enumerate()` surface. Each row
 * is one remappable/reportable control: its identity (ControlID — NOT the
 * enumeration index, RESEARCH Pitfall 4), TaskID, the Remappable / GestureSupport
 * gates, and the CURRENT action type + a human summary (BTN-04, read back by
 * introspecting which `.Action.<X>` interface exists at the button node).
 *
 * Reassignment is a strict TWO-STEP async sequence (RESEARCH Pitfall 2 /
 * T-3-02-04): first `Button.SetAction(type)` creates the action interface on the
 * button node, and ONLY after that reply lands do we open a generic
 * `QDBusInterface "pizza.pixl.LogiOps.Action.<type>"` at the SAME path for the
 * per-type parameter setter. Calling the param setter before SetAction would hit
 * UnknownMethod, so ordering is enforced via QDBusPendingCallWatcher.
 *
 * Security (T-3-02-02): setChangeHost VALIDATES the host string is "1".."N" /
 * "next" / "prev" BEFORE any D-Bus call — the daemon's std::stoi throws on
 * garbage, so we never forward an unparseable host. The host upper bound N comes
 * from the device (DeviceController.hostCount, wired from the daemon's new
 * ChangeHost.GetHostCount, HOST-01). T-3-02-03: SetAction is never sent for a
 * non-remappable button (the QML hotspot is gated on RemappableRole; the model
 * also refuses it defensively).
 *
 * Test seam (mirrors DeviceModel / DeviceController "drive via public methods, no
 * live bus"): enumeration is injected via seedButtons(); the two-step dispatch is
 * funnelled through the virtual performSetAction() / performParamCall() hooks so a
 * test subclass can record the call ORDER and arguments without a bus. The live
 * path overrides them to issue the actual async D-Bus calls.
 */
class ButtonsModel : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int hostCount READ hostCount WRITE setHostCount NOTIFY hostCountChanged)
    // Row count exposed as a NOTIFYable property: the row set is populated by an
    // async Enumerate, so QML visibility bindings must react to it changing
    // (a bare rowCount() method call would bind once, while empty, and go stale).
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        ControlIDRole = Qt::UserRole + 1,
        TaskIDRole,
        RemappableRole,
        GestureSupportRole,
        CurrentActionTypeRole,    // e.g. "Keypress", "ChangeHost", "None"
        CurrentActionSummaryRole, // human one-liner, e.g. "Ctrl + C", "Host 2"
        ButtonNameRole,           // display label, e.g. "Button (CID 0x52)"
        ButtonPathRole,           // the .../buttons/M object path (gesture scope)
    };
    Q_ENUM(Roles)

    // Per-row snapshot. The live path fills it from the .Button proxy + the
    // present .Action.<X> readback; the test injects it via seedButtons().
    struct ButtonInfo {
        QString path;             // .../devices/N/buttons/M object path
        quint16 controlId = 0;
        quint16 taskId = 0;
        bool remappable = false;
        bool gestureSupport = false;
        QString currentActionType = QStringLiteral("None");
        QString currentActionSummary;
        QString name;
    };

    // Live constructor: the device object-path + the bus the GUI is on. Kicks off
    // the async Enumerate + per-button property/action reads.
    explicit ButtonsModel(const QString& devicePath, const QDBusConnection& bus,
                          QObject* parent = nullptr);
    // Test constructor: no bus, no async — drive rows via seedButtons().
    explicit ButtonsModel(QObject* parent = nullptr);
    ~ButtonsModel() override;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int hostCount() const { return _hostCount; }
    void setHostCount(int count);

    // --- Two-step reassign + typed param setters (live-apply). ---
    // Step 1 for every assignment: SetAction(type). For param-less actions
    // (ToggleSmartShift / ToggleHiresScroll / None) this is the whole operation.
    Q_INVOKABLE void setAction(int row, const QString& type);
    // Step 1 + step 2 for parametric actions. Each issues SetAction(<type>) then,
    // on its reply, the per-type setter on the now-present .Action.<type>.
    Q_INVOKABLE void setKeypress(int row, const QStringList& evdevNames);
    Q_INVOKABLE void setChangeDpi(int row, int change);
    Q_INVOKABLE void setCycleDpi(int row, const QList<int>& dpis);
    // Validates host is "1".."N" / "next" / "prev" (case-insensitive) BEFORE any
    // D-Bus call (T-3-02-02). Returns false (and dispatches nothing) on a rejected
    // host so QML can surface the rejection.
    Q_INVOKABLE bool setChangeHost(int row, const QString& host);
    Q_INVOKABLE void setChangeProfile(int row, const QString& name);
    // Param-less convenience wrappers.
    Q_INVOKABLE void setToggleSmartShift(int row);
    Q_INVOKABLE void setToggleHiresScroll(int row);
    Q_INVOKABLE void clearAction(int row); // SetAction("None") -> "Disabled"

    // --- Test / construction seam. ---
    // Replace all rows with an injected snapshot (the parsed Enumerate result, or
    // a test fixture). Emits a model reset.
    void seedButtons(const QVector<ButtonInfo>& buttons);
    [[nodiscard]] const QVector<ButtonInfo>& rows() const { return _rows; }

    // CONF-01 (WR-03): inject the global dirty-tracker. Every successful reassign
    // marks it dirty so the "Unsaved changes" pill appears. Null in the test path.
    void setConfigState(ConfigState* configState) { _configState = configState; }

signals:
    void hostCountChanged();
    void countChanged();
    // Emitted when a host string is rejected by setChangeHost validation (so the
    // QML panel can show "invalid host" instead of silently dropping it).
    void hostRejected(int row, const QString& host);

protected:
    // The two-step dispatch hooks. The live path issues the async D-Bus calls;
    // the test subclass records (path, type) / (path, type, method, args) to
    // assert ORDERING and arguments with no bus. paramCall runs AFTER setAction's
    // reply lands (enforced by the live override via QDBusPendingCallWatcher).
    virtual void performSetAction(int row, const QString& type);
    virtual void performParamCall(int row, const QString& type,
                                  const QString& method, const QVariantList& args);

    // Optimistically update a row's current-action type/summary + emit dataChanged
    // (so the binding list reflects the reassign immediately, live-apply model).
    void applyCurrentAction(int row, const QString& type, const QString& summary);

    [[nodiscard]] bool validRow(int row) const { return row >= 0 && row < _rows.size(); }

    QVector<ButtonInfo> _rows;
    int _hostCount = 3; // overwritten by setHostCount() from DeviceController (HOST-01).

private:
    void enumerate(); // live: async .Buttons.Enumerate()

    QString _devicePath;
    QDBusConnection _bus;
    bool _live = false;

    // CONF-01 dirty-tracker (WR-03). Not owned; may be null (test path).
    ConfigState* _configState = nullptr;
};

} // namespace logiops_gui

#endif // LOGIOPS_GUI_BUTTONSMODEL_H
