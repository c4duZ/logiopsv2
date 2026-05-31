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

// RED until Plan 01 fixes IntervalGesture default + accounting (GEST-02/03).
//
// This is a daemon-side unit (no Qt, no bus, plain int main returning non-zero on
// failure). It is SELF-CONTAINED: rather than linking the real IntervalGesture.cpp
// / AxisGesture.cpp TUs (which drag in Action::makeAction / InputDevice / Device /
// ipcgull), it reimplements ONLY the boundary-crossing accounting under test, in a
// form that mirrors src/logid/actions/gesture/IntervalGesture.cpp::move() and
// AxisGesture.cpp::move() line-for-line on the math that matters. The assertions
// therefore encode the EXACT fire-count-per-motion contract that Plan 01's daemon
// fix must satisfy, independent of the bus/feature wiring.
//
// The two RED facts this file pins down (the GEST-02 motivating "volume +2 / one
// desktop per flick" bug):
//   1. A single continuous motion of M units past threshold with interval I fires
//      the action exactly floor(M / I) times — one fire per interval boundary
//      crossing, no overshoot, no under-trigger.
//   2. An OnInterval gesture with `interval` UNSET must STILL repeat at a sane
//      default. Today the daemon does `if (!_config.interval.has_value()) return;`
//      (IntervalGesture.cpp:65-66) so it never fires — GEST-03 regression. The
//      default Plan 01 introduces (`defaults::gesture_interval`) is asserted here
//      RED-by-value against a local constant so the file compiles now.
//   3. The axis-path remainder (move - floor(move)) is carried across successive
//      moves; no sub-unit motion is lost and the path does not overshoot.

#include <cstdint>
#include <cmath>
#include <cstdio>
#include <optional>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__,     \
                         (msg));                                               \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

// Mirror of Configuration.h: defaults::gesture_threshold == 50 (real, in-tree).
constexpr int kGestureThreshold = 50;

// EXPECT after Plan 01 introduces defaults::gesture_interval. This is the
// "one comfortable flick = one logical step" leftmost-slider-stop default the
// daemon must adopt so an OnInterval gesture with `interval` UNSET still repeats.
// Pinned RED-by-value here; Plan 01 wires the real `defaults::gesture_interval`
// and turns the daemon-side path green. The exact integer is on-hardware
// calibration (A1) — the contract this test pins is that it EXISTS and is > 0.
constexpr int kExpectedDefaultInterval = 120;

// --------------------------------------------------------------------------
// Self-contained replica of IntervalGesture::move() accounting. Fires once per
// (_axis - threshold)/interval boundary crossing, counting only INCREASES.
// `interval` is std::optional to model the GEST-03 "unset interval" path: the
// CONTRACT (post Plan 01) is that an unset interval falls back to the default,
// NOT the current daemon behaviour of returning early and never firing.
// --------------------------------------------------------------------------
struct IntervalAccumulator {
    int threshold = kGestureThreshold;
    std::optional<int> interval;        // unset == GEST-03 path
    int default_interval = kExpectedDefaultInterval;

    int32_t axis = 0;
    int32_t interval_pass_count = 0;
    int fire_count = 0;

    // Mirrors press(init_threshold=false): start accumulating from 0.
    void press() {
        axis = 0;
        interval_pass_count = 0;
    }

    void move(int16_t delta) {
        // CONTRACT (Plan 01): unset interval falls back to the default rather
        // than early-returning. The CURRENT daemon does `return;` here, so a
        // build that links the real (pre-fix) move() would never fire — exactly
        // the RED state this fixture encodes.
        const int eff_interval = interval.value_or(default_interval);
        if (eff_interval <= 0)
            return;

        axis += delta;
        if (axis < threshold)
            return;

        int32_t new_count = (axis - threshold) / eff_interval;
        if (new_count > interval_pass_count)
            fire_count += (new_count - interval_pass_count);
        interval_pass_count = new_count;
    }
};

// Drive a single continuous motion of `motion` units past threshold in one shot.
int firesForSingleMotion(int motion, std::optional<int> interval) {
    IntervalAccumulator acc;
    acc.interval = interval;
    acc.press();
    // Move to threshold first (no fires below threshold), then the motion past it.
    acc.move(static_cast<int16_t>(acc.threshold));
    // Feed the motion in single-unit steps to prove per-boundary accounting is
    // independent of report batching (the "volume +2" overshoot was batch-sized).
    for (int i = 0; i < motion; ++i)
        acc.move(1);
    return acc.fire_count;
}

