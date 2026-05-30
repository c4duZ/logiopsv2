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
#include "DeviceController.h"

#include "ipc_defs.h"
#include "logid_dpi_proxy.h"
#include "logid_smartshift_proxy.h"
#include "logid_hiresscroll_proxy.h"
#include "logid_thumbwheel_proxy.h"
#include "logid_buttons_proxy.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QXmlStreamReader>

#include <algorithm>

namespace logiops_gui {

namespace {
    constexpr auto kService = SERVICE_ROOT_NAME;
    constexpr auto kIntrospectable = "org.freedesktop.DBus.Introspectable";
}

DeviceController::DeviceController(const QString& devicePath, const QDBusConnection& bus,
                                   QObject* parent)
    : QObject(parent), _devicePath(devicePath), _bus(bus), _live(true) {
    // Async capability discovery — never blocks the UI thread (T-3-01-02).
    introspectInterfaces();
}

DeviceController::DeviceController(const QString& devicePath, QObject* parent)
    : QObject(parent), _devicePath(devicePath), _bus(QDBusConnection(QString())), _live(false) {
    // Test/headless ctor: no bus, no async; capabilities/values driven via the
    // applyIntrospection / seed* seams.
}

DeviceController::~DeviceController() = default;

void DeviceController::introspectInterfaces() {
    if (!_live)
        return;

    // org.freedesktop.DBus.Introspectable.Introspect() on the device node. Parse
    // the returned XML for the present feature interfaces, then build proxies for
    // exactly those (T-3-01-03: treat absent as absent, never blind-call).
    auto* iface = new QDBusInterface(QString::fromUtf8(kService), _devicePath,
                                     QString::fromUtf8(kIntrospectable), _bus, this);
    auto pending = iface->asyncCall(QStringLiteral("Introspect"));
    auto* watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, iface]() {
                QDBusPendingReply<QString> reply = *watcher;
                if (!reply.isError())
                    applyIntrospection(parseInterfaces(reply.value()));
                watcher->deleteLater();
                iface->deleteLater();
            });
}

QSet<QString> DeviceController::parseInterfaces(const QString& introspectXml) const {
    // Pull every <interface name="..."> off the introspection node; the present
    // Phase 3 feature interfaces among them form the capability set.
    QSet<QString> present;
    QXmlStreamReader xml(introspectXml);
    while (!xml.atEnd()) {
        if (xml.readNext() == QXmlStreamReader::StartElement &&
            xml.name() == QStringLiteral("interface")) {
            const QString name = xml.attributes().value(QStringLiteral("name")).toString();
            if (!name.isEmpty())
                present.insert(name);
        }
    }
    return present;
}

void DeviceController::applyIntrospection(const QSet<QString>& presentInterfaces) {
    _present = presentInterfaces;
    buildProxies();
    emit capabilitiesChanged();
    if (_live)
        readInitialValues();
}

void DeviceController::buildProxies() {
    if (!_live)
        return;
    const QString svc = QString::fromUtf8(kService);

    if (hasDpi() && _dpiProxy == nullptr)
        _dpiProxy = new PizzaPixlLogiOpsDPIInterface(svc, _devicePath, _bus, this);
    if (hasSmartShift() && _smartShiftProxy == nullptr)
        _smartShiftProxy = new PizzaPixlLogiOpsSmartShiftInterface(svc, _devicePath, _bus, this);
    if (hasHires() && _hiresProxy == nullptr)
        _hiresProxy = new PizzaPixlLogiOpsHiresScrollInterface(svc, _devicePath, _bus, this);
    if (hasThumbwheel() && _thumbProxy == nullptr)
        _thumbProxy = new PizzaPixlLogiOpsThumbWheelInterface(svc, _devicePath, _bus, this);
    if (hasButtons() && _buttonsProxy == nullptr)
        _buttonsProxy = new PizzaPixlLogiOpsButtonsInterface(svc, _devicePath, _bus, this);
}

