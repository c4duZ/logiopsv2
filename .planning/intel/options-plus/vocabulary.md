# Options+ Vocabulary — Owned Distillation (REF-01)

> **Reference-only distillation — see [legal-boundary.md](./legal-boundary.md).**
> Source: `LogiOptionsPlus/data/strings/{pt-BR,en-US}.yaml` (gitignored, **not bundled**).
> This is OUR OWN distilled string table built from cited Options+ keys. It is the
> re-implementation target Phase 4.2 (gesture UX) and Phases 5–8 consume — the
> "Our intended label" column is what **we** ship in our own QML/strings, NOT a copy
> of Logitech's resources. Only the relevant term families are distilled here; the full
> 5421-line YAMLs are never reproduced.

**Conventions used below:**
- *Options+ key* = the exact flat YAML key in `strings/*.yaml` (the citation/grep anchor).
- *pt-BR* / *en* = the Options+ string values (quoted for evidence, not for shipping verbatim).
- *Our intended label* = what we will actually render in our own UI (may match, paraphrase, or improve).

---

## 1. Gestures

The Options+ gesture model: a **gesture button** that, while held, maps four directional
"HOLD + MOVE" actions plus a plain "CLICK". The user either picks a **preset** (predefinição)
or builds a **Custom** (Personalizada) gesture. Phase 4.2 must mirror this directional model
against the daemon's existing `GestureAction` / per-direction `Gesture` objects
(`src/logid/actions/gesture/`).

### 1.1 Directional actions (HOLD + MOVE / CLICK)

| Concept | Options+ key | pt-BR | en | Our intended label |
|---------|--------------|-------|----|--------------------|
| Hold + move up | `GESTURE_ACTION_HOLD_MOVE_UP` | "SEGURAR + MOVER PARA CIMA" | "HOLD + MOVE UP" | Segurar + mover para cima / Hold + move up |
| Hold + move down | `GESTURE_ACTION_HOLD_MOVE_DOWN` | "SEGURAR + MOVER PARA BAIXO" | "HOLD + MOVE DOWN" | Segurar + mover para baixo / Hold + move down |
| Hold + move left | `GESTURE_ACTION_HOLD_MOVE_LEFT` | "SEGURAR + MOVER PARA ESQUERDA" | "HOLD + MOVE LEFT" | Segurar + mover para esquerda / Hold + move left |
| Hold + move right | `GESTURE_ACTION_HOLD_MOVE_RIGHT` | "SEGURAR + MOVER PARA DIREITA" | "HOLD + MOVE RIGHT" | Segurar + mover para direita / Hold + move right |
| Click (no move) | `GESTURE_ACTION_CLICK` | "CLIQUE EM" | "CLICK" | Clique / Click |

### 1.2 Gesture vs Custom (preset model)

| Concept | Options+ key | pt-BR | en | Our intended label |
|---------|--------------|-------|----|--------------------|
| Assignment: Gestures | `ASSIGNMENT_NAME_GESTURE` | "Gestos" | "Gestures" | Gestos / Gestures |
| Assignment: Custom gesture | `ASSIGNMENT_NAME_CUSTOM_GESTURE` | "Personalizada" | "Custom" | Personalizada / Custom |
| Card description (preset-or-custom) | `GESTURE_CARD_DESCRIPTION` | "Escolha uma predefinição ou selecione uma personalizada para criar sua própria predefinição." | "Choose a preset or select custom to create your own." | "Escolha uma predefinição ou crie a sua própria." / "Choose a preset or build your own." |
| Custom gestures (panel title) | `GESTURE_TITLE_DESCRIPTION` | "Gestos personalizados" | "Custom gestures" | Gestos personalizados / Custom gestures |
| Custom gestures for profile | `GESTURE_TITLE_DESCRIPTION_FOR_PROFILE` | "Gestos personalizados para %0" | "Custom gestures for %0" | Gestos personalizados para %0 / Custom gestures for %0 |

### 1.3 Gesture-button slot names

