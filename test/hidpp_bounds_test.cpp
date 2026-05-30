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

/*
 * HID++ bounds-guard unit (ACCESS-04).
 *
 * Dependency-free test (no gtest — the tree has no test framework). It feeds
 * short/empty reports through the free inline
 *   logid::backend::hidpp::hasHidppHeader(const std::vector<uint8_t>&)
 * which Plan 04 adds to backend/hidpp/Report.h.
 *
 * RED-BY-DESIGN: at this plan's point in time `hasHidppHeader` does NOT exist,
 * so this translation unit FAILS TO COMPILE. That is the intentional RED
 * baseline; it turns GREEN once Plan 04 (ACCESS-04) lands the helper. Do NOT
 * stub the helper here — the helper is Plan 04's deliverable; this plan only
 * owns the test.
 *
 * Report::HeaderLength == 4 (Report.h:130). The guard must accept reports of
 * length >= 4 and reject anything shorter.
 */

#include <vector>
#include <cstdint>
#include <cstdio>

#include "backend/hidpp/Report.h"

namespace {
    int g_failures = 0;

    // Report `actual` (a hasHidppHeader() result) against `expected` for one case.
    void check(const char* name, bool actual, bool expected) {
        if (actual == expected) {
            std::printf("PASS: %s (-> %s)\n", name, actual ? "true" : "false");
        } else {
            std::printf("FAIL: %s (expected %s, got %s)\n",
                        name,
                        expected ? "true" : "false",
                        actual ? "true" : "false");
            ++g_failures;
        }
    }
}

int main() {
    using logid::backend::hidpp::hasHidppHeader;

    // One explicit hasHidppHeader() call per case (five cases total).

    // Empty vector -> too short -> false.
    const std::vector<uint8_t> empty{};
    check("empty report", hasHidppHeader(empty), false);

    // 1 byte (only Type, size() == 1) -> too short -> false.
    const std::vector<uint8_t> one_byte{0x11};
    check("1-byte report", hasHidppHeader(one_byte), false);

    // 3 bytes, one short of HeaderLength=4 (size() == 3) -> false.
    const std::vector<uint8_t> three_bytes{0x11, 0x01, 0x02};
    check("3-byte report (one short of header)", hasHidppHeader(three_bytes), false);

    // Exactly 4 bytes -> exactly HeaderLength -> true.
    const std::vector<uint8_t> four_bytes{0x11, 0x01, 0x02, 0x03};
    check("exactly 4-byte report", hasHidppHeader(four_bytes), true);

    // 7-byte short report -> longer than header -> true.
    const std::vector<uint8_t> seven_bytes{0x11, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    check("7-byte short report", hasHidppHeader(seven_bytes), true);

    if (g_failures != 0) {
        std::printf("\n%d case(s) FAILED\n", g_failures);
        return 1;
    }
    std::printf("\nAll cases passed\n");
    return 0;
}
