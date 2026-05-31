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
#ifndef LOGIOPS_GUI_GESTUREMODEL_H
#define LOGIOPS_GUI_GESTUREMODEL_H

#include <QDBusConnection>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

QT_FORWARD_DECLARE_CLASS(QDBusPendingCallWatcher)

namespace logiops_gui {

class ConfigState;

/*
 * GestureModel — the C++<->QML brain of the guided gesture builder
 * (GEST-01..04). Unlike ButtonsModel (row = button), this model is scoped to
 * ONE selected button and represents the 4 cardinal directions of that
 * button's GestureAction. QML renders only; ALL business logic (mode
 * allowlist, two-step ordering, preview composition, granularity readout)
 * lives here (locked project rule: zero logic in QML/JS).
 *
 * Two-step live mode-switch (RESEARCH Pattern 1, mirrors ButtonsModel):
 * switching a direction's MODE is `Action.Gesture.SetGesture(direction, type)`
 * on the BUTTON node, and ONLY after that reply lands do we open a generic
 * `QDBusInterface "pizza.pixl.LogiOps.Gesture.<mode>"` at the .../gestures/{dir}
 * child node for the per-mode param setter. The .Gesture.<mode> interface does
 * not exist until SetGesture rebuilds the node, so ordering is mandatory.
 *
 * Mode allowlist (V5 input validation, T-04-02-01/02): the ONLY type strings
 * ever sent are OnInterval / OnRelease / Axis / None. Mode 2 is canonically
 * "OnRelease" — "OnThreshold" collides on interface_name and throws
 * InvalidGesture, so it is NEVER sent; "Nothing" maps to "None" (makeGesture
 * branches on interface_name "None"), NOT "NoPress". Unknown direction/mode is
 * rejected BEFORE any dispatch.
 *
 * Test seam (mirrors ButtonsModel): the two-step dispatch funnels through the
 * virtual performSetGesture()/performParamCall() hooks so a recording subclass
 * can assert call ORDER + args with no live bus. The live path overrides them
 * to issue the actual async D-Bus calls.
 */
class GestureModel : public QObject {
    Q_OBJECT

    // The plain-language preview sentence (GEST-04), composed in C++ from the
    // active direction's {mode, action, granularity}. NOTIFYable so the QML
    // preview card re-binds whenever the user edits the gesture.
    Q_PROPERTY(QString previewSentence READ previewSentence NOTIFY previewChanged)
    // The direction the builder body is currently editing (up/down/left/right).
    Q_PROPERTY(QString activeDirection READ activeDirection WRITE setActiveDirection
                       NOTIFY activeDirectionChanged)
    // The active direction's granularity human readout ("~1 step per small flick").
    Q_PROPERTY(QString granularityReadout READ granularityReadout NOTIFY previewChanged)
    // The active direction's current plain-language mode. NOTIFYable so the QML
    // progressive-disclosure sections re-bind when the mode changes (WR-02): a
    // bare modeOf() method binding goes stale because previewChanged is not one of
    // its tracked dependencies. previewChanged fires on both setMode (when editing
    // the active direction) and setActiveDirection, covering every transition.
    Q_PROPERTY(QString activeMode READ activeMode NOTIFY previewChanged)

public:
    // Live constructor: the BUTTON object-path (.../buttons/M) + the bus the GUI
    // is on. SetGesture/param calls target this node and its gestures/{dir} kids.
    explicit GestureModel(const QString& buttonPath, const QDBusConnection& bus,
                          QObject* parent = nullptr);
    // Test constructor: no bus, no async — drive via setMode()/setGranularity().
    explicit GestureModel(QObject* parent = nullptr);
    ~GestureModel() override;

    // --- Mode switch (two-step). Validates direction + maps plainMode through
    // the locked allowlist BEFORE any dispatch. Returns false (dispatching
    // nothing) on an unknown direction OR mode (T-04-02-01). ---
    Q_INVOKABLE bool setMode(const QString& direction, const QString& plainMode);

