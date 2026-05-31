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
#ifndef LOGIOPS_GUI_DEVICECONTROLLER_H
#define LOGIOPS_GUI_DEVICECONTROLLER_H

#include <QDBusConnection>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class PizzaPixlLogiOpsDPIInterface;
class PizzaPixlLogiOpsSmartShiftInterface;
class PizzaPixlLogiOpsHiresScrollInterface;
class PizzaPixlLogiOpsThumbWheelInterface;
class PizzaPixlLogiOpsButtonsInterface;

namespace logiops_gui {

class ConfigState;
class GestureModel;

/*
 * DeviceController — the per-device C++<->QML bridge for the Phase 3 config UI.
 *
 * Each selected device gets one DeviceController. On construction it asks the
 * daemon "which feature interfaces are present at this device path?" via
 * org.freedesktop.DBus.Introspectable.Introspect() (async — a slow/asleep device
 * must never block the UI thread, T-3-01-02), parses the returned XML for the
 * Phase 3 interface names, caches that present-set, and lazily builds the typed
 * feature proxies (from Plan 00) only for the interfaces that exist (T-3-01-03:
 * never call a method on an absent interface).
 *
 * It then exposes:
 *   - capability flags (NOTIFY): hasSmartShift / hasHires / hasThumbwheel /
 *     hasButtons / hasTorque, plus dpiMin / dpiMax / dpiStep / sensorCount /
 *     hostCount. The Wave-2 tab plans gate whole tabs/sections on these
 *     (UI-SPEC omission rule).
 *   - live config values (NOTIFY) the tab plans bind two-way: dpi,
 *     smartShiftActive / smartShiftThreshold / smartShiftTorque, hiresOn /
 *     hiresInvert, thumbDivert / thumbInvert — seeded from the Get* reads on
 *     selection and updated OPTIMISTICALLY by the Q_INVOKABLE setters (the change
 *     signal fires immediately so the slider/toggle stays responsive; the async
 *     D-Bus setter applies live in the background).
 *
 * ALL marshalling/async lives here in C++ — QML renders only (CONTEXT.md).
 *
 * Test seam: introspectInterfaces() / the value-read entry points are virtual /
 * separately invokable so DeviceControllerTest can inject a fake present-set and
 * fake DPI bounds without a live bus (mirrors the Phase 2 DeviceModelTest
 * "drive via public methods, no bus" precedent). hostCount is NOT exposed over
 * D-Bus (see Plan 02 daemon-addition decision); it defaults to 3 behind a
 * Q_PROPERTY so Plan 02 can wire the real value without reshaping this class.
 */
class DeviceController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString devicePath READ devicePath CONSTANT)

    // --- Capability flags (gating spine). ---
    Q_PROPERTY(bool hasSmartShift READ hasSmartShift NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasHires READ hasHires NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasThumbwheel READ hasThumbwheel NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasButtons READ hasButtons NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasDpi READ hasDpi NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool hasTorque READ hasTorque NOTIFY capabilitiesChanged)
    Q_PROPERTY(int dpiMin READ dpiMin NOTIFY capabilitiesChanged)
    Q_PROPERTY(int dpiMax READ dpiMax NOTIFY capabilitiesChanged)
    Q_PROPERTY(int dpiStep READ dpiStep NOTIFY capabilitiesChanged)
    Q_PROPERTY(int sensorCount READ sensorCount NOTIFY capabilitiesChanged)
    Q_PROPERTY(int hostCount READ hostCount NOTIFY capabilitiesChanged)

    // --- Live config values (two-way bound by the Wave-2 tab plans). ---
    Q_PROPERTY(int dpi READ dpi NOTIFY dpiChanged)
    // DPI-cycle presets: a list of {value:int, label:string} maps the Pointer-tab
    // editor binds to (DPI-02 / DPI-03). Device/profile-scoped via .DPI
    // GetPresets/SetPresets (option-a, Task 0).
    Q_PROPERTY(QVariantList dpiPresets READ dpiPresets NOTIFY dpiPresetsChanged)
    Q_PROPERTY(bool smartShiftActive READ smartShiftActive NOTIFY smartShiftChanged)
    Q_PROPERTY(int smartShiftThreshold READ smartShiftThreshold NOTIFY smartShiftChanged)
    Q_PROPERTY(int smartShiftTorque READ smartShiftTorque NOTIFY smartShiftChanged)
    Q_PROPERTY(bool hiresOn READ hiresOn NOTIFY hiresChanged)
    Q_PROPERTY(bool hiresInvert READ hiresInvert NOTIFY hiresChanged)
    Q_PROPERTY(bool thumbDivert READ thumbDivert NOTIFY thumbChanged)
    Q_PROPERTY(bool thumbInvert READ thumbInvert NOTIFY thumbChanged)

