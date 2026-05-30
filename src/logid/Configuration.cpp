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

#include <Configuration.h>
#include <util/log.h>
#include <utility>
#include <filesystem>
#include <stdexcept>
#include <ipc_defs.h>
#include <ipcgull/connection.h>
#include <polkit/polkit.h>

using namespace logid;
using namespace libconfig;
using namespace logid::config;

Configuration::Configuration(std::string config_file) :
        _config_file(std::move(config_file)) {
    if (std::filesystem::exists(_config_file)) {
        try {
            _config.readFile(_config_file.c_str());
        } catch (const FileIOException& e) {
            logPrintf(ERROR, "I/O Error while reading %s: %s", _config_file.c_str(),
                  e.what());
            throw;
        } catch (const ParseException& e) {
            logPrintf(ERROR, "Parse error in %s, line %d: %s", e.getFile(),
                  e.getLine(), e.getError());
            throw;
        }

        Config::operator=(get<Config>(_config.getRoot()));
    } else {
        logPrintf(INFO, "Config file does not exist, using empty config.");
    }

    if (!devices.has_value())
        devices.emplace();
}

Configuration::Configuration() {
    devices.emplace();
}

bool Configuration::checkSaveAuthorized(const std::string& callerBusName) {
    // FAIL-SAFE DENY on every branch: null authority, null result, error, empty caller.
    if (callerBusName.empty()) {
        logPrintf(WARN, "polkit: empty caller, denying save.");
        return false;
    }

    GError* error = nullptr;
    PolkitAuthority* authority = polkit_authority_get_sync(nullptr, &error);
    if (!authority) {
        logPrintf(WARN, "polkit authority unavailable, denying save: %s",
                  error ? error->message : "unknown");
        if (error)
            g_error_free(error);
        return false;
    }

    PolkitSubject* subject = polkit_system_bus_name_new(callerBusName.c_str());
    if (!subject) {
        logPrintf(WARN, "polkit: failed to build subject, denying save.");
        g_object_unref(authority);
        return false;
    }

    // NOTE (RESEARCH A9, T-01-06-04 = accept): this _sync call blocks the GLib main-loop
    // dispatch thread while the polkit agent prompts. Acceptable for the rare save action;
    // flagged for an async/deferred-reply follow-up only if a real bus stall is observed.
    PolkitAuthorizationResult* result = polkit_authority_check_authorization_sync(
            authority, subject, "pizza.pixl.logiops.save-config", nullptr,
            POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION, nullptr, &error);

    bool authorized = false;
    if (result) {
        authorized = polkit_authorization_result_get_is_authorized(result);
        g_object_unref(result);
    } else {
        logPrintf(WARN, "polkit check failed, denying save: %s",
                  error ? error->message : "unknown");
        if (error)
            g_error_free(error);
    }

    g_object_unref(subject);
    g_object_unref(authority);
    return authorized;
}

void Configuration::save() {
    const std::string caller = ipcgull::current_caller();
    if (caller.empty() || !checkSaveAuthorized(caller)) {
        logPrintf(WARN, "Unauthorized save() denied.");
        throw std::runtime_error("Not authorized to save configuration");
    }

    config::set(_config.getRoot(), *this);
    try {
        _config.writeFile(_config_file.c_str());
    } catch (const FileIOException& e) {
        logPrintf(ERROR, "I/O Error while writing %s: %s",
                  _config_file.c_str(), e.what());
        throw;
    } catch (const std::exception& e) {
        logPrintf(ERROR, "Error while writing %s: %s",
                  _config_file.c_str(), e.what());
        throw;
    }
}

Configuration::IPC::IPC(Configuration* config) :
        ipcgull::interface(SERVICE_ROOT_NAME ".Config", {
                {"Save", {config, &Configuration::save}}
        }, {}, {}) {
}
