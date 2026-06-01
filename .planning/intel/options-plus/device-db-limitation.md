# Device-DB Limitation — Encrypted, Unusable (REF-03)

> **Reference-only finding — see [legal-boundary.md](./legal-boundary.md).**
> Source: `LogiOptionsPlus/data/devices/devices_*.json` (gitignored, **not bundled**).

## What the per-device DB is

Options+ ships a per-device descriptor database at
`LogiOptionsPlus/data/devices/devices_*.json` (**45 files** in this install). Each file is
**encrypted/signed**, not plain JSON: it begins with a tiny clear-text JSON header followed by
a **binary blob**.

Observed header shape (from `head -c 200 .../devices_0000001.json`):

```
<4 binary bytes> {"key-id": "9ef13a74-b79c-4d86-98e1-90e115e43b32",
                  "file-sha": "8eff21a1...028446"}<binary payload …>
```

- `key-id` — identifies which key decrypts/verifies the payload.
- `file-sha` — integrity hash of the (decrypted) content.
- Everything after the header is opaque binary (the actual device descriptor), **not** readable
  as text or JSON.

The associated keys are present in the install but are **not usable secrets we may exploit**:

- `LogiOptionsPlus/data/firmware.pem`
- `LogiOptionsPlus/data/logitech-lap-public.pem`

(A *public* key verifies signatures; it does not grant us a legitimate path to repackage or
ship Logitech's descriptor data.)

## Our stance: we do NOT attempt decryption

We do **not** attempt to decrypt, reverse, or repackage these files. The reason is both:

1. **Legal** — the device descriptor data is Logitech proprietary content and is explicitly on
   the *Forbidden to bundle* list in `legal-boundary.md`. Even if decryptable, we could not
   ship it.
2. **Futile** — the format is encrypted/signed and key-gated; treating it as a data source
   would be fragile, unsupported, and pointless given (1).

The encrypted device DB is therefore recorded as **unusable** for this project.

## Fallback: live HID++ enumeration (+ public DBs)

Device capability does **not** depend on this DB. Our authoritative source is:

- **Live HID++ enumeration.** The `logid` daemon already discovers each device's real
  capabilities at runtime by probing HID++ 2.0 features — the existing `_addFeature` flow
  silently skips unsupported features (`features::UnsupportedFeature` as control flow). This is
  how DPI/SmartShift/HiresScroll/ThumbWheel/ReprogControls/ChangeHost/battery are already
  detected today. **Live HID++ enumeration is the canonical capability source.**
- **Public open-source databases** (Solaar, libratbag) where a genuinely *static* descriptor
  (e.g. a friendly model name or a quirk table) is needed and live probing can't provide it.

## Forward note (binding on all later phases)

**No phase (4.2, 5, 6, 7, 8) may plan work that depends on reading
`LogiOptionsPlus/data/devices/devices_*.json`.** Any device-capability or descriptor need must
be satisfied by live HID++ enumeration or a public DB — never by these encrypted files.
