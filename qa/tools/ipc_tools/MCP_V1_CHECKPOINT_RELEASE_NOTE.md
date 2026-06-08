# MCP v1 Checkpoint Release Note

## Etat global

Le checkpoint MCP v1 dispose d'une porte qualité globale profilée :

- runner : `qa/tools/ipc_tools/run_mcp_v1_checkpoint.ps1`
- summary : `qa_mcp_v1_checkpoint_summary.txt`
- profils : `smoke|core|full`
- signal de compatibilité : `mcp_v1_checkpoint_green` (dépend du profil actif)

**R111 — audit spawn** : **`Invoke-FamilyRunner`** migré vers **`Invoke-IpcQaChildScript`** (**R112**, spawn-only) — critères / summary **`qa_mcp_v1_checkpoint_summary.txt`** inchangés.

**R113 — runners v1** : **`run_sch_actions_v1_targeted_probes.ps1`** et **`run_sch_connectivity_v1_targeted_probes.ps1`** — **`Invoke-TargetedProbe`** migré spawn-only vers **`Invoke-IpcQaChildScript`** ; contrats **`qa_sch_*_v1_family_summary.txt`** inchangés.

**R114 — clôture arc spawn QA R89–R113** : audit **`run_simulation_v1_1_targeted_probes.ps1`** — **agrégation** de 9 artefacts contrat (pas de spawn enfant) ; arc spawn QA **terminé**.

**R115 — bornage stdout footprint library** : **`library_footprint_search_session`** + **`library_footprint_inspect_session`** — omission dump **`ApiResponse`** complet dans **`ipc_smoke.cpp`** ; regex probes / critères green **inchangés**.

**R116 — audit stdout library symbol** : **`library_symbol_search_session`** déjà partiellement borné (lignes greppables) ; **`library_symbol_inspect_session`** dump double — **~3 KiB** happy path, **sous** seuil R80 ; **pas de patch**.

**R117 — clôture dette harness library stdout** : footprint borné (**R115**) ; symbol audité sans patch (**R116**) ; dette **clôturée** ; patch symbol **option future** si stdout **> ~4 KiB** ou timeout reproductible lié au dump.

**R118 — audit capacité produit MCP** : recommandation **R119** = **`schematic_assign_footprint_session` V1** (assignation footprint validée sur symbole SCH ; gap manifeste post-**place_symbol**).

**R119 — cadrage technique assign footprint** : contrat JSON V1, blockers, validation via **`FootprintLibAdapter`** depuis SCH ; QA R120 = probe + extension **`sch_actions_v1`** ; implémentation recommandée R120.

**R120 — implémentation `schematic_assign_footprint_session` V1** : handler SCH + callback MCP + probe QA + extension **`sch_actions_v1`** ; validation footprint + **`pcb_ready`** ; **hors** gate MCP v2.

**R121 — mini-release `schematic_assign_footprint_session` V1** : clôture documentation officielle SCH Actions v1 (manifest, tool map, guide dev, README) ; contrat complet + limites + diff vs **`set_symbol_field`** ; QA checkpoint v1 **full** ; **aucune** nouvelle feature ; MCP v2 inchangé.

**R122 — audit `get_pcb_drc_violations_context_session` V1** : faisabilité confirmée (lecture marqueurs + option **`DRC_ENGINE::RunTests`** collecte mémoire) ; fixture **`ipc_drc_v1`** recommandée avant implémentation ; **hors** gate MCP v2 ; détail **`MCP_DECISIONS.md`** R122.

**R123 — fixture QA `ipc_drc_v1`** : plateau **`qa/data/pcbnew/drc/ipc_drc_v1/`** + **`r123_verify_ipc_drc_fixture.ps1`** ; prépare **`get_pcb_drc_violations_context_session`** ; **sans** handler MCP.

**R124 — implémentation `get_pcb_drc_violations_context_session` V1** : handler PCB read-only dual-mode (`run_drc=false` marqueurs / `run_drc=true` **`DRC_ENGINE`** mémoire) ; probe + famille **`pcb_drc_violations_context_v1`** ; fixture **`ipc_drc_v1`** ; **hors** gate MCP v2 ; détail **`MCP_DECISIONS.md`** R124.

**R125 — mini-release PCB DRC Violations Context v1** : clôture documentation officielle + observation stabilité (3× probe, 3× famille) ; **candidate stable** hors gate MCP v2 ; détail **`MCP_DECISIONS.md`** R125.

**R126 — intégration checkpoint MCP v2** : **`pcb_drc_violations_context_v1`** promue **9e** famille dans **`run_mcp_v2_checkpoint.ps1`** (après **`pcb_nearby_items_context_v1`**) ; **`checkpoint_family_count=9`** ; détail **`MCP_DECISIONS.md`** R126.

**R127 — durcissement fixture `ipc_drc_v1`** : contour **Edge.Cuts** fermé + empreintes inline sans nickname bibliothèque ; cible DRC mono-type **`clearance`** ; détail **`MCP_DECISIONS.md`** R127.

**R128 — audit enrichissement snapshot projet** : faisabilité **`drc_summary`** opt-in dans **`get_project_context_snapshot`** (metadata-only par défaut ; modes **`existing_markers`** / **`engine_run`** en opt-in ; implémentation **R129**) ; détail **`MCP_DECISIONS.md`** R128.

**R129 — implémentation `drc_summary` snapshot** : délégation **`CallTool(get_pcb_drc_violations_context_session)`** ; probe **`targeted_project_context_snapshot_drc_probe.ps1`** ; checkpoint MCP v2 **inchangé** ; détail **`MCP_DECISIONS.md`** R129.

**R130 — chemin PCB absolu snapshot** : champs **`snapshot_summary.open_pcb_board_absolute_path*`** ; probe **`targeted_project_context_snapshot_paths_probe.ps1`** ; détail **`MCP_DECISIONS.md`** R130.

**R131 — chemins absolus `open_documents[]`** : champs **`absolute_path`**, **`board_absolute_path`**, **`schematic_absolute_path`** additifs ; **`board_filename`** basename conservé ; détail **`MCP_DECISIONS.md`** R131.

**R132 — audit protobuf `DocumentSpecifier`** : promotion **`absolute_path`** au proto IPC **non retenue** (Option B) — navigation absolue officielle via MCP R130/R131 ; IPC natif reste basename ; détail **`MCP_DECISIONS.md`** R132.

**R133 — mini-release « Project Snapshot navigation-ready »** : clôture série R128–R132 ; **`get_project_context_snapshot`** = source IA recommandée chemins absolus ; pattern snapshot → tools PCB/DRC documenté ; protobuf inchangé ; QA non-régression paths + DRC snapshot + famille DRC v1 + CI VerifyOnly ; détail **`MCP_DECISIONS.md`** R133.

**R134 — audit `project_readiness_summary`** : faisabilité résumé compact IA (documents, DRC, footprints, actions) — **Option A** retenue (enrichir snapshot opt-in) ; gap footprint → tool SCH read-only **`schematic_footprint_readiness_session`** ; DRC via **`drc_summary`** existant ; **non implémenté** ; détail **`MCP_DECISIONS.md`** R134.

**R135a — `project_readiness_summary` dérivé** : bloc **`project_readiness_summary`** dans **`get_project_context_snapshot`** ; signaux dérivés + actions recommandées ; footprints unknown ; probe **`targeted_project_context_snapshot_readiness_probe.ps1`** ; détail **`MCP_DECISIONS.md`** R135a.

**R135b — `schematic_footprint_readiness_session`** : tool SCH read-only scan footprint + **`sch_to_pcb_ready`** ; snapshot opt-in **`include_footprints_summary`** ; validation library reportée **R136** ; probes **`targeted_schematic_footprint_readiness_probe.ps1`** + extension readiness ; détail **`MCP_DECISIONS.md`** R135b.

**R136 — audit `validate_footprints=true`** : faisabilité **oui** (réutilisation **`KIFACE_TEST_FOOTPRINT_LINK`** R120) ; sémantique **Option C** ; contrat / QA **R137** ; **non implémenté** ; détail **`MCP_DECISIONS.md`** R136.

**R137 — validation footprint readiness** : **`validate_footprints=true`** opérationnel ; **`sch_to_pcb_ready_validated`** ; snapshot **`footprints_validate`** ; correction non-régression **`changed=true`** post-mutation **`schematic_set_symbol_field_session`** ; détail **`MCP_DECISIONS.md`** R137.

**R138 — mini-release « Project Readiness Summary V1 complet »** : clôture R135a/R135b/R137 ; sémantique **`sch_to_pcb_ready`** vs **`sch_to_pcb_ready_validated`** ; recommended actions ; audit isolation QA **`issue19090`** ; checkpoint v2 **9 familles inchangé** ; détail **`MCP_DECISIONS.md`** R138.

**R139 — factorisation helper validation footprint SCH** : **`evaluateFootprintLinkForSchMcp`** partagé assign/readiness dans **`api_handler_sch.cpp`** ; mappings blockers/statuts inchangés ; cache readiness local préservé ; détail **`MCP_DECISIONS.md`** R139.

**R140 — audit multi-feuilles footprint readiness** : faisabilité **oui** ; Option **A** retenue pour R141 (`scope=current_sheet|hierarchy`) ; pattern ERC / **`GetSymbols`** ; **non implémenté** ; détail **`MCP_DECISIONS.md`** R140.