| Concept | Options+ key | pt-BR | en | Our intended label |
|---------|--------------|-------|----|--------------------|
| Gesture button (slot) | `SLOT_NAME_GESTURE_BUTTON` | "Botão de gesto" | "Gesture button" | Botão de gesto / Gesture button |
| Slot: left | `SLOT_NAME_GESTURE_LEFT_BUTTON` | "esquerda" | "left" | esquerda / left |
| Slot: right | `SLOT_NAME_GESTURE_RIGHT_BUTTON` | "direita" | "right" | direita / right |
| Slot: up | `SLOT_NAME_GESTURE_UP_BUTTON` | "aumentar" | "up" | cima / up |
| Slot: down | `SLOT_NAME_GESTURE_DOWN_BUTTON` | "diminuir" | "down" | baixo / down |
| Slot: click | `SLOT_NAME_GESTURE_CLICK_BUTTON` | "clique em" | "click" | clique / click |

> Note: in pt-BR the up/down slot strings are "aumentar"/"diminuir" (increase/decrease),
> reflecting Options+ reuse of these slots for value-adjusting actions (e.g. volume). For
> directional gesture rendering we standardize on **cima/baixo** (up/down); we keep the
> increase/decrease wording only where the assigned action is a value step.

### 1.4 Gesture info / help descriptions

| Concept | Options+ key | pt-BR | en | Our intended label |
|---------|--------------|-------|----|--------------------|
| Hold+move help | `GESTURE_INFO_HOLD_MOVE_DESCRIPTION` | "Segure o botão e mova o mouse" | "Hold the button and move the mouse" | Segure o botão e mova o mouse / Hold the button and move the mouse |
| Click help | `GESTURE_INFO_CLICK_DESCRIPTION` | "Clique no botão" | "Click the button" | Clique no botão / Click the button |

---

## 2. Scroll / SmartShift / Hi-Res / Thumbwheel

Maps onto the daemon's existing `SmartShift`, `HiresScroll`, and `ThumbWheel` features
(`src/logid/features/`). These are the user-facing labels Phases 3/4.2 reuse.

| Concept | Options+ key | pt-BR | en | Our intended label |
|---------|--------------|-------|----|--------------------|
| SmartShift (config title) | `CONFIGURATION_TITLE_SMARTSHIFT` | "SmartShift" | "SmartShift" | SmartShift |
| SmartShift explanation | `CONFIGURATION_COPY_SMARTSHIFT` | "Comuta automaticamente a roda de rolagem de linha por linha para rolagem hiperveloz quando você rola mais rápido." | "Automatically switches the scroll wheel from line-by-line scrolling to hyper-fast scrolling when you scroll faster." | "Alterna automaticamente entre rolagem linha a linha e rolagem livre conforme você rola mais rápido." / "Automatically switches between line-by-line and free scrolling as you scroll faster." |
| Free spin (mode) | `SCROLL_MODE_FREE_SPIN_LABEL` | "Rolagem livre" | "Free spin" | Rolagem livre / Free spin |
| Free spin tooltip | `SCROLL_MODE_FREE_SPIN_TOOLTIP` | "Rolagem hiperveloz" | "Hyper-fast scroll" | Rolagem hiperveloz / Hyper-fast scroll |
| Ratchet (mode) | `SCROLL_MODE_RATCHET_LABEL` | "Catraca" | "Ratchet" | Catraca / Ratchet |
| Ratchet tooltip | `SCROLL_MODE_RATCHET_TOOLTIP` | "Rolagem linha a linha" | "Line-by-line scroll" | Rolagem linha a linha / Line-by-line scroll |
| Shift wheel mode (action) | `ASSIGNMENT_NAME_MODE_SHIFT` | "Alterar modo da roda" | "Shift wheel mode" | Alternar modo da roda / Shift wheel mode |
| Inverted direction | `SCROLL_DIRECTION_INVERTED_LABEL` | "Invertido" | "Inverted" | Invertido / Inverted |
| Hi-res sensor enable | `ENABLE_HIGH_RESOLUTON_SENSOR` | "Amplie o alcance do sensor para 8K DPI" | "Extend sensor range to 8K DPI" | Ampliar alcance do sensor (8K DPI) / Extend sensor range (8K DPI) |
| Scroll wheel settings (slot) | `SLOT_NAME_SCROLL_WHEEL_SETTINGS` / `ASSIGNMENT_NAME_SCROLL_WHEEL_SETTINGS` | "Roda de rolagem" | "Scroll wheel" | Roda de rolagem / Scroll wheel |
| Thumb wheel (slot) | `SLOT_NAME_THUMBWHEEL` / `ASSIGNMENT_NAME_THUMBWHEEL_SETTINGS` | "Roda para o polegar" | "Thumb wheel" | Roda do polegar / Thumb wheel |
| Thumb wheel speed | `CONFIGURATION_TITLE_THUMBWHEEL_SPEED` | "Velocidade da roda de polegar" | "Thumb wheel speed" | Velocidade da roda do polegar / Thumb wheel speed |
| Zoom using scroll wheel | `ASSIGNMENT_NAME_ZOOM_USING_SCROLL_WHEEL` | "Zoom usando roda de rolagem" | "Zoom using scroll wheel" | Zoom com a roda de rolagem / Zoom using scroll wheel |

