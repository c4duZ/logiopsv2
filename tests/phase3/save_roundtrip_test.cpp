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

// Phase 3, Wave 0 — BLOCKING CONF-01 round-trip fidelity gate.
//
// The whole "live-edit over D-Bus -> Configuration::save()" model of Phase 3
// rests on one assumption: that the daemon's serialization path round-trips
// the config tree's STRUCTURE and VALUES without silent loss. This test proves
// it directly, against the exact code save() runs after the polkit gate
// (Configuration.cpp:125-127): config::set(root, cfg) + writeFile().
//
// It NEVER calls Configuration::save() (polkit-gated, needs a live caller) and
// NEVER touches /etc/logid.cfg — it owns a temp file under the build dir
// (T-3-00-04 = accept: no privilege crosses).
//
// Comment/formatting loss is EXPECTED (libconfig writeFile is canonical) and is
// deliberately NOT asserted. Only structure + values are checked.

#include <config/schema.h>
#include <config/types.h>
#include <libconfig.h++>

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

using namespace logid;
using namespace logid::config;

namespace {

// A two-device fixture (>= 2 device blocks) exercising dpi + smartshift +
// buttons, in the daemon's modern per-profile shape so it loads as config::Device.
const char* kFixture = R"CFG(
devices: (
{
    name: "Wireless Mouse MX Master";
    default_profile: "default";
    profiles: (
    {
        name: "default";
        dpi: 1000;
        smartshift:
        {
            on: true;
            threshold: 30;
            torque: 50;
        };
        hiresscroll:
        {
            hires: true;
            invert: false;
            target: false;
        };
        buttons: (
            {
                cid: 0xc4;
                action =
                {
                    type: "Keypress";
                    keys: ["KEY_A"];
                };
            }
        );
    }
    );
},
{
    name: "MX Master 3S";
    default_profile: "default";
    profiles: (
    {
        name: "default";
        dpi: 1600;
        smartshift:
        {
            on: false;
            threshold: 45;
            torque: 60;
        };
        buttons: (
            {
                cid: 0xc3;
                action =
                {
                    type: "ToggleSmartShift";
                };
            }
        );
    }
    );
}
);
)CFG";

int failures = 0;

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "FAIL: " << (msg) << " (" #cond ")\n";              \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

// Pull the named device's named profile out of an in-memory Config tree.
const Profile* profileOf(const Config& cfg, const std::string& device,
                         const std::string& profile) {
    if (!cfg.devices.has_value())
        return nullptr;
    auto dit = cfg.devices->find(device);
    if (dit == cfg.devices->end())
        return nullptr;
    if (!std::holds_alternative<Device>(dit->second))
        return nullptr;
    const Device& dev = std::get<Device>(dit->second);
    auto pit = dev.profiles.find(profile);
    if (pit == dev.profiles.end())
        return nullptr;
    return &pit->second;
}

int dpiOf(const Profile& p) {
    assert(p.dpi.has_value());
    return std::get<int>(p.dpi.value());
}

unsigned int smartshiftThresholdOf(const Profile& p) {
    assert(p.smartshift.has_value());
    assert(p.smartshift->threshold.has_value());
    return p.smartshift->threshold.value();
}

} // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path tmp = fs::path(BUILD_TMP_DIR) /
                         ("phase3_save_roundtrip_" +
                          std::to_string(::getpid()) + ".cfg");

    // Write the fixture to disk.
    {
        std::ofstream out(tmp);
        out << kFixture;
    }

    // ---- 1. Load the fixture into an in-memory config::Config tree ----------
    libconfig::Config lc;
    lc.readFile(tmp.string().c_str());
    Config cfg = get<Config>(lc.getRoot());

    CHECK(cfg.devices.has_value(), "devices block loaded");
    CHECK(cfg.devices->size() == 2, "two device blocks loaded");

    const Profile* mx = profileOf(cfg, "Wireless Mouse MX Master", "default");
    const Profile* m3 = profileOf(cfg, "MX Master 3S", "default");
    CHECK(mx != nullptr, "MX Master device+profile present pre-save");
    CHECK(m3 != nullptr, "MX Master 3S device+profile present pre-save");
    if (!mx || !m3) {
        std::remove(tmp.string().c_str());
        return failures ? 1 : 0;
    }

    // Capture an UNTOUCHED value to prove it survives unchanged.
    const int m3DpiBefore = dpiOf(*m3);
    const unsigned int m3ThreshBefore = smartshiftThresholdOf(*m3);

    // ---- 2. Mutate one DPI and one smartshift threshold in memory -----------
    const int newDpi = 2000;
    const unsigned int newThreshold = 77;
    {
        Profile& p = std::get<Device>(cfg.devices->at("Wireless Mouse MX Master"))
                         .profiles.at("default");
        p.dpi = newDpi;
        p.smartshift->threshold = newThreshold;
    }

    // ---- 3. Serialize via the EXACT save() write path -----------------------
    // This is what Configuration::save() runs after the polkit gate
    // (Configuration.cpp:125-127): config::set(root, cfg) then writeFile().
    libconfig::Config out;
    config::set(out.getRoot(), cfg);
    out.writeFile(tmp.string().c_str());

    // ---- 4. Re-parse the written file into a fresh tree ---------------------
    libconfig::Config reloaded;
    reloaded.readFile(tmp.string().c_str());
    Config after = get<Config>(reloaded.getRoot());

    // ---- 5. Assert: structure preserved -------------------------------------
    CHECK(after.devices.has_value(), "devices block survives save");
    CHECK(after.devices->size() == 2, "device-block count preserved (2 -> 2)");

    const Profile* mxAfter = profileOf(after, "Wireless Mouse MX Master", "default");
    const Profile* m3After = profileOf(after, "MX Master 3S", "default");
    CHECK(mxAfter != nullptr, "MX Master device+profile present post-save");
    CHECK(m3After != nullptr, "MX Master 3S device+profile present post-save");

    if (mxAfter) {
        // Mutated values applied.
        CHECK(dpiOf(*mxAfter) == newDpi, "mutated DPI persisted");
        CHECK(smartshiftThresholdOf(*mxAfter) == newThreshold,
              "mutated smartshift threshold persisted");
        // Untouched sibling value on the same profile unchanged.
        CHECK(mxAfter->smartshift->torque.value() == 50u,
              "untouched torque on mutated profile unchanged");
        // Button block survives.
        CHECK(mxAfter->buttons.has_value(), "MX Master button block survives");
        CHECK(mxAfter->buttons->count(0xc4) == 1, "MX Master cid 0xc4 survives");
    }

    if (m3After) {
        // The whole OTHER device must be byte-for-value identical.
        CHECK(dpiOf(*m3After) == m3DpiBefore, "untouched device DPI unchanged");
        CHECK(smartshiftThresholdOf(*m3After) == m3ThreshBefore,
              "untouched device threshold unchanged");
        CHECK(m3After->smartshift->on.value() == false,
              "untouched device smartshift.on unchanged");
        CHECK(m3After->buttons.has_value() && m3After->buttons->count(0xc3) == 1,
              "untouched device button survives");
    }

    std::remove(tmp.string().c_str());

    if (failures) {
        std::cerr << failures << " assertion(s) failed.\n";
        return 1;
    }
    std::cout << "phase3_save_roundtrip: OK — structure + values round-trip.\n";
    return 0;
}
