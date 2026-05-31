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

// `logid::global_loglevel` is defined in logid.cpp (the daemon main), which the
// Phase 4 daemon-side units do NOT compile. Any daemon TU that calls logPrintf()
// references this symbol — so the test provides it here, the only daemon symbol
// needed beyond the unit under test + log.cpp. Mirrors tests/phase3/phase3_log_stub.cpp.

#include <util/log.h>

logid::LogLevel logid::global_loglevel = logid::WARN;
