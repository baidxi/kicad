# Upstream design note — instance-safe schematic footprint write (KiCad)

**Status** : design proposal (not implemented upstream)  
**MCP missions** : R145 audit → R157 spike proof → **R158 official limit**  
**Audience** : KiCad eeschema maintainers, MCP integrators  
**Related** : `MCP_DECISIONS.md` R145, R157, R158 ; spike `r157_spike_footprint_instance_safe_write.ps1`

---

## A. Problem

Hierarchical KiCad schematics can place the **same symbol definition** (`SCH_SYMBOL` on a shared sub-sheet file) on **multiple sheet instances** (e.g. InstA / InstB → `sub_shared.kicad_sch`).

Each placement has a distinct hierarchical **`instance_path`** and reference (R1, R2), but today shares **one** `(property "Footprint" …)` on the symbol object.

**Current public write path** :

```cpp
void SCH_SYMBOL::SetFootprintFieldText( const wxString& aFootprint );
```

This sets the **global** FOOTPRINT field on the symbol screen object. MCP `schematic_assign_footprint_session` uses this path (via `SCH_COMMIT`).

**Impact for AI / automation** :

- An agent targeting **one** missing footprint in hierarchy cannot safely mutate without risking **all** instances of a shared symbol.
- `allow_shared_symbol_global_update=true` is an explicit **global** opt-in (R146), **not** per-instance isolation.
- MCP **`scope=hierarchy` mutation** remains blocked with **`hierarchy_instance_write_unsupported`** until upstream provides instance-safe write.

---

## B. R157 proof (negative)

**Fixture** : `qa/data/eeschema/hierarchy/ipc_footprint_instance_write_spike/`

| Element | Detail |
|---------|--------|
| Shared symbol UUID | `c1000005-0005-4005-8005-000000000005` |
| Instance A | R1 on InstA — distinct `instance_path` |
| Instance B | R2 on InstB — distinct `instance_path` |
| Initial footprint | empty on both (readiness `scope=hierarchy`) |

**Spike procedure** (`r157_spike_footprint_instance_safe_write.ps1`) :

1. Readiness before — A and B empty.
2. `navigate_schematic_sheet_session(instance_path=A)`.
3. `schematic_assign_footprint_session(scope=current_sheet, allow_shared_symbol_global_update=true)` — exercises **`SetFootprintFieldText`** global path.
4. Readiness after — **both A and B** show `Resistor_SMD:R_0201_0603Metric`.

**Measured summary** :

```text
instance_safe_write_supported=false
instance_a_changed_ok=true
instance_b_unchanged_ok=false
readiness_reflects_instance_write=true   # reflects global write on all instances
save_reload_preserved=false              # no MCP save; session-only without persistence proof
```

**Conclusion** : no instance isolation with current KiCad API. Mutation MCP `scope=hierarchy` must **not** be enabled on this basis.

---

## C. Target API (conceptual)

### Preferred: mirror `SetValueFieldText` pattern

`SetValueFieldText` already supports sheet path + project variant:

```cpp
void SCH_SYMBOL::SetValueFieldText( const wxString& aValue,
                                    const SCH_SHEET_PATH* aInstance = nullptr,
                                    const wxString& aVariantName = wxEmptyString );
```

**Proposed symmetric API for footprint** :

```cpp
bool SCH_SYMBOL::SetFootprintFieldTextForSheet(
        const wxString& aFootprint,
        const SCH_SHEET_PATH& aSheetPath,
        const wxString& aVariantName = wxEmptyString );
```

**R159 status** : implemented in fork (`eeschema/sch_symbol.*`) with **`m_FootprintOverride`** on **`SCH_SYMBOL_INSTANCE`**. QA-only invocation — **not** wired to public MCP assign.

```cpp
void SCH_SYMBOL::SetFootprintFieldText( const wxString& aFootprint,
                                        const SCH_SHEET_PATH* aSheetPath = nullptr,
                                        const wxString& aVariantName = wxEmptyString );
```

**Semantics when `aSheetPath` is non-null and symbol is shared** :

- Store footprint override **for that hierarchical instance only**.
- Do **not** modify `(property "Footprint" …)` global default unless explicitly documented as fallback for non-instanced symbols.

### Alternative considered in audit (R157)

| Path | Verdict |
|------|---------|
| `SCH_FIELD::SetText(text, &path, variant)` with **non-empty** variant | Stores in `instance.m_Variants` — requires **named project variant**, not sheet-instance identity |
| `SCH_SYMBOL_INSTANCE::m_Footprint` | Marked **« Do not use »** in `sch_sheet_path.h`; not read by `GetFootprintFieldText(true, &path, false)` |
| Legacy `(footprint …)` in `(instances …)` parse | Calls **`SetFootprintFieldText`** global — not instance-safe |

