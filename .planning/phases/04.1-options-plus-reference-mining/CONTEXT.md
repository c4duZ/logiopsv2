# Phase 4.1 Context — Options+ Reference Mining (INSERTED)

**Created:** 2026-05-31
**Trigger:** User brought the real Logi Options+ Windows install (`./LogiOptionsPlus/`, gitignored, 531MB) to stop guessing gesture/feature behavior and adapt from the source product.

## Decisions (2026-05-31)
- **Adaptation depth = HYBRID**: adapt behavior + vocabulary + study the UI design; re-implement everything in our own QML/strings. **Do NOT bundle** Logitech proprietary assets (app.asar content, strings, icons, device data) into the shipped app. This is a Phase 9 release gate.
- This is a **pure analysis/spec phase** — no daemon or GUI code ships. Output = intel + design specs in `.planning/intel/options-plus/` that Phases 4.2 and 5–8 consume.

## Asset inventory (what's in `LogiOptionsPlus/`)

### ✅ Readable & adaptable as reference
| Source | What it gives us | Feeds |
|--------|------------------|-------|
| `data/strings/*.yaml` (incl. `pt-BR.yaml`) | Exact vocabulary: "SEGURAR + MOVER PARA CIMA/BAIXO/ESQUERDA/DIREITA", "Gestos / Personalizada", "Botão de gesto", zoom gesture, predefinição model | 4.2 gestures, 6, 7 |
| `data/macros/predefined_*.json` | Smart-Action/macro schema: `macro.type` (e.g. `APP_WINDOWS_MANAGEMENT`), actions (`BRING_TO_FOREGROUND`), `cards`, `categories`, `applicationId` | Phase 7 |
| `integrations/plugin_*/config.json` | Per-app cards (Photoshop, Premiere, Illustrator, InDesign, Lightroom, Word, Excel, PowerPoint) | Phase 7 |
| `data/applications.json` (420KB) | App-match DB (window-class / executable matching) | Phase 5 |
| `data/overlay/osd_resources.json`, `notification_resources.json`, `overlay/icons/` | OSD/overlay/notification resource model | Phase 6 (action wheel), gesture OSD |
| `data/defaults/defaults_control_*.json`, `defaults_slot_*.json` | Default control/slot mappings per device | 4.2, 5 |
| `data/card_presets/*.json`, `data/rap/*.json` | Card presets + recommendations model | 7 |
| `resources/app.asar` (83MB, Electron archive — extract with `asar` / `npx @electron/asar extract`) | The real UI: tab/layout/interaction model, pixel-level design | 4.2 + all GUI phases (study only) |

### ❌ Not usable
- `data/devices/devices_*.json` (46 files) — **encrypted/signed** (header `{"key-id":..., "file-sha":...}` + binary blob; keys `data/firmware.pem`, `data/logitech-lap-public.pem`). The per-device descriptor DB is NOT readable. Device capability continues to come from **live HID++ enumeration** (the daemon already does this) plus public DBs (Solaar, libratbag) where a static descriptor is needed.

## Intended outputs (specs we OWN, in `.planning/intel/options-plus/`)
1. `vocabulary.md` — our string table distilled from `strings/*.yaml` (gestures, smart actions, scroll, backlight), pt-BR + en.
2. `smart-action-schema.md` — step/card vocabulary for Phase 7, from `macros/` + `integrations/`.
3. `app-match-model.md` — match-rule schema for Phase 5, from `applications.json`.
4. `overlay-osd-spec.md` — radial/OSD interaction + layout for Phase 6, from `overlay/` + app.asar study.
5. `ui-design-spec.md` — tab/layout/interaction reference from the extracted app.asar (our re-implementation target).
6. `legal-boundary.md` — the hybrid/reference-only rule + Phase 9 packaging gate.
7. `device-db-limitation.md` — why the device DB is unusable + our live-enumeration fallback.

## Requirements (new)
- REF-01: readable Options+ data parsed into owned specs under `.planning/intel/options-plus/`
- REF-02: app.asar UI extracted + a written design spec captured (study, not bundle)
- REF-03: legal boundary + encrypted-device-DB limitation documented