    // --- Per-mode granularity (interval / threshold / multiplier depending on
    // the direction's current mode). Clamps to sane positive bounds before
    // dispatch (T-04-02-03); reject <= 0 except the 0-reset sentinel. ---
    Q_INVOKABLE bool setGranularity(const QString& direction, int rawValue);

    // --- The discrete-mode action setter (OnInterval / OnRelease). Records the
    // chosen action so the preview can name it; dispatches SetAction. ---
    Q_INVOKABLE bool setGestureAction(const QString& direction, const QString& actionType);

    // --- Active-direction selector (drives the preview/readout). ---
    [[nodiscard]] QString activeDirection() const { return _active; }
    void setActiveDirection(const QString& direction);

    [[nodiscard]] QString previewSentence() const;
    [[nodiscard]] QString granularityReadout() const;
    // The active direction's plain-language mode (empty if unset). Backs the
    // NOTIFYable activeMode property; same value modeOf(_active) returns.
    [[nodiscard]] QString activeMode() const { return modeOf(_active); }

    // Query whether a direction has a non-"Nothing" gesture (the configured-dot).
    Q_INVOKABLE bool isConfigured(const QString& direction) const;
    // The current plain-language mode for a direction (empty if unset).
    Q_INVOKABLE QString modeOf(const QString& direction) const;

    // The four plain-language modes, in fixed UI order (single source of truth
    // so QML never hardcodes mode strings).
    Q_INVOKABLE static QStringList plainModes();

    // CONF-01 seam: inject the global dirty-tracker. Every successful edit marks
    // it dirty so the "Unsaved changes" pill appears. Null in the test path.
    void setConfigState(ConfigState* configState) { _configState = configState; }

signals:
    void previewChanged();
    void activeDirectionChanged();
    void configuredChanged(const QString& direction);
    // Emitted when a direction/mode/granularity is rejected by validation (so
    // QML can surface the rejection instead of silently dropping it).
    void editRejected(const QString& direction, const QString& reason);

protected:
    // The two-step dispatch hooks. The live path issues the async D-Bus calls;
    // the recording subclass records (direction, type) / (direction, type,
    // method, args) to assert ORDER + args with no bus. performParamCall runs
    // AFTER performSetGesture's reply lands (live override sequences it).
    virtual void performSetGesture(const QString& direction, const QString& type);
    virtual void performParamCall(const QString& direction, const QString& type,
                                  const QString& method, const QVariantList& args);

private:
    // Per-direction builder state (drives preview + configured-dot).
    struct DirectionState {
        QString mode;            // daemon type: OnInterval/OnRelease/Axis/None
        QString plainMode;       // the plain-language label
        QString actionType;      // chosen gesture action type (discrete modes)
        int granularity = 0;     // raw interval/threshold/multiplier value
    };

    // Map a plain-language mode to a daemon type via the locked allowlist.
    // Returns false if the plain mode is unknown (no dispatch).
    static bool mapMode(const QString& plainMode, QString& typeOut);
    static bool validDirection(const QString& direction);

    // Compose the preview sentence + granularity phrase for a DirectionState.
    static QString composePreview(const QString& direction, const DirectionState& st);
    static QString granularityPhrase(int rawValue);
    static QString actionVerbPhrase(const QString& actionType);

    DirectionState& stateFor(const QString& direction);

    QHash<QString, DirectionState> _dirs;
    QString _active = QStringLiteral("up");

    QString _buttonPath;
    QDBusConnection _bus;
    bool _live = false;

    // WR-01: the in-flight SetGesture watcher per direction. A param call for a
    // direction is chained behind THIS watcher's reply (the .Gesture.<type> child
    // node does not exist until SetGesture rebuilds it), so a param issued right
    // after setMode is never fired blind at a not-yet-created interface. Cleared
    // when the watcher finishes. Live path only.
    QHash<QString, QDBusPendingCallWatcher*> _pendingSetGesture;

    ConfigState* _configState = nullptr;
};

} // namespace logiops_gui

#endif // LOGIOPS_GUI_GESTUREMODEL_H
