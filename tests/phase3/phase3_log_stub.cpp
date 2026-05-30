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
// Phase 3 round-trip units do NOT compile. config.cpp's logError() calls
// logPrintf(), which references this symbol — so the test provides it here, the
// only daemon symbol needed beyond config.cpp + log.cpp.

#include <util/log.h>

logid::LogLevel logid::global_loglevel = logid::WARN;
