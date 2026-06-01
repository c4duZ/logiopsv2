# Smart-Action / Macro Schema — Owned Reference (REF-01)

> **Owned spec.** This is *our* distilled schema of the Options+ macro/smart-action data,
> written for **Phase 7 (Smart Actions / Macros — MACRO-01, MACRO-02)** to adapt against
> instead of guessing. It captures field names, the step (`macro.type`) vocabulary, the
> card model, and the per-app integration-card shape.
>
> **Legal (LOCKED — HYBRID / reference-only):** we distill the *schema* only. We do **not**
> bundle Logitech's `predefined_*.json`, plugin `config.json`, icons, or verbatim string
> resources, and **nothing** here is or will be copied into `src/`. This file lives only
> under `.planning/intel/`.
>
> **Sources** (all under the gitignored local install `LogiOptionsPlus/`, not redistributed):
> - `data/macros/predefined_win.json` — 38 `macro_infos`, all `originType: PREDEFINED`, `state: ACTIVE`
> - `data/macros/common_application_cards_win.json` — the global app-window/open-app cards
> - `data/macros/extended_application_cards_map_win.json` — `applicationId → [cardId]` overlay map
> - `integrations/plugin_*/config.json` — 8 first-party app plugins

---

## 1. Step (macro) vocabulary — the MACRO-01 step model

A macro is an **ordered list of cards**; each card carries one `macro` object whose `type`
selects exactly one payload sub-object. These are the distinct `macro.type` values found in
`predefined_win.json` (counts = occurrences across all 38 presets), plus the payload shape and
how **we** model the equivalent step.

