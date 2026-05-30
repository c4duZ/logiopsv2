# Conventions

Code style, naming, and recurring patterns in the logiops (`logid`) C++ codebase. Use this as the reference for matching existing style when adding or modifying code.

## Language & Standard

- Modern **C++20** throughout (`src/logid/` and the vendored `src/ipcgull/`).
- Formatting is governed by `.editorconfig` at the repo root — match its indentation and line-ending rules rather than guessing.

## Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Classes / types | PascalCase | `DeviceManager`, `RawDevice`, `ReceiverMonitor` |
| Files | PascalCase, matching the primary class | `DeviceManager.cpp`, `InputDevice.h` |
| Methods | camelCase | `addDevice()`, `listen()`, `save()` |
| Private members | leading underscore | `_devices`, `_config`, `_data` |
| Free / util functions | snake_case | utilities in `src/logid/util/` |
| Header guards | `#ifndef` / `#define` include guards (not `#pragma once`) | per-header `LOGID_..._H` |

## C++ Patterns

- **Smart pointers** for ownership — `std::shared_ptr` / `std::unique_ptr` rather than raw owning pointers.
- **`make()` factory methods** — objects that need post-construction setup or shared ownership are created through static `make()` factories rather than bare constructors.
- **Concurrency** — `std::shared_mutex` for reader/writer protection of shared state.
- **Modifiers** — `[[nodiscard]]` on value-returning queries and `final` on leaf classes are used deliberately; follow suit.
- **IPC interfaces** — D-Bus-exposed objects use nested `ipcgull` interface classes (the vendored binding in `src/ipcgull/`).

## Error Handling

- **Exception-based.** Errors propagate as thrown exceptions rather than return codes.
- A central handler, `ExceptionHandler::Default` (`src/logid/util/ExceptionHandler.*`), provides default handling. Note: this file currently has a slicing bug (see `CONCERNS.md` #1) — be careful to rethrow with bare `throw;`, not `throw error;`.

## Logging

- Custom `printf`-style logger via `logPrintf` (`src/logid/util/log.h`). Log levels include `WARN`, etc.
- The logger is explicitly flagged for future replacement (`src/logid/util/log.h:24`).
- Avoid passing untrusted/runtime strings as the format argument (format-string hazard — see `CONCERNS.md` #5).

## Build / Structure Conventions

- CMake-based build; each subtree has its own `CMakeLists.txt` (root `CMakeLists.txt`, `src/logid/CMakeLists.txt`, etc.).
- Config schema is declared declaratively in `src/logid/config/schema.h`.
- Backend code is layered by protocol generation under `src/logid/backend/` (`raw/`, `hidpp/`, `hidpp10/`, `hidpp20/`) — see `ARCHITECTURE.md` and `STRUCTURE.md`.

---
*Mapped: 2026-05-30*