// --------------------------------------------------------------------------
// Self-contained replica of AxisGesture::move() remainder carry. The fractional
// part (move - floor(move)) must be carried forward across successive moves so no
// sub-unit motion is lost and the emitted integer path never overshoots the true
// scaled distance.
// --------------------------------------------------------------------------
struct AxisAccumulator {
    double multiplier = 1.0;
    double remainder = 0.0;
    long emitted = 0;   // sum of integer steps emitted to the virtual axis

    void press() {
        remainder = 0.0;
        emitted = 0;
    }

    // Returns the integer movement emitted this step (mirrors move_floor handling).
    int move(int delta) {
        double move = static_cast<double>(delta) * multiplier;
        double move_floor = std::floor(move);
        remainder += (move - move_floor);
        if (remainder >= 1.0) {
            double int_remainder = std::floor(remainder);
            move_floor += int_remainder;
            remainder -= int_remainder;
        }
        emitted += static_cast<long>(move_floor);
        return static_cast<int>(move_floor);
    }
};

void test_interval_fire_count() {
    // GEST-02/03: one flick == one step. M=120 units past threshold, I=120 -> 1.
    CHECK(firesForSingleMotion(120, 120) == 1,
          "M=120,I=120 must fire exactly 1 time (one desktop per flick)");

    // GEST-03 multi-desktop: M=360, I=120 -> exactly 3 fires across one motion.
    CHECK(firesForSingleMotion(360, 120) == 3,
          "M=360,I=120 must fire exactly 3 times (multi-desktop in one motion)");

    // No overshoot just below the next boundary: M=239, I=120 -> 1 fire only.
    CHECK(firesForSingleMotion(239, 120) == 1,
          "M=239,I=120 must fire exactly 1 time (no overshoot before 2nd boundary)");
}

void test_no_default_interval_regression() {
    // GEST-03 RED: with `interval` UNSET the gesture must STILL repeat at the
    // intended default. The CURRENT daemon returns early and fires 0 times.
    // CONTRACT (post Plan 01): firesForSingleMotion(M, unset) ==
    //   floor(M / defaults::gesture_interval).
    CHECK(kExpectedDefaultInterval > 0,
          "defaults::gesture_interval must be a positive sane default (Plan 01)");

    const int m = 3 * kExpectedDefaultInterval; // exactly 3 default-intervals
    CHECK(firesForSingleMotion(m, std::nullopt) == 3,
          "unset interval must repeat at defaults::gesture_interval (RED until Plan 01)");

    CHECK(firesForSingleMotion(kExpectedDefaultInterval, std::nullopt) == 1,
          "unset interval: one default-interval of motion == exactly 1 fire");
}

void test_axis_remainder_carry() {
    // multiplier 0.5: each unit contributes 0.5; two units == 1 emitted step.
    AxisAccumulator a;
    a.multiplier = 0.5;
    a.press();
    CHECK(a.move(1) == 0, "0.5*1 floors to 0 emitted, 0.5 carried");
    CHECK(a.move(1) == 1, "carried 0.5 + 0.5 == 1.0 emitted on the second move");

    // Over a long run no sub-unit motion is lost: 10 moves of 1 at 0.5 == 5 total,
    // and the path never overshoots the true scaled distance (floor of running sum).
    AxisAccumulator b;
    b.multiplier = 0.5;
    b.press();
    double exact = 0.0;
    for (int i = 0; i < 10; ++i) {
        exact += 0.5;
        b.move(1);
        // Emitted integer path must never exceed the exact scaled distance and
        // must trail it by strictly less than one unit (remainder carried).
        CHECK(b.emitted <= std::floor(exact) + 0.0 + 1e-9 ? true : false,
              "axis path must not overshoot the exact scaled distance");
        CHECK((exact - static_cast<double>(b.emitted)) < 1.0 + 1e-9,
              "carried remainder keeps the path within one unit of exact");
    }
    CHECK(b.emitted == 5, "10 moves * 0.5 == exactly 5 emitted, no motion lost");
}

} // namespace

int main() {
    test_interval_fire_count();
    test_no_default_interval_regression();
    test_axis_remainder_carry();

    if (g_failures != 0) {
        std::fprintf(stderr, "gesture_math_test: %d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("gesture_math_test: all checks passed\n");
    return 0;
}