**R141 — `scope=hierarchy` footprint readiness** : **`schematic_footprint_readiness_session`** étendu (**`scope`**, **`include_sheet_paths`**, comptage instances, champs sheet) ; snapshot **`footprints_scope`** ; fixture **`ipc_footprint_readiness_v1`** ; **`current_sheet` non régressé** ; détail **`MCP_DECISIONS.md`** R141.

**R142 — audit assign footprint instance-aware** : faisabilité **oui avec spike écriture KiCad** ; assign actuel non instance-aware ; Option **C `instance_path`** retenue pour R143 ; **non implémenté** ; détail **`MCP_DECISIONS.md`** R142.

**R143 — `scope=hierarchy` assign footprint prudent** : résolution **`instance_path`** + dry-run hierarchy ; spike **instance write unsupported** → mutation **`hierarchy_instance_write_unsupported`** sans mutation ; **`current_sheet` non régressé** ; détail **`MCP_DECISIONS.md`** R143.

**R144 — Hierarchical Assign Footprint V1 safe-limited** : mini-release **documentaire** ; **`scope=hierarchy`** = résolution + dry-run + diagnostic ; mutation hierarchy = **`hierarchy_instance_write_unsupported`** (blocker propre, zéro mutation) ; workflow IA documenté ; **aucun code produit modifié** ; détail **`MCP_DECISIONS.md`** R144.

**R145 — Spike instance-safe footprint write** : fixture **`ipc_footprint_instance_write_spike`** ; spike QA **`r145_spike_schematic_footprint_instance_write.ps1`** ; **`instance_write_supported=false`** ; **`SetFootprintFieldText`** global confirmé ; mutation MCP hierarchy **non activée** ; détail **`MCP_DECISIONS.md`** R145.

**R146 — Garde-fou shared-sheet `current_sheet`** : détection **`affected_instances_count`** / **`shared_sheet_detected`** ; blocage défaut **`shared_symbol_global_update_blocked`** ; opt-in **`allow_shared_symbol_global_update`** ; **`issue19090`** inchangé ; détail **`MCP_DECISIONS.md`** R146.

**R147 — Safe Assign Footprint Guard officialisé** : mini-release **documentaire** clôturant R146 ; garde-fou **`allow_shared_symbol_global_update`** / blocker **`shared_symbol_global_update_blocked`** documentés dans manifeste + guide IA ; **sécurité IA** = aucune mutation globale silencieuse ; opt-in explicite pour mise à jour globale volontaire ; workflow 6 étapes (snapshot → dry-run → compteurs → confirmation) ; **aucun code produit modifié** ; détail **`MCP_DECISIONS.md`** R147.

**R148 — Audit workflow IA avancé assign footprint** : workflow déterministe hierarchy dry-run → bascule **`current_sheet`** si contexte sûr ; gaps navigation / active sheet identifiés ; **faisable avec supervision** ; recommandation R149 **Option A + B** ; **aucun code produit modifié** ; détail **`MCP_DECISIONS.md`** R148.

**R149 — Advanced Safe Footprint Assignment Workflow officialisé** : mini-release **documentaire** clôturant R148 ; workflow A–F (snapshot hierarchy → dry-run hierarchy → **`current_sheet`** si sûr) ; règles de sécurité et limites navigation / active sheet documentées ; contrat prévisionnel **`get_schematic_active_sheet_context_session`** (R150) ; **aucun code produit modifié** ; détail **`MCP_DECISIONS.md`** R149.

**R150 — `get_schematic_active_sheet_context_session` read-only** : tool MCP exposant **`CurrentSheet()`**, feuille active, symboles visibles + **`instance_path`** ; comble gap vérification contexte avant assign **`current_sheet`** ; probe **`targeted_schematic_active_sheet_context_probe.ps1`** ; **hors** checkpoint MCP v2 ; détail **`MCP_DECISIONS.md`** R150.

**R151 — Workflow IA consolidé active sheet context** : chaîne snapshot → readiness hierarchy → **`get_schematic_active_sheet_context_session`** (match **`instance_path`**) → assign dry-run **`current_sheet`** ; règle agent : pas de mutation sans correspondance positive ; probe workflow enrichie ; **aucun handler modifié** ; détail **`MCP_DECISIONS.md`** R151.

**R152 — Workflow active sheet hiérarchique** : extension probe sur fixture **`ipc_footprint_readiness_v1`** — cas root (instance sous-feuille absente, assign bloqué) et sous-feuille (**`qa_open_editor_frame`**, match positif, dry-run sûr) ; skip explicite si navigation manquante ; **aucun handler modifié** ; détail **`MCP_DECISIONS.md`** R152.

**R153 — Audit navigation SCH par sheet path** : faisabilité **oui** via **`SCH_ACTIONS::changeSheet`** ; tool R154 proposé **`navigate_schematic_sheet_session`** ; rejet navigation fichier seul ; **aucun code produit modifié** ; détail **`MCP_DECISIONS.md`** R153.

**R154 — `navigate_schematic_sheet_session`** : tool MCP produit navigation hiérarchique par **`sheet_path`** / **`instance_path`** ; probe **`targeted_schematic_navigate_sheet_probe.ps1`** ; **aucun critère green MCP v2 modifié** ; détail **`MCP_DECISIONS.md`** R154.

**R155 — Workflow SCH navigable sans `qa_open_editor_frame`** : probe active sheet migrée vers navigation produit ; **`global_update=false`** post-navigation native validé ; workflow IA end-to-end documenté ; **aucun handler modifié** ; détail **`MCP_DECISIONS.md`** R155.

**R156 — Exemple end-to-end Navigable SCH Footprint Workflow** : script **`r156_example_navigable_sch_footprint_workflow.ps1`** ; chaîne readiness → navigate → active sheet → assign dry-run ; **`agent_decision`** documentée ; **aucune mutation réelle** ; **aucun handler modifié** ; détail **`MCP_DECISIONS.md`** R156.

**R157 — Audit/spike API footprint instance-safe KiCad** : script **`r157_spike_footprint_instance_safe_write.ps1`** ; preuve négative isolation multi-instance ; **`instance_safe_write_supported=false`** ; mutation hierarchy **non activée** ; détail **`MCP_DECISIONS.md`** R157.

**R158 — Officialisation limite instance-safe + note design upstream** : statut **`hierarchy_assign_safe_limited`** confirmé ; note **`MCP_UPSTREAM_FOOTPRINT_INSTANCE_SAFE_WRITE_DESIGN.md`** ; critères gate R159/R160 ; **aucun handler MCP public modifié** ; détail **`MCP_DECISIONS.md`** R158.

**R159 — Prototype `SetFootprintFieldTextForSheet`** : API upstream + QA **`qa_spike_footprint_instance_api`** ; spike **`r159_spike_footprint_instance_safe_api.ps1`** ; mutation hierarchy MCP **non activée** ; détail **`MCP_DECISIONS.md`** R159.

**R160 — Mutation hierarchy MCP instance-safe** : **`schematic_assign_footprint_session scope=hierarchy`** via **`SetFootprintFieldTextForSheet`** ; probe **`targeted_schematic_assign_footprint_hierarchy_probe.ps1`** ; **`instance_safe_write_used=true`**, **`global_update=false`** ; détail **`MCP_DECISIONS.md`** R160.

**R161 — Mini-release « Hierarchy Footprint Assignment enabled »** : statut **`hierarchy_assign_instance_safe_enabled`** ; workflows IA Workflow A (direct hierarchy) + Workflow B (R156 conservateur) ; limites save/undo documentées ; **aucun handler modifié** ; checkpoint MCP v2 **inchangé, 9 familles** ; détail **`MCP_DECISIONS.md`** R161.

**R162 — Runtime discovery `hierarchy_assign_instance_safe_enabled`** : champs additifs dans **`project_readiness_summary`** (`get_project_context_snapshot`) ; probe **`targeted_project_context_snapshot_readiness_probe.ps1`** ; **aucune mutation** ; checkpoint MCP v2 **inchangé, 9 familles** ; détail **`MCP_DECISIONS.md`** R162.

**R163 — Exemple IA runtime-driven footprint workflow** : script **`r163_example_runtime_selected_footprint_workflow.ps1`** consomme champs R162 et auto-sélectionne Workflow A ; fallback R156 conservé ; **aucun handler modifié** ; checkpoint MCP v2 **inchangé, 9 familles** ; détail **`MCP_DECISIONS.md`** R163.

**R164 — CI observability R163** : **`run_mcp_v2_ci_baseline.ps1`** step **G4** post-checkpoint ; champs **`runtime_footprint_workflow_*`** ; **hors** **`ci_baseline_green`** ; **`-VerifyOnly`** → **`N/A`** ; détail **`MCP_DECISIONS.md`** R164.

**R165 — Echo runtime workflow readiness** : **`schematic_footprint_readiness_session`** expose champs R162 (helper partagé) ; probe **`runtime_workflow_echo_ok`** ; **aucun nouveau gate CI** ; détail **`MCP_DECISIONS.md`** R165.