**Recommendation** : implement **sheet-path instance override** (parallel to reference via `SetRef`), not variant-only storage.

### Read path alignment

Readiness and assign dry-run use:

```cpp
symbol->GetFootprintFieldText( true, &sheetPath, false );
```

Upstream must ensure **`GetFootprintFieldText(true, &sheetPath, …)`** resolves instance override **before** global property fallback.

### Persistence

**R159 prototype** : writer emits `(footprint …)` under `(path …)` when **`m_FootprintOverride`** ; parser stores per-instance override (no global **`SetFootprintFieldText`** on instance path tokens).

### Undo / redo

**R159 prototype** : **`SCH_COMMIT::Modify(symbol, screen)`** + **`SetFootprintFieldTextForSheet`** ; spike QA tests **`undo`** / **`redo`** via **`qa_spike_footprint_instance_api`**.

---

## D. Expected semantics (acceptance)

| Rule | Requirement |
|------|-------------|
| Targeting | Write with `(symbol, sheetPath)` affects **one** hierarchical instance |
| Readback | `GetFootprintFieldText(true, &targetSheetPath, false)` returns new value on target |
| Isolation | Other instances of same shared symbol **unchanged** |
| Readiness | `schematic_footprint_readiness_session scope=hierarchy` lists per-instance footprints correctly |
| Save/reload | Difference A vs B preserved after save and reopen |
| Undo | Undo reverts only targeted instance |
| Global opt-in | `allow_shared_symbol_global_update` remains **global** semantics — not a substitute |

---

## E. Upstream validation criteria (R159/R160 validated in fork)

**R159** : prototype **`SetFootprintFieldTextForSheet`** — isolation A/B, save/reload, undo/redo (QA tool).

**R160** : MCP public **`scope=hierarchy`** mutation wired to same API.

**R161** : product status **`hierarchy_assign_instance_safe_enabled`** — documentation mini-release.

Re-run **`r157_spike_footprint_instance_safe_write.ps1`** (or successor) against upstream build:

| Field | Required for MCP hierarchy unlock |
|-------|-------------------------------------|
| `instance_safe_write_supported` | **`true`** |
| `instance_a_changed_ok` | **`true`** |
| `instance_b_unchanged_ok` | **`true`** |
| `readiness_reflects_instance_write` | **`true`** |
| `save_reload_preserved` | **`true`** |
| `undo_redo_instance_safe` | **`true`** (new probe / manual QA) |

Additional MCP probes (non-regression):

- `targeted_schematic_assign_footprint_probe.ps1` — hierarchy mutation path green with isolation
- `r156_example_navigable_sch_footprint_workflow.ps1` — product workflow unchanged or extended
- `run_mcp_v2_ci_baseline.ps1 -VerifyOnly` — green

---

## F. Future MCP impact

**R160 (activated)** :

- **`schematic_assign_footprint_session scope=hierarchy`** : mutation **instance-safe** via **`SetFootprintFieldTextForSheet`** ; **`instance_safe_write_used=true`**, **`global_update=false`**.
- Blocker **`hierarchy_instance_write_unsupported`** : **retired** for supported builds ; fallback **`instance_safe_write_unavailable`** if API disabled.
- **Recommended workflow (conservative)** : R156 — readiness → navigate → active sheet → assign **`current_sheet` dry-run**.
- **Direct hierarchy workflow (R160+)** : readiness hierarchy → assign **`scope=hierarchy`** with **`instance_path`**.

**Unchanged** :

- **`allow_shared_symbol_global_update`** stays **global** opt-in on **`current_sheet`** — never automatic instance-safe mode.

---

## References (code audit R157)

| File | Relevance |
|------|-----------|
| `eeschema/sch_symbol.cpp` | `SetFootprintFieldText`, `SetFieldText`, `GetFootprintFieldText`, `SetValueFieldText` |
| `eeschema/sch_symbol.h` | API surface |
| `eeschema/sch_sheet_path.h` | `SCH_SYMBOL_INSTANCE`, `m_Footprint` legacy |
| `eeschema/sch_field.cpp` | `SetText(path, variant)`, `getUnescapedText` |
| `eeschema/sch_io/kicad_sexpr/sch_io_kicad_sexpr.cpp` | Instance save format |
| `eeschema/api/api_handler_sch.cpp` | MCP assign uses `SetFootprintFieldText` global |

---

**Document version** : R158 — 2026-06-07
