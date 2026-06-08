# Socle MCP v1 officiel (KiCad)

Document de référence projet : inventaire figé des outils MCP enregistrés dans `common/mcp/mcp_executor.cpp`, classification **stable / beta / QA-only / non retenu**, et liens avec le harness `run_ipc_v1_windows.ps1`.

**Mise à jour** : consolidation post-audit ; les primitives bas niveau ne sont pas étendues dans ce tour.

Guide utilisateur/dev opérationnel : `MCP_KICAD_USER_DEV_GUIDE.md`

---

## Checkpoint projet actuel

- Simulation v1.1 : 9 tools stables (famille consolidée lecture/exécution bornée).
- Library v1 : 4 tools stables (search/inspect SCH + PCB, read-only).
- SCH Connectivity v1 : 2 tools stables (`schematic_net_inspect_session`, `schematic_symbol_connectivity_session`, read-only).
- SCH Actions v1 : 4 tools stables (`schematic_set_symbol_value_session`, `schematic_set_symbol_field_session`, `schematic_set_label_text_session`, `schematic_assign_footprint_session`, write/action contrôlée).
- SCH Geometry v2 : 3 tools stables (`schematic_move_symbol_session`, `schematic_move_label_session`, `schematic_create_wire_segment_session`, actions géométriques contrôlées).
- SCH Authoring v2 : 2 tools stables (`schematic_create_label_session`, `schematic_place_symbol_session`, actions d'authoring SCH contrôlées).
- SCH Wiring v2 : 2 tools stables (`schematic_connect_symbol_pin_session`, `schematic_connect_pins_l_shape_session`, wiring SCH contrôlé pin-to-pin aligné et non aligné).
- SCH Symbol Editing v2 : 3 tools stables (`schematic_set_symbol_field_session`, `schematic_set_symbol_value_session`, `schematic_set_symbol_reference_session`).
- PCB Inspection v2 : 3 tools stables (`pcb_get_board_items_session`, `pcb_get_board_item_by_id_session`, `pcb_get_net_items_session`).
- **pcb_clearance_context v1** (**release R32**, impl. **R31**, checkpoint **R33**) : 1 tool stable — **`get_pcb_clearance_context_session`** (contexte clearance **read-only** : nets / netclasses / couche ; distance **bbox-centre à bbox-centre** si paire — **contexte géométrique**, **pas** clearance bord-à-bord ; **`required_clearance_nm`** = clearance **nominale** netclass (nm) si lisible ; **`clearance_status`** = disponibilité / **context_only** / **rule_unavailable** — **pas** verdict DRC officiel — **`is_true_drc=false`**, **pas** `RunTests`, **pas** rapport de violations) ; QA **`run_pcb_clearance_context_v1_targeted_probes.ps1`** / fixture **`ipc_clearance_v1`** ; famille **intégrée** au checkpoint MCP v2 (**huit** familles, **R33** + **R39**).
- **pcb_nearby_items_context v1** (**release R38**, gate checkpoint **R39**, impl. **R37**, QA **R37-Validation**) : 1 tool stable — **`get_pcb_nearby_items_context_session`** (**contexte de voisinage PCB** ; requête V1 = **`item_id`** + **`radius_nm`** obligatoire — **pas** de mode **point** en V1 ; **`distance_kind`** = **`bbox_center_to_query_nm`** ; scan **linéaire** V1 borné par **`radius_nm`** / **`max_items`** ; **`truncated`** si coupure ; filtres optionnels **`layer`** (**`BL_*`** / alias **`F_Cu`**), **`item_types`** (`pads` / `tracks` / `arcs` / `vias`), **`include_same_net`** défaut **`true`** ; **`max_items`** défaut **25** / plafond **500**) ; **`is_true_drc=false`** — **pas** DRC, **pas** `RunTests`, **pas** verdict clearance ; QA **`targeted_pcb_nearby_items_context_probe.ps1`** + **`run_pcb_nearby_items_context_v1_targeted_probes.ps1`** / fixture **`ipc_nearby_v1`** ; famille **intégrée** au checkpoint MCP v2 (**huit** familles officielles, **R39**).
- **pcb_drc_violations_context v1** (**release R125**, gate checkpoint **R126**, impl. **R124**) : 1 tool stable — **`get_pcb_drc_violations_context_session`** (liste violations DRC **bornée** read-only ; dual-mode **`run_drc`** ; **`is_official_drc=true`** si **`run_drc=true`**, **sans** persistance marqueurs ni mutation board) ; QA **`targeted_pcb_drc_violations_context_probe.ps1`** + **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`** / fixture **`ipc_drc_v1`** ; famille **intégrée** au checkpoint MCP v2 (**neuf** familles, **R126**).
- pcb_geometry v1 (**release produit PCB Geometry v1.3**, **R27** ; mesure distance inchangée depuis **v1.2** **R23** ; **v1.1** = **R19** `bbox_center` / `pad_anchor`) : 2 tools stables — **`pcb_measure_distance_session`** (**mesure** euclidienne 2D nm ; modes : **point–point**, **point–item**, **item–item `bbox_center`**, **item `pad_anchor`**, **item `pad_shape_origin`**) et **`get_pcb_item_geometry_session`** (**introspection géométrique** d’un seul item par `board_filename` + `item_id` — **ne mesure pas** une distance, **ne retourne pas** de polygone ; complète la mesure sans la dupliquer). Champs introspection : bbox min/max/centre nm, position logique si disponible, `layer` / `net_name` si disponibles, **`pad_anchor_*`** pour **`PCB_PAD_T`**, **`pad_shape_origin_*`** + `layer_used` **uniquement** si argument **`layer`** (`BoardLayer` **`BL_*`**, alias court **`F_Cu`**). Pour les **pads**, bbox nominale = fast-path **`ShapePos(layer)` + `GetSize(layer)`** (évite **`PAD::GetBoundingBox()`** / mutex lourd). Pour la **distance** : **`bbox_center`** = **`GetBoundingBox().Centre()`** ; **`pad_anchor`** / **`pad_shape_origin`** comme **v1.2**. Exclusions communes : **pas** clearance, **pas** bord-à-bord générique, **pas** centroïde polygone, **pas** mutation PCB ; sortie **`qa_ipc_smoke` bornée** pour **`pcb_measure_distance_session`** et **`get_pcb_item_geometry_session`** ; flake IPC occasionnel `nng_dial: Connection refused` (R16, retry borné) ; famille QA `run_pcb_geometry_v1_targeted_probes.ps1` (**`targeted_pcb_measure_distance_probe.ps1`** + **`targeted_pcb_item_geometry_probe.ps1`**), checkpoint MCP v2.
- Runners QA officiels :
  - `qa/tools/ipc_tools/run_simulation_v1_1_targeted_probes.ps1`
  - `qa/tools/ipc_tools/run_library_v1_targeted_probes.ps1`
  - `qa/tools/ipc_tools/run_sch_connectivity_v1_targeted_probes.ps1`
  - `qa/tools/ipc_tools/run_sch_actions_v1_targeted_probes.ps1`
  - `qa/tools/ipc_tools/run_sch_geometry_v2_targeted_probes.ps1`
  - `qa/tools/ipc_tools/run_sch_authoring_v2_targeted_probes.ps1`
  - `qa/tools/ipc_tools/run_sch_wiring_v2_targeted_probes.ps1`
  - `qa/tools/ipc_tools/run_sch_symbol_editing_v2_targeted_probes.ps1`
  - `qa/tools/ipc_tools/run_pcb_inspection_v2_targeted_probes.ps1`
  - `qa/tools/ipc_tools/run_pcb_geometry_v1_targeted_probes.ps1`
  - `qa/tools/ipc_tools/run_pcb_clearance_context_v1_targeted_probes.ps1` (**pcb_clearance_context v1** / **R32** + gate **R33**)
  - `qa/tools/ipc_tools/run_pcb_nearby_items_context_v1_targeted_probes.ps1` (**pcb_nearby_items_context v1** / **release R38**, gate **R39** — checkpoint-covered)
  - `qa/tools/ipc_tools/run_pcb_drc_violations_context_v1_targeted_probes.ps1` (**pcb_drc_violations_context v1** / **R126** — checkpoint-covered, **9e** famille)
- Summaries familiaux officiels :
  - `qa_simulation_v1_1_family_summary.txt`
  - `qa_library_v1_family_summary.txt`
  - `qa_sch_connectivity_v1_family_summary.txt`
  - `qa_sch_actions_v1_family_summary.txt`
  - `qa_sch_geometry_v2_family_summary.txt`
  - `qa_sch_authoring_v2_family_summary.txt`
  - `qa_sch_wiring_v2_family_summary.txt`
  - `qa_sch_symbol_editing_v2_family_summary.txt`
  - `qa_pcb_inspection_v2_family_summary.txt`
  - `qa_pcb_geometry_v1_family_summary.txt`
  - `qa_pcb_clearance_context_v1_family_summary.txt` (**R32** / checkpoint **R33**)
  - `qa_pcb_nearby_items_context_v1_family_summary.txt` (**release R38**, gate **R39** — copié dans le dossier checkpoint MCP v2)
  - `qa_pcb_drc_violations_context_v1_family_summary.txt` (**R126** — copié dans le dossier checkpoint MCP v2)
- Module commun QA : `qa/tools/ipc_tools/ipc_qa_common.ps1` utilisé par les 4 probes Library v1.
- Guide utilisateur/dev : `MCP_KICAD_USER_DEV_GUIDE.md`.
- **Baseline MCP v2 reproductible (build + checkpoint)** : `README_IPC_QA_BASELINE.md` ; préflight local `run_mcp_v2_baseline_preflight.ps1` (R24, rôle **R34** + **`pcb_negative_contracts_v1`** **R66** : fail-fast contrats + PCB + délégation au checkpoint **8 familles** **R39** ; résumé **`qa_mcp_v2_baseline_preflight_summary.txt`**) ; pipeline CI **`run_mcp_v2_ci_baseline.ps1`** (inclut garde-fou **`pcb_negative_contracts_v1`** **R65** — **CI baseline guard**, **not** a checkpoint family).
- **Release MCP v2 officielle (R185)** : **`run_mcp_v2_release_pack.ps1`** — commande standard release locale (CI baseline + advisory R180 + check R181 + pack R182/R183) ; exit **0** uniquement si **`release_decision=go`** ; artefacts **`qa_mcp_v2_release_pack_pipeline_summary.txt`**, **`qa_mcp_v2_release_pack.md`**, **`qa_mcp_v2_release_pack.json`** ; distinction **CI dev** (`ci_baseline_green`, G4/G5/G6 non bloquant) vs **release** (R181). Scripts intermédiaires : **`report_mcp_v2_ci_advisories.ps1`** (R180), **`check_mcp_v2_release_advisories.ps1`** (R181), **`export_mcp_v2_release_pack.ps1`** (R182/R183). Détail **`MCP_DECISIONS.md`** R180–R185. **`MCP_TOOL_MAP_R1.tsv`** = handlers MCP uniquement (pas les scripts release QA).
- **Audit CI/CD release pack (R186)** : CI officiel = **GitLab** (`.gitlab-ci.yml`, `.gitlab/Windows-CI.yml`, etc.) ; **pas** de job MCP v2 release aujourd'hui ; GitHub = miroir PR ; intégration recommandée **R187** = job GitLab manuel Windows (Option B). Détail **`MCP_DECISIONS.md`** R186.
- **Job GitLab release pack MCP v2 (R187)** : **`.gitlab/MCP-Windows-Release.yml`** — job manuel **`mcp_v2_release_pack_windows`** (stage **`release`**, tag **`kicad-windows-ltsc2022`**) ; build **`build/msvc-win64-release`** + **`run_mcp_v2_release_pack.ps1`** ; artefacts pack uploadés ; échec si **`release_decision != go`** ; **hors** gates dev MR. Détail **`MCP_DECISIONS.md`** R187.
- Chemin MCP supporté : `kicad.exe <project>` + `KICAD_IPC_AUTOMATION=1` + `qa_open_editor_frame`.
- **Project Snapshot navigation-ready (R133)** : **`get_project_context_snapshot`** = source IA recommandée pour chemins absolus PCB/SCH ; pattern documenté **`MCP_KICAD_USER_DEV_GUIDE.md`** ; protobuf **`DocumentSpecifier`** inchangé (R132 Option B).
- **Project readiness (R138 V1 complet ; R162 workflow discovery)** : bloc **`project_readiness_summary`** dans snapshot — inclut **`hierarchy_assign_instance_safe_enabled`** et champs workflow footprint assign (**R162**) ; détail **`MCP_DECISIONS.md`** R134–R138, R162.
- Dette connue : chemin `--frame` classé diagnostic-only / non-gating.
- Prochaine phase recommandée (au choix) : dette open-frame, factorisation mineure QA, ou nouvelle capability contrôlée.

---

## 1. Vue d’ensemble

| Classe | Nombre | Définition |
|--------|--------|------------|
| **Stable v1** | 73 | Registre actif + couverture QA Windows (résumés `qa_*_contract_*` et/ou assertions bloquantes IPC). |
| **Beta** | 4 | Registre actif, **sans** résumé contractuel dédié sur le harness Windows. |
| **QA-only** | 1 | Outil d’orchestration IPC, pas une API produit grand public. |
| **Non retenu** | 2 | Enregistrés seulement si `kEnableTextGeometryTools == true` (désactivé par défaut). |

**GUI-only** : `oui` = cadre éditeur PCB/SCH ou logique wx pour le cas nominal ; `partiel` = dépend du mode (fenêtre affichée, ex. garde `kicad-mcp`) ; `non` = snapshot / projet sans cadre requis.

**Rollback QA** : `oui` pour les écritures où le harness prouve retour (update inverse, delete, compteur, ou contrat `rollback_ok`). `N/A` pour lectures seules.

**Couverture QA Windows** : `oui` si l’outil est exercé avec preuve dans le flux IPC v1 (résumé, `throw`, ou logs contractuels listés).

---

## 2. Outils stable v1 (73)

| Outil | Catégorie | GUI | Rollback QA | QA Win |
|-------|-----------|-----|-------------|--------|
| `get_project_info` | Workspace | non | N/A | oui |
| `get_workspace_overview` | Workspace | partiel | N/A | oui |
| `get_open_documents` | Workspace | partiel | N/A | oui |
| `get_project_context_snapshot` | Workspace (composé lecture + **`drc_summary`** opt-in **R129** + chemins absolus **R130/R131** ; **navigation-ready R133** ; **`project_readiness_summary` R135a/R138** ; **`footprints_summary` opt-in R135b/R137** ; **`pcb_sync_summary` opt-in R172** ; **`footprints_validate` opt-in**) | partiel | N/A | oui |
| `netclass_patch_session` | Design-touching (composé lecture+action) | non | oui | oui |
| `electrical_profile_prepare_session` | Design-touching (composé intention métier) | non | oui | oui |
| `get_net_classes` | Projet / électrique | non | N/A | oui |
| `set_net_classes` | Design-touching | non | oui | oui |
| `get_pcb_items` | Lecture PCB | oui | N/A | oui |
| `get_pcb_items_by_id` | Lecture PCB | oui | N/A | oui |
| `pcb_get_board_items_session` | PCB Inspection v2 (lecture PCB courante, compteurs + `items_summary` borné, `board_filename` + `item_types` + `max_items`, read-only, sans auto-save) | oui | N/A | oui |
| `pcb_get_board_item_by_id_session` | PCB Inspection v2 (lecture détaillée d’un item PCB par `board_filename` + `item_id`, plat + `details` léger, read-only, sans auto-save ; `BOARD::ResolveItem(KIID, true)`) | oui | N/A | oui |
| `pcb_get_net_items_session` | PCB Inspection v2 (lecture des items PCB d’un net par `board_filename` + `net_name` / `net_code`, compteurs + `items_summary` borné, read-only, sans auto-save ; `BOARD::FindNet`) | oui | N/A | oui |
| `pcb_measure_distance_session` | pcb_geometry v1 / **PCB Geometry v1.2** (distance — inchangé en **v1.3** **R27**) : point–point ; point–item ; item–item `bbox_center` ; item `pad_anchor` ; item `pad_shape_origin` ; distance euclidienne 2D en nm ; **`bbox_center`** = `GetBoundingBox().Centre()` ; **`pad_anchor`** = `PAD::GetPosition()` ; **`pad_shape_origin`** = `PAD::ShapePos(layer)` + **`layer`** obligatoire ; **`layer`** officiel = protobuf **`BoardLayer`** (`BL_F_Cu`, …) ; alias **`F_Cu`** accepté ; pas centroïde polygone, pas clearance, pas bord-à-bord ; pas net-aware / layer-aware générique hors `pad_shape_origin` ; **V1** `{x_nm,y_nm}` sans `position` ; **V2** `{item_id}` + `position` optionnelle ; `layer` interdit sauf `pad_shape_origin` (`invalid_endpoint_*`) ; `dx_nm`/`dy_nm` signés ; `distance_nm` = `llround(hypot(dx_nm,dy_nm))` ; 1 IU = 100 nm ; read-only ; sortie **`qa_ipc_smoke` bornée** ; famille QA + checkpoint MCP v2 | oui | N/A | oui |
| `get_pcb_item_geometry_session` | pcb_geometry v1 / **PCB Geometry v1.3** (**R27**) — introspection géométrique **read-only** (**pas** une mesure de distance, **pas** de polygone) pour **un** item (`board_filename` + `item_id`) : bbox nm (`bbox_*`), centre bbox, `position_*`, `layer`, `net_name` ; **`PCB_PAD_T`** : `pad_anchor_*` = `PAD::GetPosition()` ; `pad_shape_origin_*` = `PAD::ShapePos(layer)` **si** `layer` fourni (`BoardLayer` `BL_*` ou alias `F_Cu`) ; `layer_used` = nom `BL_*` ; bbox pad = fast-path **`ShapePos` + `GetSize`** (évite `PAD::GetBoundingBox()` nominal) ; **pas** clearance, **pas** mutation PCB ; complète **`pcb_measure_distance_session`** ; sortie **`qa_ipc_smoke` bornée** | oui | N/A | oui |
| `get_pcb_clearance_context_session` | **pcb_clearance_context v1** (**release R32**, impl. **R31**, checkpoint MCP v2 **R33**) — **contexte clearance** **read-only** : **pas** DRC complète, **`is_true_drc=false`**, **pas** `RunTests`, **pas** rapport de violations ; distance géométrique = **contexte** (centres bbox), **pas** clearance bord-à-bord ; **`required_clearance_nm`** = nominal netclass (IU×100→nm) si **`HasClearance()`** ; **`clearance_status`** = **`context_only`** / **`rule_unavailable`** / **`not_evaluated`** selon disponibilité — **pas** verdict officiel ; entrées : `board_filename`, `item_id`, `other_item_id`, `layer` optionnels ; cuivre : pad / track / arc / via ; nets/netclasses via **`BOARD_CONNECTED_ITEM`** / **`GetNetClass()`** ; **`geometric_distance_kind`** = `bbox_center_to_bbox_center_context_only` ; **`qa_ipc_smoke` bornée** ; famille **`pcb_clearance_context_v1`** **checkpoint-covered** | oui | N/A | oui |
| `get_pcb_nearby_items_context_session` | **pcb_nearby_items_context v1** (**release R38**, gate **R39**, impl. **R37**) — **liste de voisins** **read-only** (contexte PCB, **pas** DRC) ; V1 = **`item_id`** + **`radius_nm`** — **pas** mode **point** ; scan linéaire ; **`distance_kind`** **`bbox_center_to_query_nm`** ; **`radius_nm`** obligatoire (plafond serveur) ; **`max_items`** défaut **25** / max **500** ; **`truncated`** si coupure ; **`layer`** / **`item_types`** / **`include_same_net`** optionnels ; **`query_kind`** = **`from_item`** ; **`is_true_drc=false`** — **pas** `RunTests`, **pas** verdict clearance ; **`qa_ipc_smoke` bornée** ; famille **`pcb_nearby_items_context_v1`** **checkpoint-covered** (**huit** familles MCP v2) | oui | N/A | oui |
| `get_pcb_drc_violations_context_session` | **pcb_drc_violations_context v1** (**release R125**, gate **R126**) — **liste violations DRC bornée** read-only ; dual-mode **`run_drc`** ; **`qa_ipc_smoke` bornée** ; famille **`pcb_drc_violations_context_v1`** **checkpoint-covered** (**neuf** familles MCP v2) | oui | N/A | oui |
| `get_pcb_sync_readiness_session` | **R170** — read-only SCH↔PCB footprint sync readiness ; matching **`reference_or_path`** ; compteurs missing/orphan/matched ; **hors checkpoint MCP v2** ; probe **`targeted_pcb_sync_readiness_probe.ps1`** | oui | N/A | oui |
| `pcb_update_from_schematic_preview_session` | **R171** — dry-run **`BOARD_NETLIST_UPDATER::SetIsDryRun(true)`** ; preview structurée `actions[]` + compteurs ; **sans mutation PCB** ; **hors checkpoint MCP v2** ; probe **`targeted_pcb_update_preview_probe.ps1`** | oui | N/A | oui |
| `pcb_update_from_schematic_session` | **`pcb_update_apply_controlled_v1`** (**R173** impl., **R174** doc, **R176** DRC advisory, **R177** workflow) — apply contrôlé **`BOARD_NETLIST_UPDATER::UpdateNetlist`** ; mutation **uniquement** via **`confirm_apply=true`** ; opt-in **`include_post_apply_drc_summary`** (advisory, non bloquant) ; preview R171 préalable ; **`delete_orphans=false`** par défaut ; restore QA = fichier fixture ; workflow chaîné **`pcb_apply_drc_save_workflow_v1`** (R177) ; **hors checkpoint MCP v2** ; probe **`targeted_pcb_update_from_schematic_probe.ps1`** + **`r177_example_pcb_apply_drc_save_workflow.ps1`** | oui | N/A | oui |
| `save_board_session` | **R176** / workflow **R177** — save contrôlé PCB ouvert ; gate **`confirm_save=true`** ; `SavePcbFile` headless si writable ; blockers structurés ; **hors checkpoint MCP v2** ; probe **`targeted_pcb_save_board_probe.ps1`** + workflow **`r177_example_pcb_apply_drc_save_workflow.ps1`** | oui | N/A | oui |
| `get_selection_pcb` | Lecture PCB | oui | N/A | oui |
| `hit_test_pcb` | Lecture PCB | oui | N/A | oui |
| `get_board_stackup` | Lecture PCB | oui | N/A | oui |
| `get_nets` | Lecture PCB | oui | N/A | oui |
| `get_bounding_box_pcb` | Lecture PCB | oui | N/A | oui |
| `get_visible_layers` | Session PCB | oui | N/A | oui |
| `get_board_enabled_layers` | Lecture PCB | oui | N/A | oui |
| `get_board_layer_names` | Lecture PCB | oui | N/A | oui |
| `get_active_layer` | Session PCB | oui | N/A | oui |
| `set_active_layer` | Session PCB | oui | oui | oui |
| `set_visible_layers` | Session PCB | oui | oui | oui |
| `get_board_editor_appearance_settings` | Session PCB | oui | N/A | oui |
| `set_board_editor_appearance_settings` | Session PCB | oui | oui | oui |
| `get_board_origin` | Design PCB | oui | N/A | oui |
| `set_board_origin` | Design PCB | oui | oui | oui |
| `get_board_extent` | Lecture PCB | oui | N/A | oui |
| `get_pad_shape_as_polygon` | Lecture PCB (géométrie) | oui | N/A | oui |
| `create_pcb_items` | Content-write PCB | oui | oui | oui |
| `update_pcb_items` | Content-write PCB | oui | oui | oui |
| `delete_pcb_items` | Content-write PCB | oui | oui | oui |
| `pcb_graphic_annotation_session` | Content-write PCB (composé annotation) | oui | oui | oui |
| `pcb_fabrication_origin_prepare_session` | Design PCB (composé intention fabrication) | oui | oui | oui |
| `pcb_origin_decision_correction_session` | Design PCB (composé décision+correction) | oui | oui | oui |
| `get_selection_sch` | Lecture SCH | oui | N/A | oui |
| `get_sch_items` | Lecture SCH | oui | N/A | oui |
| `get_sch_items_by_id` | Lecture SCH | oui | N/A | oui |
| `get_symbol_pins_by_id` | Lecture SCH | oui | N/A | oui |
| `get_symbol_fields_by_id` | Lecture SCH | oui | N/A | oui |
| `schematic_net_inspect_session` | SCH Connectivity (session read-only compacte v1) | oui | N/A | oui |
| `schematic_symbol_connectivity_session` | SCH Connectivity (session read-only compacte v1 par symbole, `symbol_id` + `pins_summary`) | oui | N/A | oui |
| `schematic_set_symbol_value_session` | SCH Actions (write/action contrôlée, undoable, dry-run, sans auto-save) | oui | oui | oui |
| `schematic_set_symbol_field_session` | SCH Actions (write/action contrôlée champ existant, `Reference` interdit, `Value` redirigé, `new_value=""` autorisé, sans auto-save) | oui | oui | oui |
| `schematic_set_label_text_session` | SCH Actions (write/action contrôlée texte local label, undoable, dry-run, sans auto-save) | oui | oui | oui |
| `schematic_assign_footprint_session` | SCH Actions (write/action contrôlée assignation footprint validée, `pcb_ready`, sans auto-save, **R121** release officielle) | oui | oui | oui |
| `schematic_footprint_readiness_session` | SCH Readiness V1 (scan footprint read-only ; **`scope=current_sheet|hierarchy` R141** ; power symbols exclus ; compteurs + **`sch_to_pcb_ready`** ; validation library opt-in ; cache footprint/appel ; sans auto-save) | oui | N/A | oui |
| `schematic_set_symbol_reference_session` | SCH Symbol Editing v2 (write/action contrôlée, référence `Reference` via tool dédié, feuille courante `CurrentSheet()` en v1, sans détection globale de conflit, sans auto-save) | oui | oui | oui |
| `library_symbol_search_session` | Bibliothèques SCH (session de recherche lecture seule) | oui | N/A | oui |
| `hit_test_sch` | Lecture SCH | oui | N/A | oui |
| `create_sch_items` | Content-write SCH | oui | oui | oui |
| `update_sch_items` | Content-write SCH | oui | oui | oui |
| `delete_sch_items` | Content-write SCH | oui | oui | oui |
| `sch_wire_edit_session` | Content-write SCH (composé fil) | oui | oui | oui |
| `sch_wire_quick_fix_session` | Content-write SCH (composé correction fil) | oui | oui | oui |
| `sch_pin_connection_fix_session` | Content-write SCH (composé connexion pin) | oui | oui | oui |
| `sch_connection_repair_decision_session` | Content-write SCH (composé décision+correction) | oui | oui | oui |
| `cross_context_preflight_correction_session` | Multi-contexte global (composé préflight+correction) | partiel | oui | oui |
| `workflow_preflight_gate_session` | Workflow global assistant (composé gate ready/blocked) | partiel | oui | oui |
| `workflow_next_best_action_session` | Workflow assistant global (composé recommandation+exécution) | partiel | oui | oui |
| `workflow_handoff_readiness_session` | Workflow handoff global (composé readiness target) | partiel | oui | oui |
| `workflow_assistant_session` | Assistant global workflow (composé sélection workflow) | partiel | oui | oui |
| `simulation_directive_inspect` | Simulation (lecture seule directive SPICE) | non | N/A | oui |
| `simulation_netlist_preview_session` | Simulation (preview netlist SPICE lecture seule) | partiel | N/A | oui |
| `simulation_model_library_inspect` | Simulation (inspection bibliothèque modèles SPICE lecture seule) | non | N/A | oui |
| `simulation_preflight_readiness_session` | Simulation (préflight global ready/blocked lecture seule) | partiel | N/A | oui |
| `simulation_run_status_session` | Simulation (exécution minimale status-only avec timeout strict) | partiel | N/A | oui |
| `simulation_run_minimal_summary_session` | Simulation (run + résumé minimal borné: plot_name/vectors_count) | partiel | N/A | oui |
| `simulation_run_vector_catalog_session` | Simulation (run + catalogue minimal des vecteurs disponibles, sans valeurs) | partiel | N/A | oui |
| `simulation_vector_scalar_probe_session` | Simulation (run + lecture scalaire unique sur vecteur validé, sample_position=first) | partiel | N/A | oui |
| `simulation_xaxis_probe_session` | Simulation (run + méta axe X minimale: nom d’axe + première valeur uniquement) | partiel | N/A | oui |

---

## 3. Outils beta (4)

| Outil | Catégorie | GUI | Rollback QA | QA Win |
|-------|-----------|-----|-------------|--------|
| `get_kicad_binary_path` | Environnement | non | N/A | **non** |
| `get_plugin_settings_path` | Environnement | non | N/A | **non** |
| `get_text_variables` | Projet / texte | non | N/A | **non** |
| `expand_text_variables` | Projet / texte | non | N/A | **non** |

*Évolution recommandée* : ajouter des résumés contractuels légers ou un job CI non bloquant jusqu’à promotion en stable v1.

---

## 4. QA-only (1)

| Outil | Catégorie | GUI | Rollback QA | QA Win |
|-------|-----------|-----|-------------|--------|
| `qa_open_editor_frame` | Harness / orchestration | oui | N/A | oui |

---

## 5. Non retenus (2)

Compile-time : `kEnableTextGeometryTools == false` dans `mcp_executor.cpp`.

| Outil | Raison |
|-------|--------|
| `get_text_extents` | Quarantaine / non exposé en build par défaut |
| `get_text_as_shapes` | Idem |

---

## 6. Surface publique MCP v1 (recommandation)

### 6.1 Publique (documentation utilisateur / agents)

Tous les outils **stable v1** du §2, **sauf** mise en retrait documentaire optionnelle des niches :

- **Hors doc principale (niche)** : `get_pad_shape_as_polygon`, `get_board_extent` — restent **stables** et sous CI, mais décrits dans une section « avancé / géométrie ».

### 6.2 Internes / beta / QA

- **Beta (§3)** : documenter comme *non garantis* au même niveau que le socle ; ne pas les présenter comme équivalents aux stables.
- **QA-only** : `qa_open_editor_frame` — réservé harness / automation, pas surface produit.

### 6.3 Non retenus

Ne pas documenter comme API v1 tant que le flag de build reste à `false`.

---

## 7. Garde-fous CI minimaux

1. **Portes obligatoires** : build `qa_ipc_smoke` + binaires requis ; exécution `run_ipc_v1_windows.ps1` avec `VsDevCmd` ; échec si `ListTools` ou contrats `qa_*` attendus échouent après ouverture PCB/SCH.
2. **Sources de vérité** : répertoires `build/.../ipc-v1-artifacts/<run>/` ; fichiers `qa_*_contract_summary.txt` / `qa_*_contract_before_summary.txt` (dont `qa_get_open_documents_contract_summary.txt` après ouverture cadre) ; `qa_global_contract_summary.txt` ; logs `mcp_call_tool_args_*_output.log`.
3. **Couverture minimale obligatoire** : tous les outils **stable v1** du §2 (les résumés existants + assertions sur `get_project_info` / `get_workspace_overview` / `get_open_documents`).
4. **Best-effort** : scénarios « No project » secondaires ; parité OS non-Windows ; outils **beta** jusqu’à promotion.

---

## 8. Dette résiduelle minimale

- Quatre outils **beta** sans preuve harness dédiée sur Windows.
- Comportements **NOT_READY** / messages callback à normaliser dans la doc utilisateur (hors ce fichier).
- Pas de `qa_*_contract_before_summary` pour `get_open_documents` (probe avant cadre optionnelle ; le résumé **après** ouverture est la source de vérité principale).

---

## 9. Synthèse finale simulation v1.1

Famille simulation considérée **stabilisée** (lecture seule + exécution bornée), sans surface de résultats riches.

### 9.1 Capabilities simulation stabilisées (9)

1. `simulation_directive_inspect`
2. `simulation_netlist_preview_session`
3. `simulation_model_library_inspect`
4. `simulation_preflight_readiness_session`
5. `simulation_run_status_session`
6. `simulation_run_minimal_summary_session`
7. `simulation_run_vector_catalog_session`
8. `simulation_vector_scalar_probe_session`
9. `simulation_xaxis_probe_session`

### 9.2 Artefacts QA canoniques associés (9)

1. `qa_simulation_directive_inspect_contract_summary.txt`
2. `qa_simulation_netlist_preview_session_contract_summary.txt`
3. `qa_simulation_model_library_inspect_contract_summary.txt`
4. `qa_simulation_preflight_readiness_session_contract_summary.txt`
5. `qa_simulation_run_status_session_contract_summary.txt`
6. `qa_simulation_run_minimal_summary_session_contract_summary.txt`
7. `qa_simulation_run_vector_catalog_session_contract_summary.txt`
8. `qa_simulation_vector_scalar_probe_session_contract_summary.txt`
9. `qa_simulation_xaxis_probe_session_contract_summary.txt`

### 9.3 Nomenclature artefacts (note de packaging)

Les artefacts `qa_simulation_*_contract_open_sch_summary.txt` sont conservés comme **secondaires / historiques** (trace de contexte SCH ouvert).  
La source de vérité packaging reste la série canonique `qa_simulation_*_contract_summary.txt`.

### 9.4 Candidate gardée en réserve (non ouverte)

Prochaine candidate possible si reprise explicite d’un lot simulation :  
`simulation_xaxis_probe_named_session` (nom à confirmer au démarrage du lot).

### 9.5 Runner QA léger officiel (Simulation v1.1)

- Runner officiel : `qa/tools/ipc_tools/run_simulation_v1_1_targeted_probes.ps1`
- Summary agrégé : `qa_simulation_v1_1_family_summary.txt`

Modes supportés :

- `-ExistingRunDir` : mode strict, agrégation depuis un dossier source unique.
- Sans `-ExistingRunDir` : agrégation multi-runs contrôlée (dernier artefact canonique disponible par tool).

Critères de succès attendus dans le summary agrégé :

1. `simulation_v1_1_family_green=true`
2. `directive_ok=true`
3. `netlist_preview_ok=true`
4. `model_library_ok=true`
5. `preflight_ok=true`
6. `run_status_ok=true`
7. `run_minimal_summary_ok=true`
8. `run_vector_catalog_ok=true`
9. `vector_scalar_probe_ok=true`
10. `xaxis_probe_ok=true`
11. `exact_blocker_if_any=N/A`

Précision d’usage :

- Le mode multi-runs est accepté car les artefacts simulation read-only/project-context et SCH-open/runtime peuvent provenir de contextes QA distincts.
- Chaque artefact utilisé doit exister et être vert (`exact_blocker_if_any=N/A`) ; sinon le summary familial reste non vert.

---

## 10. Consolidation famille Library v1 (read-only)

Périmètre consolidé : capacités de recherche/inspection bibliothèque SCH + PCB, sans mutation.

### 10.1 Tools consolidés

1. `library_symbol_search_session`
2. `library_symbol_inspect_session`
3. `library_footprint_search_session`
4. `library_footprint_inspect_session`

### 10.2 Artefacts QA canoniques (preuves ciblées actuelles)

1. `qa_library_symbol_search_session_contract_summary.txt`
2. `qa_library_symbol_inspect_session_contract_summary.txt`
3. `qa_library_footprint_search_session_contract_summary.txt`
4. `qa_library_footprint_inspect_session_contract_summary.txt`

### 10.3 Probes ciblés utilisés comme preuve

1. `targeted_library_symbol_search_probe.ps1`
2. `targeted_library_symbol_inspect_probe.ps1`
3. `targeted_library_footprint_search_probe.ps1`
4. `targeted_library_footprint_inspect_probe.ps1`

### 10.4 Runner QA léger officiel (famille Library v1)

- Runner officiel : `qa/tools/ipc_tools/run_library_v1_targeted_probes.ps1`
- Summary agrégé : `qa_library_v1_family_summary.txt`

Critères de succès attendus dans le summary agrégé :

1. `library_v1_family_green=true`
2. `symbol_search_ok=true`
3. `symbol_inspect_ok=true`
4. `footprint_search_ok=true`
5. `footprint_inspect_ok=true`
6. `exact_blocker_if_any=N/A`

Consolidation QA post-factorisation (sans changement runtime) :

- Module commun : `qa/tools/ipc_tools/ipc_qa_common.ps1`
- Probes Library v1 migrés vers le module commun :
  - `targeted_library_symbol_search_probe.ps1`
  - `targeted_library_symbol_inspect_probe.ps1`
  - `targeted_library_footprint_search_probe.ps1`
  - `targeted_library_footprint_inspect_probe.ps1`
- Validation agrégée post-migration (runner officiel) :
  - `library_v1_family_green=true`
  - `exact_blocker_if_any=N/A`

### 10.5 Notes de process (dette légère)

- Harmonisation future possible : factoriser le helper PowerShell de résolution URL IPC (`mcp_list_tools` / ping / sockets) pour réduire la duplication entre probes ciblés.
- Les probes ciblés constituent la preuve de stabilité actuelle ; l’intégration exhaustive au harness global reste optionnelle tant qu’aucune exigence CI bloquante additionnelle n’est posée.
- Chemin QA MCP supporté/gating (familles Simulation v1.1 et Library v1) :
  - lancement projet : `kicad.exe <project>`
  - automation runtime : `KICAD_IPC_AUTOMATION=1`
  - ouverture éditeur via MCP : `qa_open_editor_frame`
- `qa/tools/ipc_tools/run_open_frame_ipc_smoke.ps1` est classé **diagnostic only** :
  - **non-gating** (ne bloque pas les validations Simulation v1.1 / Library v1)
  - le chemin direct `kicad.exe --frame sch <file.kicad_sch>` reste une dette connue hors chemin supporté MCP
  - correction runtime volontairement différée (risque élevé sur bootstrap `kicad.cpp` / `KIWAY::Player`, alors que le chemin projet est déjà stabilisé)

---

## 11. SCH Connectivity v1

Première capability SCH Connectivity v1 stabilisée côté runtime/probe avec contrat compact borné.

### 11.1 Tools stables

1. `schematic_net_inspect_session` (read-only, résumé compact `nodes_summary`)
2. `schematic_symbol_connectivity_session` (read-only, entrée `symbol_id`, résumé compact `pins_summary`)

### 11.2 Contexte requis (chemin supporté)

- Lancement : `kicad.exe <project>`
- Automation : `KICAD_IPC_AUTOMATION=1`
- Ouverture SCH : `qa_open_editor_frame`

### 11.3 Contrats v1 (résumé)

1. `schematic_net_inspect_session`
   - Input : `schematic_filename`, `net_name`, `ensure_fresh_connectivity`, `max_nodes`
   - Output compact : `ok`, `net_found`, `net_name`, `net_code`, `node_count_total`, `nodes_returned`, `truncated`, `nodes_summary`, `diagnostics`, `exact_blocker_if_any`
2. `schematic_symbol_connectivity_session`
   - Input : `schematic_filename`, `symbol_id`, `ensure_fresh_connectivity`, `max_pins`
   - Output compact : `ok`, `symbol_found`, `symbol_id`, `symbol_ref`, `pin_count_total`, `pins_returned`, `connected_pins_count`, `unconnected_pins_count`, `truncated`, `pins_summary`, `diagnostics`, `exact_blocker_if_any`

### 11.4 Précision contractuelle importante

- `nodes[]` structuré plein **n'est pas émis en v1** (coût/timeout payload observé en validation terrain).
- `nodes_summary` est le format compact **stable v1**.
- `pins[]` structuré plein n'est pas émis en v1 ; `pins_summary` est le format compact stable pour la connectivité par symbole.

### 11.5 Artefacts QA canoniques

- `qa_schematic_net_inspect_session_contract_summary.txt`
- `qa_schematic_symbol_connectivity_session_contract_summary.txt`

### 11.6 Critères de succès

Pour `schematic_net_inspect_session` :

1. `ok=true`
2. `net_found=true`
3. `node_count_valid=true`
4. `nodes_returned_valid=true`
5. `nodes_summary_present=true`
6. `diagnostics_present=true`
7. `exact_blocker_if_any=N/A`

Pour `schematic_symbol_connectivity_session` :

1. `ok=true`
2. `symbol_found=true`
3. `pin_count_valid=true`
4. `pins_returned_valid=true`
5. `pins_summary_present=true`
6. `diagnostics_present=true`
7. `exact_blocker_if_any=N/A`

### 11.7 Runner QA léger officiel

- Runner officiel : `qa/tools/ipc_tools/run_sch_connectivity_v1_targeted_probes.ps1`
- Summary agrégé : `qa_sch_connectivity_v1_family_summary.txt`

Critères de succès (summary agrégé) :

1. `sch_connectivity_v1_family_green=true`
2. `schematic_net_inspect_ok=true`
3. `schematic_symbol_connectivity_ok=true`
4. `exact_blocker_if_any=N/A`

---

## 12. SCH Actions v1

Actions SCH contrôlées validées côté runtime/probe, avec mutation bornée, garde-fous explicites, et rollback vérifié.

### 12.1 Tools stables

1. `schematic_set_symbol_value_session`
2. `schematic_set_symbol_field_session`
3. `schematic_set_label_text_session`
4. `schematic_assign_footprint_session`

Type :

- write/action contrôlée
- undoable
- sans sauvegarde automatique

### 12.2 Contrat v1 (résumé)

`schematic_set_symbol_value_session`  
Input :

- `schematic_filename`
- `symbol_id`
- `new_value`
- `expected_old_value`
- `dry_run`

Output :

- `ok`
- `changed` (**`true`** après mutation réussie si la valeur a changé — fix non-régression **R137/R138** ; **`false`** si dry-run ou valeur inchangée)
- `symbol_id`
- `symbol_ref`
- `old_value`
- `new_value`
- `dry_run`
- `diagnostics`
- `exact_blocker_if_any`

`schematic_set_symbol_field_session`  
Input :

- `schematic_filename`
- `symbol_id`
- `field_id` ou `field_name` (au moins un requis)
- `new_value` (chaîne, `""` autorisé)
- `expected_old_value`
- `dry_run`
- `create_if_missing` (supporté contractuellement, `true` refusé en v1)

Output :

- `ok`
- `changed` (**`true`** après mutation réussie si la valeur a changé — fix non-régression **R137/R138** ; **`false`** si dry-run ou valeur inchangée)
- `symbol_id`
- `symbol_ref`
- `field_id`
- `field_name`
- `old_value`
- `new_value`
- `dry_run`
- `diagnostics`
- `exact_blocker_if_any`

`schematic_set_label_text_session`  
(voir probe `targeted_schematic_set_label_text_probe.ps1` — contrat aligné sessions SCH write.)

`schematic_assign_footprint_session` (**R120** implémentation ; **R121** release officielle SCH Actions v1 ; **R146/R147** Safe Assign Footprint Guard ; **R160** mutation hierarchy instance-safe ; **R161** mini-release **Hierarchy Footprint Assignment enabled**)

**Statut produit (R161)** : **`hierarchy_assign_instance_safe_enabled`**

| `scope` | Mutation | Garde-fous |
|---------|----------|------------|
| **`current_sheet`** | Feuille active — **`SetFootprintFieldText`** (global) | **`shared_symbol_global_update_blocked`** si multi-instance ; opt-in **`allow_shared_symbol_global_update`** |
| **`hierarchy`** | Par instance — **`SetFootprintFieldTextForSheet`** via **`instance_path`** | **`global_update=false`**, **`instance_safe_write_used=true`** ; **`allow_shared_symbol_global_update`** **ignoré** |

**Blocker historique** : **`hierarchy_instance_write_unsupported`** — **obsolète** pour builds R160+ ; fallback **`instance_safe_write_unavailable`** si API upstream indisponible au runtime.

Objectif : assigner un footprint qualifié (`LibNick:FootprintName`) au champ Footprint d’un symbole SCH existant, avec validation bibliothèque footprint et signal **`pcb_ready`** pour l’IA (post-**place_symbol**). Tool dédié — **ne pas** confondre avec **`schematic_set_symbol_field_session`** sur le champ FOOTPRINT (mutation générique sans validation library ni **`pcb_ready`**).

**Safe Assign Footprint Guard (R147 — officialisé)** :

| `scope` | Symbole | Dry-run | Mutation |
|---------|---------|---------|----------|
| **`current_sheet`** (défaut) | non partagé (`affected_instances_count ≤ 1`) | oui | **oui** — comportement historique |
| **`current_sheet`** | partagé (`affected_instances_count > 1`) | oui (expose compteurs) | **non** par défaut → **`shared_symbol_global_update_blocked`** |
| **`current_sheet`** | partagé + **`allow_shared_symbol_global_update=true`** | oui | **oui** — mise à jour globale **volontaire** |
| **`hierarchy`** | toute instance (`instance_path` requis) | oui (résolution + diagnostic) | **oui** (R160) — **`SetFootprintFieldTextForSheet`**, **`global_update=false`**, **`instance_safe_write_used=true`** |

**Changement comportemental intentionnel (R146/R147)** : avant le garde-fou, **`scope=current_sheet`** sur écran hiérarchique partagé pouvait muter silencieusement toutes les instances via **`SetFootprintFieldText`** global ; désormais **bloqué par défaut** — opt-in explicite requis.

**Advanced Safe Footprint Assignment Workflow (R149 — workflow conservateur ; toujours valide post-R161)** :

Workflow **alternatif** lorsque l’agent préfère mutation **`current_sheet`** après navigation (voir **R156**) :

1. **`get_project_context_snapshot`** — **`include_footprints_summary=true`**, **`footprints_scope=hierarchy`**, **`footprints_validate=true`**
2. Cibler symbole incomplet — **`instance_path`**, **`sheet_path`**, **`symbol_id`**, **`sch_to_pcb_ready`**
3. **`navigate_schematic_sheet_session`** + **`get_schematic_active_sheet_context_session`**
4. **`schematic_assign_footprint_session`** — **`scope=current_sheet`**, **`dry_run=true`**
5. Mutation **`current_sheet`** seulement si **`global_update=false`**, **`affected_instances_count ≤ 1`**, **`exact_blocker_if_any=N/A`**

**Workflow direct hierarchy (R161 — recommandé si `instance_path` connu)** : readiness hierarchy → assign **`scope=hierarchy` dry_run** → assign **`scope=hierarchy` mutation** → relire readiness (A changed / B unchanged). Voir **`MCP_KICAD_USER_DEV_GUIDE.md`** R161.

**Règles sécurité R149** : **`symbol_not_found`** = heuristique négative ; **`allow_shared_symbol_global_update`** jamais automatique ; dry-run **`current_sheet`** obligatoire.

**Limites jusqu’à R150** : pas de navigation instance produit ; pas d’exposition **`CurrentSheet()`** ; workflow supervisé. Prochaine brique : **`get_schematic_active_sheet_context_session`** (contrat **`MCP_DECISIONS.md`** R149).

**Statut hierarchy instance-safe (R160 — remplace safe-limited mutation bloquée R158)** :

| `scope` | Résolution | Dry-run | Mutation |
|---------|------------|---------|----------|
| **`current_sheet`** (défaut) | écran courant + `symbol_id` | oui | **oui** (sauf écran partagé multi-instance sans opt-in — **R146**) |
| **`hierarchy`** | `instance_path` via **`GetSymbols`** | oui | **oui** — instance-safe (**R160**) ; **`global_update=false`** |

Raison (R157 spike, chemin **`current_sheet`** global) : **`SetFootprintFieldText`** reste **global** sur **`scope=current_sheet`** — opt-in **`allow_shared_symbol_global_update`** = mutation globale volontaire, **pas** instance-safe. **`scope=hierarchy`** (R160) utilise **`SetFootprintFieldTextForSheet`**. Note : **`MCP_UPSTREAM_FOOTPRINT_INSTANCE_SAFE_WRITE_DESIGN.md`**.

Input :

- `schematic_filename` (absolu, doit correspondre au schéma ouvert)
- `symbol_id` (UUID symbole)
- `footprint` (`LibNick:FootprintName`, max 1024 ; parse strict **`LIB_ID::Parse(..., false)`**)
- `dry_run` (défaut `false`) — si `true` : aucune mutation ; **`changed=true`** si le footprint changerait (contrairement aux autres sessions SCH où dry-run retourne souvent `changed=false`)
- `allow_overwrite` (défaut `false`) — si `false` et footprint déjà non vide : blocker **`footprint_already_assigned`**
- `expected_current_footprint` (optionnel) — garde-fou optimistic locking ; mismatch → **`expected_current_footprint_mismatch`**
- `validate_footprint` (défaut `true`) — si `true` : résolution via **`KIFACE_TEST_FOOTPRINT_LINK`** / **`FACE_PCB`** ; si `false` : assignation textuelle sans preuve library (**`footprint_validated=false`**, **`pcb_ready=false`**)
- `allow_shared_symbol_global_update` (**R146**, défaut **`false`**) — opt-in mutation globale **`scope=current_sheet`** uniquement ; **ignoré** pour **`scope=hierarchy`**
- `scope` (**R143**, défaut **`current_sheet`**) — **`hierarchy`** exige **`instance_path`** ; résolution via **`Hierarchy().GetSymbols`**
- `instance_path` (**R143**) — clé instance stable (alignée readiness **`PathAsString()+symbolUuid`**)
- `sheet_path` (**R143**, optionnel) — validation croisée **`PathHumanReadable`**

Output :

- `ok`
- `changed`
- `dry_run`
- `symbol_id`
- `symbol_ref`
- `old_footprint`
- `new_footprint`
- `footprint_validated` (`true` si validation library réussie ou footprint inchangé déjà validé)
- `pcb_ready` (`true` si footprint assigné/validé et prêt pour flux PCB — **pas** placement board)
- `exact_blocker_if_any`
- `diagnostics`
- `session_log`

Sorties additifs **`scope=current_sheet`** (**R146**) : `scope`, `affected_instances_count`, `shared_sheet_detected`, `global_update`.

Sorties additifs **`scope=hierarchy`** (**R143/R160/R161**) : `scope`, `instance_path`, `sheet_path`, `instance_targeted`, `affected_instances_count`, **`shared_sheet_detected`**, **`global_update`**, **`instance_safe_write_used`**.

Blockers : `invalid_symbol_id`, `schematic_filename_mismatch`, `sch_context_unavailable`, `symbol_not_found`, `invalid_footprint_identifier`, `library_not_loadable:*`, `footprint_not_found_in_library:*`, `footprint_already_assigned`, `expected_current_footprint_mismatch`, `mutation_failed`, **`post_mutation_instance_readback_mismatch`**, **`instance_safe_write_failed`**, **`shared_symbol_global_update_blocked`** (**R146**), **`invalid_scope`**, **`missing_instance_selector`**, **`instance_path_not_found`**, **`sheet_path_not_found`**, **`symbol_instance_not_found`**, **`symbol_instance_mismatch`**, **`ambiguous_symbol_id`**, **`instance_safe_write_unavailable`**, **`hierarchy_instance_write_unsupported`** *(fallback legacy / API indisponible)*.

**Sémantique blockers `current_sheet` shared (R146)** :

| Blocker | Cas |
|---------|-----|
| **`shared_symbol_global_update_blocked`** | `affected_instances_count > 1` et mutation sans **`allow_shared_symbol_global_update=true`** |

**Sémantique blockers hierarchy (R144/R160/R161)** :

| Blocker | Cas |
|---------|-----|
| **`invalid_scope`** | `scope` hors `current_sheet` \| `hierarchy` |
| **`missing_instance_selector`** | `scope=hierarchy` sans `instance_path` |
| **`instance_path_not_found`** | `instance_path` ne matche aucune instance |
| **`sheet_path_not_found`** | `sheet_path` fourni incohérent avec la cible |
| **`symbol_instance_not_found`** | instance résolue sans symbole |
| **`symbol_instance_mismatch`** | `symbol_id` ≠ symbole de l’`instance_path` |
| **`ambiguous_symbol_id`** | `symbol_id` ambigu sans désambiguïsation |
| **`instance_safe_write_unavailable`** | API upstream indisponible au runtime |
| **`instance_safe_write_failed`** | `SetFootprintFieldTextForSheet` a échoué |
| **`post_mutation_instance_readback_mismatch`** | readback instance ≠ footprint attendu |
| **`hierarchy_instance_write_unsupported`** | **Fallback legacy** — builds pré-R160 ou API désactivée |

Limites v1 (hors périmètre produit) :

1. **Pas de mutation PCB** — écrit uniquement le champ Footprint SCH
2. **Pas de placement footprint sur board**
3. **Pas d’annotation automatique** (références / netlist update globale)
4. **`validate_footprint=false`** : usage exceptionnel documenté (restore QA fixture)
5. **`scope=hierarchy`** (**R144**) : mutation **intentionnellement absente** — dry-run + diagnostic uniquement
6. **`scope=current_sheet`** sur écran partagé (**R146**) : mutation bloquée par défaut si **`affected_instances_count > 1`** — opt-in **`allow_shared_symbol_global_update`**

Différence vs **`schematic_set_symbol_field_session`** :

| Aspect | `set_symbol_field` | `assign_footprint` |
|--------|-------------------|-------------------|
| Cible | tout champ existant (sauf Reference/Value) | champ Footprint uniquement |
| Validation library | non | oui (défaut) |
| Sémantique IA | mutation texte générique | contrat produit + **`pcb_ready`** |
| Overwrite | via `expected_old_value` | via `allow_overwrite` + `expected_current_footprint` |

QA : probe **`targeted_schematic_assign_footprint_probe.ps1`** ; restore fixture **`issue19090`** avec **`validate_footprint=false`** car footprint initial **`Resistor_SMD:R_0805_2012Metric`** absent de la lib QA **`qa_resistors`**.

`get_schematic_active_sheet_context_session` (**R150 — read-only active sheet context**)  

Objectif : exposer le **contexte feuille SCH actif** de l’éditeur ouvert — preuve positive avant mutation **`schematic_assign_footprint_session scope=current_sheet`** (workflow Advanced Safe Footprint Assignment **R149**).

Input :

- `schematic_filename` (optionnel, absolu) — si fourni : doit correspondre au fichier éditeur ouvert
- `include_symbols` (défaut **`true`**) — liste bornée de symboles visibles sur l’écran actif
- `max_symbols` (défaut **`50`**, **1..500**)

Output :

- `ok`, `schematic_filename`, `schematic_absolute_path`
- `active_sheet_available`, `active_sheet_path`, `active_sheet_name`, `active_sheet_file`
- `active_sheet_is_root`, `active_sheet_is_shared`
- `symbols_count`, `symbols[]` (`symbol_id`, `reference`, `value`, `footprint`, `has_footprint`, `sheet_path`, `instance_path`)
- `truncated`, `exact_blocker_if_any`, `diagnostics`

Blockers : **`sch_context_unavailable`**, **`schematic_not_loaded`**, **`schematic_filename_mismatch`**, **`active_sheet_unavailable`**, **`invalid_max_symbols`**.

Garde-fous : **read-only** — aucun **`SCH_COMMIT`**, aucune navigation, aucune mutation.

**Workflow consolidé R151 (post-R150)** :

```text
1. get_project_context_snapshot(footprints_scope=hierarchy)
2. schematic_footprint_readiness_session(scope=hierarchy, include_sheet_paths=true)
3. get_schematic_active_sheet_context_session — instance_path cible ∈ symbols[] ?
4. Si non → ne pas muter (navigation requise)
5. schematic_assign_footprint_session(scope=current_sheet, dry_run=true) — garde-fou R146
6. Mutation seulement si dry-run sûr
```

QA workflow : **`targeted_schematic_active_sheet_context_probe.ps1`** — **`workflow_instance_path_match_ok`**.

**Workflow hiérarchique R152** (fixture **`ipc_footprint_readiness_v1`**) :

- Cas root : **`instance_path`** sous-feuille absent → assign bloqué (**`symbol_not_found`**).
- Cas sous-feuille : match positif + dry-run **`current_sheet`** sûr (**`qa_open_editor_frame`** QA).
- Critères : **`hierarchy_wrong_context_*`**, **`hierarchy_active_context_*`**, **`hierarchy_current_sheet_dry_run_safe_ok`**.
- Skip navigation : **`skipped_navigation_tool_missing`** → **R153**.

**Navigation produit R154** — **`navigate_schematic_sheet_session`** :

- Entrée : `sheet_path` \| `instance_path`, `dry_run`, `verify_instance_path`
- Navigation : `SCH_ACTIONS::changeSheet` — aucune mutation SCH
- Probe : **`targeted_schematic_navigate_sheet_probe.ps1`**

**Workflow produit R155** — sans **`qa_open_editor_frame`** :

```text
readiness hierarchy → navigate_schematic_sheet_session → active sheet context → assign dry-run
```

**Exemple end-to-end R156** — script **`r156_example_navigable_sch_footprint_workflow.ps1`** :

```text
readiness hierarchy → navigate → active sheet context → assign dry-run current_sheet → agent_decision (no real mutation)
```

Summary : **`qa_r156_navigable_sch_footprint_workflow_summary.txt`**. Détail : **`MCP_DECISIONS.md`** R156.

**Exemple runtime-driven R163** — script **`r163_example_runtime_selected_footprint_workflow.ps1`** :

```text
get_project_context_snapshot (R162 fields) → auto-select Workflow A hierarchy dry-run OR fallback R156
```

Summary : **`qa_r163_runtime_selected_footprint_workflow_summary.txt`**. Détail : **`MCP_DECISIONS.md`** R163.

**CI observability R164** — step **G4** dans **`run_mcp_v2_ci_baseline.ps1`** : exécute R163 post-checkpoint ; champs **`runtime_footprint_workflow_*`** ; **ne modifie pas** **`ci_baseline_green`**. Détail : **`MCP_DECISIONS.md`** R164.

**Echo readiness R165** — **`schematic_footprint_readiness_session`** expose les hints workflow R162 (helper **`mcp_footprint_assignment_workflow.h`**) pour agents direct-readiness. Détail : **`MCP_DECISIONS.md`** R165.

**Dual-source R166** — priorité agent : **`project_readiness_summary`** (principal) > echo readiness (local) ; divergence → ne pas muter, relire snapshot. Script **`report_footprint_workflow_discovery_consistency.ps1`**. Détail : **`MCP_DECISIONS.md`** R166.

**CI advisory R167** — step **G5** dans **`run_mcp_v2_ci_baseline.ps1`** : surveille cohérence dual-source ; champs **`footprint_workflow_discovery_consistency_*`** ; **ne modifie pas** **`ci_baseline_green`**. Détail : **`MCP_DECISIONS.md`** R167.

**Politique alerte R168** — clôture série R162–R167 : G4/G5 observability-only ; **`ci_baseline_green=true` ne suffit pas** pour ignorer advisory rouge ; niveaux Info / Warning / Release hold / Incident QA / Divergence agent. Détail : **`MCP_DECISIONS.md`** R168.

**Audit R169 — SCH→PCB post-footprint** : capacité sync PCB auditée ; dry-run KiCad natif ; tools futurs **`get_pcb_sync_readiness_session`** + **`pcb_update_from_schematic_preview_session`** recommandés en R170 ; détail : **`MCP_DECISIONS.md`** R169.

**R170 — `get_pcb_sync_readiness_session` V1** : read-only SCH↔PCB footprint sync readiness ; matching reference/path ; probe **`targeted_pcb_sync_readiness_probe.ps1`** ; **hors checkpoint MCP v2** ; détail : **`MCP_DECISIONS.md`** R170.

**R171 — `pcb_update_from_schematic_preview_session` V1** : dry-run **`BOARD_NETLIST_UPDATER`** ; reporter structuré ; **sans mutation PCB** ; probe **`targeted_pcb_update_preview_probe.ps1`** ; **hors checkpoint MCP v2** ; détail : **`MCP_DECISIONS.md`** R171.

**R172 — `pcb_sync_summary` snapshot opt-in** : bloc **`pcb_sync_summary`** dans **`get_project_context_snapshot`** ; délégation readiness R170 + preview R171 opt-in ; probe **`targeted_project_context_snapshot_pcb_sync_probe.ps1`** ; **hors checkpoint MCP v2** ; détail : **`MCP_DECISIONS.md`** R172.

**R173 — `pcb_update_from_schematic_session` prototype** : apply contrôlé **`BOARD_NETLIST_UPDATER`** ; gate **`confirm_apply`** ; restore QA ; probe **`targeted_pcb_update_from_schematic_probe.ps1`** ; **hors checkpoint MCP v2** ; détail : **`MCP_DECISIONS.md`** R173.

**R174 — mini-release doc `pcb_update_apply_controlled_v1`** : officialisation capacité mutation PCB contrôlée (gate, preview, limites save/undo, workflow IA) ; **aucun code** ; QA clôture green ; **hors checkpoint MCP v2** ; détail : **`MCP_DECISIONS.md`** R174.

**R177 — mini-release doc `pcb_apply_drc_save_workflow_v1`** : workflow IA snapshot → preview → apply (DRC advisory) → sync → save → verify ; probe **`r177_example_pcb_apply_drc_save_workflow.ps1`** ; **aucun handler** ; **hors checkpoint MCP v2** ; détail : **`MCP_DECISIONS.md`** R177.

**R178 — CI observability G6** : **`r177_example_pcb_apply_drc_save_workflow.ps1`** intégré en step G6 advisory dans **`run_mcp_v2_ci_baseline.ps1`** ; champs **`pcb_apply_drc_save_workflow_*`** ; **hors** **`ci_baseline_green`** ; détail : **`MCP_DECISIONS.md`** R178.

**R179 — Politique alerte G6 + clôture série R170–R179** : doc only ; advisory critique G6 ; série PCB apply/save clôturée ; détail : **`MCP_DECISIONS.md`** R179.

**R180–R183 — Observabilité advisory + release pack** : **`report_mcp_v2_ci_advisories.ps1`**, **`check_mcp_v2_release_advisories.ps1`**, **`export_mcp_v2_release_pack.ps1`** (`-IncludeCheckpointSummary`) ; détail **`MCP_DECISIONS.md`** R180–R183.

**R184–R185 — Pipeline / commande officielle release MCP v2** : **`run_mcp_v2_release_pack.ps1`** (commande standard **R185**) ; exit **0** = **`release_decision=go`** ; artefacts pipeline + pack MD/JSON ; détail **`MCP_DECISIONS.md`** R184–R185.

### 12.3 Garde-fous v1

1. `dry_run=true` ne modifie rien
2. `expected_old_value` évite les modifications accidentelles
3. mutation unique et bornée
4. pas de sauvegarde automatique
5. restauration vérifiée en QA
6. `schematic_set_symbol_field_session` :
   - champs existants uniquement (pas de création de champ)
   - `Reference` interdit
   - `Value` redirigé vers `schematic_set_symbol_value_session`
   - `new_value=""` autorisé
7. `schematic_assign_footprint_session` :
   - validation footprint library par défaut (`validate_footprint=true`)
   - overwrite explicite (`allow_overwrite=true`) ou footprint vide requis
   - **`pcb_ready`** distinct de la simple écriture texte FOOTPRINT via `set_symbol_field`
   - pas de mutation PCB ni placement board

### 12.4 Artefacts QA canoniques

- `qa_schematic_set_symbol_value_session_contract_summary.txt`
- `qa_schematic_set_symbol_field_session_contract_summary.txt`
- `qa_schematic_set_label_text_session_contract_summary.txt`
- `qa_schematic_assign_footprint_session_contract_summary.txt`

### 12.5 Runner QA léger officiel

- Runner officiel : `qa/tools/ipc_tools/run_sch_actions_v1_targeted_probes.ps1`
- Summary agrégé : `qa_sch_actions_v1_family_summary.txt`

Critères de succès (summary agrégé) :

1. `sch_actions_v1_family_green=true`
2. `schematic_set_symbol_value_ok=true`
3. `schematic_set_symbol_field_ok=true`
4. `schematic_set_label_text_ok=true`
5. `schematic_assign_footprint_ok=true`
6. `exact_blocker_if_any=N/A`

---

## 13. Runner global MCP v1 checkpoint

Porte qualité globale MCP v1 couvrant les familles stables, avec profils explicites.

- Runner officiel : `qa/tools/ipc_tools/run_mcp_v1_checkpoint.ps1`
- Summary global : `qa_mcp_v1_checkpoint_summary.txt`
- Paramètre : `-Profile smoke|core|full` (défaut `full`)
- Compatibilité : `-IncludeLibrary` force `Profile=full`

### 13.1 Profils checkpoint

1. `smoke`
   - Inclus : Simulation v1.1, SCH Actions v1
   - Skipped : SCH Connectivity v1, Library v1
   - Usage : gate rapide / PR / validation de base
2. `core`
   - Inclus : Simulation v1.1, SCH Connectivity v1, SCH Actions v1
   - Skipped : Library v1
   - Usage : validation schématique plus complète
3. `full`
   - Inclus : Simulation v1.1, Library v1, SCH Connectivity v1, SCH Actions v1
   - Usage : validation complète / diagnostic lourd / release

Etat terrain actuel :

- `smoke` : vert
- `core` : vert
- `full` : vert (`mcp_v1_full_checkpoint_green=true`, `mcp_v1_checkpoint_green=true`)

Précision `core` :

- Couverture : Simulation v1.1 + SCH Connectivity v1 + SCH Actions v1
- Library v1 explicitement exclue :
  - `library_v1_included=false`
  - `library_v1_ok=skipped`

Note stabilité SCH Connectivity v1 :

- Les probes ciblés SCH Connectivity utilisent une fixture stable `issue19090` pour réduire les `open_editor_timeout`.

Note Library v1 (gating actuel) :

- Le gating Library v1 valide :
  - `symbol_search_ok=true`
  - `symbol_inspect_ok=true`
  - `footprint_inspect_ok=true`
- `library_footprint_search_session` reste disponible et exécuté en diagnostic-only (non-gating).
- Traces diagnostic footprint search dans `qa_library_v1_family_summary.txt` :
  - `footprint_search_ok`
  - `footprint_search_diagnostic_ok`
  - `footprint_search_gating=false`
  - `exact_blocker_footprint_search_if_any`

### 13.2 Champs summary clés

- Profil actif : `mcp_v1_checkpoint_profile=smoke|core|full`
- Etats de gate :
  - `mcp_v1_smoke_checkpoint_green=true|false|skipped`
  - `mcp_v1_core_checkpoint_green=true|false|skipped`
  - `mcp_v1_full_checkpoint_green=true|false|skipped`
- Inclusions de familles :
  - `library_v1_included=true|false`
  - `sch_connectivity_v1_included=true|false`
- Statuts familles :
  - `simulation_v1_1_ok=true|false`
  - `library_v1_ok=true|false|skipped`
  - `sch_connectivity_v1_ok=true|false|skipped`
  - `sch_actions_v1_ok=true|false`
- Blockers :
  - `exact_blocker_smoke_if_any=N/A|...`
  - `exact_blocker_core_if_any=N/A|skipped|...`
  - `exact_blocker_library_if_any=N/A|skipped|...`
  - `exact_blocker_if_any=N/A|...`

### 13.3 Règle de compatibilité

`mcp_v1_checkpoint_green` dépend du profil actif :

1. `smoke` : `mcp_v1_checkpoint_green = mcp_v1_smoke_checkpoint_green`
2. `core` : `mcp_v1_checkpoint_green = mcp_v1_core_checkpoint_green`
3. `full` : `mcp_v1_checkpoint_green = mcp_v1_full_checkpoint_green`

Important :

- Toujours lire `mcp_v1_checkpoint_profile` en premier.
- Un `smoke` vert ne valide ni Library v1 ni SCH Connectivity v1 (elles sont explicitement `skipped`).
- `core` et `full` restent nécessaires pour le diagnostic complet.

### 13.4 Frontière v1/v2 (important)

- `SCH Geometry v2` est une famille MCP v2 indépendante.
- Elle n'est pas encore intégrée dans `run_mcp_v1_checkpoint.ps1`.
- Le checkpoint global MCP v1 reste séparé et inchangé.
- `SCH Authoring v2` est aussi une famille MCP v2 indépendante.

### 13.5 Checkpoint MCP v2 (séparé)

Porte qualité globale MCP v2 (agrégation des familles v2), distincte du checkpoint MCP v1.

- Runner officiel : `qa/tools/ipc_tools/run_mcp_v2_checkpoint.ps1`
- Summary global : `qa_mcp_v2_checkpoint_summary.txt`
- Familles incluses actuellement :
  - `SCH Geometry v2`
  - `SCH Authoring v2`
  - `SCH Wiring v2`
  - `SCH Symbol Editing v2` (3 tools stables : `schematic_set_symbol_field_session`, `schematic_set_symbol_value_session`, `schematic_set_symbol_reference_session`)
  - `PCB Inspection v2` (3 tools stables : `pcb_get_board_items_session`, `pcb_get_board_item_by_id_session`, `pcb_get_net_items_session`)
  - `pcb_geometry v1` (2 tools stables : `pcb_measure_distance_session`, `get_pcb_item_geometry_session`)

Champs summary clés :

- `mcp_v2_checkpoint_green=true|false`
- `sch_geometry_v2_ok=true|false`
- `sch_authoring_v2_ok=true|false`
- `sch_wiring_v2_ok=true|false`
- `sch_symbol_editing_v2_ok=true|false`
- `pcb_inspection_v2_ok=true|false`
- `pcb_geometry_v1_ok=true|false`
- `exact_blocker_if_any=N/A|...`
- `sch_geometry_v2_target_run_dir=...`
- `sch_authoring_v2_target_run_dir=...`
- `sch_wiring_v2_target_run_dir=...`
- `sch_symbol_editing_v2_target_run_dir=...`
- `pcb_inspection_v2_target_run_dir=...`
- `pcb_geometry_v1_target_run_dir=...`
- `sch_geometry_v2_exit_code=...`
- `sch_authoring_v2_exit_code=...`
- `sch_wiring_v2_exit_code=...`
- `sch_symbol_editing_v2_exit_code=...`
- `pcb_inspection_v2_exit_code=...`
- `pcb_geometry_v1_exit_code=...`

Critères green :

1. `mcp_v2_checkpoint_green=true`
2. `sch_geometry_v2_ok=true`
3. `sch_authoring_v2_ok=true`
4. `sch_wiring_v2_ok=true`
5. `sch_symbol_editing_v2_ok=true`
6. `pcb_inspection_v2_ok=true`
7. `pcb_geometry_v1_ok=true`
8. `exact_blocker_if_any=N/A`

Format blocker :

- priorité 1 (Geometry) : `exact_blocker_if_any=sch_geometry_v2:<blocker>`
- priorité 2 (Authoring) : `exact_blocker_if_any=sch_authoring_v2:<blocker>`
- priorité 3 (Wiring) : `exact_blocker_if_any=sch_wiring_v2:<blocker>`
- priorité 4 (Symbol Editing) : `exact_blocker_if_any=sch_symbol_editing_v2:<blocker>`
- priorité 5 (PCB Inspection) : `exact_blocker_if_any=pcb_inspection_v2:<blocker>`
- priorité 6 (pcb_geometry v1) : `exact_blocker_if_any=pcb_geometry_v1:<blocker>`

---

## 14. SCH Geometry v2

Première famille MCP v2 stabilisée pour action géométrique SCH contrôlée.

### 14.1 Tool stable

1. `schematic_move_symbol_session`
2. `schematic_move_label_session`
3. `schematic_create_wire_segment_session`

Type :

- action géométrique contrôlée
- undoable
- `dry_run` supporté
- readback explicite
- restauration QA vérifiée (probe ciblé)
- sans sauvegarde automatique

### 14.2 Contrat opérationnel v2 (résumé)

Entrées clés :

- `schematic_filename` (requis)
- `symbol_id` (requis)
- `new_position` absolue `{x_nm, y_nm}` (requis)
- `expected_old_position` (optionnel)
- `dry_run` (optionnel)
- `ensure_fresh_connectivity` (optionnel)
- `position_tolerance_nm` (optionnel)

Sorties clés :

- `ok`
- `changed`
- `exact_blocker_if_any`
- `symbol_id`
- `symbol_ref`
- `old_position_x_nm`
- `old_position_y_nm`
- `new_position_x_nm`
- `new_position_y_nm`
- `readback_position_x_nm`
- `readback_position_y_nm`
- `dry_run`
- `connectivity_refresh_performed`
- `diagnostics`
- `session_log`

### 14.3 Précisions importantes

- `schematic_move_symbol_session` : déplacement absolu de symbole
- déplacement absolu uniquement
- `schematic_move_label_session` : local label uniquement
- `schematic_create_wire_segment_session` : wiring SCH contrôlé (création d'un segment unique)
- `symbol_id` obligatoire
- `label_id` obligatoire
- `schematic_create_wire_segment_session` : `start`/`end` absolus en `*_nm`
- `schematic_create_wire_segment_session` : `require_orthogonal=true` par défaut
- `schematic_create_wire_segment_session` : wire uniquement (`LAYER_WIRE`), pas bus
- `schematic_create_wire_segment_session` : undoable, `dry_run`, readback, restore QA par suppression du wire créé
- `schematic_create_wire_segment_session` : output plat uniquement
- `schematic_create_wire_segment_session` : pas d'autorouting, pas de multi-segment
- pas de déplacement automatique des fils/labels
- output position plat en `*_x_nm` / `*_y_nm`
- pas de sauvegarde automatique
- `schematic_move_label_session` : pas de déplacement automatique des fils
- `schematic_move_label_session` : pas de déplacement automatique d'autres labels

### 14.4 Runner QA officiel (famille v2)

- Runner officiel : `qa/tools/ipc_tools/run_sch_geometry_v2_targeted_probes.ps1`
- Summary agrégé : `qa_sch_geometry_v2_family_summary.txt`

Critères green :

1. `sch_geometry_v2_family_green=true`
2. `schematic_move_symbol_ok=true`
3. `schematic_move_label_ok=true`
4. `schematic_create_wire_segment_ok=true`
5. `exact_blocker_if_any=N/A`

## 15. SCH Authoring v2

Première famille MCP v2 stabilisée pour action d'authoring SCH contrôlée.

### 15.1 Tools stables

1. `schematic_create_label_session`
2. `schematic_place_symbol_session`

Type :

- action d'authoring SCH contrôlée
- undoable
- `dry_run` supporté
- readback explicite
- restauration QA vérifiée (suppression du label créé)
- sans sauvegarde automatique

### 15.2 Précisions importantes

- label local uniquement (`SCH_LABEL_T`)
- texte requis, non vide, borné
- position absolue en `*_nm`
- output plat uniquement
- pas de global label
- pas de hierarchical label
- pas de sauvegarde automatique
- `schematic_place_symbol_session` : placement d'un symbole SCH depuis `lib_id`
- `schematic_place_symbol_session` : entrées principales `lib_id`, `position.x_nm/y_nm`, `reference`, `value`
- `schematic_place_symbol_session` : pas d'auto-annotation globale
- `schematic_place_symbol_session` : pas d'assignation footprint automatique
- `schematic_place_symbol_session` : pas de sauvegarde automatique
- validation QA officielle en deux phases :
  - phase A : dry-run dans process KiCad A
  - phase B : mutation/readback/restore dans process KiCad B

### 15.3 Runner QA officiel (famille v2)

- Runner officiel : `qa/tools/ipc_tools/run_sch_authoring_v2_targeted_probes.ps1`
- Summary agrégé : `qa_sch_authoring_v2_family_summary.txt`

Critères green :

1. `sch_authoring_v2_family_green=true`
2. `schematic_create_label_ok=true`
3. `schematic_place_symbol_ok=true`
4. `exact_blocker_if_any=N/A`

## 16. SCH Wiring v2

Famille MCP v2 stabilisée pour le wiring SCH contrôlé pin-to-pin aligné et non aligné.

### 16.1 Tools stables

1. `schematic_connect_symbol_pin_session`
2. `schematic_connect_pins_l_shape_session`

Type :

- action wiring SCH contrôlée
- undoable
- `dry_run` supporté
- readback explicite
- restauration QA vérifiée (suppression du wire créé + restore move fixture)
- sans sauvegarde automatique

### 16.2 Contrats opérationnels v2 (résumé)

Entrées clés :

- `schematic_filename`
- `from.symbol_id`, `from.pin_number`
- `to.symbol_id`, `to.pin_number`
- `dry_run`
- `require_aligned` (v1 strict : alignement obligatoire)
- `reject_hidden_pins` (v1 strict : rejet par défaut)
- `reject_power_pins` (v1 strict : rejet par défaut)
- `ensure_fresh_connectivity`

Sorties clés (payload minimal stabilisé) :

- `ok`
- `changed`
- `exact_blocker_if_any`
- `wire_id`
- `dry_run`
- `diagnostics`
- `session_log`

Entrées clés (`schematic_connect_pins_l_shape_session`) :

- `schematic_filename`
- `from.symbol_id`, `from.pin_number`
- `to.symbol_id`, `to.pin_number`
- `elbow_mode` (`horizontal_then_vertical` ou `vertical_then_horizontal`)
- `dry_run`
- `reject_hidden_pins` (rejet par défaut)
- `reject_power_pins` (rejet par défaut)
- `ensure_fresh_connectivity`

Sorties clés (payload minimal stabilisé) :

- `ok`
- `changed`
- `exact_blocker_if_any`
- `wire1_id`
- `wire2_id`
- `junction_id` (vide en v1)
- `dry_run`
- `diagnostics`
- `session_log`

### 16.3 Précisions importantes

- v1 strict : segment unique uniquement
- v1 strict : pins alignées uniquement (pas de L-shape)
- v1 strict : pas d'autorouting
- pins hidden/power rejetées par défaut
- pins ambiguës refusées
- readback avec fallback endpoints si UUID post-commit différent
- restore QA validé
- `schematic_connect_pins_l_shape_session` : deux segments orthogonaux non alignés
- `schematic_connect_pins_l_shape_session` : refuse les pins déjà alignées (`pins_already_aligned`)
- `schematic_connect_pins_l_shape_session` : pas d'autorouting, pas d'obstacle avoidance
- `schematic_connect_pins_l_shape_session` : `junction_id` vide en v1 (junction non créée)
- `schematic_connect_pins_l_shape_session` : readback wire via UUID puis fallback endpoints
- `schematic_connect_pins_l_shape_session` : restore QA validé (delete wire1/wire2 + restore move symbole)
- pas de sauvegarde automatique

### 16.4 Runner QA officiel (famille v2)

- Runner officiel : `qa/tools/ipc_tools/run_sch_wiring_v2_targeted_probes.ps1`
- Summary agrégé : `qa_sch_wiring_v2_family_summary.txt`

Critères green :

1. `sch_wiring_v2_family_green=true`
2. `schematic_connect_symbol_pin_ok=true`
3. `schematic_connect_pins_l_shape_ok=true`
4. `exact_blocker_if_any=N/A`
5. `schematic_connect_symbol_pin_exit_code=0`
6. `schematic_connect_pins_l_shape_exit_code=0`

## 17. SCH Symbol Editing v2

Famille MCP v2 stabilisée pour l'édition contrôlée des champs `Field`, `Value` et de la référence `Reference` d'un symbole SCH existant.

### 17.1 Tools stables

1. `schematic_set_symbol_field_session`
2. `schematic_set_symbol_value_session`
3. `schematic_set_symbol_reference_session`

Type :

- action d'édition symbole SCH contrôlée
- undoable
- `dry_run` supporté
- readback explicite
- restore QA vérifié
- `expected_old_value` / `expected_old_reference` supportés selon le tool
- sans sauvegarde automatique

### 17.2 Profil fonctionnel v1

`schematic_set_symbol_field_session` :

- modifie un champ existant d'un symbole existant
- cible le symbole via `symbol_id`
- sélection du champ via `field_id` ou `field_name`
- champs autorisés v1 :
  - champs existants non `REFERENCE`
  - champs existants non `VALUE`
  - ex: `DATASHEET`, `FOOTPRINT`, `DESCRIPTION`, custom field existant
- `REFERENCE` refusé : `reference_field_forbidden_in_v1`
- `VALUE` redirigé : `use_schematic_set_symbol_value_session`
- création de champ non supportée : `field_creation_not_supported`
- champ absent : `field_not_found`

`schematic_set_symbol_value_session` :

- modifie le champ `Value` d'un symbole existant
- cible le symbole via `symbol_id`
- input clé : `schematic_filename`, `symbol_id`, `new_value`, `expected_old_value`, `dry_run`
- supporte `dry_run`, mutation, readback, restore QA et `expected_old_value`
- blockers importants : `invalid_symbol_id`, `symbol_not_found`, `value_field_not_found`, `expected_old_value_mismatch`, `mutation_failed`
- `new_value` limité à 1024 caractères
- sans sauvegarde automatique

`schematic_set_symbol_reference_session` :

- modifie la référence `Reference` d'un symbole existant (outil dédié ; `REFERENCE` reste refusé dans `schematic_set_symbol_field_session`)
- cible le symbole via `symbol_id`
- périmètre v1 : édition limitée à la feuille courante (`CurrentSheet()`), sans détection globale de conflit de référence, sans annotation globale
- input clé : `schematic_filename`, `symbol_id`, `new_reference`, `expected_old_reference`, `dry_run`
- mutation via sémantique KiCad : `SCH_SYMBOL::SetRef(...)`
- supporte `dry_run`, mutation, readback, restore QA et `expected_old_reference`
- validation officielle non interactive du designator (référence)
- blockers importants : `invalid_symbol_id`, `symbol_not_found`, `invalid_reference`, `expected_old_reference_mismatch`, `mutation_failed`, `readback_failed`
- pas de sauvegarde automatique

### 17.3 Runner QA officiel (famille v2)

- Runner officiel : `qa/tools/ipc_tools/run_sch_symbol_editing_v2_targeted_probes.ps1`
- Summary agrégé : `qa_sch_symbol_editing_v2_family_summary.txt`
- Probes contractuels :
  - `qa_schematic_set_symbol_field_session_contract_summary.txt`
  - `qa_schematic_set_symbol_value_session_contract_summary.txt`
  - `qa_schematic_set_symbol_reference_session_contract_summary.txt`

Critères green :

1. `sch_symbol_editing_v2_family_green=true`
2. `schematic_set_symbol_field_ok=true`
3. `schematic_set_symbol_value_ok=true`
4. `schematic_set_symbol_reference_ok=true`
5. `exact_blocker_if_any=N/A`
6. `schematic_set_symbol_field_exit_code=0`
7. `schematic_set_symbol_value_exit_code=0`
8. `schematic_set_symbol_reference_exit_code=0`

## 18. PCB Inspection v2

Famille MCP v2 stabilisée pour une **lecture** du PCB ouvert dans l’éditeur : inventaire compact et zoom par item, sans mutation ni sauvegarde automatique.

### 18.1 Tools stables

1. `pcb_get_board_items_session`
2. `pcb_get_board_item_by_id_session`
3. `pcb_get_net_items_session`

### 18.2 Profil `pcb_get_board_items_session`

- lecture du PCB **courant** dans le cadre PCB (GUI-only) ; corrélation via `board_filename` (chemin absolu, doit correspondre au document ouvert)
- **read-only** : pas de mutation de design
- pas de sauvegarde automatique
- sort des **compteurs** exacts : `footprint_count`, `pad_count`, `track_count`, `via_count`, `zone_count`, `drawing_count`
- sort un `items_summary` **borné** par `max_items` (défaut 500, plage 1–5000)
- filtres optionnels : `item_types` (`footprints`, `pads`, `tracks`, `vias`, `zones`, `drawings`)
- troncature signalée par `items_summary_truncated=true` lorsque la synthèse est coupée par `max_items`
- cas négatifs validés : `invalid_item_type` (bad request), `board_filename_mismatch`

### 18.3 Profil `pcb_get_board_item_by_id_session`

- lecture détaillée ciblée d’un **item PCB existant** (pas le payload brut `get_pcb_items_by_id`)
- cible via `board_filename` + `item_id` (KIID)
- **read-only** ; pas de mutation ; pas de sauvegarde automatique
- résolution : `BOARD::ResolveItem(KIID, true)`
- distingue : `invalid_item_id` (validation `KIID::SniffTest`), `item_not_found`, `unsupported_item_type`
- types v1 supportés : `footprint`, `pad`, `track`, `arc`, `via`, `zone`, `drawing` (graphiques au niveau board uniquement pour shape/text/textbox/barcode)
- sortie plate : entre autres `item_type`, `layer`, `x_nm`, `y_nm`, `net_name`, `parent_footprint_id`, `reference`, `value`, `details` (léger, JSON compact possible)
- cas négatifs validés : `invalid_item_id`, `item_not_found`, `board_filename_mismatch`

### 18.4 Profil `pcb_get_net_items_session`

- lecture des items PCB **associés à un net donné** dans le cadre ouvert ; corrélation via `board_filename` (chemin absolu)
- sélection du net par **`net_name`** et/ou **`net_code`** (au moins l’un des deux) ; si les deux sont fournis, **cohérence obligatoire** (sinon `net_selector_mismatch`)
- **`net_name`** : correspondance **exacte** via `BOARD::FindNet(wxString)`
- résolution par code : `BOARD::FindNet(int)` ; **`net_code=0` refusé** en v1 (`invalid_net_selector`)
- **read-only** ; pas de mutation ; pas de sauvegarde automatique
- périmètre items : **pads**, **tracks**, **arcs**, **vias**, **zones cuivre** connectées au net (rule areas exclues lorsque détectables proprement)
- `track_count` agrège segments droits et arcs ; entrées `items_summary` distinguent `item_type` `track` vs `arc`
- sort des **compteurs** : `pad_count`, `track_count`, `via_count`, `zone_count`
- sort un `items_summary` **borné** par `max_items` (défaut 500, plage 1–5000) ; troncature signalée par `items_summary_truncated=true`
- blockers importants : `invalid_net_selector`, `net_not_found`, `net_selector_mismatch`, `board_filename_mismatch`, `invalid_max_items`, `read_failed` (et erreurs de contexte PCB héritées des autres tools session : `pcb_context_unavailable`, `board_not_loaded`, etc.)
- cas négatifs validés : absence de sélecteur net, `net_code=0`, nom de net inexistant, mismatch `board_filename`
- le cas `net_selector_mismatch` peut rester **skipped** sur la fixture IPC v1 actuelle faute de **deux nets nommés** exploitables côte à côte

### 18.5 Runner QA officiel (famille v2)

- Runner officiel : `qa/tools/ipc_tools/run_pcb_inspection_v2_targeted_probes.ps1`
- Summary agrégé : `qa_pcb_inspection_v2_family_summary.txt`
- Probes contractuels enfants :
  - `qa_pcb_get_board_items_session_contract_summary.txt`
  - `qa_pcb_get_board_item_by_id_session_contract_summary.txt`
  - `qa_pcb_get_net_items_session_contract_summary.txt`

Critères green famille :

1. `pcb_inspection_v2_family_green=true`
2. `pcb_get_board_items_ok=true`
3. `pcb_get_board_item_by_id_ok=true`
4. `pcb_get_net_items_ok=true`
5. `exact_blocker_if_any=N/A`
6. `pcb_get_board_items_exit_code=0`
7. `pcb_get_board_item_by_id_exit_code=0`
8. `pcb_get_net_items_exit_code=0`

## 19. PCB DRC Violations Context v1 (**release R125**, impl. **R124**)

Famille **read-only** pour lister des **violations DRC PCB bornées** à destination de l’IA. **Complète** **`get_pcb_clearance_context_session`** et **`get_pcb_nearby_items_context_session`** (context-only, **`is_true_drc=false`**) **sans** les remplacer. **Intégrée** au gate **`run_mcp_v2_checkpoint.ps1`** comme **9e** famille (**R126**).

### 19.1 Tool stable

1. `get_pcb_drc_violations_context_session`

### 19.2 Objectif

Exposer une **liste structurée** de violations DRC (type, sévérité, message, couche, position, items, nets, règle) avec plafond **`max_violations`**, sans modifier le board ni persister de marqueurs DRC dans le fichier `.kicad_pcb`.

### 19.3 Contrat entrée V1

| Champ | Obligatoire | Défaut | Notes |
|-------|-------------|--------|-------|
| `board_filename` | oui | — | chemin **absolu** ; doit correspondre au PCB ouvert |
| `max_violations` | non | **50** | plage **1–500** |
| `run_drc` | non | **`false`** | voir §19.4 |
| `include_items` | non | **`true`** | inclut `item_ids` / `net_names` par violation |
| `include_excluded` | non | **`false`** | marqueurs exclus (mode `run_drc=false` uniquement) |
| `test_footprints` | non | **`false`** | passé à **`DRC_ENGINE::RunTests`** ; **`true`** risqué en CI |
| `report_all_track_errors` | non | **`false`** | passé à **`RunTests`** |
| `severity` | non | all | `error` / `warning` / `exclusion` / `all` |
| `violation_types[]` | non | — | filtre par settings key (ex. `clearance`) |
| `layer` | non | — | **`BL_*`** ou alias **`F_Cu`** |

### 19.4 Modes `run_drc`

| | **`run_drc=false`** | **`run_drc=true`** |
|--|---------------------|-------------------|
| Source | **`BOARD::Markers()`** / **`MARKER_DRC`** | **`DRC_ENGINE::InitEngine`** + **`RunTests`** (collecteur mémoire) |
| `drc_source` | `existing_markers` | `engine_run` |
| `is_official_drc` | **`false`** | **`true`** |
| `is_true_drc` | **`false`** (= `is_official_drc`) | **`true`** |
| `drc_was_run` | **`false`** | **`true`** |
| Violations typiques | souvent **0** (marqueurs non persistés) | liste fraîche engine |
| Mutation board | **non** | **non** |
| Persistance marqueurs | **non** | **non** |

**Ne pas** utiliser **`DRC_TOOL::RunTests`** (dialogue GUI — mutation marqueurs).

### 19.5 Contrat sortie V1

- `ok`, `exact_blocker_if_any`, `diagnostics`, `session_log`
- `is_official_drc`, `is_true_drc`, `drc_was_run`, `drc_source`
- `violations_count`, `returned_count`, `truncated`
- `violations[]` : `type`, `severity`, `message`, `layer`, `x_nm`, `y_nm`, `item_ids`, `net_names`, `rule_name` (si dispo)

### 19.6 Blockers

`missing_or_invalid_board_filename`, `board_filename_mismatch`, `pcb_context_unavailable`, `board_not_loaded`, `drc_engine_unavailable`, `drc_engine_not_initialized`, `drc_run_failed`, `invalid_max_violations`, `invalid_layer`, `N/A`.

### 19.7 Limites v1 (documentées R125)

1. **Pas de parité garantie** avec le dialogue DRC GUI KiCad.
2. **Pas de persistance** de marqueurs DRC dans `.kicad_pcb`.
3. **Pas de mutation** board (read-only).
4. **`run_drc=true`** peut être **lent** sur grandes cartes — privilégier **`max_violations`** et **`run_drc=false`** pour un snapshot rapide.
5. **`test_footprints=false`** par défaut (risque CI / dépendances schéma).
6. Fixture **`ipc_drc_v1`** peut produire **plusieurs types** (`clearance`, `invalid_outline`, `lib_footprint_issues`) — le probe QA valide surtout **≥1 clearance** et nets **`QA_DRC_NET_A`** / **`QA_DRC_NET_B`**.

### 19.8 Diff vs clearance / nearby

| Aspect | clearance context | nearby context | **DRC violations context** |
|--------|------------------|----------------|------------------------------|
| Rôle | contexte nominal / distance bbox | voisinage géométrique | **liste violations DRC** |
| `is_true_drc` | **`false`** | **`false`** | **`true`** si `run_drc=true` |
| `RunTests` | non | non | **oui** si `run_drc=true` |
| Checkpoint v2 | **oui** (R33) | **oui** (R39) | **oui** (R126) |

### 19.9 Runner QA (checkpoint v2, **9e** famille **R126**)

- Probe : **`targeted_pcb_drc_violations_context_probe.ps1`**
- Runner : **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`**
- Summaries : **`qa_pcb_drc_violations_context_summary.txt`**, **`qa_pcb_drc_violations_context_v1_family_summary.txt`**
- Fixture : **`qa/data/pcbnew/drc/ipc_drc_v1/`**

Critères green famille :

1. `pcb_drc_violations_context_v1_family_green=true`
2. `pcb_drc_violations_context_ok=true`
3. `exact_blocker_if_any=N/A`

---

## 20. Project Readiness Summary V1 complet (**R138**)

Clôture documentaire **R135a + R135b + R137**. Checkpoint MCP v2 reste à **9 familles** ; **`footprints_validate=false`** par défaut.

### 20.1 Composants

| Composant | Rôle |
|-----------|------|
| **`get_project_context_snapshot`** → **`project_readiness_summary`** | Hub IA : documents, PCB absolu, DRC opt-in, actions recommandées ; **R162** : **`hierarchy_assign_instance_safe_enabled`**, **`recommended_footprint_assignment_workflow`**, **`recommended_assign_scope`** |
| **`schematic_footprint_readiness_session`** | Agrégat footprint SCH read-only (source de vérité compteurs) |
| Opt-in snapshot **`include_footprints_summary`** | Relay vers tool footprint ; défaut **false** (R135a inchangé) |
| Opt-in snapshot **`footprints_validate`** | Relay **`validate_footprints`** ; défaut **false** |

### 20.2 Sémantique readiness footprint

| Champ | Sens V1 |
|-------|---------|
| **`sch_to_pcb_ready`** | Tous symboles **scannés** (non-power, **écran courant**) ont footprint **non vide** |
| **`sch_to_pcb_ready_validated`** | **`null`** si pas de validation ; sinon tous scannés ont footprint non vide **et** valide librairie |
| **`symbols_with_unvalidated_footprint_count`** | Si **`validate_footprints=false`** : symboles avec footprint présent mais non validés |

**Périmètre scan** : **`scope=current_sheet`** (défaut) = écran courant inchangé R135b–R139 ; **`scope=hierarchy`** (**R141**) = instances hiérarchiques via **`GetSymbols`** ; power symbols exclus.

### 20.3 Entrées / sorties tool `schematic_footprint_readiness_session`

Entrées clés : `schematic_filename`, `scope` (**`current_sheet`|`hierarchy`**, défaut **`current_sheet`**), `include_sheet_paths`, `validate_footprints` (défaut **false**), `max_symbols`.

Sorties **`scope=hierarchy`** : `sheets_count`, `symbols_instances_count`, `symbols[].sheet_path`, `sheet_name`, `instance_path` (si **`include_sheet_paths`**).

Sorties validation (si **`validate_footprints=true`**) : `footprint_validation_available`, `footprint_validation_performed`, `symbols_with_invalid_footprint_count`, `invalid_footprints[]`, champs symbole `footprint_validated` / `footprint_valid` / `footprint_validation_status`.

Cache : map footprint unique → résultat link test ; **limité à l'appel** ; diagnostics `validation_cache_entries` / `validation_cache_hits`.

### 20.4 Recommended actions snapshot

| Action | Condition |
|--------|-----------|
| **`assign_missing_footprints`** | `symbols_without_footprint_count > 0` |
| **`fix_invalid_footprints`** | validation effectuée et `symbols_with_invalid_footprint_count > 0` → **`schematic_assign_footprint_session`** |
| **`run_drc_detail`** | DRC engine run avec violations |

### 20.5 Non-régression SCH actions (**R137/R138**)

**`schematic_set_symbol_field_session`** : après mutation réussie, **`changed=true`** (aligné **`schematic_set_symbol_value_session`**). Régression corrigée : `makeResult(true,false)` → **`makeResult(true,true)`** dans **`SchematicSetSymbolFieldSessionToolInvoke`**.

### 20.6 QA & isolation fixture **`issue19090`**

Probes : **`targeted_schematic_footprint_readiness_probe.ps1`**, **`targeted_project_context_snapshot_readiness_probe.ps1`**.

Fixtures : **`issue19090`** (non-régression **`current_sheet`**) ; **`qa/data/eeschema/hierarchy/ipc_footprint_readiness_v1/`** (**`scope=hierarchy`**, R141).

**Audit R138** : plusieurs probes mutent **`issue19090`** en mémoire (footprint, DATASHEET, wires, labels). Recommandation : **`Stop-KiCadProcesses`** avant checkpoint v2 et entre suites lourdes. Détail **`MCP_DECISIONS.md`** R138.

*Fin du manifeste MCP v1 officiel.*
