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

// Phase 3, Wave 0 — unknown-key resilience (T-3-00-02 mitigation).
//
// Documents the RESEARCH A5 risk: a config file containing a schema-unknown key
// must NOT crash the load -> serialize -> reload cycle. The unknown key is
// silently dropped (it was never parsed into the in-memory tree), but every
// schema-known value survives and no exception escapes.

#include <config/schema.h>
#include <config/types.h>
#include <libconfig.h++>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <variant>

using namespace logid;
using namespace logid::config;

namespace {

// Top-level `frobnicate` AND a per-profile `unsupported_key` the schema models
// nowhere. Schema-known dpi/smartshift must still survive.
const char* kFixture = R"CFG(
frobnicate: true;
devices: (
{
    name: "MX Master 3S";
    default_profile: "default";
    unsupported_key: "this is not in the schema";
    profiles: (
    {
        name: "default";
        dpi: 1200;
        smartshift:
        {
            on: true;
            threshold: 40;
        };
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

} // namespace

int main() {
    namespace fs = std::filesystem;

    const fs::path tmp = fs::path(BUILD_TMP_DIR) /
                         ("phase3_save_unknownkey_" +
                          std::to_string(::getpid()) + ".cfg");

    {
        std::ofstream out(tmp);
        out << kFixture;
    }

    bool threw = false;
    int dpiAfter = -1;
    try {
        // Load — unknown keys are dropped here, never fatal.
        libconfig::Config lc;
        lc.readFile(tmp.string().c_str());
        Config cfg = get<Config>(lc.getRoot());

        CHECK(cfg.devices.has_value(), "devices survive unknown top-level key");

        // Serialize + reload — must not throw on the unknown-key fixture.
        libconfig::Config out;
        config::set(out.getRoot(), cfg);
        out.writeFile(tmp.string().c_str());

        libconfig::Config reloaded;
        reloaded.readFile(tmp.string().c_str());
        Config after = get<Config>(reloaded.getRoot());

        if (after.devices.has_value()) {
            auto it = after.devices->find("MX Master 3S");
            if (it != after.devices->end() &&
                std::holds_alternative<Device>(it->second)) {
                const Device& dev = std::get<Device>(it->second);
                auto pit = dev.profiles.find("default");
                if (pit != dev.profiles.end() && pit->second.dpi.has_value())
                    dpiAfter = std::get<int>(pit->second.dpi.value());
            }
        }
    } catch (const std::exception& e) {
        threw = true;
        std::cerr << "Unexpected exception: " << e.what() << "\n";
    }

    std::remove(tmp.string().c_str());

    CHECK(!threw, "no crash/throw on schema-unknown key");
    CHECK(dpiAfter == 1200, "schema-known dpi value survives unknown-key round-trip");

    if (failures) {
        std::cerr << failures << " assertion(s) failed.\n";
        return 1;
    }
    std::cout << "phase3_save_unknownkey: OK — unknown key dropped, no crash.\n";
    return 0;
}
