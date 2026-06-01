# Legal Boundary — Hybrid / Reference-Only Rule (REF-03)

> **LOCKED DECISION** (Phase 4.1 `CONTEXT.md`, 2026-05-31): adaptation depth = **HYBRID**.
> This document is the standing rule for every forward phase and the **Phase 9 packaging gate**.

## The rule

We **adapt behavior + vocabulary** and **study the UI design** of Logitech Options+ as
reference material, and we **re-implement everything in our own QML, our own strings, and our
own assets**. We treat the local `LogiOptionsPlus/` install (gitignored, ~531 MB) strictly as
**reference-only** source material that is mined into owned specs under
`.planning/intel/options-plus/`.

We do **NOT** bundle, copy, embed, or redistribute any Logitech proprietary asset into the
shipped application (`src/`) or the produced `.deb`.

This is a **reference-only** boundary: read and learn from the source product; ship only what
we wrote ourselves.

## Forbidden to bundle (must NOT reach `src/` or the `.deb`)

| Proprietary asset category | Where it lives in `LogiOptionsPlus/` | Why forbidden |
|----------------------------|--------------------------------------|---------------|
| Electron UI archive (`app.asar` content) | `resources/app.asar` (HTML/CSS/JS, ~83 MB) | Logitech's actual application code/markup — study only, never ship |
| Verbatim Options+ string resources | `data/strings/*.yaml` (incl. `pt-BR.yaml`, `en-US.yaml`) | Logitech-authored copy; we ship our own re-worded strings (see `vocabulary.md`) |
| Logitech icons / art / imagery | `data/overlay/icons/`, `resources/icon_kiros.png`, app.asar image assets | Logitech trademarks/artwork |
| Compiled UI resource bundle | `resources/resources.pak` | Logitech-packed UI resources |
| Device descriptor data | `data/devices/devices_*.json` (encrypted), `data/defaults/*.json` | Logitech device DB (also encrypted — see `device-db-limitation.md`) |
| Macro / integration content | `data/macros/predefined_*.json`, `integrations/plugin_*/config.json`, `data/card_presets/*.json`, `data/rap/*.json` | Logitech-authored preset content (schema may be adapted; **content** is not shipped) |
| App-match database | `data/applications.json` | Logitech-authored matching DB (the *model* may be adapted; the data file is not shipped) |

**Schema vs content distinction:** we MAY adapt the *structure/model* of a Logitech data file
(e.g. the Smart-Action step schema, the app-match rule shape) into our own spec and our own
re-authored content. We MUST NOT ship Logitech's actual files or their verbatim payload.

## Allowed

- Our **own distilled specs** under `.planning/intel/options-plus/` (`vocabulary.md`,
  `legal-boundary.md`, `device-db-limitation.md`, and the Plan 02–05 specs). These are
  planning artifacts that cite Options+ keys as evidence — they are not shipped in the app.
- Our **own re-implemented QML and strings** authored from scratch (informed by, not copied
  from, the reference).
- **Behavior and vocabulary adaptation** — replicating *how a feature works* and *what concepts
  are called*, expressed in our own wording.
- **UI-design study** — learning layout/flow/interaction patterns from `app.asar` and writing
  our own design spec (Plan 05, `ui-design-spec.md`); the extracted archive itself stays under
  reference, never bundled.
- Capability data from **live HID++ enumeration** (the daemon already does this) and **public
  open-source DBs** (Solaar, libratbag) where a static descriptor is needed.

## Phase 9 packaging gate (release blocker)

The Debian packaging phase MUST verify, as a **BLOCKING** release gate, that the produced
`.deb` ships **ONLY our own assets/strings/UI** and contains **no Logitech proprietary asset**
(app.asar content, Options+ strings, icons/art, `resources.pak`, device/descriptor data).

- This is already wired into the roadmap: **Phase 9 → `09-02-PLAN.md` (PKG-02)** carries a
  *"BLOCKING legal-asset audit"*, and the Phase 9 section records *"Legal gate (from 4.1)"* as
  a release blocker.
- The audit fails the build if any forbidden-category file or its payload is found inside the
  package tree.
- No release may ship until this audit passes.

**Cross-links:** `.planning/ROADMAP.md` Phase 9 (`09-02-PLAN.md`, PKG-02 legal-asset audit);
this boundary; and `vocabulary.md` (our re-authored strings) + `device-db-limitation.md`
(encrypted device DB).