public:
    // Live constructor: device object-path + the (system) bus the GUI is on.
    // Kicks off the async Introspect + the Get* seed reads.
    explicit DeviceController(const QString& devicePath, const QDBusConnection& bus,
                              QObject* parent = nullptr);
    // Test constructor: no bus, no async — drive capabilities/values via the
    // applyIntrospection / seed* entry points below.
    explicit DeviceController(const QString& devicePath, QObject* parent = nullptr);
    ~DeviceController() override;

    [[nodiscard]] QString devicePath() const { return _devicePath; }

    [[nodiscard]] bool hasSmartShift() const { return _present.contains(QStringLiteral("pizza.pixl.LogiOps.SmartShift")); }
    [[nodiscard]] bool hasHires() const { return _present.contains(QStringLiteral("pizza.pixl.LogiOps.HiresScroll")); }
    [[nodiscard]] bool hasThumbwheel() const { return _present.contains(QStringLiteral("pizza.pixl.LogiOps.ThumbWheel")); }
    [[nodiscard]] bool hasButtons() const { return _present.contains(QStringLiteral("pizza.pixl.LogiOps.Buttons")); }
    [[nodiscard]] bool hasDpi() const { return _present.contains(QStringLiteral("pizza.pixl.LogiOps.DPI")); }
    [[nodiscard]] bool hasTorque() const { return _hasTorque; }
    [[nodiscard]] int dpiMin() const { return _dpiMin; }
    [[nodiscard]] int dpiMax() const { return _dpiMax; }
    [[nodiscard]] int dpiStep() const { return _dpiStep; }
    [[nodiscard]] int sensorCount() const { return _sensorCount; }
    [[nodiscard]] int hostCount() const { return _hostCount; }

    [[nodiscard]] int dpi() const { return _dpi; }
    [[nodiscard]] QVariantList dpiPresets() const { return _dpiPresets; }
    [[nodiscard]] bool smartShiftActive() const { return _smartShiftActive; }
    [[nodiscard]] int smartShiftThreshold() const { return _smartShiftThreshold; }
    [[nodiscard]] int smartShiftTorque() const { return _smartShiftTorque; }
    [[nodiscard]] bool hiresOn() const { return _hiresOn; }
    [[nodiscard]] bool hiresInvert() const { return _hiresInvert; }
    [[nodiscard]] bool thumbDivert() const { return _thumbDivert; }
    [[nodiscard]] bool thumbInvert() const { return _thumbInvert; }

    // --- Optimistic setters (live-apply). Emit the change immediately, then
    // fire the async D-Bus setter; QML never blocks. ---
    Q_INVOKABLE void setDpi(int dpi);

    // --- DPI-cycle preset editing (DPI-02 / DPI-03). Each mutator updates the
    // local model (emits dpiPresetsChanged) and pushes the FULL {value,label} list
    // to the daemon via .DPI.SetPresets (so values persist device-scoped and
    // labels live in the same schema list). Values clamp to [dpiMin,dpiMax] before
    // emit when bounds are known (defense in depth; the daemon also snaps). ---
    Q_INVOKABLE void addPreset(int dpi, const QString& label);
    Q_INVOKABLE void removePreset(int index);
    Q_INVOKABLE void setPresetLabel(int index, const QString& label);
    Q_INVOKABLE void setPresetValue(int index, int dpi);

    Q_INVOKABLE void setSmartShiftActive(bool active);
    Q_INVOKABLE void setSmartShiftThreshold(int threshold);
    Q_INVOKABLE void setSmartShiftTorque(int torque);
    Q_INVOKABLE void setHiresOn(bool on);
    Q_INVOKABLE void setHiresInvert(bool invert);
    Q_INVOKABLE void setThumbDivert(bool divert);
    Q_INVOKABLE void setThumbInvert(bool invert);
    // Assign a SIMPLE action to the thumbwheel TAP (SCR-03). VERIFIED the daemon's
    // ThumbWheel.SetTap builds a simple action via makeAction at .../thumb_wheel/tap
    // (no daemon change needed). `type` is a simple action type string, e.g.
    // "ToggleSmartShift" / "ToggleHiresScroll" / "None". left/right are gesture-
    // typed and deferred to Phase 4 (CONTEXT).
    Q_INVOKABLE void setThumbTap(const QString& type);

    // GEST-01: hand QML a button-scoped GestureModel for the .../buttons/M node
    // the reassign panel is editing. The gesture builder binds the returned
    // model's previewSentence/activeDirection and calls setMode/setGranularity/
    // setGestureAction on it (QML renders only). The controller OWNS the model:
    // it is lazily constructed, cached per button path, and re-pointed when a
    // different button's Gesture category opens, so it always targets the right
    // node. The same ConfigState is injected so every gesture edit dirties the
    // inherited "Unsaved changes" pill (no second Save path). Returns nullptr for
    // an empty path. The pointer is owned by this controller (QML must not delete).
    Q_INVOKABLE logiops_gui::GestureModel* gestureModelForButton(const QString& buttonPath);

    // --- Test / construction seams (also reused by the live path). ---
    // Apply a present-interface set (the parsed Introspect result, or an injected
    // fake) and rebuild proxies + capability flags. Emits capabilitiesChanged.
    void applyIntrospection(const QSet<QString>& presentInterfaces);
    // Parse an Introspectable XML blob into the present-interface set. virtual so
    // a test can override the source; static-pure so it needs no bus.
    [[nodiscard]] virtual QSet<QString> parseInterfaces(const QString& introspectXml) const;
    // Seed the capability/value caches directly (the live path calls these from
    // the async Get* replies; the test calls them with fakes).
    void seedDpiBounds(const QList<int>& dpis, int step, bool isRange, int sensorCount);
    // Seed the preset model from the daemon's GetPresets reply (parallel arrays)
    // or an injected test fixture. Emits dpiPresetsChanged.
    void seedPresets(const QList<uint>& values, const QStringList& labels);
    void seedTorqueSupport(bool supported);
    void seedSmartShift(bool active, int threshold, int torque);
    void seedHires(bool hires, bool invert);
    void seedThumb(bool divert, bool invert);
    void setHostCount(int count);

    // CONF-01: the global dirty-tracker. Injected by the factory after
    // construction; every config-mutating setter calls markDirty() on it after a
    // successful optimistic live-apply so the "Unsaved changes" pill appears. Null
    // in the headless/test path (markDirty is then a no-op). WR-03.
    void setConfigState(ConfigState* configState) { _configState = configState; }