void DeviceController::readInitialValues() {
    if (!_live)
        return;

    // DPI bounds + sensor count + current value (sensor 0).
    if (_dpiProxy != nullptr) {
        auto* w = new QDBusPendingCallWatcher(_dpiProxy->GetDPIs(0), this);
        connect(w, &QDBusPendingCallWatcher::finished, this, [this, w]() {
            QDBusPendingReply<QList<int>, ushort, bool> r = *w;
            if (!r.isError())
                seedDpiBounds(r.argumentAt<0>(), r.argumentAt<1>(), r.argumentAt<2>(), _sensorCount);
            w->deleteLater();
        });
        auto* sw = new QDBusPendingCallWatcher(_dpiProxy->GetSensors(), this);
        connect(sw, &QDBusPendingCallWatcher::finished, this, [this, sw]() {
            QDBusPendingReply<uchar> r = *sw;
            if (!r.isError()) {
                _sensorCount = r.value();
                emit capabilitiesChanged();
            }
            sw->deleteLater();
        });
        auto* dw = new QDBusPendingCallWatcher(_dpiProxy->GetDPI(0), this);
        connect(dw, &QDBusPendingCallWatcher::finished, this, [this, dw]() {
            QDBusPendingReply<ushort> r = *dw;
            if (!r.isError()) {
                _dpi = r.value();
                emit dpiChanged();
            }
            dw->deleteLater();
        });
        // DPI-cycle presets (DPI-02 / DPI-03) — two parallel arrays.
        auto* pw = new QDBusPendingCallWatcher(_dpiProxy->GetPresets(), this);
        connect(pw, &QDBusPendingCallWatcher::finished, this, [this, pw]() {
            QDBusPendingReply<QList<uint>, QStringList> r = *pw;
            if (!r.isError())
                seedPresets(r.argumentAt<0>(), r.argumentAt<1>());
            pw->deleteLater();
        });
    }

    // SmartShift config + torque support.
    if (_smartShiftProxy != nullptr) {
        seedTorqueSupport(_smartShiftProxy->torqueSupport());
        auto* w = new QDBusPendingCallWatcher(_smartShiftProxy->GetConfig(), this);
        connect(w, &QDBusPendingCallWatcher::finished, this, [this, w]() {
            QDBusPendingReply<uchar, uchar, uchar> r = *w;
            if (!r.isError())
                seedSmartShift(r.argumentAt<0>() != 0, r.argumentAt<1>(), r.argumentAt<2>());
            w->deleteLater();
        });
    }

    // Hi-res scroll config.
    if (_hiresProxy != nullptr) {
        auto* w = new QDBusPendingCallWatcher(_hiresProxy->GetConfig(), this);
        connect(w, &QDBusPendingCallWatcher::finished, this, [this, w]() {
            QDBusPendingReply<bool, bool, bool> r = *w;
            if (!r.isError())
                seedHires(r.argumentAt<0>(), r.argumentAt<1>());
            w->deleteLater();
        });
    }

    // Thumbwheel config.
    if (_thumbProxy != nullptr) {
        auto* w = new QDBusPendingCallWatcher(_thumbProxy->GetConfig(), this);
        connect(w, &QDBusPendingCallWatcher::finished, this, [this, w]() {
            QDBusPendingReply<bool, bool> r = *w;
            if (!r.isError())
                seedThumb(r.argumentAt<0>(), r.argumentAt<1>());
            w->deleteLater();
        });
    }
}

// --- Seed helpers (live replies + injected test fakes share these). ---

void DeviceController::seedDpiBounds(const QList<int>& dpis, int step, bool isRange, int sensorCount) {
    // bounds = min/max(dpis) when isRange; otherwise the list is a discrete set so
    // the slider still spans first..last reported value.
    if (!dpis.isEmpty()) {
        _dpiMin = *std::min_element(dpis.cbegin(), dpis.cend());
        _dpiMax = *std::max_element(dpis.cbegin(), dpis.cend());
    }
    _dpiStep = (step > 0) ? step : (isRange ? 50 : 0);
    if (sensorCount > 0)
        _sensorCount = sensorCount;
    emit capabilitiesChanged();
}

void DeviceController::seedTorqueSupport(bool supported) {
    if (_hasTorque == supported)
        return;
    _hasTorque = supported;
    emit capabilitiesChanged();
}

void DeviceController::seedSmartShift(bool active, int threshold, int torque) {
    _smartShiftActive = active;
    _smartShiftThreshold = threshold;
    _smartShiftTorque = torque;
    emit smartShiftChanged();
}

void DeviceController::seedHires(bool hires, bool invert) {
    _hiresOn = hires;
    _hiresInvert = invert;
    emit hiresChanged();
}

void DeviceController::seedThumb(bool divert, bool invert) {
    _thumbDivert = divert;
    _thumbInvert = invert;
    emit thumbChanged();
}

void DeviceController::setHostCount(int count) {
    if (_hostCount == count)
        return;
    _hostCount = count;
    emit capabilitiesChanged();
}

// --- Optimistic setters: emit the change at once, then fire the async setter. ---

void DeviceController::setDpi(int dpi) {
    if (_dpi != dpi) {
        _dpi = dpi;
        emit dpiChanged();
    }
    // No GUI-side rounding: hand the raw value to the daemon, which snaps via
    // getClosestDPI (DPI.cpp). We DO re-read GetDPI to reflect the snapped value.
    if (_dpiProxy != nullptr) {
        _dpiProxy->SetDPI(static_cast<ushort>(dpi), 0);
        auto* w = new QDBusPendingCallWatcher(_dpiProxy->GetDPI(0), this);
        connect(w, &QDBusPendingCallWatcher::finished, this, [this, w]() {
            QDBusPendingReply<ushort> r = *w;
            if (!r.isError() && _dpi != static_cast<int>(r.value())) {
                _dpi = r.value();
                emit dpiChanged();
            }
            w->deleteLater();
        });
    }
}

// --- DPI-cycle preset model + persistence (DPI-02 / DPI-03, option-a). ---