> **Ratchet vs free-spin** is the user-facing framing of `HiresScroll`/SmartShift's
> mechanical hyper-scroll toggle. Our Pointer/Scroll tabs already model this (Phase 3);
> the vocabulary here just locks the labels.

---

## 3. Smart Actions (preview)

> **Preview only.** The full step/card/trigger schema is **Plan 02**
> (`smart-action-schema.md`, sourced from `data/macros/` + `integrations/`). Here we capture
> only the top-level user-facing Smart Action UI labels so Phase 7 planning has the wording.

| Concept | Options+ key | pt-BR | en | Our intended label |
|---------|--------------|-------|----|--------------------|
| Feature title | `MACROS_TITLE` | "Smart Actions" | "Smart Actions" | Smart Actions / Ações inteligentes |
| When… (trigger) | `MACROS_WHEN` | "Se..." | "If..." | Se… / If… |
| Then… (action) | `MACROS_THEN` | "Então..." | "Then..." | Então… / Then… |
| Or | `MACROS_OR` | "Ou" | "Or" | Ou / Or |
| Add trigger | `MACROS_ADD_TRIGGER` | "Adicionar gatilho" | "Add trigger" | Adicionar gatilho / Add trigger |
| Add action | `MACROS_ADD_ACTION` | "Adicionar ação" | "Add action" | Adicionar ação / Add action |
| Keystroke | `MACROS_KEYSTROKE` | "Pressionamento de tecla" | "Keystroke" | Pressionar tecla / Keystroke |
| Text | `MACROS_TEXT` | "Texto" | "Text" | Texto / Text |
| Key shortcut | `MACROS_KEY_SHORTCUT` | "Atalho de tecla" | "Key shortcut" | Atalho de teclado / Key shortcut |
| Application (trigger) | `MACROS_APPLICATION` | "Aplicação" | "Application" | Aplicativo / Application |
| System | `MACROS_SYSTEM` | "Sistema" | "System" | Sistema / System |
| Device | `MACROS_DEVICE` | "Dispositivo" | "Device" | Dispositivo / Device |
| Delay | `MACROS_DELAY` | "Atraso" | "Delay" | Atraso / Delay |

---

## 4. Predefinição vs Personalizada model (mental model note)

Options+ frames gestures (and several other configurable controls) around a single recurring
choice the user makes:

> **Pick a preset (predefinição), OR build a custom (personalizada) one.**

Evidence: `GESTURE_CARD_DESCRIPTION` = *"Escolha uma predefinição ou selecione uma
personalizada para criar sua própria predefinição."* ("Choose a preset or select custom to
create your own."), with `ASSIGNMENT_NAME_GESTURE` ("Gestos"/"Gestures") vs
`ASSIGNMENT_NAME_CUSTOM_GESTURE` ("Personalizada"/"Custom") as the two assignment modes.

**What Phase 4.2 must mirror:**
1. A gesture button presents a **card** offering ready-made **predefinições** (presets) — a
   curated set of common directional mappings the user can apply in one click.
2. Selecting **Personalizada/Custom** opens the per-direction editor where each of the four
   HOLD+MOVE directions plus CLICK gets an individually assigned action (this is exactly the
   daemon's per-direction `Gesture` map).
3. A custom configuration can itself be saved/labeled as the user's own preset
   ("criar sua própria predefinição").

This preset-or-custom duality is the load-bearing UX pattern. Our re-implementation keeps the
two-mode framing but ships entirely our own preset content and our own strings (per
`legal-boundary.md`).
