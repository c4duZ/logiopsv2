# Options+ `app.asar` Inventory — Navigation Map

> **STUDY-ONLY.** This is a navigation map of an extracted *proprietary* Logitech
> Electron archive, produced for DESIGN STUDY under the HYBRID / reference-only rule
> (see `legal-boundary.md`). **The extracted source is NOT bundled** into the shipped
> app and is **not committed** (the tree is gitignored). We re-implement the UI in our
> own QML/strings. This file only records *where things live* so the Plan-04 spec
> author can navigate ~81M without re-exploring. No extracted content is copied into `src/`.

**Extracted tree:** `.planning/phases/04.1-options-plus-reference-mining/asar-extract/`
(565 files, ~81M; from `LogiOptionsPlus/resources/app.asar`, 82,945,069 bytes — `@electron/asar` v4.2.0, 2026-06-01)

---

## Top-level layout

The archive is a **webpack-bundled React (Electron) app** — three renderer windows + the
Electron main process, plus one flat content-hashed asset dir. All `.js`/`.css` are minified.

| Path (under `asar-extract/`) | Purpose |
|------------------------------|---------|
| `main.js` (1.0M) | **Electron main process** — least-minified. Creates `BrowserWindow`s, loads the HTML entry points, owns app lifecycle. Best place to read *which windows exist* and what loads them. |
| `preload.bundle.js` (11K) | Preload bridge (contextBridge IPC surface between main and renderers). |
| `index.html` + `app.min.js` (10.5M) | **Main app window** — the primary device-configuration UI (`<div id="root">`, React). Largest renderer bundle. |
| `cc.html` + `cc.min.js` (9.3M) | **"Configuration"/overlay companion window** ("Logi Options+ Configuration"). Has `#root`, `#modal-root`, `#toast-top-right-root`. Carries overlay/radial/OSD + profile/wheel code. |
| `marketplace.html` + `marketplace.min.js` (44K) + `marketplace.bundle.js` | Plugin marketplace window (not relevant to our phases). |
| `cc.html` + `cc.bundle.js` | CC window bootstrap. |
| `runtime.min.js`, `vendors.min.js` (2.6M), `vendors.min.css` | Webpack runtime + shared vendor libs/styles (React, etc.). |
| `46.min.js` (4.2M), `520.min.js` (5.0M), `888.min.js`, `166.min.js` | **Lazy-loaded route chunks** (webpack split chunks). Feature screens (e.g. OSD/overlay editors) live here, loaded on demand by the windows above. |
| `app.min.css` (934K), `888.min.css`, `cc.min.css`, `marketplace.min.css` | Compiled styles per window. |
| `*.LICENSE.txt` | Third-party license banners (proof it's a bundled React app). |
| `assets/` (538 files) | **Flat content-hashed media**: 278 png, 158 svg, 73 gif, fonts (ttf/woff2/otf/woff/eot), 4 webp. The **GIFs are gesture/feature demo animations**; SVGs are icons/glyphs; PNGs are device renders + screens. Filenames are hashes (`<sha>.png`) — identify by opening, not by name. |
| `package.json` | App manifest. `name: logioptionsplus`, `main: main.js`. Deps reveal the stack: `@rjsf/*` (react-jsonschema-form → schema-driven config forms), `lottie-web` (animated illustrations), `fabric-guideline-plugin` (canvas — likely the radial/overlay editor), `electron-store`, `crypto-js`, `uuid`, `@sentry/*`. |

**Window → entry map** (from `main.js`): `index.html` (main), `cc.html` (← `../cc/cc_template.html`),
`marketplace.html` (← `../marketplace/marketplace_template.html`). Source template was `../src/template.html`.

---

## Where the UI lives

- **The renderer UI is minified-bundled React**, not loose component files. There is **no readable
  component tree, no QML, no per-screen HTML** — everything is compiled into the big `*.min.js` bundles.
- **Primary UI logic:** `app.min.js` (main window) and `cc.min.js` (config/overlay window) are the two
  bundles that carry our concerns. Feature screens are further split into the lazy chunks
  `46.min.js` / `520.min.js` (largest non-entry chunks).
- **Readable anchors that *do* survive minification:** bound identifiers (function/handler names) and
  webpack-split i18n key fragments, e.g. `closeGestureConfiguration`, `GestureConfiguration`,
  `gestureActions`, `gestureInfo`, `gestureUrl`, `backLightAssignment`, `backlightConfigurationType`,
  and string-split i18n keys like `GESTURE_AC…`, `GESTURE_BU…`, `GESTURE_LI…`, `GESTURE_MA…`.
  These are enough to *grep-locate* a screen but **not** to read its layout from the JS.
- **Styling** is in the compiled CSS bundles (`app.min.css`, `cc.min.css`) — readable class rules exist
  but are mangled; treat as color/spacing reference only, study live instead.
- **Schema-driven forms:** the `@rjsf/*` dependency means many config panels are rendered from JSON
  schemas — pair this with the readable `LogiOptionsPlus/data/` JSON (defaults/cards/macros) when
  reconstructing a panel's fields.

---

## Screens relevant to our phases

Paths are into `.planning/phases/04.1-options-plus-reference-mining/asar-extract/`.
"Anchors" = readable identifiers/keys to grep; bundles are minified so layout must be studied live.

| Our concern (phase) | Where it lives | Readable anchors / how to find it |
|---------------------|----------------|-----------------------------------|
| **Gesture builder** (4.2, 6) | `app.min.js` (also `cc.min.js`, `vendors.min.js`) | grep `GestureConfiguration`, `closeGestureConfiguration`, `gestureActions`, `gestureInfo`, `gestureUrl`, i18n `GESTURE_*`. Cross-ref `vocabulary.md` keys (e.g. `GESTURE_ACTION_HOLD_MOVE_UP`). Demo GIFs in `assets/`. |
| **Button assign / cards** (3, 7) | `app.min.js`, `cc.min.js` | grep `button`, `card`, `applicationId`; pair with `LogiOptionsPlus/data/card_presets/*.json` + `data/macros/predefined_*.json` (readable) for the actual card/step vocabulary. |
| **Scroll / SmartShift / ThumbWheel** (3) | `app.min.js` | grep `smartshift`, `thumbwheel`, `backLightAssignment`, `backlightConfigurationType`. |
| **Overlay / OSD / radial action wheel** (6) | `cc.min.js` (overlay window) + lazy chunks `520.min.js`, `46.min.js` | grep `overlay`, `radial`, `wheel`; the `fabric-guideline-plugin` (canvas) dep is the likely radial editor. **NOTE: most `osd`/`radial` hits in the minified chunks are mangled noise** — do NOT treat raw grep tokens as real names here. Use `LogiOptionsPlus/data/overlay/osd_resources.json` + `notification_resources.json` + `overlay/icons/` (readable) as the real OSD model, and study the wheel live. |
| **Profiles / per-app** (3, 5) | `cc.min.js`, `app.min.js`, `main.js` | grep `profile`; pair with `LogiOptionsPlus/data/applications.json` (readable app-match DB) and `app-match-model.md`. |
| **Backlight** (8) | `app.min.js` | grep `backlight`, `backLightAssignment`, `backlightConfigurationType`. |

---

## How to study (for the Plan-04 spec author)

1. **Prefer the running app + readable data over the minified JS.** The bundles are mangled — you can
   *locate* a screen by grepping the anchors above, but you cannot read its layout from the JS.
   - Run the real Options+ (Windows install) and screenshot each screen for the design spec.
   - For exact strings/vocabulary, use the **readable** `LogiOptionsPlus/data/strings/*.yaml` (already
     distilled in `vocabulary.md`) — not the string-split keys in `app.min.js`.
2. **Open the HTML entry points in a browser** (`index.html`, `cc.html`) to confirm window structure
   (`#root`, `#modal-root`, `#toast-top-right-root` in `cc.html` ⇒ modal + toast layers).
3. **Browse `assets/`** for the gesture/feature **demo GIFs** and device-render PNGs/SVGs — these are
   the clearest source for the *visual* design (filenames are hashes; open them to identify).
4. **Grep bundles only to confirm a feature exists / find its anchor**, e.g.
   `grep -o 'GestureConfiguration[A-Za-z]*' app.min.js`, then study that screen live.
5. **Reconstruct schema-driven panels** by pairing the `@rjsf/*` form pattern with the readable
   `LogiOptionsPlus/data/` JSON (defaults, card_presets, macros, overlay).
6. The companion specs in this dir (`vocabulary.md`, `smart-action-schema.md`, `app-match-model.md`,
   `overlay-osd-spec.md` once written) are the *owned* outputs — this inventory just points you at
   the raw material so Plan 04 can write `ui-design-spec.md` and `overlay-osd-spec.md`.