| `macro.type` | count | payload sub-object | payload fields (observed) | Our equivalent step (Phase 7 / MACRO-01) |
|---|---|---|---|---|
| `KEYSTROKE` | 47 | `keystroke` | `code` (int), `modifiers` (int[] of HID usage codes, e.g. `224`=LeftCtrl); sibling `actionName` display string (e.g. `"Ctrl + T"`) | **Keystroke step** — a single chord. Map `code`/`modifiers` (HID usage) → our libevdev `KEY_*` via the existing KeyNameMapper layer. |
| `TEXT_BLOCK` | 53 | `textBlock` | `text` (string, e.g. a URL or snippet) | **Text step** — type a literal string (synthesized via the daemon's uinput device). |
| `DELAY` | 83 | `delay` | `durationMs` (int, e.g. `1000`, `3000`) | **Inter-step delay** — pause N ms between steps. Modeled as a step attribute, not a separate uinput event. |
| `INPUT_SEQUENCE` | 73 | `inputSequence` | `componentLists[].components[]`, each component is either `{delay:{durationMs}}` or `{keyboard:{displayName, hidUsage, virtualKeyId, isDown?}}`; plus `defaultDelay` (int), `useDefaultDelay` (bool) | **Composite key sequence** — an explicit down/up timeline. We flatten it into an ordered list of keystroke + delay steps (our native step model), honoring `isDown` for press vs. release and `defaultDelay` between components. |
| `APP_WINDOWS_MANAGEMENT` | 39 | `appWindowsManagement` | `action` (enum, see §2); optional `path`, `appName` | **Window action step** — focus/minimize/maximize/close a window. On Linux this routes to the **session helper** (per MACRO-02), not the root daemon. |
| `OPEN_FILE_FOLDER` | 10 | `open_file_folder` | `path` (string, often empty), `appName` (string, e.g. `"Microsoft OneDrive"`, `"qqmusic"`) | **Launch / open step** — open a file/folder or launch an app. Routes to the **session helper** (per MACRO-02); the root daemon never spawns user processes. |

Card-level sibling fields seen on some steps: `continuous` (bool — whether the action repeats
while held), `icons` (`{icons:[png,svg], uri}` — proprietary assets, **not** mirrored), `taskId`
(int, an internal action id).

> Note: `predefined_win.json` did **not** surface a standalone `MOUSE` step `type`; mouse/HID
> usage appears inside `INPUT_SEQUENCE` components (`keyboard.hidUsage`, 190 occurrences) and in
> `applications.json` cards (`macro.mouse.action`/`hidUsage`) — the latter is covered by the
> per-app `applications.json` analysis in `app-match-model.md`. Our step model still includes a
> media/mouse step; Options+ expresses it through `hidUsage` rather than a dedicated macro type.

---

## 2. Action enum (grouped by step type)

Distinct `action` values observed (`predefined_win.json` surfaced `BRING_TO_FOREGROUND` ×37 and
`MINIMIZE` ×2 directly; the global card map `common_application_cards_win.json` enumerates the
full window-management set):

**`APP_WINDOWS_MANAGEMENT.appWindowsManagement.action`:**
- `BRING_TO_FOREGROUND` — raise/focus the window
- `MINIMIZE`
- `MAXIMIZE`
- `CLOSE`

**Keyboard usage codes** (inside `KEYSTROKE.keystroke.modifiers` and `INPUT_SEQUENCE`
`keyboard.hidUsage`): raw **HID usage** integers (e.g. `224` = Left Ctrl, `40` = Enter), with a
parallel `virtualKeyId` string (e.g. `VK_NUMPAD_ENTER`) and a `displayName`. These are HID usage
IDs, **not** evdev codes — our layer must translate.

---

## 3. Card model

Every step is a **card**. Observed card object fields (`predefined_win.json` +
`common_application_cards_win.json`):

| field | type | meaning |
|---|---|---|
| `id` | string | stable card id (e.g. `macro_presets_bring_to_foreground_win`); used as the map target in `extended_application_cards_map_win.json` |
| `name` | string | display name (a string-resource token, e.g. `ASSIGNMENT_NAME_BRING_TO_FOREGROUND`) — **not** mirrored verbatim |
| `attribute` | enum | the action category. Only value observed: `MACRO_PLAYBACK` (this card plays a macro) |
| `applicationId` | string | which app this card is bound to (e.g. `application_id_google_chrome`), or `""` for a global card. See `app-match-model.md`. |
| `macro` | object | the step payload (`type` + one sub-object, §1) |
| `readOnly` | bool | true for shipped presets (user can't edit) |
| `continuous` | bool | whether the action repeats while the button is held |
| `tags` | string[] | binding scope: `PRESET_TAG_KEY_OR_BUTTON`, `PRESET_TAG_BUTTON` |
| `taskId` | int | internal action/task id (optional) |
| `icons` | object | `{icons:[png,svg], uri}` — proprietary, **not** mirrored |

**Macro (preset) wrapper** — `macro_infos[]` entry fields:

| field | type | meaning |
|---|---|---|
| `id` | string | preset id (e.g. `macros_preset_kimi`) |
| `name` / `description` | string | display tokens (not mirrored) |
| `state` | enum | lifecycle; only `ACTIVE` observed |
| `originType` | enum | provenance; only `PREDEFINED` observed (a user-authored macro would presumably differ — `state`/`originType` are the lifecycle axes we mirror with our own `user` vs `builtin` distinction) |
| `categories` | string[] | grouping for the picker: `AI`, `FOR_DESIGNERS`, `FOR_DEVELOPERS`, `LEISURE`, `MEETINGS`, `PRODUCTIVITY` |
| `cards` | object[] | the ordered step list (§1) |
| `lastEditTimestamp`, `specific_regions` | — | bookkeeping (`specific_regions` e.g. `["CN","HK"]`); not behaviorally relevant to us |

**Card maps** (how cards attach beyond a single preset):
- `common_application_cards_win.json` → a flat `cards[]` of global, app-agnostic window/open-app cards (`applicationId: ""`).
- `extended_application_cards_map_win.json` → `data[]` of `{applicationId, cards:[cardId]}` — overlays extra card ids onto specific apps (e.g. `application_id_google_chrome` + `application_id_microsoft_edge_chromium` both get `card_global_presets_new_browser_tab`).

---

## 4. Per-app integration cards (plugins)

Per-app deep integrations live under `integrations/plugin_*/config.json`. Each plugin declares
which apps it targets and an `actionSdk.actions[]` list (the app-specific actions a button can be
bound to). Shape:

| field | type | meaning |
|---|---|---|
| `supportedApplicationIds` | string[] | the apps this plugin binds to (an `application_id_*` token + a GUID) |
| `name` / `description` / `author` | string | display tokens (not mirrored) |
| `guid` | string | plugin id |
| `enabled` | bool | plugin active |
| `actionSdk.actions[]` | object[] | each: `name` (token), `actionId` (string, e.g. `thumbwheel_turn_action`), `actionIcon` (string), `parameters` (array, empty in the bundled set) |
| `eventsSdk.events[]` | object[] | app events the plugin reacts to (`eventId`, e.g. `tool_change`) |
| `installationCommands[]` | object[] | Windows installer invocations — **N/A on Linux**, not mirrored |

**The 8 bundled first-party plugins** (evidence of the per-app integration-card model Phase 7 mirrors):

| plugin dir | targets (`application_id_*`) |
|---|---|
| `plugin_photoshop` | `application_id_adobe_photoshop` |
| `plugin_premiere_pro` | `application_id_adobe_premierepro` |
| `plugin_illustrator` | `application_id_adobe_illustrator` |
| `plugin_indesign` | `application_id_adobe_indesign` |
| `plugin_lightroom_classic` | `application_id_adobe_lightroom_classic` |
| `plugin_word` | `application_id_microsoft_word` |
| `plugin_excel` | `application_id_microsoft_excel` |
| `plugin_powerpoint` | `application_id_microsoft_powerpoint` |

Each exposes 6 generic device-surface actions in the bundled data
(`thumbwheel_turn_action`, `controlid_click_action`, `activate_plugin`, `deactivate_plugin`,
`crown_touch_action`, `crown_turn_action`) — i.e. the plugin maps **device controls** to
**app-defined actions**, with the heavy per-tool logic shipped in the (Windows) plugin binary,
not this JSON.

---

## 5. Our re-implementation note (→ MACRO-01 / MACRO-02)

- **MACRO-01 (step model):** a macro is *our* ordered list of typed steps —
  **keystroke** (chord), **text** (literal string), **media/mouse** (HID-usage action),
  **delay** (inter-step ms), plus **launch / open-URL / window action**. This is a strict superset
  flattening of the Options+ `macro.type` vocabulary above; `INPUT_SEQUENCE` collapses into our
  keystroke+delay timeline, `DELAY` becomes an inter-step attribute.
- **MACRO-02 (privilege boundary):** steps that spawn processes or touch the windowing system
  (`OPEN_FILE_FOLDER`, `APP_WINDOWS_MANAGEMENT`) route to a **non-root session helper**, never the
  root `logid` daemon. Keystroke/text/media steps stay in the daemon's existing uinput path.
- **Per-app cards** are *our* schema (default + per-app overrides keyed by an app token), seeded
  by the Options+ `applicationId` taxonomy as a catalog — but we ship our own QML/strings/icons.
  We do **not** bundle Logitech's predefined macro JSON, plugin configs, or icon assets, and we
  write nothing under `src/` from this reference.
