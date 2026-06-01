# App-Match Model — Owned Reference (REF-01)

> **Owned spec.** This is *our* distilled schema of the Options+ application/app-match data,
> written for **Phase 5 (Per-App Profiles — PROF-02, PROF-03)** to adapt against instead of
> guessing. It captures the `applications[]` entry schema, how Options+ actually *detects* an
> application, how cards bind per app, and where our own rule layer begins.
>
> **Legal (LOCKED — HYBRID / reference-only):** we distill the *schema* only. We do **not** bundle
> `applications.json` (420 KB), its icons, or its verbatim string resources, and **nothing** here
> is or will be copied into `src/`.
>
> **Source** (gitignored local install, not redistributed): `LogiOptionsPlus/data/applications.json`
> — top-level `{ "applications": [...] }`, **22 application entries** (`grep -c '"applicationId"'`).

---

## 1. Application entry schema

Top level is a single key `applications` → an array of 22 entries. Observed entry fields
(union across all entries):

| field | type | meaning |
|---|---|---|
| `applicationId` | string | the app's identity token (e.g. `application_id_google_chrome`, `application_id_adobe_illustrator`). The primary join key everywhere (also appears bare as a GUID like `420fd454-…` for the generic plugin app). |
| `name` | string | display token (not mirrored) |
| `detection` | object[] | **how the app is matched at runtime** — see §2 |
| `cards` | object[] | the per-app action cards — see §3 |
| `commands` | array | app-specific command bindings |
| `categoryColors` | object | UI theming for the app |
| `additional_application_data` | object | misc app metadata |
| `poster_url` / `profile_url` | string | proprietary asset URLs (not mirrored) |
| `version` | string | data-version bookkeeping |

App count: **22**. Sample `applicationId`s: `application_id_google_chrome`,
`application_id_ding_talk`, `application_id_microsoft_edge_chromium`,
`application_id_microsoft_excel`, `application_id_adobe_illustrator`,
`application_id_adobe_indesign`, `application_id_kugoumusic`.

---

## 2. Match-rule model (honest report: what the JSON has vs. what is OUR layer)

Options+ identifies a running application via a per-entry **`detection`** array. Each detection
element holds exactly one **Windows-only** matcher object. The field union across all 22 apps is:

| detection variant | fields | example |
|---|---|---|
| `winRegistry` | `executable`, `registryKey`, `registryPath` | `executable: "chrome.exe"`, `registryPath: "HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Windows/CurrentVersion/App Paths/chrome.exe"` |
| `winExecutablePath` | `path` (with `{ProgramFiles}` / `{ProgramFiles-x86}` env templating) | `{ProgramFiles}\\Adobe\\Adobe InDesign 2023\\InDesign.exe` |
| `winStoreApp` | (Windows Store package identity) | — |

**Honest finding — there is NO X11 `WM_CLASS` / Linux executable / `bundleId` field in the data.**
The match layer is **100% Windows**: executable basenames (`chrome.exe`, `msedge.exe`,
`EXCEL.EXE`, `Illustrator.exe`, `DingTalk.exe`), Windows registry `App Paths`, and Store packages.
There is no `windowClass`, no `bundleId`, no POSIX process/path field. (`grep -iE
"windowClass|bundleId|wm_class"` over the file returns nothing; the only `executable`/`path`
hits are inside the three Windows `detection` variants above.)

**Therefore PROF-03 (match by window class / executable) is OUR rule layer to design.** Options+
gives us the *catalog* (the `applicationId` taxonomy + the Windows executable basenames as a
naming hint), but the actual Linux matching primitive — X11 `WM_CLASS` (`WM_CLASS` instance/class
pair) and/or the resolved executable of the focused window — is ours to build. We seed our app
catalog from the Options+ `applicationId` taxonomy and reuse the Windows `executable` basenames
only as *hints* for the default per-app rule, but the match itself is a Linux-native rule we own.

---

## 3. Card binding (app → actions)

Each app entry's `cards[]` is how a matched app gains its actions. Observed card fields (union):

| field | type | meaning |
|---|---|---|
| `id` | string | card id |
| `name` | string | display token (not mirrored) |
| `attribute` | enum | action category (590 occurrences across the file; e.g. the macro/playback attribute) |
| `taskId` | int | internal action/task id (442 occurrences) |
| `continuous` | bool | repeats while held (120 occurrences) |
| `readOnly` | bool | shipped vs. user-editable |
| `tags` | string[] | binding scope (key/button) |
| `macro` | object | the action payload — see below |
| `nestedCards` | object[] | sub-cards (hierarchical action groups) |
| `gestureInfo` | object | gesture metadata when the card is gesture-bound |

The `macro` object inside an app card carries `type` plus a typed payload. Macro `type` values
present in `applications.json`: **`KEYSTROKE`, `TEXT_BLOCK`, `MOUSE`, `ACTION`, `SYSTEM`** (note:
this includes a first-class `MOUSE` step absent from `predefined_win.json`). Other macro fields
seen: `action`, `actionName`, `mouse{action, hidUsage}`, `keystroke`, `text_block`, `system`,
`icon`, `alternateMacros`, `onboardable`, `init_period`, `repeat_period`.

- **`MOUSE`** → `mouse: { action, hidUsage }`; observed `mouse.action` values include `BUTTON`,
  `WIN_BACK`, `WIN_FORWARD` (browser back/forward, etc.) — the mouse/media step the
  `smart-action-schema.md` step model accounts for.
- Step `type` vocabulary cross-references **`smart-action-schema.md` §1** (the macro step model).

So the binding chain is: **`applicationId` → `detection` (match) → `cards[]` → `macro` (action)**.
A match on the focused app unlocks that app's card/action set; that is the per-app override Phase 5
delivers.

---

## 4. Our re-implementation note (→ PROF-02 / PROF-03)

- **PROF-02 (per-app profiles):** a **default profile + per-app overrides**, keyed by our own app
  token (seeded from the Options+ `applicationId` taxonomy as a catalog). When the focused app
  matches an override, we switch the active profile by calling the **existing daemon
  `ChangeProfile`** action — no new daemon match logic required.
- **PROF-03 (the match rule — OURS to build):** match the focused window by **X11 `WM_CLASS`**
  (instance/class) and/or its resolved **executable**. This primitive does **not** exist in
  `applications.json` (whose `detection` is Windows-only: registry / `*.exe` basename / Store
  package); we design it natively for Linux, using the Options+ executable basenames only as hints.
- **Privilege boundary:** the focus watcher runs as a **non-root session helper** (it needs the
  user's display/session to read the focused window); it calls into the root daemon over D-Bus to
  switch profiles. The root `logid` daemon never inspects the user's windowing session.
- **Legal:** we reference the Options+ `applicationId` taxonomy as a catalog but do **not** bundle
  `applications.json`, its icons, or its strings, and write nothing under `src/` from this reference.