**R166 — Dual-source workflow consistency** : script **`report_footprint_workflow_discovery_consistency.ps1`** ; priorité agent snapshot > readiness ; **hors gate MCP v2** ; détail **`MCP_DECISIONS.md`** R166.

**R167 — CI advisory dual-source consistency** : step **G5** dans **`run_mcp_v2_ci_baseline.ps1`** ; champs **`footprint_workflow_discovery_consistency_*`** ; **hors** **`ci_baseline_green`** ; détail **`MCP_DECISIONS.md`** R167.

**R168 — Politique alerte observability G4/G5** : clôture série R162–R167 ; **`ci_baseline_green=true` ne suffit pas** pour ignorer advisory rouge ; détail **`MCP_DECISIONS.md`** R168.

**R169 — Audit post-footprint SCH→PCB** : faisabilité sync PCB depuis schéma auditée ; API **`BOARD_NETLIST_UPDATER`** + dry-run natif ; **aucune implémentation** ; recommandation R170 **Option A puis B** ; détail **`MCP_DECISIONS.md`** R169.

**R170 — `get_pcb_sync_readiness_session` V1** : read-only SCH↔PCB footprint sync readiness ; matching reference/path ; fixtures **`ipc_pcb_sync_v1_*`** ; **hors checkpoint MCP v2** ; détail **`MCP_DECISIONS.md`** R170.

**R171 — `pcb_update_from_schematic_preview_session` V1** : dry-run **`BOARD_NETLIST_UPDATER`** ; reporter structuré ; fixtures **`ipc_pcb_sync_v1_*`** ; **hors checkpoint MCP v2** ; détail **`MCP_DECISIONS.md`** R171.

**R172 — `pcb_sync_summary` snapshot opt-in** : bloc dans **`get_project_context_snapshot`** ; readiness R170 + preview R171 opt-in ; **hors checkpoint MCP v2** ; détail **`MCP_DECISIONS.md`** R172.

**R173 — `pcb_update_from_schematic_session` prototype** : apply contrôlé avec gate **`confirm_apply`** ; restore QA fichier ; **hors checkpoint MCP v2** ; détail **`MCP_DECISIONS.md`** R173.

**R174 — mini-release doc `PCB Update Apply V1 controlled`** : officialisation **`pcb_update_apply_controlled_v1`** ; workflow IA + limites save/undo ; **aucun code** ; QA clôture green ; **hors checkpoint MCP v2** ; détail **`MCP_DECISIONS.md`** R174.

**R175 — audit save / undo / DRC gate post-apply** : audit doc only ; **R176 = `save_board_session` (Option A)** + DRC advisory opt-in ; undo MCP rejeté ; détail **`MCP_DECISIONS.md`** R175.

**R176 — `save_board_session` + DRC advisory post-apply** : save gate **`confirm_save`** ; apply opt-in **`include_post_apply_drc_summary`** (non bloquant) ; probes dédiées ; **hors checkpoint MCP v2** ; critères green inchangés ; détail **`MCP_DECISIONS.md`** R176.

**R177 — mini-release doc `pcb_apply_drc_save_workflow_v1`** : workflow IA apply → DRC advisory → save ; probe **`r177_example_pcb_apply_drc_save_workflow.ps1`** ; **aucun code** ; **hors checkpoint MCP v2** ; détail **`MCP_DECISIONS.md`** R177.

**R178 — CI observability G6** : intégration R177 en **`run_mcp_v2_ci_baseline.ps1`** step G6 advisory ; champs **`pcb_apply_drc_save_workflow_*`** ; **hors** **`ci_baseline_green`** ; détail **`MCP_DECISIONS.md`** R178.

**R179 — Politique alerte G6 + clôture série PCB apply/save R170–R179** : doc only ; **`ci_baseline_green=true` ne suffit pas** si G6 rouge ; décision PM avant release ; détail **`MCP_DECISIONS.md`** R179.

**R180 — Rapport local advisory G4/G5/G6** : **`report_mcp_v2_ci_advisories.ps1`** read-only ; interprète summary CI baseline ; détail **`MCP_DECISIONS.md`** R180.

**R181 — Wrapper release advisory** : **`check_mcp_v2_release_advisories.ps1`** ; go/no-go release sur rapport R180 ; VerifyOnly non `go` ; détail **`MCP_DECISIONS.md`** R181.

**R182 — Export release pack MD/JSON** : **`export_mcp_v2_release_pack.ps1`** ; agrège CI + R180 + R181 ; détail **`MCP_DECISIONS.md`** R182.

**R183 — Checkpoint detail dans release pack** : **`-IncludeCheckpointSummary`** sur export ; 9 familles MCP v2 ; détail **`MCP_DECISIONS.md`** R183.

**R184 — Pipeline release local** : **`run_mcp_v2_release_pack.ps1`** ; CI + R180 + R181 + export pack ; détail **`MCP_DECISIONS.md`** R184.

**R185 — Commande officielle release MCP v2** : **`run_mcp_v2_release_pack.ps1 -SkipBuild -KillRunningKiCad`** ; artefacts pack + pipeline summary ; exit **0** = `go` ; distinction CI dev / release ; détail **`MCP_DECISIONS.md`** R185.

**R186 — Audit CI/CD release pack** : GitLab CI identifié ; MCP v2 non intégré ; recommandation **R187** = job manuel Windows Option B ; détail **`MCP_DECISIONS.md`** R186.

**R187 — Job GitLab manuel release pack** : **`.gitlab/MCP-Windows-Release.yml`** — `mcp_v2_release_pack_windows` ; stage `release` manuel ; détail **`MCP_DECISIONS.md`** R187.

**R188 — Premier run GitLab réel** : pré-vol YAML **OK** ; run runner **non exécuté** (`blocked_by_gitlab_access=true`) ; validation runner officiel **en attente**.

**R189 — Procédure mainteneur GitLab** : procédure 9 étapes, checklist 18 champs, classification 9 échecs, bloc handoff copier-coller — **`MCP_DECISIONS.md`** R189 ; run réel **en attente** retour mainteneur `kicad`.

Etat terrain observé :

- `smoke` : vert (`mcp_v1_checkpoint_green=true`)
- `core` : vert (`mcp_v1_core_checkpoint_green=true`, `mcp_v1_checkpoint_green=true`)
- `full` : vert (`mcp_v1_full_checkpoint_green=true`, `mcp_v1_checkpoint_green=true`)

Checkpoint MCP v2 (séparé) observé :

- runner : `qa/tools/ipc_tools/run_mcp_v2_checkpoint.ps1`
- summary : `qa_mcp_v2_checkpoint_summary.txt`
- familles actuellement agrégées (**neuf**, **R33** + **R39** + **R126**) : `SCH Geometry v2`, `SCH Authoring v2`, `SCH Wiring v2`, `SCH Symbol Editing v2`, `PCB Inspection v2`, `pcb_geometry v1`, `pcb_clearance_context v1`, **`pcb_nearby_items_context v1`**, **`pcb_drc_violations_context v1`** (`get_pcb_drc_violations_context_session`)
- état : vert (`mcp_v2_checkpoint_green=true`, … **`pcb_drc_violations_context_v1_ok=true`**, `exact_blocker_if_any=N/A`)

## Capabilities stabilisées

