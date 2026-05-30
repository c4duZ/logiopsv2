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
 * Wave 0 placeholder translation unit for the optional logiops-gui target.
 *
 * The static `logiops-gui-lib` needs at least one source to configure and link
 * cleanly before any real GUI code exists. Plan 03 adds DeviceModel.{h,cpp} to
 * this target's source list (and may drop this placeholder once a real source
 * is present); Plan 04 adds main.cpp + the QML module + the `logiops-gui`
 * executable. Until then this empty namespace keeps the library buildable so
 * test/DeviceModelTest can link against the GUI lib as soon as the model lands.
 */

namespace logiops_gui {}