int DeviceController::_clampDpi(int dpi) const {
    // Defense in depth (T-3-03-01): clamp to [dpiMin,dpiMax] when bounds are known
    // before the value ever reaches the daemon. The daemon also snaps.
    if (_dpiMin > 0 && dpi < _dpiMin)
        return _dpiMin;
    if (_dpiMax > 0 && dpi > _dpiMax)
        return _dpiMax;
    return dpi;
}

void DeviceController::seedPresets(const QList<uint>& values, const QStringList& labels) {
    _presetValues.clear();
    _presetLabels.clear();
    for (int i = 0; i < values.size(); ++i) {
        _presetValues.append(static_cast<int>(values.at(i)));
        _presetLabels.append(i < labels.size() ? labels.at(i) : QString());
    }
    rebuildPresetModel();
    emit dpiPresetsChanged();
}

void DeviceController::rebuildPresetModel() {
    _dpiPresets.clear();
    for (int i = 0; i < _presetValues.size(); ++i) {
        QVariantMap m;
        m.insert(QStringLiteral("value"), _presetValues.at(i));
        m.insert(QStringLiteral("label"),
                 i < _presetLabels.size() ? _presetLabels.at(i) : QString());
        _dpiPresets.append(m);
    }
}

void DeviceController::syncPresets() {
    rebuildPresetModel();
    emit dpiPresetsChanged();
    // Push the FULL {values,labels} list to the daemon (.DPI.SetPresets) — the
    // actual device-scoped persistence path (option-a), NOT a per-button action.
    QList<uint> values;
    values.reserve(_presetValues.size());
    for (int v : _presetValues)
        values.append(static_cast<uint>(v));
    pushPresets(values, _presetLabels);
}

void DeviceController::pushPresets(const QList<uint>& values, const QStringList& labels) {
    if (_dpiProxy != nullptr)
        _dpiProxy->SetPresets(values, labels);
}

void DeviceController::addPreset(int dpi, const QString& label) {
    _presetValues.append(_clampDpi(dpi));
    _presetLabels.append(label);
    syncPresets();
}

void DeviceController::removePreset(int index) {
    if (index < 0 || index >= _presetValues.size())
        return;
    _presetValues.removeAt(index);
    if (index < _presetLabels.size())
        _presetLabels.removeAt(index);
    syncPresets();
}

void DeviceController::setPresetLabel(int index, const QString& label) {
    if (index < 0 || index >= _presetLabels.size())
        return;
    if (_presetLabels.at(index) == label)
        return;
    _presetLabels[index] = label;
    syncPresets();
}

void DeviceController::setPresetValue(int index, int dpi) {
    if (index < 0 || index >= _presetValues.size())
        return;
    const int clamped = _clampDpi(dpi);
    if (_presetValues.at(index) == clamped)
        return;
    _presetValues[index] = clamped;
    syncPresets();
}

void DeviceController::setSmartShiftActive(bool active) {
    if (_smartShiftActive != active) {
        _smartShiftActive = active;
        emit smartShiftChanged();
    }
    if (_smartShiftProxy != nullptr)
        _smartShiftProxy->SetActive(active, false);
}

void DeviceController::setSmartShiftThreshold(int threshold) {
    if (_smartShiftThreshold != threshold) {
        _smartShiftThreshold = threshold;
        emit smartShiftChanged();
    }
    if (_smartShiftProxy != nullptr)
        _smartShiftProxy->SetThreshold(static_cast<uchar>(threshold), false);
}

void DeviceController::setSmartShiftTorque(int torque) {
    if (_smartShiftTorque != torque) {
        _smartShiftTorque = torque;
        emit smartShiftChanged();
    }
    if (_smartShiftProxy != nullptr)
        _smartShiftProxy->SetTorque(static_cast<uchar>(torque), false);
}

void DeviceController::setHiresOn(bool on) {
    if (_hiresOn != on) {
        _hiresOn = on;
        emit hiresChanged();
    }
    if (_hiresProxy != nullptr)
        _hiresProxy->SetHires(on);
}

void DeviceController::setHiresInvert(bool invert) {
    if (_hiresInvert != invert) {
        _hiresInvert = invert;
        emit hiresChanged();
    }
    if (_hiresProxy != nullptr)
        _hiresProxy->SetInvert(invert);
}

void DeviceController::setThumbDivert(bool divert) {
    if (_thumbDivert != divert) {
        _thumbDivert = divert;
        emit thumbChanged();
    }
    if (_thumbProxy != nullptr)
        _thumbProxy->SetDivert(divert);
}

void DeviceController::setThumbInvert(bool invert) {
    if (_thumbInvert != invert) {
        _thumbInvert = invert;
        emit thumbChanged();
    }
    if (_thumbProxy != nullptr)
        _thumbProxy->SetInvert(invert);
}

void DeviceController::setThumbTap(const QString& type) {
    // Simple-action assignment to the thumbwheel tap (SCR-03). The daemon's
    // SetTap builds the handler via makeAction at .../thumb_wheel/tap.
    if (_thumbProxy != nullptr)
        _thumbProxy->SetTap(type);
}

} // namespace logiops_gui