- Simulation v1.1 : 9 tools stables (lecture + exécution bornée).
- Library v1 : 4 tools stables (search/inspect SCH + PCB, read-only).
- SCH Connectivity v1 : 2 tools stables (`schematic_net_inspect_session`, `schematic_symbol_connectivity_session`, read-only compact).
- SCH Actions v1 : 4 tools stables (`schematic_set_symbol_value_session`, `schematic_set_symbol_field_session`, `schematic_set_label_text_session`, `schematic_assign_footprint_session` ; **`assign_footprint`** release officielle **R121**).
- SCH Geometry v2 : 3 tools stables (`schematic_move_symbol_session`, `schematic_move_label_session`, `schematic_create_wire_segment_session`, actions géométriques SCH contrôlées, undoable, dry-run/readback/restore QA).
- SCH Authoring v2 : 2 tools stables (`schematic_create_label_session`, `schematic_place_symbol_session`, actions d'authoring SCH contrôlées, undoable, dry-run/readback/restore QA).
- SCH Wiring v2 : 2 tools stables (`schematic_connect_symbol_pin_session`, `schematic_connect_pins_l_shape_session`, wiring SCH contrôlé pin-to-pin aligné et non aligné, dry-run/readback/restore QA).
- SCH Symbol Editing v2 : 3 tools stables (`schematic_set_symbol_field_session`, `schematic_set_symbol_value_session`, `schematic_set_symbol_reference_session`, édition contrôlée de symbole existant, dry-run/mutation/readback/restore QA).
- PCB Inspection v2 : 3 tools stables (`pcb_get_board_items_session`, `pcb_get_board_item_by_id_session`, `pcb_get_net_items_session` ; probes contractuels `qa_pcb_get_board_items_session_contract_summary.txt`, `qa_pcb_get_board_item_by_id_session_contract_summary.txt`, `qa_pcb_get_net_items_session_contract_summary.txt`).
- pcb_geometry v1 : 2 tools stables (`pcb_measure_distance_session`, `get_pcb_item_geometry_session`) — **release produit documentée PCB Geometry v1.3** (**R27**) ; **mesure** (**v1.2** **R23**) = modes **point–point**, **point–item**, **item–item `bbox_center`**, **`pad_anchor`**, **`pad_shape_origin`** ; **`layer`** officiel = protobuf **`BoardLayer`** (**`BL_F_Cu`**, …) ; alias KiCad court (**`F_Cu`**) accepté ; **introspection** (**`get_pcb_item_geometry_session`**) = bbox nm / centre / position / net / couche ; pads : **`pad_anchor`**, **`pad_shape_origin`** si **`layer`** ; fast-path bbox pad + **`qa_ipc_smoke` borné** (**R26-Diag**) ; même famille QA et runners (`targeted_pcb_measure_distance_probe.ps1`, `targeted_pcb_item_geometry_probe.ps1`, `qa_pcb_measure_distance_session_contract_summary.txt`, `qa_pcb_item_geometry_session_contract_summary.txt`, `qa_pcb_geometry_v1_family_summary.txt`).

### PCB Geometry v1.1 (mini-release R19 — clôture `position` + `pad_anchor`)

Référence historique : la **famille QA** reste **`pcb_geometry_v1`** ; **v1.1** = baseline après **R18** (**`bbox_center`** + **`pad_anchor`**), avant **`pad_shape_origin`** stable documenté en **v1.2**.

| Élément | Détail |
|--------|--------|
| Tool | `pcb_measure_distance_session` |
| Nouveauté vs mesure point+bbox seule | Sur les endpoints **`item_id`**, clé optionnelle **`position`** : défaut **`bbox_center`** ; **`pad_anchor`** pour les pads (`PCB_PAD_T`) uniquement |
| Valeurs `position` (v1.1) | **`bbox_center`** → **`BOARD_ITEM::GetBoundingBox().Centre()`** en nm ; **`pad_anchor`** → **`PAD::GetPosition()`** en nm (**pas** centroïde du polygone de pastille, **pas** clearance, **pas** bord-à-bord ; **`pad_anchor`** exige **`PCB_PAD_T`**) |
| Modes supportés (v1.1) | point–point ; point–item ; item–item **`bbox_center`** ; item **`pad_anchor`** (pads) |
| Hors périmètre (non implémentés) | centroïde polygone explicite, clearance métier, **`bbox_edge`**, distance layer-aware **générique** / net-aware, mutation PCB |
| Famille QA | `pcb_geometry_v1` — runner `run_pcb_geometry_v1_targeted_probes.ps1` |
| Statut | **Checkpoint-covered** (checkpoint MCP v2, priorité blocker 6) |
| QA agrégée | Probe ciblé + famille **`pcb_geometry_v1`** + **`run_mcp_v2_checkpoint.ps1`** |
| Sémantique | Distance **euclidienne 2D** en **nm** ; `dx_nm` / `dy_nm` **signés** ; `distance_nm` = **`llround(hypot(dx_nm,dy_nm))`** ; 1 IU = 100 nm |
| Harness | Sortie **`qa_ipc_smoke` bornée** pour cet outil (évite timeouts stdout massifs) ; détails techniques **R15–R18** |
| Limites connues | Flake occasionnel **`nng_dial: Connection refused`** (**R16**) : retry borné + champ **`ipc_smoke_ipc_retry_extra_total`** ; cartes très denses : bbox d’empreinte plus coûteuse (**R15**) |

### PCB Geometry v1.2 (mini-release R23 — clôture **`pad_shape_origin`**)

Baseline produit / QA **actuelle** pour la distance PCB en MCP v2 : extension **v1.1** avec **`position="pad_shape_origin"`** = **`PAD::ShapePos(layer)`** sur **`PCB_PAD_T`**, **`layer`** obligatoire.

| Élément | Détail |
|--------|--------|
| Nouveauté vs v1.1 | **`pad_shape_origin`** : **`layer`** requis (enum protobuf **`BoardLayer`**, ex. **`BL_F_Cu`** ; alias court **`F_Cu`** accepté côté handler → **`BL_F_Cu`**) |
| Négatifs QA (probe + checkpoint) | **`layer`** manquant → **`pad_shape_origin_requires_layer_*`** ; item non-pad → **`pad_shape_origin_requires_pad_*`** ; **`layer`** invalide → **`invalid_layer_*`** |
| Fixture dédiée | **`qa/data/pcbnew/geometry/ipc_geometry_v1/`** (projet **`ipc_geometry_v1`**, pad KIID fixe, **offset non nul** pour distinguer **`ShapePos(F_Cu)`** vs **`pad_anchor`**) ; copie harness vers **`build/.../ipc-geometry-v1-proj/`** |
| Statut | **Checkpoint-covered** (famille **`pcb_geometry_v1`**, priorité blocker 6) |
| Références | Implémentation **R22** ; parsing layer **R22-Diag** ; clôture doc **R23** |

### PCB Geometry v1.3 (mini-release **R27** — clôture **`get_pcb_item_geometry_session`**)

Baseline produit **actuelle** pour la famille **`pcb_geometry_v1`** au checkpoint MCP v2 : **mesure** inchangée (**v1.2** **R23**) + introspection **`get_pcb_item_geometry_session`** documentée **stable**, checkpoint-covered.

| Élément | Détail |
|--------|--------|
| Nouveauté vs v1.2 | Tool **`get_pcb_item_geometry_session`** : introspection **read-only** d’un item PCB (`board_filename` + `item_id`) — **pas** une mesure de distance, **pas** de polygone ; **complète** **`pcb_measure_distance_session`**. Champs : **`bbox_*`**, **`bbox_center_*`**, **`position_*`**, **`layer`**, **`net_name`** ; pads : **`pad_anchor_*`** ; **`pad_shape_origin_*`** + **`layer_used`** si **`layer`** (`BoardLayer` **`BL_*`**, alias **`F_Cu`**) |
| Implémentation pad | Bbox nominale pad = **`ShapePos(layer)` + `GetSize(layer)`** (fast-path), évite **`PAD::GetBoundingBox()`** sur le chemin nominal (**R26-Diag**) |
| Harness | Sortie **`qa_ipc_smoke` bornée** pour **`get_pcb_item_geometry_session`** (comme **`pcb_measure_distance_session`**) — évite dumps **`ApiResponse`** massifs |
| Probes | **`targeted_pcb_item_geometry_probe.ps1`** ; famille **`run_pcb_geometry_v1_targeted_probes.ps1`** agrège **mesure** + **item_geometry** |
| QA / checkpoint | **`pcb_geometry_v1_family_green`** ; **`pcb_item_geometry_green`** ; **`run_mcp_v2_checkpoint.ps1`** (priorité blocker 6) |
| Hors périmètre | clearance, bord-à-bord générique, centroïde polygone, mutation PCB, net-aware distance (inchangé vs familles précédentes) |
| Références | Impl **R26** ; stabilité terrain **R26-Diag** ; clôture release **R27** |

### PCB Clearance Context v1 (release **R32**)

Capacité stable documentée : **`get_pcb_clearance_context_session`** — **contexte clearance** (lecture structurée), **pas** une DRC KiCad complète.

| Élément | Détail |
|--------|--------|
| Nouvelle capacité (produit) | **`get_pcb_clearance_context_session`** — **`is_true_drc=false`** ; **pas** `RunTests` ; **pas** rapport de violations ; distance **`geometric_distance_nm`** = **contexte** (centres bbox), **pas** clearance bord-à-bord ; **`required_clearance_nm`** = clearance **nominale** netclass (nm) si lisible ; **`clearance_status`** = disponibilité / contexte (**`context_only`**, **`rule_unavailable`**, etc.), **pas** verdict officiel |
| Implémentation handler | **R31** (callback + harness borné) |
| Fixture QA | **`ipc_clearance_v1`** (`qa/data/pcbnew/clearance/ipc_clearance_v1/`) |
| Probes | **`targeted_pcb_clearance_context_probe.ps1`** ; **`run_pcb_clearance_context_v1_targeted_probes.ps1`** |
| Famille QA | **`pcb_clearance_context_v1`** |
| Statut terrain | **Checkpoint-covered** (**R33**) : agrégée dans **`run_mcp_v2_checkpoint.ps1`** après **pcb_geometry v1** ; sémantique **context-only** inchangée (**`is_true_drc=false`**, **pas** verdict DRC officiel) |
| Blocker global | Préfixe **`pcb_clearance_context_v1:`** si cette famille échoue (**priorité** après **`pcb_geometry_v1`**) — voir liste des priorités checkpoint MCP v2 |

**Harness** : sortie **`qa_ipc_smoke` bornée** (comme **`pcb_measure_distance_session`**).

**Hors périmètre v1** (inchangé) : RunTests DRC, rapport violations, clearance exacte bord-à-bord, mutation PCB — voir **`MCP_DECISIONS.md`** **R32** / intégration gate **R33**. *(La capacité **nearby** est désormais couverte par **`get_pcb_nearby_items_context_session`** — **PCB Nearby Items Context v1**, **release R38**.)*

### PCB Nearby Items Context v1 (release **R38**, gate checkpoint **R39**)

Capacité stable documentée ; **intégrée** au checkpoint MCP v2 comme **8e** famille officielle (**R39**) : **`get_pcb_nearby_items_context_session`** — **contexte de voisinage PCB** (liste d’items proches), **pas** une DRC KiCad.

| Élément | Détail |
|--------|--------|
| Nouvelle capacité (produit) | **`get_pcb_nearby_items_context_session`** — **`is_true_drc=false`** ; **pas** `RunTests` ; **pas** verdict clearance / DRC ; **pas** rapport combiné nearby+clearance ; requête V1 = **`item_id`** + **`radius_nm`** (**pas** mode **point** en V1) ; **`distance_kind`** = **`bbox_center_to_query_nm`** ; scan **linéaire** V1 borné par **`radius_nm`** et **`max_items`** ; **`truncated`** si coupure ; filtres **`layer`** (**`BL_*`** / alias **`F_Cu`**), **`item_types`** (`pads`, `tracks`, `arcs`, `vias`), **`include_same_net`** (défaut **`true`**) |
| Implémentation handler | **R37** (callback + harness borné) ; QA terrain **R37-Validation** ; gate **R39** |
| Fixture QA | **`ipc_nearby_v1`** (`qa/data/pcbnew/nearby/ipc_nearby_v1/`) |
| Probes | **`targeted_pcb_nearby_items_context_probe.ps1`** ; **`run_pcb_nearby_items_context_v1_targeted_probes.ps1`** |
| Famille QA | **`pcb_nearby_items_context_v1`** |
| Statut terrain | **Checkpoint-covered** (**R39**) : agrégée dans **`run_mcp_v2_checkpoint.ps1`** après **`pcb_clearance_context_v1`** ; copie **`qa_pcb_nearby_items_context_v1_family_summary.txt`** ; blocker **`pcb_nearby_items_context_v1:<blocker>`** — **priorité 8** ; sémantique **context-only** inchangée |
| Avertissement QA | Ne **pas** lancer en **parallèle** sur une même machine plusieurs scripts familles **PCB** lourds **hors** checkpoint (plusieurs **`kicad.exe`**) ; le checkpoint enchaîne les familles **séquentiellement** |

**Harness** : sortie **`qa_ipc_smoke` bornée**.

**Hors périmètre v1** : DRC / `RunTests` / verdict clearance / mode point / mutation PCB / index RTree DRC — voir **`MCP_DECISIONS.md`** **R37** / **R38** / **R39**.

## Non-promoted / Experimental capabilities

`schematic_create_junction_session`

- Implémenté côté code et callable en direct.
- Rescope v1 appliqué : la mutation est refusée si `junction_needed=false`.
- Test négatif validé : point isolé -> `junction_not_needed`.
- Non promu stable : aucune fixture positive fiable disponible pour `junction_needed=true && junction_already_exists=false`.
- Non inclus dans `SCH Wiring v2`.
- Non inclus dans le checkpoint MCP v2 global.
- Prochaine action possible : créer une fixture dédiée ou étudier un mode topologique avec `BreakSegments`.

## QA et preuves

**Pipeline baseline MCP v2 (CI / local, R28, garde-fou contrats R65)** : **`qa/tools/ipc_tools/run_mcp_v2_ci_baseline.ps1`** agrège pré-check processus, toolchain MSVC, build minimal optionnel, smoke KIFACE, probes **`pcb_measure_distance`** et **`get_pcb_item_geometry`**, famille **`pcb_geometry_v1`**, famille **`pcb_negative_contracts_v1`** (**contrats `board_filename` / `max_items`** — **CI baseline guard**, **pas** une 9e famille checkpoint), checkpoint MCP v2 (**8 familles** inchangées) ; voir **`README_IPC_QA_BASELINE.md`** § *Pipeline CI baseline MCP v2* et résumé **`qa_mcp_v2_ci_baseline_summary.txt`**.

Runners officiels :

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
- `qa/tools/ipc_tools/run_pcb_nearby_items_context_v1_targeted_probes.ps1` (**pcb_nearby_items_context v1**, **release R38**, gate **R39** — checkpoint-covered)

Summaries familiaux :

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

Summary global checkpoint :

- `qa_mcp_v1_checkpoint_summary.txt`

Commandes de référence :

```powershell
& "c:\dev\kicad-master\qa\tools\ipc_tools\run_mcp_v1_checkpoint.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release" -Profile smoke

& "c:\dev\kicad-master\qa\tools\ipc_tools\run_mcp_v1_checkpoint.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release" -Profile core

& "c:\dev\kicad-master\qa\tools\ipc_tools\run_mcp_v1_checkpoint.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release" -Profile full
```

Interpretation minimale recommandee :

1. Lire `mcp_v1_checkpoint_profile`.
2. Lire `mcp_v1_checkpoint_green` en tenant compte du profil.
3. Verifier les flags d'inclusion (`library_v1_included`, `sch_connectivity_v1_included`).
4. Verifier `exact_blocker_if_any` et, selon besoin, les blockers dedies.

Profils :

1. `smoke`
   - Inclus : Simulation v1.1, SCH Actions v1
   - Skipped : SCH Connectivity v1, Library v1
   - Usage : gate rapide fiable PR / verification de base
2. `core`
   - Inclus : Simulation v1.1, SCH Connectivity v1, SCH Actions v1
   - Skipped : Library v1
   - Usage : validation schematique plus complete
   - Etat actuel : vert sur checkpoint core
3. `full`
   - Inclus : Simulation v1.1, Library v1, SCH Connectivity v1, SCH Actions v1
   - Usage : validation complete/release/diagnostic lourd
   - Etat actuel : vert sur checkpoint full

Mise a jour stabilisation SCH Connectivity v1 :

- `schematic_net_inspect_session` et `schematic_symbol_connectivity_session` sont stabilises sur les probes cibles via fixture SCH `issue19090`.
- Le gate `core` couvre maintenant de facon verte :
  - `simulation_v1_1_ok=true`
  - `sch_connectivity_v1_ok=true`
  - `sch_actions_v1_ok=true`
  - `library_v1_included=false`
  - `library_v1_ok=skipped`
  - `exact_blocker_if_any=N/A`

Mise a jour Library v1 gating :

- `library_v1_family_green=true` avec gating sur :
  - `symbol_search_ok=true`
  - `symbol_inspect_ok=true`
  - `footprint_search_ok=true` (**R82**)
  - `footprint_search_board_filename_negative_green=true` (**R83**)
  - `footprint_inspect_ok=true`
  - `footprint_inspect_board_filename_negative_green=true`
- Champs de suivi footprint search :
  - `footprint_search_ok`
  - `footprint_search_diagnostic_ok`
  - `footprint_search_gating=true`
  - `footprint_search_board_filename_negative_gating=true`
  - `exact_blocker_footprint_search_if_any`
  - `footprint_search_board_filename_invalid_path_ok` (advisory wx permissif)

Artefacts canoniques :

- Simulation v1.1 : `qa_simulation_*_contract_summary.txt`
- Library v1 : `qa_library_*_contract_summary.txt`
- SCH Connectivity v1 :
  - `qa_schematic_net_inspect_session_contract_summary.txt`
  - `qa_schematic_symbol_connectivity_session_contract_summary.txt`
- SCH Actions v1 :
  - `qa_schematic_set_symbol_value_session_contract_summary.txt`
  - `qa_schematic_set_symbol_field_session_contract_summary.txt`
  - `qa_schematic_set_label_text_session_contract_summary.txt`
  - `qa_schematic_assign_footprint_session_contract_summary.txt`
- SCH Geometry v2 :
  - `qa_schematic_move_symbol_session_contract_summary.txt`
  - `qa_schematic_move_label_session_contract_summary.txt`
  - `qa_schematic_create_wire_segment_session_contract_summary.txt`
- SCH Authoring v2 :
  - `qa_schematic_create_label_session_contract_summary.txt`
  - `qa_schematic_place_symbol_session_contract_summary.txt`
- SCH Wiring v2 :
  - `qa_schematic_connect_symbol_pin_session_contract_summary.txt`
  - `qa_schematic_connect_pins_l_shape_session_contract_summary.txt`
- SCH Symbol Editing v2 :
  - `qa_schematic_set_symbol_field_session_contract_summary.txt`
  - `qa_schematic_set_symbol_value_session_contract_summary.txt`
  - `qa_schematic_set_symbol_reference_session_contract_summary.txt`
- PCB Inspection v2 :
  - `qa_pcb_get_board_items_session_contract_summary.txt`
  - `qa_pcb_get_board_item_by_id_session_contract_summary.txt`
  - `qa_pcb_get_net_items_session_contract_summary.txt`

Capabilities SCH Connectivity v1 :

1. `schematic_net_inspect_session`
2. `schematic_symbol_connectivity_session`

Capabilities SCH Actions v1 :

1. `schematic_set_symbol_value_session`
2. `schematic_set_symbol_field_session`
3. `schematic_set_label_text_session`
4. `schematic_assign_footprint_session` (**R121** — validation library + **`pcb_ready`**, hors gate MCP v2)

Capability SCH Geometry v2 :

1. `schematic_move_symbol_session`
2. `schematic_move_label_session`
3. `schematic_create_wire_segment_session`

## SCH Geometry v2 (indépendant du checkpoint v1)

Périmètre stable :

- déplacement absolu de symbole SCH uniquement
- déplacement absolu de local label SCH uniquement
- création d'un segment de fil SCH unique (wire uniquement, pas bus)
- segment orthogonal par défaut (`require_orthogonal=true`)
- `start`/`end` absolus en `*_nm`
- `symbol_id` obligatoire
- `label_id` obligatoire
- output plat (pas de struct imbriquée)
- pas d'autorouting
- pas de multi-segment
- pas de déplacement automatique des fils
- pas de déplacement automatique d'autres labels
- output position plat : `*_x_nm` / `*_y_nm`
- pas de sauvegarde automatique

Runner/summary :

- runner : `qa/tools/ipc_tools/run_sch_geometry_v2_targeted_probes.ps1`
- summary : `qa_sch_geometry_v2_family_summary.txt`

Critères green :

1. `sch_geometry_v2_family_green=true`
2. `schematic_move_symbol_ok=true`
3. `schematic_move_label_ok=true`
4. `schematic_create_wire_segment_ok=true`
5. `exact_blocker_if_any=N/A`

Note d'intégration :

- `SCH Geometry v2` n'est pas encore intégré dans le checkpoint global MCP v1.
- Le runner global v1 reste séparé et inchangé.
- Le checkpoint MCP v2 agrège les **huit** familles MCP v2 stables (**R33** **`pcb_clearance_context_v1`** + **R39** **`pcb_nearby_items_context_v1`**) — voir section « Checkpoint MCP v2 (séparé) ».

## SCH Authoring v2 (indépendant des checkpoints globaux)

Capability SCH Authoring v2 :

1. `schematic_create_label_session`
2. `schematic_place_symbol_session`

Périmètre stable :

- label local uniquement
- texte requis, non vide, borné
- position absolue en `*_nm`
- output plat
- pas de global label
- pas de hierarchical label
- pas de sauvegarde automatique
- restore QA par suppression du label créé
- `schematic_place_symbol_session` : placement d'un symbole SCH depuis `lib_id`
- entrées contrôlées : `lib_id`, `position.x_nm/y_nm`, `reference`, `value`
- pas d'auto-annotation globale
- pas d'assignation footprint automatique
- pas de sauvegarde automatique
- validation QA officielle en deux phases :
  - phase A : dry-run dans process KiCad A
  - phase B : mutation/readback/restore dans process KiCad B

Runner/summary :

- runner : `qa/tools/ipc_tools/run_sch_authoring_v2_targeted_probes.ps1`
- summary : `qa_sch_authoring_v2_family_summary.txt`

Critères green :

1. `sch_authoring_v2_family_green=true`
2. `schematic_create_label_ok=true`
3. `schematic_place_symbol_ok=true`
4. `exact_blocker_if_any=N/A`

### Télémétrie advisory post-mutation (`schematic_place_symbol`) — **R103 / R104**

Les champs **`sch_authoring_place_symbol_*`** dans **`qa_mcp_v2_checkpoint_summary.txt`** sont **advisory-only** : ils **ne modifient pas** **`mcp_v2_checkpoint_green`**, **`sch_authoring_v2_ok`**, ni **`exact_blocker_if_any`**.

- **`sch_authoring_place_symbol_kicad_exited_post_mutation_detected=true`** : le process KiCad est sorti après une mutation tool **AS_OK** (exit/crash), distinct d’une simple indisponibilité socket transitoire.
- Un probe peut rester **vert** (`exact_blocker_if_any=N/A`) si le harness R102 a récupéré via restart cleanup-only ; le checkpoint reste **vert** ; l’advisory signale tout de même l’événement.
- Un probe **rouge** conserve les blockers finaux (`restore_connection_refused`, `connection_refused`, `dry_run_phase_not_ok`, …) ; un checkpoint **rouge** reflète **`sch_authoring_v2_ok=false`**, pas l’advisory seul.

**Seuils d’alerte (documentation, pas gate CI) :**

| Niveau | Condition | Action |
|--------|-----------|--------|
| Info | 1× advisory **`true`**, run vert | Conserver artefacts ; pas de blocage |
| Investigation | **≥ 2×** sur 10 runs **`targeted_schematic_place_symbol_probe.ps1`**, ou **≥ 1×** advisory **`true`** dans un checkpoint CI quotidien | Crash root-cause eeschema / KiCad |
| Incident QA | Advisory **`true`** + blocker **`restore_connection_refused`** ou **`connection_refused`** | Incident QA ; artefacts probe + famille + checkpoint |

Artefacts : répertoire **`*-schematic-place-symbol-targeted`**, **`post_mutation_ipc_status.txt`**, **`ipc_reconnect_*`**, **`delete_sch_items_*`**, summaries probe / famille / checkpoint (détail **`README_IPC_QA_BASELINE.md`** §7bis).

**R105 — script read-only `report_sch_authoring_advisory.ps1`** : lit **`qa_mcp_v2_checkpoint_summary.txt`** (dernier **`*-mcp-v2-checkpoint`** ou **`-CheckpointRunDir`**) ; émet **`Write-Warning`** si **`kicad_exited_post_mutation_detected`**, cleanup restart, ou retries > 0 ; résumé **`qa_sch_authoring_advisory_report_summary.txt`** ; **exit 0** même avec warnings ; **non intégré** au gate **`mcp_v2_checkpoint_green`**.

**R106 — intégration CI baseline** : **`run_mcp_v2_ci_baseline.ps1`** appelle le report **après** le checkpoint enfant ; champs **`sch_authoring_advisory_*`** dans **`qa_mcp_v2_ci_baseline_summary.txt`** ; **hors** **`ci_baseline_green`** ; **`-VerifyOnly`** ne lance pas le report (**`N/A`**).

**R107 — scan multi-checkpoint** : **`report_sch_authoring_advisory.ps1 -ScanLastNCheckpoints N`** agrège les N derniers checkpoints ; warnings frequence R104 (**`kicad_exited_post_mutation_detected_count >= 2`**, ou **>= 1** sur checkpoint du jour UTC) ; **exit 0** même avec warnings ; incompatible avec **`-CheckpointRunDir`**.

**R108 — scan intégré CI baseline** : **`run_mcp_v2_ci_baseline.ps1`** appelle **`-ScanLastNCheckpoints`** après le report checkpoint unique ; champs **`sch_authoring_advisory_scan_*`** ; **hors** **`ci_baseline_green`** ; **`-VerifyOnly`** : **`N/A`**.

**R110 — `-SchAuthoringAdvisoryScanLastN`** : paramètre CLI CI baseline (défaut **10**) ; **`sch_authoring_advisory_scan_last_n`** dans le summary ; **`<= 0`** refusé.

### Clôture release observability R100–R108 (**R109**)

Synthèse : diagnostic flake (R100) → durcissement probe (R101–R102) → télémétrie advisory (R103) → politique + outils read-only (R104–R105) → intégration CI observability-only report + scan (R106–R108). **Gate MCP v2 et critères familiaux inchangés.**

Lecture opérationnelle : **`sch_authoring_place_symbol_*`** au checkpoint ; **`sch_authoring_advisory_report_*`** et **`sch_authoring_advisory_scan_*`** au CI baseline — tous **hors** **`ci_baseline_green`**. Détail complet : **`README_IPC_QA_BASELINE.md`** §7ter.

Note d'intégration :

- `SCH Authoring v2` est désormais intégré au checkpoint MCP v2 global.
- Le checkpoint MCP v2 agrège désormais `SCH Geometry v2`, `SCH Authoring v2`, `SCH Wiring v2`, `SCH Symbol Editing v2` (3 tools stables : field, value, reference) et `PCB Inspection v2` (3 tools stables : `pcb_get_board_items_session`, `pcb_get_board_item_by_id_session`, `pcb_get_net_items_session`).

## SCH Wiring v2 (indépendant du checkpoint v1)

Capability SCH Wiring v2 :

1. `schematic_connect_symbol_pin_session`
2. `schematic_connect_pins_l_shape_session`

Périmètre stable :

- connecte deux pins de symboles SCH par un wire contrôlé
- entrée : `from.symbol_id`, `from.pin_number`, `to.symbol_id`, `to.pin_number`
- v1 strict : segment unique uniquement
- v1 strict : pins alignées uniquement
- v1 strict : pas de L-shape
- v1 strict : pas d'autorouting
- pins hidden/power rejetées par défaut
- pins ambiguës refusées
- output minimal stabilisé
- readback via fallback endpoints si l'UUID post-commit diffère
- `dry_run` supporté
- restore QA validé
- `schematic_connect_pins_l_shape_session` : connecte deux pins non alignées via deux segments orthogonaux
- `schematic_connect_pins_l_shape_session` : `elbow_mode` supporté (`horizontal_then_vertical`, `vertical_then_horizontal`)
- `schematic_connect_pins_l_shape_session` : refuse les pins déjà alignées (`pins_already_aligned`)
- `schematic_connect_pins_l_shape_session` : pas d'autorouting
- `schematic_connect_pins_l_shape_session` : pas d'obstacle avoidance
- `schematic_connect_pins_l_shape_session` : junction non créée en v1 (`junction_id` vide)
- `schematic_connect_pins_l_shape_session` : readback wire UUID puis fallback endpoints
- `schematic_connect_pins_l_shape_session` : restore QA validé (delete wire1, delete wire2, restore move symbole)
- pas de sauvegarde automatique

Runner/summary :

- runner : `qa/tools/ipc_tools/run_sch_wiring_v2_targeted_probes.ps1`
- summary : `qa_sch_wiring_v2_family_summary.txt`

Critères green :

1. `sch_wiring_v2_family_green=true`
2. `schematic_connect_symbol_pin_ok=true`
3. `schematic_connect_pins_l_shape_ok=true`
4. `exact_blocker_if_any=N/A`
5. `schematic_connect_symbol_pin_exit_code=0`
6. `schematic_connect_pins_l_shape_exit_code=0`

## SCH Symbol Editing v2 (indépendant du checkpoint v1)

Capabilities SCH Symbol Editing v2 :

1. `schematic_set_symbol_field_session`
2. `schematic_set_symbol_value_session`
3. `schematic_set_symbol_reference_session`

Périmètre stable :

- `schematic_set_symbol_field_session` : modifie un champ existant non `REFERENCE`/`VALUE` d'un symbole existant
- `schematic_set_symbol_value_session` : modifie le champ `Value` d'un symbole existant
- `schematic_set_symbol_reference_session` : modifie la référence `Reference` d'un symbole existant via `symbol_id` ; périmètre v1 limité à `CurrentSheet()` ; mutation alignée KiCad (`SCH_SYMBOL::SetRef`) ; validation non interactive du designator ; pas de détection globale de conflit de référence en v1 ; pas d'annotation globale ; pas de sauvegarde automatique
- cible `symbol_id` explicite
- `dry_run`, mutation, readback, restore QA
- `expected_old_value` / `expected_old_reference` supportés selon le tool
- `schematic_set_symbol_value_session` : `new_value` limité à 1024 caractères
- pas de sauvegarde automatique

Blockers importants (`schematic_set_symbol_value_session`) :

- `invalid_symbol_id`
- `symbol_not_found`
- `value_field_not_found`
- `expected_old_value_mismatch`
- `mutation_failed`

Blockers importants (`schematic_set_symbol_reference_session`) :

- `invalid_symbol_id`
- `symbol_not_found`
- `invalid_reference`
- `expected_old_reference_mismatch`
- `mutation_failed`
- `readback_failed`

Runner/summary :

- runner : `qa/tools/ipc_tools/run_sch_symbol_editing_v2_targeted_probes.ps1`
- summary : `qa_sch_symbol_editing_v2_family_summary.txt`
- probes contractuels :
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

## PCB Inspection v2 (indépendant du checkpoint v1)

Capability PCB Inspection v2 :

1. `pcb_get_board_items_session`
2. `pcb_get_board_item_by_id_session`
3. `pcb_get_net_items_session`

Périmètre stable :

- lecture du PCB ouvert dans l’éditeur ; alignement requis sur `board_filename` (absolu)
- read-only, pas de mutation, pas de sauvegarde automatique
- `pcb_get_board_items_session` : compteurs (`footprint_count`, `pad_count`, `track_count`, `via_count`, `zone_count`, `drawing_count`) ; `items_summary` borné par `max_items` ; `item_types` optionnel ; troncature via `items_summary_truncated=true` ; négatifs validés : `invalid_item_type`, `board_filename_mismatch`
- `pcb_get_board_item_by_id_session` : item unique via `item_id` ; résolution `BOARD::ResolveItem(KIID, true)` ; distingue `invalid_item_id`, `item_not_found`, `unsupported_item_type` ; types v1 : footprint, pad, track, arc, via, zone, drawing ; sortie plate (`item_type`, `layer`, `x_nm`, `y_nm`, `net_name`, `parent_footprint_id`, `reference`, `value`, `details`) ; négatifs validés : `invalid_item_id`, `item_not_found`, `board_filename_mismatch`
- `pcb_get_net_items_session` : items du net demandé (pads, tracks, arcs, vias, zones cuivre) ; sélection par `net_name` et/ou `net_code` ; cohérence nom/code si les deux sont fournis ; `net_name` via `BOARD::FindNet(wxString)` (exact) ; `net_code=0` refusé en v1 ; compteurs `pad_count`, `track_count` (traces+arcs), `via_count`, `zone_count` ; `items_summary` borné par `max_items` ; `items_summary_truncated` ; blockers notables : `invalid_net_selector`, `net_not_found`, `net_selector_mismatch`, `board_filename_mismatch`, `invalid_max_items`, `read_failed` ; négatifs validés : absence de sélecteur net, `net_code=0`, nom inventé, mismatch `board_filename` ; `net_selector_mismatch` peut être **skipped** sur la fixture IPC v1 actuelle faute de deux nets nommés exploitables

Runner/summary :

- runner : `qa/tools/ipc_tools/run_pcb_inspection_v2_targeted_probes.ps1`
- summary : `qa_pcb_inspection_v2_family_summary.txt`
- probes contractuels :
  - `qa_pcb_get_board_items_session_contract_summary.txt`
  - `qa_pcb_get_board_item_by_id_session_contract_summary.txt`
  - `qa_pcb_get_net_items_session_contract_summary.txt`

Critères green :

1. `pcb_inspection_v2_family_green=true`
2. `pcb_get_board_items_ok=true`
3. `pcb_get_board_item_by_id_ok=true`
4. `pcb_get_net_items_ok=true`
5. `exact_blocker_if_any=N/A`
6. `pcb_get_board_items_exit_code=0`
7. `pcb_get_board_item_by_id_exit_code=0`
8. `pcb_get_net_items_exit_code=0`

Note d'intégration :

- `PCB Inspection v2` est intégrée au checkpoint MCP v2 global (priorité blocker 5).

## pcb_geometry v1 / PCB Geometry v1.1–v1.3 (checkpoint MCP v2)

Capability : même famille QA **`pcb_geometry_v1`** ; **PCB Geometry v1.3** (**R27**) = baseline produit **actuelle** (introspection **`get_pcb_item_geometry_session`** stable + mesure **v1.2**) ; **v1.2** (**R23**) = **`pad_shape_origin`** + **`BoardLayer`** / alias ; **v1.1** (**R19**) = **`bbox_center`** / **`pad_anchor`** sans **`pad_shape_origin`** (cf. sections ci-dessus).

1. `pcb_measure_distance_session` (point–point ; point–item ; item–item **`bbox_center`** ; item **`pad_anchor`** sur `PCB_PAD_T` ; item **`pad_shape_origin`** sur `PCB_PAD_T` avec **`layer`**)

2. `get_pcb_item_geometry_session` (introspection : **`board_filename`** + **`item_id`** ; bbox nm ; position ; net / layer ; pads : **`pad_anchor_*`** ; **`pad_shape_origin_*`** si **`layer`** ; **pas** polygone ; sortie harness bornée)

Périmètre stable :

- **V1** : distance euclidienne 2D en nm entre deux points board-space (`a`, `b` avec `x_nm`/`y_nm` uniquement) ; `board_filename` aligné sur le PCB ouvert ; `dx_nm` / `dy_nm` signés ; `distance_nm` via `llround(hypot(dx_nm,dy_nm))` ; 1 IU = 100 nm ; read-only.
- **V2** : `a` et/ou `b` comme `{ item_id }` (KIID) avec option **`position`** : absent ou **`bbox_center`** → centre **`GetBoundingBox().Centre()`** en nm ; **`pad_anchor`** → **`PAD::GetPosition()`** en nm si l’item est un pad (`PCB_PAD_T`) — ni centroïde de polygone de pastille, ni clearance, ni bord à bord ; **`pad_shape_origin`** → **`PAD::ShapePos(layer)`** si pad + **`layer`** (**`BoardLayer`**) — ni centroïde polygone, ni clearance générique, ni bord à bord ; **`layer`** avec point ou avec **`bbox_center`**/**`pad_anchor`** → **`invalid_endpoint_*`** ; **`position` avec `x_nm`/`y_nm`** sur le même endpoint → **`invalid_endpoint_*`** ; pas de distance layer-aware générique hors **`pad_shape_origin`** nommé (R18 / R22).
- **Stabilité harness (R15 / R27)** : Les timeouts très longs observés avec certaines paires d’items correspondaient surtout à d’anciennes sorties **`qa_ipc_smoke`** trop volumineuses (dump `ApiResponse`) ; la sortie pour **`pcb_measure_distance_session`** et **`get_pcb_item_geometry_session`** est **bornée**. Les paires **empreinte + pad enfant** restent des entrées valides sur la **mesure** ; pour des cartes extrêmes, le seul risque notable est un **coût CPU** plus élevé sur le calcul de bbox d’empreinte, pas une sémantique différente.
- **Flake IPC (R16)** : Un **`nng_dial: Connection refused`** occasionnel peut encore apparaître ; le harness applique un **retry borné** sur ce motif (`Invoke-SmokeBoundedIpcRetry`) pour les appels `qa_ipc_smoke` critiques (résolution d’URL / charge projet / probe PCB Geometry). Le résumé peut exposer **`ipc_smoke_ipc_retry_extra_total`** lorsque des réessais ont été nécessaires.

Runner/summary :

- runner : `qa/tools/ipc_tools/run_pcb_geometry_v1_targeted_probes.ps1`
- summary : `qa_pcb_geometry_v1_family_summary.txt`
- probes contractuels : `qa_pcb_measure_distance_session_contract_summary.txt`, `qa_pcb_item_geometry_session_contract_summary.txt`

Critères green :

1. `pcb_geometry_v1_family_green=true`
2. `pcb_measure_distance_ok=true`
3. `exact_blocker_if_any=N/A`
4. `pcb_measure_distance_exit_code=0`
5. `measure_item_to_item_ok=true` (probe : item–item bbox + contrats V1 conservés)
6. `measure_pad_anchor_ok=true` (probe : ancrage pad `PAD::GetPosition()` + cohérence point)
7. `negative_pad_anchor_non_pad_ok=true` (probe : `pad_anchor` sur item non-pad → `pad_anchor_requires_pad_*`)
8. `negative_invalid_item_position_ok=true` (probe : valeur `position` inconnue → `invalid_item_position_*`)
9. `measure_pad_shape_origin_ok=true` (probe : fixture **`ipc_geometry_v1`**, self **`pad_shape_origin`** + sources `item_pad_shape_origin`)
10. `measure_pad_anchor_to_shape_origin_ok=true` (probe : même fixture, distance **`pad_anchor`** → **`pad_shape_origin`** non nulle, cohérente avec **`hypot(dx_nm,dy_nm)`**)
11. `negative_pad_shape_origin_missing_layer_ok=true` (probe : `pad_shape_origin` sans `layer` → `pad_shape_origin_requires_layer_*`)
12. `negative_pad_shape_origin_non_pad_ok=true` (probe : `pad_shape_origin` sur dessin → `pad_shape_origin_requires_pad_*`)
13. `negative_invalid_layer_ok=true` (probe : `layer` non résolu → `invalid_layer_*`)
14. `pcb_item_geometry_ok=true` (probe **`targeted_pcb_item_geometry_probe.ps1`** : introspection item + pads **`ipc_geometry_v1`** + négatifs)
15. `geometry_basic_ok=true`, `geometry_pad_anchor_ok=true`, `geometry_pad_shape_origin_ok=true`, négatifs item id / not found / layer (cf. `qa_pcb_item_geometry_session_contract_summary.txt`)

Note d'intégration :

- `pcb_geometry v1` est intégrée au checkpoint MCP v2 global (priorité blocker 6).

## Checkpoint MCP v2 (séparé)

Runner/summary :

- runner : `qa/tools/ipc_tools/run_mcp_v2_checkpoint.ps1`
- summary : `qa_mcp_v2_checkpoint_summary.txt`

Champs summary attendus :

1. `mcp_v2_checkpoint_green=true|false`
2. `sch_geometry_v2_ok=true|false`
3. `sch_authoring_v2_ok=true|false`
4. `sch_wiring_v2_ok=true|false`
5. `sch_symbol_editing_v2_ok=true|false`
6. `pcb_inspection_v2_ok=true|false`
7. `pcb_geometry_v1_ok=true|false`
8. `pcb_clearance_context_v1_ok=true|false` (**R33**)
9. `pcb_nearby_items_context_v1_ok=true|false` (**R39**)
10. `exact_blocker_if_any=N/A|...`
11. `sch_geometry_v2_target_run_dir=...`
12. `sch_authoring_v2_target_run_dir=...`
13. `sch_wiring_v2_target_run_dir=...`
14. `sch_symbol_editing_v2_target_run_dir=...`
15. `pcb_inspection_v2_target_run_dir=...`
16. `pcb_geometry_v1_target_run_dir=...`
17. `pcb_clearance_context_v1_target_run_dir=...` (**R33**)
18. `pcb_nearby_items_context_v1_target_run_dir=...` (**R39**)
19. `sch_geometry_v2_exit_code=...`
20. `sch_authoring_v2_exit_code=...`
21. `sch_wiring_v2_exit_code=...`
22. `sch_symbol_editing_v2_exit_code=...`
23. `pcb_inspection_v2_exit_code=...`
24. `pcb_geometry_v1_exit_code=...`
25. `pcb_clearance_context_v1_exit_code=...` (**R33**)
26. `pcb_nearby_items_context_v1_exit_code=...` (**R39**)
27. `ipc_smoke_ipc_retry_extra_total_max_across_families=...` (R24 + **R33** + **R39** : max des compteurs retry IPC agrégés depuis les résumés familiaux copiés ; motifs **`ipc_smoke_ipc_retry_extra_total*`**, **`ipc_smoke_ipc_retry_extra_family`**, etc.)
28. `family_summary_files_in_checkpoint_dir=...` (liste des noms de fichiers `qa_*_family_summary.txt` attendus, inclut **`qa_pcb_clearance_context_v1_family_summary.txt`** et **`qa_pcb_nearby_items_context_v1_family_summary.txt`**)

**Reproduction baseline** : voir `qa/tools/ipc_tools/README_IPC_QA_BASELINE.md` ; **gate officiel** = **`run_mcp_v2_checkpoint.ps1`** (8 familles **R39**) ; **préflight local** = **`run_mcp_v2_baseline_preflight.ps1`** (**R34** + garde-fou PATH **R86** + **`pcb_negative_contracts_v1`** **R66** : ne duplique pas clearance/nearby — le checkpoint les inclut ; résumé **`qa_mcp_v2_baseline_preflight_summary.txt`**) ; **pipeline CI** = **`run_mcp_v2_ci_baseline.ps1`** (garde-fou PATH **R87** + garde-fou contrats **R65**).

Note : **huit** familles sont agrégées (**R33** + **R39**) ; `SCH Symbol Editing v2` regroupe 3 tools stables (`schematic_set_symbol_field_session`, `schematic_set_symbol_value_session`, `schematic_set_symbol_reference_session`) ; `PCB Inspection v2` regroupe 3 tools stables (`pcb_get_board_items_session`, `pcb_get_board_item_by_id_session`, `pcb_get_net_items_session`) ; `pcb_geometry v1` regroupe 2 tools stables (`pcb_measure_distance_session`, `get_pcb_item_geometry_session`) — release produit documentée **PCB Geometry v1.3** (**R27**) ; **`pcb_clearance_context v1`** regroupe 1 tool stable (**`get_pcb_clearance_context_session`**, **PCB Clearance Context v1** **R32**, gate **R33**) — **context-only**, **`is_true_drc=false`** ; **`pcb_nearby_items_context v1`** regroupe 1 tool stable (**`get_pcb_nearby_items_context_session`**, **PCB Nearby Items Context v1** **R38**, gate **R39**) — **context-only**, **`is_true_drc=false`**.

Critères green :

1. `mcp_v2_checkpoint_green=true`
2. `sch_geometry_v2_ok=true`
3. `sch_authoring_v2_ok=true`
4. `sch_wiring_v2_ok=true`
5. `sch_symbol_editing_v2_ok=true`
6. `pcb_inspection_v2_ok=true`
7. `pcb_geometry_v1_ok=true`
8. `pcb_clearance_context_v1_ok=true` (**R33**)
9. `pcb_nearby_items_context_v1_ok=true` (**R39**)
10. `exact_blocker_if_any=N/A`

Format blocker :

- priorité 1 (`SCH Geometry v2`) : `exact_blocker_if_any=sch_geometry_v2:<blocker>`
- priorité 2 (`SCH Authoring v2`) : `exact_blocker_if_any=sch_authoring_v2:<blocker>`
- priorité 3 (`SCH Wiring v2`) : `exact_blocker_if_any=sch_wiring_v2:<blocker>`
- priorité 4 (`SCH Symbol Editing v2`) : `exact_blocker_if_any=sch_symbol_editing_v2:<blocker>`
- priorité 5 (`PCB Inspection v2`) : `exact_blocker_if_any=pcb_inspection_v2:<blocker>`
- priorité 6 (`pcb_geometry v1`) : `exact_blocker_if_any=pcb_geometry_v1:<blocker>`
- priorité 7 (`pcb_clearance_context v1`, **R33**) : `exact_blocker_if_any=pcb_clearance_context_v1:<blocker>`
- priorité 8 (`pcb_nearby_items_context v1`, **R39**) : `exact_blocker_if_any=pcb_nearby_items_context_v1:<blocker>`

## Chemin runtime supporté

Chemin supporté/gating MCP :

- lancement projet : `kicad.exe <project>`
- automation : `KICAD_IPC_AUTOMATION=1`
- ouverture éditeur via MCP : `qa_open_editor_frame`

## Documentation disponible

- Manifeste officiel : `qa/tools/ipc_tools/MCP_V1_OFFICIAL_MANIFEST.md`
- Guide utilisateur/dev : `qa/tools/ipc_tools/MCP_KICAD_USER_DEV_GUIDE.md`

## Dettes restantes

- `--frame` direct (`kicad.exe --frame sch <file.kicad_sch>`) : classé diagnostic-only / non-gating.
- Warning build non bloquant observé : `C5246` dans `pcbnew/api/api_handler_pcb.cpp`.
- Intégration CI future possible pour renforcer les portes sur runners légers.

## Prochaine phase possible

- Ouvrir une nouvelle capability MCP contrôlée (lot borné, read-only prioritaire).
- Durcir la CI autour des summaries familiaux Simulation v1.1 / Library v1 / SCH Connectivity v1 / SCH Actions v1.
- Traiter séparément la correction open-frame (flux `--frame`) dans un lot dédié, hors chemin supporté MCP courant.