signals:
    void capabilitiesChanged();
    void dpiChanged();
    void dpiPresetsChanged();
    void smartShiftChanged();
    void hiresChanged();
    void thumbChanged();

protected:
    // The live path's Introspect entry point — overridable so a subclass/test can
    // substitute the present-set without touching D-Bus.
    virtual void introspectInterfaces();
    // Issue the async seed reads for whichever feature proxies now exist.
    void readInitialValues();
    // Push the current preset model to the daemon as the two parallel
    // {values,labels} arrays (.DPI.SetPresets). Virtual so DpiCycleTest can record
    // the EXACT payload (the actual persistence path) with no live bus; the live
    // override issues the async D-Bus call.
    virtual void pushPresets(const QList<uint>& values, const QStringList& labels);

private:
    // Re-derive _dpiPresets from the local store and emit dpiPresetsChanged, then
    // push to the daemon via pushPresets().
    void syncPresets();
    // Rebuild the QML-facing _dpiPresets projection from the parallel stores.
    void rebuildPresetModel();
    // Clamp a DPI value to [dpiMin,dpiMax] when bounds are known (T-3-03-01).
    [[nodiscard]] int _clampDpi(int dpi) const;
    void buildProxies();

    QString _devicePath;
    QDBusConnection _bus;
    bool _live = false;

    QSet<QString> _present;

    // Lazily built only for present interfaces (T-3-01-03).
    PizzaPixlLogiOpsDPIInterface* _dpiProxy = nullptr;
    PizzaPixlLogiOpsSmartShiftInterface* _smartShiftProxy = nullptr;
    PizzaPixlLogiOpsHiresScrollInterface* _hiresProxy = nullptr;
    PizzaPixlLogiOpsThumbWheelInterface* _thumbProxy = nullptr;
    PizzaPixlLogiOpsButtonsInterface* _buttonsProxy = nullptr;

    // GEST-01: the button-scoped gesture builder brain for the currently edited
    // button. Owned (QObject child); re-created when a different button's Gesture
    // category opens so it always targets the right .../buttons/M node.
    GestureModel* _gestureModel = nullptr;
    QString _gesturePath; // the button path _gestureModel is scoped to.

    // Capability cache.
    bool _hasTorque = false;
    int _dpiMin = 0;
    int _dpiMax = 0;
    int _dpiStep = 0;
    int _sensorCount = 0;
    int _hostCount = 3; // NOT exposed over D-Bus yet — Plan 02 wires the real value.

    // Live-value cache.
    int _dpi = 0;
    // DPI-cycle preset store: parallel value/label vectors are the source of
    // truth; _dpiPresets is the QML-facing {value,label}-map projection.
    QList<int> _presetValues;
    QStringList _presetLabels;
    QVariantList _dpiPresets;
    bool _smartShiftActive = false;
    int _smartShiftThreshold = 0;
    int _smartShiftTorque = 0;
    bool _hiresOn = false;
    bool _hiresInvert = false;
    bool _thumbDivert = false;
    bool _thumbInvert = false;

    // CONF-01 dirty-tracker (WR-03). Not owned; may be null (test/headless path).
    ConfigState* _configState = nullptr;
    // Mark the global config dirty after a successful live-apply (no-op if unset).
    void markDirty() const;
};

} // namespace logiops_gui

#endif // LOGIOPS_GUI_DEVICECONTROLLER_H
