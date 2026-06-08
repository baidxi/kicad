# Baseline QA IPC / MCP v2 (Windows)

Ce document décrit comment reproduire un **checkpoint MCP v2 vert** sur une machine propre ou en CI, après **R24** (durcissement baseline).

## 1. Scripts QA — classification

### Officiel baseline (à utiliser pour valider MCP v2)

| Script | Rôle |
|--------|------|
| `run_mcp_v2_checkpoint.ps1` | Agrège les **neuf** familles MCP v2 + résumé global `qa_mcp_v2_checkpoint_summary.txt` (**R33** : **`pcb_clearance_context_v1`** ; **R39** : **`pcb_nearby_items_context_v1`** ; **R126** : **`pcb_drc_violations_context_v1`**). |
| **`run_mcp_v2_ci_baseline.ps1`** | Pipeline baseline MCP v2 **bout-en-bout** (toolchain, build optionnel, smoke KIFACE, probes pcb_geometry, checkpoint) + **`qa_mcp_v2_ci_baseline_summary.txt`** (**R28**). **Gate dev** — G4/G5/G6 advisory **non bloquant** pour `ci_baseline_green` (**R168/R179**). |
| **`run_mcp_v2_release_pack.ps1`** | **Commande officielle release MCP v2** (**R185**) : CI baseline + R180 + R181 + export pack R182/R183 ; **`qa_mcp_v2_release_pack_pipeline_summary.txt`** ; exit **0** = `release_decision=go` uniquement. Détail **`MCP_DECISIONS.md`** R184–R185. |
| `run_sch_geometry_v2_targeted_probes.ps1` | Famille SCH Geometry v2. |
| `run_sch_authoring_v2_targeted_probes.ps1` | Famille SCH Authoring v2. Probe **`targeted_schematic_place_symbol_probe.ps1`** : retry IPC borné post-mutation (**R101**) ; restart cleanup-only si KiCad exit post-mutation (**R102**). |
| `run_sch_wiring_v2_targeted_probes.ps1` | Famille SCH Wiring v2. |
| `run_sch_symbol_editing_v2_targeted_probes.ps1` | Famille SCH Symbol Editing v2. |
| `run_pcb_inspection_v2_targeted_probes.ps1` | Famille PCB Inspection v2. |
| `run_pcb_geometry_v1_targeted_probes.ps1` | Famille **pcb_geometry** (**PCB Geometry v1.3** **R27**) : agrège **deux** probes — mesure + introspection item. |
| `run_pcb_clearance_context_v1_targeted_probes.ps1` | Famille **pcb_clearance_context v1** (**PCB Clearance Context v1**, **R32** / intégration gate **R33**) : **`get_pcb_clearance_context_session`** — **intégrée** à **`run_mcp_v2_checkpoint.ps1`** (sémantique **context-only** inchangée : **`is_true_drc=false`**, pas `RunTests`). |
| `run_pcb_nearby_items_context_v1_targeted_probes.ps1` | Famille **pcb_nearby_items_context v1** (**PCB Nearby Items Context v1**, **release R38**, gate checkpoint **R39**) : **`get_pcb_nearby_items_context_session`** — **intégrée** au gate **`run_mcp_v2_checkpoint.ps1`** (**huit** familles) ; exécution **isolée** possible pour diagnostic ; rebuild **`kicommon`** / **`pcbnew_kiface`** si handlers MCP PCB modifiés. |
| `run_pcb_drc_violations_context_v1_targeted_probes.ps1` | Famille **pcb_drc_violations_context v1** (**R124–R126**) : **`get_pcb_drc_violations_context_session`** — **intégrée** au gate **`run_mcp_v2_checkpoint.ps1`** (**neuf** familles, **R126**) ; fixture **`ipc_drc_v1`**. |
| `targeted_pcb_drc_violations_context_probe.ps1` | Probe contractuel **`get_pcb_drc_violations_context_session`** (liste violations DRC bornée — dual-mode `run_drc`). |
| `targeted_pcb_measure_distance_probe.ps1` | Probe contractuel **`pcb_measure_distance_session`** (distance). |
| `targeted_pcb_item_geometry_probe.ps1` | Probe contractuel **`get_pcb_item_geometry_session`** (introspection géométrique, **pas** une distance). |
| `targeted_pcb_clearance_context_probe.ps1` | Probe contractuel **`get_pcb_clearance_context_session`** (contexte clearance / nets — **pas** DRC). |
| `targeted_pcb_nearby_items_context_probe.ps1` | Probe contractuel **`get_pcb_nearby_items_context_session`** (liste de voisins — **pas** DRC). |
| **`run_pcb_negative_contracts_v1_targeted_probes.ps1`** | Famille **pcb_negative_contracts v1** (**R65**) : garde-fou contrats **`board_filename`** / **`max_items`** — **intégrée** au **CI baseline** uniquement (pas au checkpoint **8 familles**). |
| **`targeted_pcb_negative_contracts_probe.ps1`** | Probe contractuel négatif **`board_filename`** (legacy **21 tools** : 6 R64 + 3 R68 + 4 R71 + 4 R73 appearance/origins + 4 R75 CRUD/set origin) ; session **3 tools** ; **`max_items`** Inspection v2. |

**Note (R38 / R39)** : les scripts familles **PCB** qui démarrent **`kicad.exe`** (**`run_pcb_geometry_v1_*`**, **`run_pcb_clearance_context_v1_*`**, **`run_pcb_inspection_v2_*`**, **`run_pcb_nearby_items_context_v1_*`**, etc.) ne doivent **pas** être lancés **en parallèle** sur la même machine (interférences observées). Le checkpoint **`run_mcp_v2_checkpoint.ps1`** enchaîne les familles **séquentiellement** (**huit** familles **R39**). Pour une QA **isolée**, enchaîner **manuellement** une famille à la fois.

### Officiel diagnostic / repro (ne remplacent pas le checkpoint complet)

| Script | Rôle |
|--------|------|
| `targeted_pcb_measure_distance_r15_repro.ps1` | Repro timeouts / harness `pcb_measure_distance` (historique R15). |
| `r21_verify_ipc_geometry_fixture.ps1` | Vérification smoke fixture `ipc_geometry_v1` (R21). |
| `targeted_qa_open_editor_frame_runtime_diag.ps1` | Diagnostic ouverture éditeur. |
| `diag_schematic_net_inspect_session_timeout.ps1` | Timeout session réseau SCH. |
| `targeted_pcb_get_board_items_probe.ps1`, `targeted_pcb_get_board_item_by_id_probe.ps1`, `targeted_pcb_get_net_items_probe.ps1` | Probes unitaires PCB Inspection (appelés par le runner famille). |
| `targeted_schematic_*.ps1` | Probes SCH (appelés par les runners familles). |
| **`targeted_library_footprint_search_probe.ps1`** | Probe **`library_footprint_search_session`** — happy path + négatifs **`board_filename`** (**R78** ; ordre path **après** `query` — diffère des legacy R61–R74). |
| **`targeted_library_footprint_inspect_probe.ps1`** | Probe **`library_footprint_inspect_session`** — happy path + négatifs **`board_filename`** (**R78** ; trim ASCII, mismatch **`makeInspectResult`** structuré, pas **`IsAbsolute`**). |

### Autres runners MCP v1 / IPC v1 (hors agrégat MCP v2)

`run_mcp_v1_checkpoint.ps1`, `run_ipc_v1_windows.ps1`, **`run_library_v1_targeted_probes.ps1`** (agrège symbol + footprint ; **R82/R83** : gating **`footprint_search_ok`** + négatifs **`footprint_search_board_filename_negative_green`** + **`footprint_inspect_board_filename_negative_green`** ; champs advisory invalid-path/wx conservés), `run_simulation_v1_1_targeted_probes.ps1`, `run_sch_connectivity_v1_targeted_probes.ps1`, `run_sch_actions_v1_targeted_probes.ps1`, etc. — utiles pour d’autres profils, **pas** dans `run_mcp_v2_checkpoint.ps1`.

### Harness regression (hors checkpoint MCP v2)

| Script | Rôle |
|--------|------|
| **`test_ipc_qa_path_idempotence.ps1`** | **R85** — vérifie que **`Initialize-IpcQaEnvironment`** n’accumule pas les préfixes build dans **`$env:PATH`** (appels répétés + **`Test-IpcKifaceDllLoad`**) ; résumé **`qa_ipc_path_idempotence_summary.txt`**. Appelé via **`Invoke-IpcPathIdempotenceGuard`** (**`ipc_qa_common.ps1`**, **R88**) depuis préflight (**R86**) et CI baseline (**R87**). **Hors** checkpoint MCP v2. |
| **`report_sch_authoring_advisory.ps1`** | **R105** — lecture **read-only** des champs advisory **`sch_authoring_place_symbol_*`** depuis **`qa_mcp_v2_checkpoint_summary.txt`** (dernier **`*-mcp-v2-checkpoint`** ou **`-CheckpointRunDir`**) ; **`Write-Warning`** si seuils R104 dépassés ; résumé **`qa_sch_authoring_advisory_report_summary.txt`** sous **`…-sch-authoring-advisory-report`** ; **exit 0** même avec warnings ; **non intégré** au gate checkpoint. |
| **`Invoke-IpcQaChildScript`** (**`ipc_qa_common.ps1`**, **R89**) | Spawn PowerShell commun pour scripts enfants QA — capture stdout/stderr/exit/**`TARGET_RUN_DIR`** ; utilisé par wrappers préflight/CI baseline, **`Invoke-IpcPathIdempotenceGuard`**, **tous** les runners ciblés (**R90–R97**) et **`run_mcp_v2_checkpoint.ps1`** (**`Invoke-FamilyRunner`**, **R99** ; **`StdoutAndStderr`**, **`ExtraArgs`** ex. wiring **`-MinimalPayload`** dans le runner enfant). |

```powershell
& qa\tools\ipc_tools\test_ipc_qa_path_idempotence.ps1 -BuildDir build\msvc-win64-release
# Optionnel : -InitRepeatCount 10
```

### Historique / CI helpers

| Script | Rôle |
|--------|------|
| `build_ipc_targets_with_vsenv.ps1` | Build batch IPC (VsDevCmd + cmake preset). |
| `build_one_target_with_vsenv.ps1` | Build une cible avec environnement MSVC. |
| **`run_mcp_v2_ci_baseline.ps1`** | **Pipeline baseline MCP v2** (pré-check processus, toolchain MSVC, build minimal optionnel, smoke KIFACE, garde-fou PATH **`test_ipc_qa_path_idempotence.ps1`** **R87**, probes **`pcb_measure_distance`** + **`get_pcb_item_geometry`**, famille **`pcb_geometry_v1`**, famille **`pcb_negative_contracts_v1`** **R65**, checkpoint MCP v2) — **R28** ; résumé **`qa_mcp_v2_ci_baseline_summary.txt`**. |
| `run_open_frame_ipc_smoke.ps1` | Smoke open-frame (non-gating). |
| `generate_mcp_tool_map_r1.py` | Régénère `MCP_TOOL_MAP_R1.tsv`. |

### À éviter pour un « baseline vert » sans contexte

- Appels manuels à `kicad.exe --frame` pour du gating (documenté comme diagnostic-only dans le guide).
- Relancer uniquement un sous-probe sans le runner famille si la politique équipe exige le résumé familial.

### Artefacts R7–R23

- **`MCP_TOOL_MAP_R1.tsv`** : **conserver** ; régénérer après changements d’outils dans `mcp_executor.cpp`.
- **Scripts `r21_*`**, **fixtures `qa/data/pcbnew/geometry/ipc_geometry_v1`** : **conserver** ; documentés ici et dans `MCP_DECISIONS.md`.
- **Dossiers `ipc-v1-artifacts\`** sous le build tree : journaux d’exécution ; **ne pas supprimer** dans le dépôt (générés localement). Aucun script `r8_*` dans ce dossier à date — les diagnostics LoadLibrary historiques sont couverts par **`Test-IpcKifaceDllLoad`** (`KICAD_IPC_KIFACE_SMOKE=1`).

## 2. Prérequis build (cohérence `kicommon` / `_pcbnew.dll`)

Après toute modification dans **`common/mcp/`** ou **`pcbnew/api/`**, **relinker** au minimum **`pcbnew_kiface`** (produit `pcbnew/_pcbnew.dll`) pour éviter un **artefact stale** (symptômes : erreurs d’import, MCP incohérent, R8-like). **Fermer toutes les instances de `kicad.exe`** (et **`qa_ipc_smoke.exe`** si besoin) avant le link — sinon **`LNK1104`** (*cannot open `_pcbnew.dll`*) ; si le link échoue, supprimer l’objet **`api_handler_pcb.cpp.obj`** et recompiler sous **`vcvars64`** / **VsDevCmd**.

### Commande minimale recommandée (MSVC, preset `win64-release`)

Depuis la **racine du dépôt** (où se trouve le preset CMake), avec **VsDevCmd** (équivalent pratique à **`vcvars64.bat`** pour INCLUDE/LIB/PATH) :

```bat
"%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=amd64 ^

&& cmake --build --preset win64-release --target kicommon pcbnew_kiface kicad qa_ipc_smoke
```

Variante équivalente : **`vcvars64.bat`** puis la même ligne **`cmake --build`** (répertoire de build défini par le preset).

Helper PowerShell : `qa/tools/ipc_tools/build_one_target_with_vsenv.ps1` (une cible) ou **`run_mcp_v2_baseline_preflight.ps1 -BuildMinimal`** (enchaîne les quatre cibles ci-dessus).

Binaires attendus dans le build tree (ex. `build/msvc-win64-release`) :

- `common/kicommon.dll`
- `pcbnew/_pcbnew.dll`
- `kicad/kicad.exe`
- `qa/tools/ipc_tools/qa_ipc_smoke.exe`

## 3. Variables d’environnement (échappement PowerShell)

- **`KICAD_IPC_KIFACE_SMOKE=1`** : active le smoke **`LoadLibrary`** sur `_pcbnew.dll` / `_eeschema.dll` au début de **`run_mcp_v2_checkpoint.ps1`** (et utilisable seul avec **`Test-IpcKifaceDllLoad`** après **`Initialize-IpcQaEnvironment`**).
- Définir en PowerShell : `$env:KICAD_IPC_KIFACE_SMOKE = '1'` puis appeler **`run_mcp_v2_checkpoint.ps1`** **dans la même session** (un `cmd /c "set … && powershell …"` peut **ne pas** transmettre la variable au processus attendu).

## 4. Préflight baseline (**`run_mcp_v2_baseline_preflight.ps1`**, rôle **R34**)

Ce script **n’est pas** le même objet que le pipeline CI baseline : c’est un **enchaînement local court** (diagnostic / iter rapide), puis délégation au **gate officiel**.

| Script | Rôle |
|--------|------|
| **`run_mcp_v2_checkpoint.ps1`** | **Gate MCP v2** : **9 familles** (**R39** + **R126** `pcb_drc_violations_context_v1`) — référence **mcp_v2_checkpoint_green**. |
| **`run_mcp_v2_ci_baseline.ps1`** | **Baseline reproductible** (pré-checks processus, toolchain MSVC, build optionnel, smoke, probes **mesure + item_geometry**, famille **pcb_geometry_v1**, famille **pcb_negative_contracts_v1** **R65**, checkpoint) + **`qa_mcp_v2_ci_baseline_summary.txt`**. |
| **`run_mcp_v2_baseline_preflight.ps1`** | **Préflight local** : binaires (option) ; smoke KIFACE (si **`KICAD_IPC_KIFACE_SMOKE=1`**) ; garde-fou PATH **`test_ipc_qa_path_idempotence.ps1`** (**R86**) ; **fail-fast** **`pcb_negative_contracts_v1`** (**R66**) puis **`pcb_measure_distance`** + famille **`pcb_geometry_v1`** ; puis **`run_mcp_v2_checkpoint.ps1`** sauf **`-SkipCheckpoint`**. **Ne duplique pas** clearance / nearby / les autres familles du gate : elles sont exécutées **dans** le checkpoint (**8 familles** **R39**). |

Résumé généré par le préflight : **`qa_mcp_v2_baseline_preflight_summary.txt`** dans le dossier dont le chemin est imprimé à la fin du script (**ligne `TARGET_RUN_DIR=...`**).

Options :

- **`-VerifyBinariesOnly`** : vérifie la présence des DLL/exe critiques sans builder ; **sortie** `BASELINE_BINARIES_OK=1`, **pas** de résumé préflight long ; **n’exécute pas** le garde-fou PATH R86 (convention : quitte avant init env / probes).
- **`-BuildMinimal`** : enchaîne **`kicommon`**, **`pcbnew_kiface`**, **`kicad`**, **`qa_ipc_smoke`** via VsDevCmd.
- **`-SkipCheckpoint`** : s’arrête après les étapes PCB (pas d’appel au gate 8 familles) — utile pour un test **très** court.

Par défaut (sans **`-SkipCheckpoint`**) : smoke KIFACE (si env) → **`test_ipc_qa_path_idempotence.ps1`** (**R86**) → **`run_pcb_negative_contracts_v1_targeted_probes.ps1`** (**R66**) → **`targeted_pcb_measure_distance_probe.ps1`** → **`run_pcb_geometry_v1_targeted_probes.ps1`** → **`run_mcp_v2_checkpoint.ps1`** (même variable **`KICAD_IPC_KIFACE_SMOKE`** héritée par le processus enfant du checkpoint).

Pour **seulement** le checkpoint complet sans étapes « pré » : **`run_mcp_v2_checkpoint.ps1`** directement.

## 4bis. Pipeline CI baseline MCP v2 (**`run_mcp_v2_ci_baseline.ps1`**, **R28**)

Script unique prévu pour une **reproduction locale** et une **future CI** : enchaîne explicitement les étapes **A→G6** (pré-check **`kicad.exe` / `qa_ipc_smoke.exe`** ; vérification **`cl` / `cmake` / `ninja`** sous VsDevCmd ; build minimal optionnel ; **`Test-IpcKifaceDllLoad`** ; **`test_ipc_qa_path_idempotence.ps1`** **R87** ; **`targeted_pcb_measure_distance_probe.ps1`** ; **`targeted_pcb_item_geometry_probe.ps1`** ; **`run_pcb_geometry_v1_targeted_probes.ps1`** ; **`run_pcb_negative_contracts_v1_targeted_probes.ps1`** **R65** ; **`run_mcp_v2_checkpoint.ps1`** avec transmission de **`KICAD_IPC_KIFACE_SMOKE=1`** au processus enfant lorsque **`EnableKifaceSmoke`** est vrai ; **`report_sch_authoring_advisory.ps1`** checkpoint unique **R106** puis scan **`-ScanLastNCheckpoints`** (**`-SchAuthoringAdvisoryScanLastN`**, défaut **10**, **R108/R110**) ; **`r163_example_runtime_selected_footprint_workflow.ps1`** (**R164**) ; **`report_footprint_workflow_discovery_consistency.ps1`** (**R167**) ; **`r177_example_pcb_apply_drc_save_workflow.ps1`** (**R178**) — observability-only, **hors** **`ci_baseline_green`**). La famille **`pcb_negative_contracts_v1`** est un **garde-fou CI baseline** — **pas** une **9e** famille du checkpoint (**8 familles** inchangées).

Paramètres utiles :

| Paramètre | Effet |
|-----------|--------|
| **`-BuildDir`** (obligatoire) | Répertoire de build (ex. `build/msvc-win64-release`). |
| **`-BuildMinimal`** | Lance **`cmake --build --preset win64-release`** pour **`kicommon`**, **`pcbnew_kiface`**, **`kicad`**, **`qa_ipc_smoke`** (nécessite VsDevCmd dans le script). |
| **`-SkipBuild`** | Ne compile pas (s’appuie sur un build déjà à jour). |
| **`-KillRunningKiCad`** | Arrête **`kicad`** / **`qa_ipc_smoke`** si encore actifs (sinon le script **échoue** pour éviter **`LNK1104`** ou IPC flaky). |
| **`-SkipCheckpoint`** | S’arrête avant **`run_mcp_v2_checkpoint.ps1`** (diagnostic plus court). |
| **`-EnableKifaceSmoke`** | Par défaut **`$true`** : smoke DLL au début du run **et** dans le checkpoint enfant. **`$false`** pour désactiver (résumé : `kiface_smoke_ok=skipped`). |
| **`-VerifyOnly`** | Vérifie toolchain MSVC + binaires critiques + smoke KIFACE (si activé) + garde-fou PATH **R87** après init env ; **ne lance pas** les probes PCB, le checkpoint, ni les reports advisory (**`sch_authoring_advisory_*=N/A`**, **`sch_authoring_advisory_scan_*=N/A`**, **`sch_authoring_advisory_scan_last_n=N/A`**, **`runtime_footprint_workflow_*=N/A`**, **`footprint_workflow_discovery_consistency_*=N/A`**, **`pcb_apply_drc_save_workflow_*=N/A`**). |
| **`-SchAuthoringAdvisoryScanLastN`** | **R110** — nombre de checkpoints MCP v2 scannés en G3 (défaut **10**) ; passé à **`report_sch_authoring_advisory.ps1 -ScanLastNCheckpoints`** ; **≥ 1** obligatoire (refus **`<= 0`**) ; résumé CI : **`sch_authoring_advisory_scan_last_n`** ; **hors** **`ci_baseline_green`**. |

**Toolchain MSVC (R69)** : le check exécute **`cmd.exe /c "set PATH=%SystemRoot%\System32;%PATH% && call VsDevCmd.bat -arch=amd64 && where.exe cl && …"`** — nécessaire quand le shell parent (ex. Cursor) n’injecte pas **System32** dans le subprocess. Log détaillé : **`toolchain_verify.log`** ; summary : **`toolchain_missing_binary`** si échec.

Résumé généré : **`qa_mcp_v2_ci_baseline_summary.txt`** dans le dossier **`…-mcp-v2-ci-baseline`** (chemins des résumés contractuels agrégés). **Exit 0** si tout vert ; **exit 1** sinon.

### Politique minimale d’artefacts CI (**R41**)

Pour qu’un run soit **exploitable hors machine** (ticket, CI, repro), archiver au minimum le dossier dont le chemin est imprimé en fin de script (**`TARGET_RUN_DIR=…`**) — c’est **`artifact_root`** / **`target_run_dir`** dans **`qa_mcp_v2_ci_baseline_summary.txt`**.

| Fichier | Emplacement typique | Rôle |
|---------|---------------------|------|
| **`qa_mcp_v2_ci_baseline_summary.txt`** | racine du run CI baseline | Agrégat pipeline : **`ci_elapsed_ms`**, **`build_elapsed_ms`**, **`pcb_negative_contracts_v1_ok`**, **`checkpoint_elapsed_ms`** (lu depuis l’enfant), **`checkpoint_summary_path`**, **`checkpoint_target_run_dir`**, **`mcp_v2_checkpoint_green`**, **`exact_blocker_if_any`**, champs observability **`sch_authoring_advisory_*`** (**R106**) et **`sch_authoring_advisory_scan_*`** (**R108**, non gating), **`runtime_footprint_workflow_*`** (**R164**, non gating), **`footprint_workflow_discovery_consistency_*`** (**R167**, non gating), etc. |
| **`qa_sch_authoring_advisory_report_summary.txt`** | sous **`sch_authoring_advisory_report_target_run_dir`** (**`…-sch-authoring-advisory-report`**) | Report advisory post-checkpoint (**R105/R106**) ; **`advisory_warning_emitted`**, **`recommended_action`** — **ne modifie pas** **`ci_baseline_green`**. |
| **`qa_mcp_v2_checkpoint_summary.txt`** | sous **`checkpoint_target_run_dir`** (run **`…-mcp-v2-checkpoint`**) | Gate **8 familles** : **`checkpoint_elapsed_ms`**, **`checkpoint_started_at` / `checkpoint_finished_at`**, **`sch_*_elapsed_ms`**, **`pcb_*_elapsed_ms`**, **`ipc_smoke_ipc_retry_extra_total_max_across_families`**, **`ipc_smoke_ipc_retry_extra_max_*`** (max par famille) ; télémétrie advisory **`sch_authoring_place_symbol_*`** (**R103**, non bloquante). |
| **`qa_*_family_summary.txt`** (×8) | copiés **dans** le dossier checkpoint + dans chaque **`TARGET_RUN_DIR`** famille | Détail par famille ; retries IPC par famille. |
| **`qa_kiface_dll_smoke.txt`** | racine checkpoint si **`KICAD_IPC_KIFACE_SMOKE=1`** | Résultat **`LoadLibrary`** KIFACE. |
| **Logs stdout/stderr** des runners | même dossier que le summary qui les référence (`*.stdout.log` / `*.stderr.log`) | Diagnostic ; en cas d’échec, prioriser le log du runner dont **`exact_blocker_if_any`** cite la famille. |

Les chemins absolus vers le checkpoint enfant et les probes sont déjà listés dans **`qa_mcp_v2_ci_baseline_summary.txt`** (**`checkpoint_summary_path`**, **`pcb_measure_summary_path`**, etc.).

Commande locale typique (build déjà fait) :

```powershell
.\qa\tools\ipc_tools\run_mcp_v2_ci_baseline.ps1 `
  -BuildDir "c:\dev\kicad-master\build\msvc-win64-release" `
  -SkipBuild -KillRunningKiCad
```

Pré-check machine / agent CI sans QA longue :

```powershell
.\qa\tools\ipc_tools\run_mcp_v2_ci_baseline.ps1 -BuildDir "<build>" -VerifyOnly -KillRunningKiCad
```

**Différence avec `run_mcp_v2_baseline_preflight.ps1`** : le CI baseline vérifie en plus la **toolchain MSVC** (`cl` / `cmake` / `ninja`), peut **tuer** les processus KiCad, exécute **`targeted_pcb_item_geometry_probe.ps1`** en plus de la mesure, et écrit **`qa_mcp_v2_ci_baseline_summary.txt`**. Le préflight (**§4**, **R34** + **`pcb_negative_contracts_v1`** **R66**) est volontairement **plus léger** : pas de pre-check processus/MSVC, garde-fou contrats négatifs, une probe mesure (la famille **pcb_geometry** relance mesure + item-geometry en interne) ; le **gate 8 familles** (**R39**) reste **`run_mcp_v2_checkpoint.ps1`** à l’intérieur des deux chemins.

## 5. Famille **`pcb_clearance_context_v1`** (PCB Clearance Context v1 — gate **R33**)

Cette famille valide **`get_pcb_clearance_context_session`** (contexte clearance **read-only** — **pas** DRC complète, **`is_true_drc=false`**, **pas** `RunTests`, **pas** rapport de violations, **pas** verdict DRC officiel). Depuis **R33**, elle fait partie des familles agrégées par **`run_mcp_v2_checkpoint.ps1`** (ordre PCB : **Inspection v2** → **pcb_geometry v1** → **pcb_clearance_context_v1** → **`pcb_nearby_items_context_v1`** **R39**). Une copie locale **`qa_pcb_clearance_context_v1_family_summary.txt`** est déposée dans le dossier checkpoint avec les autres résumés familiaux.

**Exécution isolée** (repro / diagnostic, même **`BuildDir`** que le baseline) :

```powershell
& "c:\dev\kicad-master\qa\tools\ipc_tools\targeted_pcb_clearance_context_probe.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release"

& "c:\dev\kicad-master\qa\tools\ipc_tools\run_pcb_clearance_context_v1_targeted_probes.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release"
```

Résumés : **`qa_pcb_clearance_context_v1_family_summary.txt`** (runner famille) et sorties du probe ciblé sous le **`TARGET_RUN_DIR`** du run.

## 5bis. Famille **`pcb_nearby_items_context_v1`** (PCB Nearby Items Context v1 — **release R38**, gate checkpoint **R39**)

Cette famille valide **`get_pcb_nearby_items_context_session`** (**contexte de voisinage PCB** — **pas** DRC, **`is_true_drc=false`**, **pas** `RunTests`, **pas** verdict clearance). Depuis **R39**, elle est **agrégée** par **`run_mcp_v2_checkpoint.ps1`** (**huit** familles officielles) ; **`qa_pcb_nearby_items_context_v1_family_summary.txt`** est **copié** dans le dossier checkpoint. Voir **`MCP_V1_CHECKPOINT_RELEASE_NOTE.md`** et **`MCP_DECISIONS.md`** **R39**.

**Exécution isolée** (diagnostic / repro, même **`BuildDir`** ; **après** rebuild **`pcbnew_kiface`** si changement handler) — **ne pas** paralléliser avec d’autres familles PCB sur la même machine :

```powershell
& "c:\dev\kicad-master\qa\tools\ipc_tools\targeted_pcb_nearby_items_context_probe.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release"

& "c:\dev\kicad-master\qa\tools\ipc_tools\run_pcb_nearby_items_context_v1_targeted_probes.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release"
```

Résumés : **`qa_pcb_nearby_items_context_v1_family_summary.txt`** (runner famille) ; **`qa_pcb_nearby_items_context_session_contract_summary.txt`** sous le **`TARGET_RUN_DIR`** du probe ciblé.

## 6. Résumé checkpoint MCP v2 (`qa_mcp_v2_checkpoint_summary.txt`)

Champs utiles (voir fichier généré sous `.../ipc-v1-artifacts/...-mcp-v2-checkpoint/`) :

- **`mcp_v2_checkpoint_green`**, **`exact_blocker_if_any`**
- **`pcb_geometry_v1_ok`**, **`pcb_inspection_v2_ok`**, **`pcb_clearance_context_v1_ok`**, **`pcb_nearby_items_context_v1_ok`** (**R39**), familles SCH `*_ok`
- **`kiface_smoke_ok`** / **`kiface_smoke_skipped`** (si smoke non demandé, `kiface_smoke_ok=N/A`)
- **`ipc_smoke_ipc_retry_extra_total_max_across_families`** : maximum des compteurs flake IPC agrégés depuis les résumés familiaux copiés (**R33** : inclut les motifs **`ipc_smoke_ipc_retry_extra_total`**, **`ipc_smoke_ipc_retry_extra_family`**, et les totaux **pcb_geometry** mesure / item-geometry).
- **R41 — observabilité** : **`checkpoint_started_at`**, **`checkpoint_finished_at`**, **`checkpoint_elapsed_ms`** ; **`sch_geometry_v2_elapsed_ms`**, … **`pcb_nearby_items_context_v1_elapsed_ms`** (durée par famille, ms) ; **`ipc_smoke_ipc_retry_extra_max_<famille>`** = max détecté dans le résumé familial copié (complète le max global ci-dessus).
- **R103 — télémétrie advisory authoring (non bloquante)** : **`sch_authoring_place_symbol_kicad_exited_post_mutation_detected`**, **`sch_authoring_place_symbol_cleanup_restart_*`**, **`sch_authoring_place_symbol_*_retry_extra_total`** — relais depuis **`qa_sch_authoring_v2_family_summary.txt`** (champs **`schematic_place_symbol_*`** du probe place_symbol).
- Copies locales des résumés familiaux : **`qa_*_family_summary.txt`** dans le même dossier checkpoint.

Détail **`ipc_smoke_ipc_retry_extra_total`** par famille : lire **`qa_pcb_geometry_v1_family_summary.txt`** (et les autres) dans ce dossier ou dans le **`TARGET_RUN_DIR`** indiqué pour chaque famille.

## 7. Flakes IPC connus

- **`nng_dial: Connection refused`** / **Connection shutdown** : retry borné côté harness (`Invoke-SmokeBoundedIpcRetry`). Si échec persistant, vérifier une seule instance KiCad / charge machine.
- **`sch_authoring_v2` / `schematic_place_symbol`** : voir **§7bis** (télémétrie advisory post-mutation ; **ne modifie pas** **`mcp_v2_checkpoint_green`**).

## 7bis. Politique d’alerte advisory SCH authoring post-mutation (**R104**)

Champs **advisory-only** (checkpoint **`qa_mcp_v2_checkpoint_summary.txt`**) — **ne participent pas** à **`mcp_v2_checkpoint_green`**, **`sch_authoring_v2_ok`**, ni **`exact_blocker_if_any`** :

| Champ checkpoint | Sens |
|------------------|------|
| **`sch_authoring_place_symbol_kicad_exited_post_mutation_detected`** | KiCad process **`HasExited`** après mutation **`schematic_place_symbol_session`** réussie (crash/exit, pas simple perte socket sur process vivant). |
| **`sch_authoring_place_symbol_cleanup_restart_attempted`** | Restart cleanup-only R102 tenté. |
| **`sch_authoring_place_symbol_cleanup_restart_ok`** | Restart + reopen IPC réussi. |
| **`sch_authoring_place_symbol_*_retry_extra_total`** | Retries harness R101 (mutation / restore / ping post-mutation). |

**Distinction des états :**

| Situation | **`exact_blocker_if_any`** (probe/famille) | **`mcp_v2_checkpoint_green`** | Advisory |
|-----------|---------------------------------------------|-------------------------------|----------|
| Probe vert, crash récupéré (R102) | **`N/A`** | **`true`** | **`kicad_exited…=true`**, cleanup **`ok=true`** possible |
| Probe rouge (échec définitif) | **`restore_connection_refused`**, **`connection_refused`**, **`dry_run_phase_not_ok`**, … | **`false`** si famille authoring KO | Retries élevés possibles |
| Checkpoint rouge | Blocker famille **`sch_authoring_v2:…`** | **`false`** | Indépendant du seul advisory |

**Seuils d’attention (pas de blocage CI/checkpoint) :**

1. **Ponctuel** — **`kicad_exited_post_mutation_detected=true`** une fois : conserver les artefacts ; **pas** d’escalade obligatoire si probe/checkpoint **verts**.
2. **Fréquence** — **≥ 2** occurrences sur **10** runs **`targeted_schematic_place_symbol_probe.ps1`**, **ou** **≥ 1** fois dans un **checkpoint CI quotidien** avec advisory **`true`** : ouvrir une **investigation crash root-cause** (eeschema / process KiCad).
3. **Incident QA** — advisory **`true`** **et** blocker final **`restore_connection_refused`** ou **`connection_refused`** (probe rouge ou checkpoint rouge) : traiter comme **incident QA** (pas seulement observabilité).

**Artefacts à conserver** (chemins sous **`build/.../ipc-v1-artifacts/`**) :

- **`…-schematic-place-symbol-targeted/`** (répertoire probe)
- **`qa_schematic_place_symbol_session_contract_summary.txt`**
- **`post_mutation_ipc_status.txt`**
- **`ipc_reconnect_*.log`**
- **`delete_sch_items_*`** (logs restore)
- **`qa_sch_authoring_v2_family_summary.txt`** (famille)
- **`qa_mcp_v2_checkpoint_summary.txt`** (checkpoint)

**Investigation future recommandée :** logs crash eeschema / Windows Event si fréquence élevée ; corrélation timings CI (**`sch_authoring_v2_elapsed_ms`**, charge amont checkpoint) ; comparaison **standalone** vs **checkpoint** vs **CI baseline**.

### Script read-only **`report_sch_authoring_advisory.ps1`** (**R105**)

Lit la télémétrie advisory depuis un checkpoint existant et émet des **warnings console** (sans fail gate) selon la politique §7bis :

```powershell
$BuildDir = "build\msvc-win64-release"
$tools = "qa\tools\ipc_tools"

# Dernier *-mcp-v2-checkpoint sous BuildDir\ipc-v1-artifacts
& "$tools\report_sch_authoring_advisory.ps1" -BuildDir $BuildDir

# Checkpoint explicite
& "$tools\report_sch_authoring_advisory.ps1" -BuildDir $BuildDir -CheckpointRunDir "<…-mcp-v2-checkpoint>"

# Scan des N derniers checkpoints MCP v2 (R107, seuil frequence R104)
& "$tools\report_sch_authoring_advisory.ps1" -BuildDir $BuildDir -ScanLastNCheckpoints 5

# Racine artefacts alternative (optionnel)
& "$tools\report_sch_authoring_advisory.ps1" -BuildDir $BuildDir -ArtifactRoot "<chemin ipc-v1-artifacts>"
```

Paramètres : **`-BuildDir`** (obligatoire), **`-CheckpointRunDir`**, **`-ArtifactRoot`**, **`-ScanLastNCheckpoints`** (mutuellement exclusif avec **`-CheckpointRunDir`**). Résumé : **`qa_sch_authoring_advisory_report_summary.txt`** (`sch_authoring_advisory_report_green=true`, **`exact_blocker_if_any=N/A`** = succès lecture ; **`advisory_warning_emitted`** = warnings R104). Mode scan : champs **`scan_mode=last_n_checkpoints`**, compteurs agrégés, **`source_checkpoint_run_dirs`**. **Exit 1** uniquement si summary checkpoint introuvable, paramètres incompatibles, ou parsing impossible.

## 7ter. Clôture release QA harness SCH authoring observability (**R100–R108**, synthèse **R109**)

Série **documentation + harness + observability-only** autour du flake **`sch_authoring_v2` / `schematic_place_symbol`** — **aucune** modification des critères **`mcp_v2_checkpoint_green`**, **`sch_authoring_v2_family_green`**, **`ci_baseline_green`**, ni des handlers C++.

| Mission | Apport |
|---------|--------|
| **R100** | Diagnostic flake : blockers **`restore_connection_refused`**, **`connection_refused`**, **`dry_run_phase_not_ok`** ; KiCad peut quitter post-mutation AS_OK. |
| **R101** | Retry/reconnect harness borné dans **`targeted_schematic_place_symbol_probe.ps1`** (dry-run, mutation, ping/readback/restore). |
| **R102** | **`Invoke-PostMutationCleanupRestart`** si **`kicad_exited_post_mutation_detected=true`**. |
| **R103** | Propagation advisory probe → famille → checkpoint (**`sch_authoring_place_symbol_*`**). |
| **R104** | Politique d’alerte (§7bis) — advisory **non gating**. |
| **R105** | Script read-only **`report_sch_authoring_advisory.ps1`**. |
| **R106** | Report checkpoint unique intégré au CI baseline (**`sch_authoring_advisory_report_*`**). |
| **R107** | Mode **`-ScanLastNCheckpoints N`** + seuils frequence R104. |
| **R108** | Scan multi-checkpoint intégré au CI baseline (**`sch_authoring_advisory_scan_*`**). |
| **R110** | **`-SchAuthoringAdvisoryScanLastN`** (défaut **10**) configurable au CI baseline ; **`sch_authoring_advisory_scan_last_n`**. |

**Statut final :** gate MCP v2 **inchangé** ; advisory **observability-only** ; CI baseline **enrichi** (G2 report + G3 scan, hors green) ; **`run_mcp_v2_checkpoint.ps1`** **inchangé** depuis R103.

**Champs importants :**

| Couche | Champs |
|--------|--------|
| Checkpoint | **`sch_authoring_place_symbol_kicad_exited_post_mutation_detected`**, **`sch_authoring_place_symbol_cleanup_restart_*`**, **`sch_authoring_place_symbol_*_retry_extra_total`** |
| CI report unique | **`sch_authoring_advisory_report_*`** |
| CI scan multi-checkpoint | **`sch_authoring_advisory_scan_*`** (dont **`_checkpoints_scanned`**, **`_kicad_exited_count`**, **`sch_authoring_advisory_scan_last_n`**, sommes retries) |

**Politique de lecture :**

| Signal | Action |
|--------|--------|
| Advisory **false/0** | Aucune action |
| Advisory **ponctuel**, run vert | Conserver artefacts probe/famille/checkpoint |
| **Fréquence élevée** (≥2/10 place_symbol, ≥2 exits / N scans, ≥1 exit/jour UTC) | Investigation crash **eeschema** / timings IPC |
| Blocker final **`restore_connection_refused`** / **`connection_refused`** | **Incident QA** (pas seulement observabilité) |

**Limites connues :**

- Checkpoints **legacy** pré-R103 : comptés **`false`/`0`** en mode scan (sous-estimation historique possible).
- Scan **N** configurable via **`-SchAuthoringAdvisoryScanLastN`** (défaut **10**, **R110**) ; **`<= 0`** refusé.
- **`-VerifyOnly`** : reports advisory **non** lancés (**`sch_authoring_advisory_*=N/A`**, **`sch_authoring_advisory_scan_*=N/A`**, **`sch_authoring_advisory_scan_last_n=N/A`**, **`runtime_footprint_workflow_*=N/A`**, **`footprint_workflow_discovery_consistency_*=N/A`**).
- L’advisory **ne remplace pas** les logs détaillés probe (**`post_mutation_ipc_status.txt`**, **`ipc_reconnect_*`**, **`delete_sch_items_*`**).

**Pistes ultérieures (hors R109/R110) :** job CI séparé observability-only ; investigation **eeschema** si seuil dépassé ; dette harness library stdout **clôturée R117** (footprint R115 + symbol audité R116).

### Audit checkpoint MCP v1 — **`run_mcp_v1_checkpoint.ps1`** (**R111**)

**Verdict R111/R112 :** migration **spawn-only** **R112** appliquée — **`Invoke-FamilyRunner`** délègue à **`Invoke-IpcQaChildScript`** (**`StdoutAndStderr`**, **`LogBaseName=$RunnerScriptName`**, **`SummaryFileName`**) ; critères / champs **`qa_mcp_v1_checkpoint_summary.txt`** inchangés.

| Élément | Constat |
|---------|---------|
| Spawn local | **`Invoke-FamilyRunner`** (~L71–166) — bloc **`ProcessStartInfo`** dupliqué |
| **`TARGET_RUN_DIR`** | **stdout + stderr** (regex sur texte combiné) → **`TargetRunDirSource=StdoutAndStderr`** |
| Logs | **`$RunnerScriptName.stdout.log`** / **`.stderr.log`** sous dossier checkpoint |
| Ordre **full** | simulation → library → sch_connectivity → sch_actions |
| Summaries copiés | **`qa_simulation_v1_1_family_summary.txt`**, **`qa_library_v1_family_summary.txt`**, **`qa_sch_connectivity_v1_family_summary.txt`**, **`qa_sch_actions_v1_family_summary.txt`** |
| Green | **`mcp_v1_*_checkpoint_green`** via **`.Ok`** (= **`exact_blocker_if_any=N/A`** + exit 0) — **pas** de lecture directe **`*_family_green`** |
| Blockers | **`Get-PrefixedFamilyBlocker`** — préfixes **`simulation_v1_1:`**, **`library_v1:`**, **`sch_connectivity_v1:`**, **`sch_actions_v1:`** |
| Retry IPC | **Aucune** agrégation checkpoint v1 |
| Hors scope R112 | **`simulation`** (agrégation artefacts, pas spawn familial) |

**R113 — runners v1 SCH actions / connectivity :** audit **`mapping_exact=true`** (pattern **R94** v2) ; migration spawn-only **`Invoke-TargetedProbe`** → **`Invoke-IpcQaChildScript`** (**`StdoutAndStderr`**, **`LogBaseName=$ProbeScriptName`**, **`SummaryFileName`**, pas d’**`ExtraArgs`**) ; **`sch_actions_v1_family_green`** / **`sch_connectivity_v1_family_green`** et contrats summary **inchangés**.

### Clôture arc spawn QA **R89–R113** + audit simulation v1.1 (**R114**)

**Synthèse factorisation spawn :**

| Mission | Périmètre migré |
|---------|-----------------|
| **R89** | Helper **`Invoke-IpcQaChildScript`** + **`Get-IpcQaChildTargetRunDir`** ; préflight + CI baseline (wrappers locaux) |
| **R90–R93** | Runners pilotes PCB : negative contracts, library, geometry, clearance, nearby, inspection v2 |
| **R94–R97** | Runners SCH v2 : geometry, authoring, wiring, symbol editing |
| **R98–R99** | Audit + migration **`Invoke-FamilyRunner`** **`run_mcp_v2_checkpoint.ps1`** |
| **R111–R112** | Audit + migration **`Invoke-FamilyRunner`** **`run_mcp_v1_checkpoint.ps1`** |
| **R113** | Runners v1 **`sch_actions`** / **`sch_connectivity`** |

Critères **`*_family_green`** / checkpoint green **inchangés** sur tout l’arc. Handlers C++ **non** modifiés.

**Audit `run_simulation_v1_1_targeted_probes.ps1` (R114) :**

| Élément | Constat |
|---------|---------|
| Spawn enfant répétitif | **Non** — aucun **`ProcessStartInfo`** / **`Invoke-TargetedProbe`** |
| Modèle | **Agrégation** de 9 summaries contrat (copie depuis **`ExistingRunDir`** ou **`Resolve-LatestArtifactPath`** sous **`ipc-v1-artifacts`**) |
| **`TARGET_RUN_DIR`** | Émis en **stdout** par le runner lui-même (pas de parse enfant) |
| Logs | Aucun log probe enfant sous le dossier familial |
| Green | **`simulation_v1_1_family_green`** = AND des 9 **`*_ok`** (lecture **`exact_blocker_if_any`** par artefact) |
| Blockers | **`missing_artifact`**, **`exact_blocker_missing`**, préfixés **`directive:`**, **`netlist_preview:`**, etc. |
| **`Invoke-IpcQaChildScript`** | **Non applicable** — pas de script enfant à spawner |
| Verdict | **`mapping_exact=false`**, **`patch_recommended=false`**, **`risk_level=low`** (statu quo) |

**Décision R114 :** arc spawn QA **clôturé** ; **pas** de migration simulation (gain nul, modèle différent). Scripts diag conservent spawn local intentionnel.

### Bornage stdout **`qa_ipc_smoke`** footprint library (**R115**)

**Audit :**

| Élément | Search | Inspect |
|---------|--------|---------|
| **`printResponse`** / dump **`ApiResponse` complet** | **Oui** (avant R115) | **Oui** |
| Résultat tool | **`out.result().Utf8DebugString()`** (regex probes) | idem (scalaires compacts) |
| Probes parsent | **`status=`**, **`error_message=`**, **`key: "search_ok"`**, **`count`**, **`items`**, **`qualified_name`**, **`exact_blocker_if_any`**, … | **`inspect_ok`**, **`footprint_found`**, **`qualified_name`**, **`pads_count`**, … |
| Taille happy path (R80) | **~4,8 KiB** (double dump) | faible |
| **`mapping_exact`** | **true** | **true** (skip ApiResponse dump suffit) |

**Patch R115 :** branche header léger (comme **`get_net_classes`**) + **`library_footprint_search_session`** (Struct trim **`items[]`** si **>5**) + **`library_footprint_inspect_session`** (Struct seul). **Handlers MCP inchangés** ; critères green inchangés.

### Audit stdout library symbol (**R116**)

**Tools MCP :** **`library_symbol_search_session`**, **`library_symbol_inspect_session`**.

| Élément | Symbol search | Symbol inspect |
|---------|---------------|----------------|
| **`printResponse`** / dump **`ApiResponse`** | **Oui** (avant greppable) | **Oui** |
| Sortie résultat bornée | **Oui** — lignes **`search_ok=`**, **`count=`**, **`items_present=`**, … (**~L997–1071**) | **Non** — **`out.result().Utf8DebugString()`** complet (**else** final) |
| Probes parsent | lignes **`key=value`** (+ regex **`source_context=`** / **`search_strategy=`** dans diagnostics) | **`inspect_ok=`**, **`symbol_found=`**, … + fallback regex **`key: "…"`** |
| Taille happy path mesurée | **~3030 B** | **~3209 B** |
| Seuil R80 footprint | ~4800 B (avant R115) | — |
| Lien **`symbol_search_tool_timeout`** | **Faible** — flake observé R115 = charge IPC / enchaînement **4 probes** ; **R81** corrige pipe ; stdout **< 4 KiB** |

**Verdict R116 :** **`mapping_exact=true`**, **`patch_recommended=false`** en urgence, **`risk_level=low`** — stdout symbol **< 4 KiB** ; flake timeout **non** relié au dump.

### Clôture dette harness library stdout (**R117**)

| Mission | Statut |
|---------|--------|
| **R115** | **Footprint** **`library_footprint_search_session`** / **`library_footprint_inspect_session`** — bornage **`ipc_smoke.cpp`** (header léger, trim **`items[]`** search) ; **`qa_ipc_smoke`** rebuild ; critères green **inchangés**. |
| **R116** | **Symbol** **`library_symbol_search_session`** / **`library_symbol_inspect_session`** — audit ; happy path **~3030 B / ~3209 B** ; **pas de patch**. |
| **R117** | **Dette harness library stdout clôturée** — pas de patch symbol ; option future documentée ci-dessous. |

**Règle future (patch symbol)** — rouvrir un patch **`ipc_smoke.cpp`** **uniquement si** :

1. stdout happy path symbol search ou inspect **> ~4096 B** (seuil R80) **mesuré** sur probe library, **ou**
2. timeout harness **`symbol_search_tool_timeout`** / **`symbol_inspect_tool_timeout`** **reproductible** et **attribué** au dump stdout (après exclusion IPC flake / **`Invoke-SmokeBounded`** R81).

Patch symbol connu si déclenché : skip **`printResponse`** + branche greppable inspect (alignement symétrique R115) ; **`mapping_exact=true`** (R116).

### Prochaine capacité produit MCP (**R118** audit → **R119** recommandé)

**Manques bloquants IA (priorisés)** : assignation footprint sur symbole SCH ; liste violations DRC PCB read-only ; mutation PCB session safe ; orchestration simulation ; contexte projet unifié.

**Recommandation R119** : **`schematic_assign_footprint_session` V1** — comble la lacune manifeste « pas d’assignation footprint automatique » sur **`schematic_place_symbol_session`** ; session SCH contrôlée (dry-run) + validation footprint library ; **hors** gate MCP v2 initial.

**Alternatives R118** : **`get_pcb_drc_violations_context_session`** (read-only) ; **`simulation_assistant_session`** (orchestration). Détail complet : **`MCP_DECISIONS.md`** R118.

### Cadrage technique assign footprint (**R119** audit → **R120** implémentation)

**Contrat V1** : `schematic_filename`, `symbol_id`, `footprint` (`LibNick:Name`), `dry_run`, `allow_overwrite`, `expected_current_footprint`, `validate_footprint` (défaut true).

**Handler** : **`SetFootprintFieldText`** + validation **`PROJECT::FootprintLibAdapter(Kiway)`** (sans frame PCB) ; sorties **`pcb_ready`**, **`footprint_validated`**.

**QA R120** : probe **`targeted_schematic_assign_footprint_probe.ps1`** ; extension **`run_sch_actions_v1_targeted_probes.ps1`** ; fixture **`issue19090`** + **`qa_resistors`**. Détail : **`MCP_DECISIONS.md`** R119 / R120.

### Implémentation assign footprint (**R120**)

Tool **`schematic_assign_footprint_session` V1** implémenté (handler SCH + callback + probe + extension **`sch_actions_v1`**) ; validation **`KIFACE_TEST_FOOTPRINT_LINK`** via **`FACE_PCB`** ; **hors** gate MCP v2. Détail : **`MCP_DECISIONS.md`** R120.

### Mini-release assign footprint (**R121**)

**Objectif** : clôturer **`schematic_assign_footprint_session` V1** comme capacité officielle SCH Actions v1 (documentation + QA checkpoint **full**), sans nouvelle feature.

**Documentation** : manifest §12, tool map, release note, guide dev §9/§10, décision R121.

**Contrat documenté** : entrée (`schematic_filename`, `symbol_id`, `footprint`, `dry_run`, `allow_overwrite`, `expected_current_footprint`, `validate_footprint`) ; sortie (`pcb_ready`, `footprint_validated`, blockers) ; limites (pas PCB, pas placement board, pas annotation auto).

**Diff vs `set_symbol_field`** : mutation générique vs validation library + sémantique IA + **`pcb_ready`**.

**Limite QA** : restore fixture **`issue19090`** avec **`validate_footprint=false`** (footprint initial absent lib QA **`qa_resistors`**).

**Hors périmètre** : handlers C++, schémas MCP, gate MCP v2 inchangés.

### Audit DRC violations PCB (**R122** audit-only)

**Objectif** : auditer **`get_pcb_drc_violations_context_session` V1** (liste violations DRC bornée read-only pour IA) — **sans** implémentation.

**Constats clés** : marqueurs **`PCB_MARKER`** **non persistés** dans `.kicad_pcb` ; API **`DRC_ENGINE::RunTests`** + **`RC_ITEM::GetJsonViolation`** réutilisables ; pattern **`WriteDRCReport`** (collecte sans commit) préféré à **`DRC_TOOL::RunTests`** (mutation marqueurs).

**V1 recommandée** : tool **`get_pcb_drc_violations_context_session`** ; `run_drc=false` (marqueurs session) / `run_drc=true` (DRC engine, **`is_official_drc=true`**) ; fixture **`ipc_drc_v1`** avant implémentation (**R123**).

Détail complet : **`MCP_DECISIONS.md`** R122.

### Fixture DRC **`ipc_drc_v1`** (**R123**)

**Objectif** : plateau PCB minimal (2 pads proches, nets distincts, clearance violée) pour futur **`get_pcb_drc_violations_context_session`**.

**Chemin** : **`qa/data/pcbnew/drc/ipc_drc_v1/`** ; verify **`r123_verify_ipc_drc_fixture.ps1`** ; **ne valide pas** une DRC officielle MCP.

Détail : **`MCP_DECISIONS.md`** R123 ; **`README_ipc_drc_v1.md`**.

### Famille **`pcb_drc_violations_context_v1`** (**R124** impl., **R126** gate checkpoint)

**Objectif** : valider **`get_pcb_drc_violations_context_session`** — **intégrée** au checkpoint MCP v2 comme **9e** famille (**R126**), après **`pcb_nearby_items_context_v1`**.

**Fixture** : **`qa/data/pcbnew/drc/ipc_drc_v1/`** (copiée vers **`build/.../ipc-drc-v1-proj/`**).

**Contrat QA (résumé)** :

| Mode | Attendu probe |
|------|---------------|
| `run_drc=false` | `drc_source=existing_markers`, `is_official_drc=false`, `drc_was_run=false` ; 0 violation = OK |
| `run_drc=true` | ≥1 violation type **`clearance`**, nets **`QA_DRC_NET_A`** / **`QA_DRC_NET_B`**, `is_official_drc=true`, `drc_was_run=true` |
| `max_violations=1` | `returned_count=1`, `truncated` cohérent |

**Types observés sur fixture** (engine, **`run_drc=true`**) : depuis **R127**, cible **`clearance`** uniquement (`violations_count=1`) ; avant R127 : `clearance`, `invalid_outline`, `lib_footprint_issues` (4 total).

**Limites documentées** : pas parité GUI DRC ; pas persistance marqueurs ; pas mutation board ; `run_drc=true` lent sur grands boards ; `test_footprints=false` par défaut.

**Probe** : **`targeted_pcb_drc_violations_context_probe.ps1`** ; négatifs : missing board, mismatch, invalid max, invalid layer.

**Observation stabilité R125** : 3× probe ciblé + 3× runner famille recommandés avant toute intégration checkpoint v2.

**Runner** :

```powershell
& "c:\dev\kicad-master\qa\tools\ipc_tools\targeted_pcb_drc_violations_context_probe.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release"
& "c:\dev\kicad-master\qa\tools\ipc_tools\run_pcb_drc_violations_context_v1_targeted_probes.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release"
```

Résumés : **`qa_pcb_drc_violations_context_summary.txt`** (probe) ; **`qa_pcb_drc_violations_context_v1_family_summary.txt`** (famille).

Détail : **`MCP_DECISIONS.md`** R124 / R125 / R126.

### Audit R128 — résumé DRC optionnel dans `get_project_context_snapshot`

**Objectif** : auditer l’enrichissement opt-in **`drc_summary`** (sans implémentation R128).

**État actuel** : tool composé **`invokeGetProjectContextSnapshot`** (`common/mcp/mcp_executor.cpp`) — agrège project/workspace/open docs/netclasses/nets ; **mixte** project-only + PCB GUI ; **sans DRC**.

**Recommandation R129** : opt-in progressif — défaut **metadata-only** ; **`include_drc_summary=true`** + **`drc_run_mode=existing_markers`** (cheap, souvent 0) ; **`engine_run`** explicite seulement (officiel, coûteux) ; snapshot = **compteurs/types compacts** — détail via **`get_pcb_drc_violations_context_session`**.

**Implémentation proposée** : délégation interne **`CallTool`** (éviter duplication handler DRC) ; corriger résolution **`board_filename` absolu**.

Détail complet : **`MCP_DECISIONS.md`** R128.

### Famille probe **`project_context_snapshot_drc`** (**R129**)

**Objectif** : valider **`drc_summary`** opt-in dans **`get_project_context_snapshot`** — **hors** checkpoint MCP v2.

**Fixture** : **`ipc_drc_v1`** (même plateau que DRC v1).

**Probe** : **`targeted_project_context_snapshot_drc_probe.ps1`** — résumé **`qa_project_context_snapshot_drc_summary.txt`**.

| Cas | Attendu |
|-----|---------|
| défaut (sans `include_drc_summary`) | snapshot ok ; **`drc_summary_present=0`** ; pas d’appel DRC |
| `existing_markers` | `drc_was_run=false` ; 0 violation OK |
| `engine_run` | `is_official_drc=true` ; `types` contient **`clearance`** |
| invalid mode / max | **`AS_BAD_REQUEST`** |

Détail : **`MCP_DECISIONS.md`** R129.

### Probe **`project_context_snapshot_paths`** (**R130**, étendu **R131**)

**Objectif** : valider **`snapshot_summary.open_pcb_board_absolute_path*`** et **`open_documents[]`** chemins absolus.

**Probe** : **`targeted_project_context_snapshot_paths_probe.ps1`** — résumé **`qa_project_context_snapshot_paths_summary.txt`**.

| Cas | Attendu |
|-----|---------|
| snapshot défaut | chemin absolu présent, suffix `.kicad_pcb`, match fixture |
| `open_documents[]` PCB | **`absolute_path`** + **`board_absolute_path`** = même chemin que `snapshot_summary` |
| `engine_run` sans `drc_board_filename` | DRC summary OK via résolution interne |
| défaut | pas de DRC (`drc_summary_present=0`) |

Détail : **`MCP_DECISIONS.md`** R130 / R131.

**R132 (audit)** : chemins absolus **MCP-only** — protobuf **`DocumentSpecifier`** **inchangé** ; pas de probe IPC supplémentaire requis tant que Option B (MCP source navigation) est retenue. Détail **`MCP_DECISIONS.md`** R132.

### Mini-release « Project Snapshot navigation-ready » (**R133**)

**Objectif** : clôturer R128–R132 — documenter **`get_project_context_snapshot`** comme source navigation IA ; valider non-régression QA légère.

**Pattern IA** : snapshot → **`snapshot_summary.open_pcb_board_absolute_path`** ou **`open_documents[].board_absolute_path`** → tools PCB/DRC avec chemin absolu.

**Probes R133** (hors checkpoint v2) :

```powershell
& targeted_project_context_snapshot_paths_probe.ps1 -BuildDir $BuildDir
& targeted_project_context_snapshot_drc_probe.ps1 -BuildDir $BuildDir
```

**Non-régression familles** : **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`** ; CI **`run_mcp_v2_ci_baseline.ps1 -VerifyOnly`**.

Détail : **`MCP_DECISIONS.md`** R133 ; guide **`MCP_KICAD_USER_DEV_GUIDE.md`**.

### Audit R134 — `project_readiness_summary` (non implémenté)

**Objectif** : vue compacte IA (documents, PCB, DRC, footprints, actions) — audit faisabilité.

**Option retenue (audit)** : enrichir **`get_project_context_snapshot`** avec bloc **`project_readiness_summary`** opt-in progressif (**Option A**) ; footprint aggregate nécessite **nouveau tool SCH read-only** (**R135b**).

Détail : **`MCP_DECISIONS.md`** R134.

### Probe **`project_context_snapshot_readiness`** (**R135a**)

**Objectif** : valider bloc **`project_readiness_summary`** dérivé — défaut sans DRC ; footprints unknown ; DRC opt-in.

**Probe** : **`targeted_project_context_snapshot_readiness_probe.ps1`** — résumé **`qa_project_context_snapshot_readiness_summary.txt`**.

| Cas | Attendu |
|-----|---------|
| défaut | `project_readiness_summary_present` ; pas DRC ; footprints unknown ; actions `request_drc_summary_engine_run` |
| `existing_markers` | `drc_summary_present` ; ne prétend pas clean si count=0 |
| `engine_run` | `drc_has_violations=true` ; action `run_drc_detail` |

Détail : **`MCP_DECISIONS.md`** R135a.

### Probe **`schematic_footprint_readiness_session`** (**R135b**)

**Objectif** : valider tool SCH read-only footprint aggregate + négatifs contrat.

**Probe** : **`targeted_schematic_footprint_readiness_probe.ps1`** — fixture **`issue19090`** ; résumé **`qa_schematic_footprint_readiness_summary.txt`**.

| Cas | Attendu |
|-----|---------|
| happy path | `symbols_count > 0` ; footprints présents ; `sch_to_pcb_ready` cohérent |
| filename absent | blocker `missing_or_invalid_schematic_filename` |
| mismatch | blocker `schematic_filename_mismatch` |

### Extension probe **`project_context_snapshot_readiness`** (**R135b**)

| Cas | Attendu |
|-----|---------|
| défaut | `footprints_summary_available=false` (R135a inchangé) |
| `include_footprints_summary=true` sans SCH | `footprints_summary_present` ; `footprints_summary_available=false` |
| `include_footprints_summary=true` + SCH **`issue19090`** | compteurs numériques ; `sch_to_pcb_ready=true` ; pas `assign_missing_footprints` |

Détail : **`MCP_DECISIONS.md`** R135b.

### Audit R136 — `validate_footprints=true` (non implémenté)

**Objectif** : faisabilité validation library dans **`schematic_footprint_readiness_session`**.

**Conclusion audit** : **faisable** via **`KIFACE_TEST_FOOTPRINT_LINK`** (pattern R120) ; **Option C** sémantique (`sch_to_pcb_ready` + `sch_to_pcb_ready_validated`) ; cache footprint unique recommandé ; impl **R137**.

Détail : **`MCP_DECISIONS.md`** R136.

### Probe **`schematic_footprint_readiness_session`** validation (**R137**)

| Cas | Attendu |
|-----|---------|
| `validate_footprints=false` | R135b inchangé ; `sch_to_pcb_ready_validated=null` |
| `validate_footprints=true` + fp-lib-table QA | `footprint_validation_performed=true` ; invalid=0 ; `sch_to_pcb_ready_validated=true` |
| footprint invalide temporaire | invalid≥1 ; `invalid_footprints[]` ; restore |

Détail : **`MCP_DECISIONS.md`** R137.

### Mini-release « Project Readiness Summary V1 complet » (**R138**)

**Statut** : R135a (snapshot dérivé) + R135b (tool footprint) + R137 (validation opt-in) = **V1 complet documenté**.

| Signal | Sémantique |
|--------|------------|
| **`sch_to_pcb_ready`** | Tous symboles scannés (non-power, écran courant) : footprint **non vide** |
| **`sch_to_pcb_ready_validated`** | **`null`** sans validation ; sinon footprint non vide **et** valide librairie |

**Limites V1** : écran courant ; power symbols exclus ; pas multi-feuilles ; **`footprints_validate=false`** par défaut ; checkpoint MCP v2 **9 familles** inchangé.

**Recommended actions** : `assign_missing_footprints` ; `fix_invalid_footprints` ; `run_drc_detail`.

**Non-régression SCH** : fix **`changed=true`** post-mutation **`schematic_set_symbol_field_session`** (R137).

Détail : **`MCP_DECISIONS.md`** R138.

### Audit isolation QA SCH — fixture **`issue19090`** (**R138**)

**Probes mutantes principales** :

| Probe | Mutation | Restore |
|-------|----------|---------|
| `targeted_schematic_footprint_readiness_probe.ps1` | FOOTPRINT temporaire ; `fp-lib-table` projet | restore `R_0805` ; `Stop-KiCadProcesses` |
| `targeted_project_context_snapshot_readiness_probe.ps1` | idem (bloc validate) | restore footprints |
| `targeted_schematic_set_symbol_field_probe.ps1` | DATASHEET test | restore `""` |
| `run_sch_actions_v1_targeted_probes.ps1` | 4 tools SCH sur `issue19090` | restore par probe enfant |
| Familles checkpoint v2 SCH | geometry/authoring/wiring/symbol-editing | restore par probe ; **KiCad partagé** |

**Risque** : pollution **session KiCad en mémoire** si suites mutantes enchaînées sans teardown.

**Recommandation** : **`Stop-KiCadProcesses`** (+ optionnel **`Clear-IpcSockets`**) avant **`run_mcp_v2_checkpoint.ps1`** et entre suites lourdes. Teardown harness commun = **R139+** si besoin.

Détail : **`MCP_DECISIONS.md`** R138.

### Factorisation validation footprint (**R139**)

Helper statique partagé dans **`api_handler_sch.cpp`** :

- **`evaluateFootprintLinkForSchMcp`** — parse **`LIB_ID`**, **`KIFACE_TEST_FOOTPRINT_LINK`**, mappings status
- Consommé par **`schematic_assign_footprint_session`** et **`schematic_footprint_readiness_session`**
- Cache readiness **par appel** inchangé (R137) ; **pas** de cache global

Détail : **`MCP_DECISIONS.md`** R139.

### Audit multi-feuilles footprint readiness (**R140**)

Audit-only : faisabilité **oui** ; pattern ERC + **`SCH_SHEET_LIST::GetSymbols`** ; footprint path-aware via **`GetFootprintFieldText(true, &sheetPath, false)`** ; risque double comptage si dédup **`symbol_id`** seul.

**Option retenue R141** : **Option A** — étendre tool existant avec **`scope=current_sheet|hierarchy`** (défaut inchangé).

Fixtures QA existantes : **`qa/data/eeschema/netlists/complex_hierarchy/`** ; flat **`issue19090`** pour non-régression.

Détail : **`MCP_DECISIONS.md`** R140.

### `scope=hierarchy` footprint readiness (**R141**)

**Implémenté** : **`schematic_footprint_readiness_session`** accepte **`scope=current_sheet|hierarchy`** (défaut **`current_sheet`**) et **`include_sheet_paths`** ; snapshot relay **`footprints_scope`**.

| Mode | Comportement |
|------|--------------|
| **`current_sheet`** | Inchangé R135b–R139 (`issue19090`) |
| **`hierarchy`** | **`Hierarchy().GetSymbols`** ; footprint path-aware ; **`sheets_count`**, **`symbols_instances_count`**, **`sheet_path`**, **`instance_path`** |

Fixture dédiée : **`qa/data/eeschema/hierarchy/ipc_footprint_readiness_v1/`**.

Probes : **`targeted_schematic_footprint_readiness_probe.ps1`**, **`targeted_project_context_snapshot_readiness_probe.ps1`**.

Détail : **`MCP_DECISIONS.md`** R141.

### Audit assign footprint instance-aware (**R142**)

Audit-only : **`schematic_assign_footprint_session`** reste **écran courant + `symbol_id` + `SetFootprintFieldText` global** ; risque mutation multi-instances sur écran partagé ; clé R143 recommandée : **`instance_path`** (alignée readiness) + **`scope=hierarchy`** ; défaut **`current_sheet`** inchangé.

Détail : **`MCP_DECISIONS.md`** R142.

### `scope=hierarchy` assign footprint prudent (**R143**)

Spike KiCad : **écriture instance-safe FOOTPRINT absente** (variant vide) → **`SetFootprintFieldText`** global ; mutation **`scope=hierarchy`** retourne **`hierarchy_instance_write_unsupported`** sans mutation. Résolution **`instance_path`** + readback dry-run alignés readiness R141 ; **`current_sheet` inchangé**.

Probe : extension **`targeted_schematic_assign_footprint_probe.ps1`** (dry-run hierarchy + blocker mutation + négatifs) sur **`ipc_footprint_readiness_v1`**.

Détail : **`MCP_DECISIONS.md`** R143.

### Hierarchical Assign Footprint V1 safe-limited (**R144**)

Mini-release **documentaire** clôturant R141–R143. Comportement **volontairement safe** :

| Principe | Détail |
|----------|--------|
| Pas de mutation globale accidentelle | mutation **`scope=hierarchy`** refusée — **`hierarchy_instance_write_unsupported`** |
| Pas de hack interne | pas d’écriture **`symbol_instances`** / **`m_Variants`** |
| Extension future | uniquement si API instance-safe KiCad **prouvée** + QA isolation |

- **`scope=current_sheet`** : mutation supportée (R121, inchangé)
- **`scope=hierarchy`** : résolution **`instance_path`** + dry-run + diagnostic ; mutation **bloquée**

QA non-régression : assign + readiness + snapshot readiness + checkpoint MCP v2 + CI VerifyOnly — **sans modification code produit**.

Détail : **`MCP_DECISIONS.md`** R144.

### Spike instance-safe footprint write (**R145**)

Fixture dédiée : **`qa/data/eeschema/hierarchy/ipc_footprint_instance_write_spike/`** (InstA/InstB → **`sub_shared.kicad_sch`**, symbole UUID partagé, **`instance_path`** distincts).

Spike script : **`r145_spike_schematic_footprint_instance_write.ps1`** — écriture expérimentale **`scope=current_sheet`** uniquement ; mutation hierarchy **non activée**.

Conclusion : **`instance_write_supported=false`** — **`SetFootprintFieldText`** global ; variant FOOTPRINT instance-only via **`m_Variants`** + variant projet (hors chemin readiness/assign variant vide).

Détail : **`MCP_DECISIONS.md`** R145.

### Audit/spike API footprint instance-safe (**R157**)

Approfondit R145 : readiness **after** mesurée sur instances A/B (in-session), navigation produit, audit disque, reload sans save MCP.

Script : **`r157_spike_footprint_instance_safe_write.ps1`** — summary **`qa_r157_spike_footprint_instance_safe_write_summary.txt`**.

Conclusion : **`instance_safe_write_supported=false`** — écriture **`SetFootprintFieldText`** mute A **et** B ; mutation **`scope=hierarchy`** reste **`hierarchy_instance_write_unsupported`**.

Détail : **`MCP_DECISIONS.md`** R157.

### Officialisation limite instance-safe (**R158**)

Clôture piste mutation **`scope=hierarchy`** côté MCP tant qu’**aucune API KiCad instance-safe** n’existe.

| Règle officialisée | Valeur |
|--------------------|--------|
| **`scope=hierarchy` mutation** | **Bloquée** — **`hierarchy_instance_write_unsupported`** |
| **`scope=hierarchy` dry-run** | Supporté — diagnostic / preview |
| **`allow_shared_symbol_global_update`** | Mutation **globale** volontaire — **pas** instance-safe |
| Workflow produit stable | **R156** — navigate → active sheet → assign **`current_sheet`** dry-run |
| Statut manifest | **`hierarchy_assign_safe_limited`** |

Note design upstream : **`MCP_UPSTREAM_FOOTPRINT_INSTANCE_SAFE_WRITE_DESIGN.md`** — API cible + critères gate R159/R160.

Détail : **`MCP_DECISIONS.md`** R158.

### Prototype upstream instance-safe API (**R159**)

**`SetFootprintFieldTextForSheet`** dans eeschema + QA tool **`qa_spike_footprint_instance_api`** (non public).

Spike : **`r159_spike_footprint_instance_safe_api.ps1`** — isolation A/B, save/reload, undo/redo. **R159 validé (2026-06-07)** : **`r159_spike_green=true`**, **`instance_safe_write_supported=true`** (API prototype QA-only).

### Mutation hierarchy MCP instance-safe (**R160**)

**`schematic_assign_footprint_session scope=hierarchy`** — mutation via **`SetFootprintFieldTextForSheet`** ; probe **`targeted_schematic_assign_footprint_hierarchy_probe.ps1`**. **`global_update=false`**, **`instance_safe_write_used=true`**. **`current_sheet`** inchangé (R146).

Détail : **`MCP_DECISIONS.md`** R160.

### Mini-release « Hierarchy Footprint Assignment enabled » (**R161**)

**Statut manifest** : **`hierarchy_assign_instance_safe_enabled`** (remplace **`hierarchy_assign_safe_limited`** pour mutation hierarchy).

**Capacité** : **`schematic_assign_footprint_session scope=hierarchy`** — instance-safe via **`SetFootprintFieldTextForSheet`**.

**Workflows documentés** : Workflow A (direct hierarchy) + Workflow B conservateur R156 — voir **`MCP_KICAD_USER_DEV_GUIDE.md`** R161.

**Gate QA minimal avant release footprint hierarchy** :

```powershell
& targeted_schematic_assign_footprint_probe.ps1
& targeted_schematic_assign_footprint_hierarchy_probe.ps1
& r159_spike_footprint_instance_safe_api.ps1
& r157_spike_footprint_instance_safe_write.ps1
& r156_example_navigable_sch_footprint_workflow.ps1
& targeted_schematic_footprint_readiness_probe.ps1
& run_mcp_v2_checkpoint.ps1   # 9 familles — critères inchangés
& run_mcp_v2_ci_baseline.ps1 -VerifyOnly
```

Détail : **`MCP_DECISIONS.md`** R161.

### Runtime workflow discovery (**R162**)

**`get_project_context_snapshot`** → **`project_readiness_summary`** expose (additif, sans scan supplémentaire) :

| Champ | Valeur (build R160+) |
|-------|----------------------|
| **`hierarchy_assign_instance_safe_enabled`** | `true` |
| **`recommended_footprint_assignment_workflow`** | `hierarchy_instance_safe` |
| **`fallback_footprint_assignment_workflow`** | `navigate_current_sheet_dry_run` |
| **`recommended_assign_tool`** | `schematic_assign_footprint_session` |
| **`recommended_assign_scope`** | `hierarchy` |

Probe : **`targeted_project_context_snapshot_readiness_probe.ps1`** — critère **`hierarchy_assign_runtime_ok`**.

Détail : **`MCP_DECISIONS.md`** R162.

### Garde-fou shared-sheet `current_sheet` (**R146** ; officialisé **R147**)

**`schematic_assign_footprint_session`** — **Safe Assign Footprint Guard** : si **`affected_instances_count > 1`**, mutation **`scope=current_sheet`** bloquée par défaut (**`shared_symbol_global_update_blocked`**) ; opt-in **`allow_shared_symbol_global_update=true`**. Schéma simple (**`issue19090`**) inchangé. **`scope=hierarchy`** mutation **instance-safe** (**R161** — **`hierarchy_assign_instance_safe_enabled`**).

**Changement comportemental intentionnel (R146)** : avant le garde-fou, une mutation **`current_sheet`** sur écran partagé pouvait modifier silencieusement toutes les instances ; désormais bloquée sans opt-in explicite.

**QA probe `targeted_schematic_assign_footprint_probe.ps1`** (fixture **`ipc_footprint_instance_write_spike`**) vérifie :

| Critère | Attendu |
|---------|---------|
| Blocage par défaut | **`shared_symbol_global_update_blocked`**, **`changed=false`** |
| Readback inchangé | footprint inchangé après blocage (dry-run / readiness) |
| Opt-in accepté | **`allow_shared_symbol_global_update=true`** → mutation globale volontaire |
| Hierarchy instance-safe | probe **`targeted_schematic_assign_footprint_hierarchy_probe.ps1`** — A changed / B unchanged |
| Non-régression simple | **`issue19090`** — mutation **`current_sheet`** OK |

**Note compteur** : **`affected_instances_count`** peut **sur-compter** de manière **conservatrice** (ex. 3 vs 2 attendus sur spike — scan références **`SCH_SHEET`** + hiérarchie) ; le garde-fou s’active dès **`> 1`**.

Détail : **`MCP_DECISIONS.md`** R146 / R147.

### Audit workflow IA avancé — assign sans mutation hierarchy (**R148**)

**Objectif** : corriger footprints détectés en hiérarchie via **`scope=current_sheet`** + garde-fou R146/R147, sans **`scope=hierarchy`** mutation.

**Faisabilité** : **oui avec supervision** — diagnostic hierarchy fiable ; mutation safe si **`global_update=false`** et contexte éditeur correct.

**Gaps identifiés** :

| Gap | Statut MCP actuel |
|-----|-------------------|
| Navigation vers feuille cible | **Absent** (produit) — **`qa_open_editor_frame`** QA-only ouvre un **fichier**, pas un **`instance_path`** |
| Vérification feuille active vs **`sheet_path`** | **Absent** — **`get_open_documents`** expose fichier ouvert, pas **`CurrentSheet()`** |
| Preuve positive contexte instance | **Absent** — heuristique **`symbol_not_found`** en dry-run seulement |

**Workflow documenté** : snapshot hierarchy → dry-run hierarchy → bascule **`current_sheet`** si contexte sûr → dry-run obligatoire → mutation si **`affected_instances_count ≤ 1`**.

**Recommandation R149** : **Option A** (doc workflow) puis **Option B** (**`get_schematic_active_sheet_context_session`** read-only).

Détail : **`MCP_DECISIONS.md`** R148.

### Mini-release « Advanced Safe Footprint Assignment Workflow » (**R149**)

**Objectif** : officialiser le workflow IA avancé R148 — correction footprints hiérarchiques via **`scope=current_sheet`** + garde-fou R146/R147, **sans** mutation **`scope=hierarchy`**.

**Workflow officialisé** : snapshot hierarchy (A) → cible (B) → dry-run hierarchy (C) → pas de force hierarchy (D) → readiness + dry-run **`current_sheet`** (E) → mutation si sûr (F).

**Règles de sécurité** : **`global_update=true`** → ne pas muter ; **`allow_shared_symbol_global_update`** jamais automatique ; dry-run **`current_sheet`** obligatoire ; **`symbol_not_found`** = heuristique négative.

**Limites jusqu’à R150** : pas de navigation instance produit ; pas d’exposition **`CurrentSheet()`** ; workflow supervisé.

**Prochaine brique** : **`get_schematic_active_sheet_context_session`** (R150) — contrat prévisionnel dans **`MCP_DECISIONS.md`** R149.

Détail : **`MCP_DECISIONS.md`** R149.

### Tool read-only active sheet context (**R150**)

**`get_schematic_active_sheet_context_session`** : expose **`CurrentSheet()`**, fichier écran actif, signal **`active_sheet_is_shared`**, symboles visibles (**power symbols exclus**) avec **`instance_path`** / **`sheet_path`**. Read-only — aucun **`SCH_COMMIT`**.

Probe : **`targeted_schematic_active_sheet_context_probe.ps1`** (fixture **`issue19090`**).

Détail : **`MCP_DECISIONS.md`** R150.

### Workflow IA consolidé avec active sheet context (**R151**)

Chaîne officialisée post-R150 :

**snapshot hierarchy → readiness hierarchy → active sheet context (`instance_path` match) → assign dry-run `current_sheet` → mutation si sûr**

Règle agent : **ne pas muter** si l’`instance_path` readiness est absent de **`active_sheet_context.symbols[]`**.

Probe **`targeted_schematic_active_sheet_context_probe.ps1`** — critères **`workflow_instance_path_match_ok`**, **`workflow_symbol_id_match_ok`**, **`workflow_assign_dry_run_ok`**.

Détail : **`MCP_DECISIONS.md`** R151.

### Workflow active sheet hiérarchique (**R152**)

Extension probe **`targeted_schematic_active_sheet_context_probe.ps1`** — fixture **`ipc_footprint_readiness_v1`** :

| Cas | Attendu |
|-----|---------|
| Root ouvert (mauvais contexte) | **`instance_path`** sous-feuille absent ; assign dry-run bloqué |
| Sous-feuille ouverte (**`qa_open_editor_frame`**) | **`instance_path`** présent ; dry-run **`current_sheet`** sûr |

Critères summary : **`workflow_hierarchy_fixture_checked`**, **`hierarchy_wrong_context_instance_absent_ok`**, **`hierarchy_wrong_context_blocks_assignment_ok`**, **`hierarchy_active_context_instance_match_ok`**, **`hierarchy_active_context_symbol_id_match_ok`**, **`hierarchy_current_sheet_dry_run_safe_ok`**.

Si navigation sous-feuille impossible : **`skipped_navigation_tool_missing`** → recommander **R153** (tool navigation sheet).

Détail : **`MCP_DECISIONS.md`** R152.

### Audit navigation SCH (**R153**)

Audit faisabilité tool produit **`navigate_schematic_sheet_session`** — API candidate : **`SCH_ACTIONS::changeSheet`** / **`SCH_NAVIGATE_TOOL::changeSheet`** ; identifiant primaire **`sheet_path` (`PathAsString`)** ; rejet navigation par fichier seul (`qa_open_editor_frame`).

Contrat, blockers et QA R154 proposés dans **`MCP_DECISIONS.md`** R153.

### Navigation hiérarchique produit (**R154**)

Tool **`navigate_schematic_sheet_session`** — focalise une feuille SCH par **`sheet_path`** ou **`instance_path`** via **`SCH_ACTIONS::changeSheet`** ; **`dry_run`** supporté ; aucune mutation SCH.

Probe : **`targeted_schematic_navigate_sheet_probe.ps1`** (fixture **`ipc_footprint_readiness_v1`**).

Détail : **`MCP_DECISIONS.md`** R154.

### Workflow produit consolidé (**R155**)

Chaîne officialisée sans **`qa_open_editor_frame`** :

```text
readiness hierarchy → navigate_schematic_sheet_session → active sheet context → assign dry-run current_sheet
```

Probe **`targeted_schematic_active_sheet_context_probe.ps1`** — critères **`hierarchy_navigation_product_used`**, **`workflow_uses_qa_open_editor_frame=false`**, **`hierarchy_current_sheet_dry_run_global_update_false_ok`**.

Détail : **`MCP_DECISIONS.md`** R155.

### Exemple end-to-end R156 — Navigable SCH Footprint Workflow

Script reproductible : **`r156_example_navigable_sch_footprint_workflow.ps1`**

Summary : **`qa_r156_navigable_sch_footprint_workflow_summary.txt`**

Chaîne documentée (dry-run only, pas de mutation réelle) :

```text
1. schematic_footprint_readiness_session(scope=hierarchy) — pick missing footprint instance
2. navigate_schematic_sheet_session(instance_path=...)
3. get_schematic_active_sheet_context_session — verify instance_path + symbol_id
4. schematic_assign_footprint_session(scope=current_sheet, dry_run=true)
5. agent_decision — mutate only if dry-run safe AND policy/user allows
```

Critères : **`workflow_uses_qa_open_editor_frame_for_navigation=false`**, **`mutation_performed=false`**.

Détail : **`MCP_DECISIONS.md`** R156 ; guide : **`MCP_KICAD_USER_DEV_GUIDE.md`**.

### Exemple runtime-driven R163 — Auto-sélection workflow footprint

Script reproductible : **`r163_example_runtime_selected_footprint_workflow.ps1`**

Summary : **`qa_r163_runtime_selected_footprint_workflow_summary.txt`**

Chaîne documentée (runtime → Workflow A par défaut, dry-run only) :

```text
1. get_project_context_snapshot — lire project_readiness_summary (champs R162)
2. Si hierarchy_assign_instance_safe_enabled=true et recommended_assign_scope=hierarchy → Workflow A
3. schematic_footprint_readiness_session(scope=hierarchy) — pick instance cible
4. schematic_assign_footprint_session(scope=hierarchy, dry_run=true, instance_path=...)
5. Sinon → fallback navigate_current_sheet_dry_run (Workflow B / R156, script séparé)
```

Critères : **`selected_workflow=hierarchy_instance_safe`**, **`selected_workflow_matches_runtime_ok=true`**, **`mutation_performed=false`**, R156 fallback **non supprimé**.

Détail : **`MCP_DECISIONS.md`** R163 ; guide : **`MCP_KICAD_USER_DEV_GUIDE.md`**.

### CI observability R164 — Runtime footprint workflow

Step **G4** dans **`run_mcp_v2_ci_baseline.ps1`** (après checkpoint G + advisory G2/G3) :

- Appelle **`r163_example_runtime_selected_footprint_workflow.ps1`**
- Écrit **`runtime_footprint_workflow_*`** dans **`qa_mcp_v2_ci_baseline_summary.txt`**
- **Observability-only** — échec R163 **n'affecte pas** **`ci_baseline_green`**
- **`-VerifyOnly`** : champs **`N/A`**, R163 non exécuté

Détail : **`MCP_DECISIONS.md`** R164.

### CI advisory R167 — Dual-source footprint workflow consistency

Step **G5** dans **`run_mcp_v2_ci_baseline.ps1`** (après G4) :

- Appelle **`report_footprint_workflow_discovery_consistency.ps1`**
- Écrit **`footprint_workflow_discovery_consistency_*`** dans **`qa_mcp_v2_ci_baseline_summary.txt`**
- **Observability-only** — échec **n'affecte pas** **`ci_baseline_green`**
- **`-VerifyOnly`** : champs **`N/A`**

Détail : **`MCP_DECISIONS.md`** R167.

### Politique d'alerte R168 — Observability G4/G5 (clôture R162–R167)

**`ci_baseline_green=true` ne suffit pas** pour ignorer un advisory rouge G4/G5.

#### Signaux G4 — `runtime_footprint_workflow_*`

Champs à surveiller dans **`qa_mcp_v2_ci_baseline_summary.txt`** :

- `runtime_footprint_workflow_ok`, `runtime_footprint_workflow_green`
- `runtime_footprint_workflow_selected_workflow`
- `runtime_footprint_workflow_runtime_status_read_ok`
- `runtime_footprint_workflow_hierarchy_assign_instance_safe_enabled`
- `runtime_footprint_workflow_assign_hierarchy_dry_run_ok`
- `runtime_footprint_workflow_mutation_performed`
- `runtime_footprint_workflow_blocker`

Politique :

- **`ok=false`** → alerte CI advisory (lire **`runtime_footprint_workflow_target_run_dir`**)
- **`green=false`** → release hold manuel sans analyse
- **`mutation_performed=true`** → incident QA (R163 dry-run only)
- **`selected_workflow != hierarchy_instance_safe`** avec runtime enabled → divergence workflow

#### Signaux G5 — `footprint_workflow_discovery_consistency_*`

Champs à surveiller :

- `footprint_workflow_discovery_consistency_ok`, `footprint_workflow_discovery_consistency_green`
- `footprint_workflow_discovery_consistency_snapshot_status_ok`
- `footprint_workflow_discovery_consistency_readiness_status_ok`
- `footprint_workflow_discovery_consistency_scope_match_ok`
- `footprint_workflow_discovery_consistency_agent_primary_source`
- `footprint_workflow_discovery_consistency_agent_local_source`
- `footprint_workflow_discovery_consistency_on_divergence`
- `footprint_workflow_discovery_consistency_blocker`

Politique :

- **`ok=false`** → alerte advisory
- **`green=false`** → ne pas muter footprint via agent autonome avant investigation
- **`scope_match_ok=false`** → divergence critique
- Agent : **`do_not_mutate_and_recheck_snapshot`**

#### Niveaux d'alerte

| Niveau | Condition | Action |
|--------|-----------|--------|
| Info | G4/G5 = `N/A` en VerifyOnly | Normal |
| Warning | G4 ou G5 `ok=false` en CI complet | Lire target_run_dir + summary |
| Release hold manuel | G4/G5 `green=false` sur branche release | Ne pas publier sans analyse |
| Incident QA | `mutation_performed=true` dans R163 | Stopper et corriger |
| Divergence agent | snapshot/readiness mismatch (G5) | Ne pas muter ; relire snapshot ; ticket |

Source de vérité agent : **`project_readiness_summary`** (principal) > echo readiness (local) ; divergence → consulter G5.

Détail : **`MCP_DECISIONS.md`** R168.

#### Extension R179 — Signaux G6 (`pcb_apply_drc_save_workflow_*`)

Voir section **R179** ci-dessous ; principe identique : **`ci_baseline_green=true` ne suffit pas** si G6 rouge.

Détail : **`MCP_DECISIONS.md`** R179.

### Audit R169 — Post-footprint vers PCB (sans implémentation)

Prochaine capacité MCP envisagée : après footprints SCH assignés, orienter l'IA vers un PCB cohérent.

- API KiCad identifiée : **`FetchNetlistFromSchematic`** + **`BOARD_NETLIST_UPDATER`** (dry-run via **`SetIsDryRun`**).
- **`sch_to_pcb_ready`** ne garantit **pas** la sync PCB — seulement footprints SCH présents.
- Recommandation R170 : **`get_pcb_sync_readiness_session`** (read-only) puis **`pcb_update_from_schematic_preview_session`** (dry-run) ; mutation **`pcb_update_from_schematic_session`** différée.
- Détail audit : **`MCP_DECISIONS.md`** R169.

### R170 — `get_pcb_sync_readiness_session` (read-only, hors checkpoint v2)

- Tool : **`get_pcb_sync_readiness_session`** — compare footprints SCH (via readiness hierarchy) vs PCB ; **sans mutation**.
- Probe : **`targeted_pcb_sync_readiness_probe.ps1`** ; fixtures **`qa/data/pcbnew/sync/ipc_pcb_sync_v1_{empty,synced,orphan}/`**.
- **Non intégré** au checkpoint MCP v2 en R170.
- Détail : **`MCP_DECISIONS.md`** R170.

### R171 — `pcb_update_from_schematic_preview_session` (dry-run, hors checkpoint v2)

- Tool : **`pcb_update_from_schematic_preview_session`** — **`BOARD_NETLIST_UPDATER::SetIsDryRun(true)`** ; reporter structuré `actions[]` ; **sans mutation PCB**.
- Probe : **`targeted_pcb_update_preview_probe.ps1`** ; fixtures **`qa/data/pcbnew/sync/ipc_pcb_sync_v1_{empty,synced,orphan}/`** (même base projet que R170).
- **Non intégré** au checkpoint MCP v2 en R171.
- Détail : **`MCP_DECISIONS.md`** R171.

### R172 — `pcb_sync_summary` opt-in (snapshot, hors checkpoint v2)

- Bloc **`pcb_sync_summary`** dans **`get_project_context_snapshot`** via **`include_pcb_sync_summary=true`**.
- Preview dry-run optionnelle : **`pcb_sync_include_preview=true`** (défaut `false`).
- Probe : **`targeted_project_context_snapshot_pcb_sync_probe.ps1`** ; fixtures **`ipc_pcb_sync_v1_*`**.
- **Non intégré** au checkpoint MCP v2.
- Détail : **`MCP_DECISIONS.md`** R172.

### R173 — `pcb_update_from_schematic_session` prototype (hors checkpoint v2)

- Mutation contrôlée **`BOARD_NETLIST_UPDATER::UpdateNetlist`** avec gate **`confirm_apply=true`** obligatoire.
- Preview gate R171 interne (`require_preview_green=true` par défaut).
- Probe **`targeted_pcb_update_from_schematic_probe.ps1`** : confirm / empty apply / restore fichier / orphan safe default.
- Détail : **`MCP_DECISIONS.md`** R173.

### R174 — mini-release doc `pcb_update_apply_controlled_v1` (doc only, hors checkpoint v2)

- **R173 validée** — apply contrôlé livré et testé (confirm gate, preview, restore fixture, orphan safe default).
- **R174** = officialisation documentaire ; **aucun handler / schéma MCP modifié**.
- Probe mutation : **`targeted_pcb_update_from_schematic_probe.ps1`** — **hors checkpoint MCP v2**.
- Checkpoint MCP v2 reste **9 familles** — apply **non couvert**.
- **`run_mcp_v2_ci_baseline.ps1 -VerifyOnly`** ne gate **pas** apply ; QA apply = probe dédiée manuelle / clôture R174.
- Détail : **`MCP_DECISIONS.md`** R174.

### R175 — audit save / undo / DRC gate post-apply (doc only)

- Audit : save (`SaveBoard` / IPC `SaveDocument`), undo (`BOARD_COMMIT` + limites `OnNetlistChanged`), DRC post-apply (`run_drc=true` advisory).
- **R176 prioritaire** : Option A **`save_board_session`** ; Option C advisory **`include_post_apply_drc_summary`** (non bloquant).
- **Rejeté R176** : undo MCP (Option D), save inline apply (Option B), gate DRC bloquant.
- Détail : **`MCP_DECISIONS.md`** R175.

### R176 — `save_board_session` + DRC advisory post-apply (hors checkpoint v2)

- Tool **`save_board_session`** : gate **`confirm_save=true`** ; blockers structurés ; `SavePcbFile` headless si writable.
- Apply **`pcb_update_from_schematic_session`** : opt-in **`include_post_apply_drc_summary`** (advisory, non bloquant).
- Probes : **`targeted_pcb_save_board_probe.ps1`**, extension **`targeted_pcb_update_from_schematic_probe.ps1`** (cas DRC advisory).
- Checkpoint MCP v2 **9 familles** inchangé ; critères green inchangés.
- Détail : **`MCP_DECISIONS.md`** R176.

### R177 — mini-release doc « PCB Apply → DRC Advisory → Save Workflow » (doc + probe, hors checkpoint v2)

- Workflow officialisé **`pcb_apply_drc_save_workflow_v1`** : snapshot R172 → preview R171 → apply R173 + DRC advisory R176 → sync R170 → save R176 → verify.
- Probe exemple : **`r177_example_pcb_apply_drc_save_workflow.ps1`** (gates confirm apply/save, DRC non bloquante).
- **Aucun handler C++** ; checkpoint MCP v2 **9 familles** inchangé ; critères green inchangés.
- Détail : **`MCP_DECISIONS.md`** R177.

### R178 — CI observability G6 « PCB apply DRC save workflow » (hors `ci_baseline_green`)

- Step **G6** dans **`run_mcp_v2_ci_baseline.ps1`** (après G5) : appelle **`r177_example_pcb_apply_drc_save_workflow.ps1`**.
- Champs **`pcb_apply_drc_save_workflow_*`** dans **`qa_mcp_v2_ci_baseline_summary.txt`**.
- **`-VerifyOnly`** → G6 = **`N/A`** ; **`-SkipCheckpoint`** → G6 = **`skipped`**.
- Échec G6 **ne modifie pas** **`ci_baseline_green`** ni **`exact_blocker_if_any`**.
- Détail : **`MCP_DECISIONS.md`** R178.

### R179 — Politique d'alerte G6 et clôture série PCB apply/save R170–R179 (doc only)

**`ci_baseline_green=true` ne suffit pas** pour ignorer un G6 rouge — advisory critique, décision PM requise avant release.

#### Signaux G6 — `pcb_apply_drc_save_workflow_*`

Champs à surveiller dans **`qa_mcp_v2_ci_baseline_summary.txt`** :

- `pcb_apply_drc_save_workflow_ok`, `pcb_apply_drc_save_workflow_green`
- `pcb_apply_drc_save_workflow_preview_ok`
- `pcb_apply_drc_save_workflow_apply_confirm_required_ok`
- `pcb_apply_drc_save_workflow_apply_with_drc_advisory_ok`
- `pcb_apply_drc_save_workflow_post_apply_drc_summary_present_ok`
- `pcb_apply_drc_save_workflow_post_apply_sync_readiness_ok`
- `pcb_apply_drc_save_workflow_save_confirm_required_ok`
- `pcb_apply_drc_save_workflow_save_confirmed_ok`
- `pcb_apply_drc_save_workflow_post_save_verify_ok`
- `pcb_apply_drc_save_workflow_blocker`

Politique :

- **`ok=false`** / **`green=false`** reproductible → release hold manuel
- **`apply_confirm_required_ok=false`** ou **`save_confirm_required_ok=false`** → incident QA (gates confirm)
- **`post_save_verify_ok=false`** → incident QA
- apply OK + **`save_confirmed_ok=false`** → incident produit potentiel
- apply OK + **`post_apply_sync_readiness_ok=false`** → incident produit potentiel
- **`post_apply_drc_summary_present_ok=false`** avec DRC demandé → incident produit potentiel

#### Niveaux d'alerte G6

| Niveau | Condition | Action |
|--------|-----------|--------|
| Info | G6 = `N/A` en VerifyOnly | Normal |
| Info | G6 = `skipped` en SkipCheckpoint | Normal |
| Warning | G6 KO local, R177 standalone green au rerun | Investiguer flake |
| Release hold | `ok=false` ou `green=false` reproductible en CI complet | Ne pas publier sans analyse PM |
| Incident QA | gates confirm ou post-save verify KO | Stopper ; corriger |
| Incident produit | apply OK mais save/sync/DRC advisory KO | Analyse produit ; pas de rollback auto |

Série **R170–R179** clôturée : sync → preview → apply → DRC advisory → save → verify ; G6 = surveillance chaîne.

Détail : **`MCP_DECISIONS.md`** R179.

### R180 — Rapport local advisory G4/G5/G6 (read-only)

Script **`report_mcp_v2_ci_advisories.ps1`** :

- Lit le dernier **`qa_mcp_v2_ci_baseline_summary.txt`** (ou **`-SummaryPath`**)
- Interprète G4/G5/G6 selon **R168/R179**
- Produit **`qa_mcp_v2_ci_advisory_report.txt`** + **`.md`**
- **Ne modifie pas** `ci_baseline_green` ni le summary source
- Exit **0** même si `release_hold` ; exit **1** si summary introuvable

```powershell
& report_mcp_v2_ci_advisories.ps1 -BuildDir $BuildDir
```

Détail : **`MCP_DECISIONS.md`** R180.

### R181 — Wrapper release checklist advisories (read-only)

Script **`check_mcp_v2_release_advisories.ps1`** :

- Consomme le rapport **R180** (ou le génère si absent)
- Applique politique release **go/no-go** ; exit **1** si `release_hold`, `incident_qa`, `unknown`, `release_requires_manual_review=true`, ou VerifyOnly sans runtime G4/G5/G6
- Options **`-AllowWarning`**, **`-RequireGreen`**
- Produit **`qa_mcp_v2_release_advisory_check_summary.txt`**
- **Ne modifie pas** `ci_baseline_green` ni le checkpoint

```powershell
& report_mcp_v2_ci_advisories.ps1 -BuildDir $BuildDir
& check_mcp_v2_release_advisories.ps1 -BuildDir $BuildDir
```

Détail : **`MCP_DECISIONS.md`** R181.

### R182 — Export Release Pack Markdown + JSON (read-only)

Script **`export_mcp_v2_release_pack.ps1`** :

- Agrège CI baseline + rapport **R180** + décision **R181**
- Produit **`qa_mcp_v2_release_pack.md`** + **`qa_mcp_v2_release_pack.json`**
- Fail propre si advisory ou release check absent dans le dossier de run
- Exit **0** si pack généré (même `manual_review_required` / `hold_release`)
- **`release_decision`** = valeur **R181** (VerifyOnly jamais `go`)
- **R183** : **`-IncludeCheckpointSummary`** embarque le détail checkpoint 9 familles (MD + JSON)

```powershell
& report_mcp_v2_ci_advisories.ps1 -BuildDir $BuildDir
& check_mcp_v2_release_advisories.ps1 -BuildDir $BuildDir
& export_mcp_v2_release_pack.ps1 -BuildDir $BuildDir
& export_mcp_v2_release_pack.ps1 -BuildDir $BuildDir -IncludeCheckpointSummary
```

Détail : **`MCP_DECISIONS.md`** R182 · R183.

### R184 — Pipeline release local (wrapper)

Script **`run_mcp_v2_release_pack.ps1`** :

- Orchestre CI baseline + R180 + R181 + export R182/R183 (`-IncludeCheckpointSummary`)
- Produit **`qa_mcp_v2_release_pack_pipeline_summary.txt`** + pack MD/JSON
- Exit **0** si `release_decision=go` ; exit **1** si release non-go (outil release, pas gate dev)
- Options **`-SkipBuild`**, **`-KillRunningKiCad`**, **`-AllowWarning`**, **`-OutDir`**

```powershell
& run_mcp_v2_release_pack.ps1 -BuildDir $BuildDir -SkipBuild -KillRunningKiCad
```

Détail : **`MCP_DECISIONS.md`** R184.

### R185 — Commande officielle release MCP v2 (documentation)

**`run_mcp_v2_release_pack.ps1`** est la **commande standard** pour une décision release locale (PM / publication) :

```powershell
$BuildDir = "c:\dev\kicad-master\build\msvc-win64-release"
$tools = "c:\dev\kicad-master\qa\tools\ipc_tools"

& "$tools\run_mcp_v2_release_pack.ps1" -BuildDir $BuildDir -SkipBuild -KillRunningKiCad
```

**Artefacts officiels** (dossier run CI baseline, sauf `-OutDir`) :

- `qa_mcp_v2_release_pack_pipeline_summary.txt` — `release_pack_pipeline_green`, `release_decision`
- `qa_mcp_v2_release_pack.md` / `.json` — pack release complet (checkpoint 9 familles inclus)
- `qa_mcp_v2_ci_baseline_summary.txt` — gate dev
- `qa_mcp_v2_ci_advisory_report.txt` — G4/G5/G6 (R180)
- `qa_mcp_v2_release_advisory_check_summary.txt` — go/no-go (R181)

**Exit code release** :

| Exit | Signification |
|------|----------------|
| **0** | `release_decision=go` (ou `warning` + `-AllowWarning`) |
| **1** | `manual_review_required`, `hold_release`, `incident_qa`, `unknown`, ou erreur technique |

**CI dev vs release** :

| | CI dev | Release |
|---|--------|---------|
| Script | `run_mcp_v2_ci_baseline.ps1` | **`run_mcp_v2_release_pack.ps1`** |
| Gate | `ci_baseline_green` | `release_decision` |
| G4/G5/G6 | Non bloquant pour merge dev | Bloquant release (R181) |

Scripts R180–R183 restent disponibles en diagnostic manuel ; la release officielle utilise le wrapper.

Détail : **`MCP_DECISIONS.md`** R185.

### R186 — Audit CI/CD intégration release pack (doc only)

Audit de l'environnement CI/CD pour intégrer **`run_mcp_v2_release_pack.ps1`** :

| Constat | Détail |
|---------|--------|
| CI principal | **GitLab CI** (`.gitlab-ci.yml`, `.gitlab/*.yml`) |
| GitHub Actions | Miroir PR seulement (`lockdown.yml`) — pas de QA MCP |
| MCP v2 en CI officielle | **Absent** aujourd'hui |
| Runner cible | Windows `kicad-windows-ltsc2022` (gap : layout `build/msvc-win64-release` vs `build/windows` Debug) |
| Durée release pack | ~5–8 min (`-SkipBuild`) |

**Options** : A local (R185) · **B job manuel Windows (recommandé R187)** · C nightly advisory · D tag release (plus tard).

Détail complet : **`MCP_DECISIONS.md`** R186.

### R187 — Job GitLab manuel Windows release pack

Fichier **`.gitlab/MCP-Windows-Release.yml`** — job **`mcp_v2_release_pack_windows`** :

| Propriété | Valeur |
|-----------|--------|
| Stage | **`release`** (manuel) |
| Runner | `kicad-windows-ltsc2022` |
| Build | `build/msvc-win64-release` (preset CMake si binaires absents) |
| Commande | `run_mcp_v2_release_pack.ps1 -SkipBuild -KillRunningKiCad` |
| Échec job | exit script ≠ 0 (`release_decision != go`) |
| Dev gates | **Inchangés** — job hors pipeline bloquant MR |

Artefacts : pack MD/JSON/pipeline summary + summaries CI/advisory/release/checkpoint (`expire_in: 30 days`, `when: always`).

Détail : **`MCP_DECISIONS.md`** R187.

### R188 — Premier run GitLab réel (bloqué accès)

Tentative premier run **`mcp_v2_release_pack_windows`** sur runner officiel :

| Élément | R188 |
|---------|------|
| Pré-vol YAML local | **OK** (include, stage, job, runner tag, artefacts) |
| Run GitLab réel | **non** — `blocked_by_gitlab_access=true` |
| Job visible / déclenché | **inconnu / non** |
| Référence locale R187 | `release_decision=go`, 9 familles checkpoint |

**Procédure mainteneur** : merger R187 → pipeline `kicad` → stage `release` → Play `mcp_v2_release_pack_windows` → artefacts *MCP v2 Release Pack*.

Checklist retour : **`MCP_DECISIONS.md`** R188.

### R189 — Procédure mainteneur GitLab (handoff)

Procédure mainteneur formalisée pour clôturer le blocage R188 :

| Élément | R189 |
|---------|------|
| Procédure 9 étapes | **`MCP_DECISIONS.md`** R189 (merge → Pipelines → stage `release` → Play) |
| Checklist standardisée | 18 champs + pièces jointes `pipeline_summary` / `pack.json` |
| Bloc handoff copier-coller | **`MAINTAINER_GITLAB_MCP_V2_RELEASE_HANDOFF.md`** (canonique) · **`MCP_DECISIONS.md`** R189 |
| Classification échecs | 9 classes (runner, MSVC, build, GUI, IPC, non-go, artefacts, YAML) |
| Run GitLab réel | **non** — `blocked_by_gitlab_access=true` |

**Résultat idéal** : `job_status=success`, `release_decision=go`, 9 familles checkpoint, artefacts uploadés.

Détail : **`MCP_DECISIONS.md`** R189.

### Echo runtime R165 — Footprint readiness workflow hints

**`schematic_footprint_readiness_session`** echo les champs R162 (helper partagé **`mcp_footprint_assignment_workflow.h`**) :

- Pas de nouveau gate CI baseline
- Probe **`targeted_schematic_footprint_readiness_probe.ps1`** : **`runtime_workflow_echo_ok`**, **`hierarchy_assign_runtime_echo_ok`**, **`recommended_assign_scope_echo_ok`**

Détail : **`MCP_DECISIONS.md`** R165.

### Dual-source consistency R166 — Footprint workflow discovery

Script read-only : **`report_footprint_workflow_discovery_consistency.ps1`**

- Compare les 5 champs runtime R162 entre **`project_readiness_summary`** et **`schematic_footprint_readiness_session`**
- Summary : **`qa_footprint_workflow_discovery_consistency_summary.txt`**
- **Pas un gate MCP v2** ; standalone **exit 1** si divergence
- **CI advisory R167** : step **G5** dans **`run_mcp_v2_ci_baseline.ps1`** ; champs **`footprint_workflow_discovery_consistency_*`** ; **hors** **`ci_baseline_green`**

Détail : **`MCP_DECISIONS.md`** R166 / R167.

## 8. Études MCP (non implémentées ; références)

- **R35 — `get_pcb_nearby_items_context_session`** : étude de besoin pour une recherche **context-only** d’objets proches (sans DRC). **Implémenté** (**R37**) ; **release produit** **PCB Nearby Items Context v1** (**R38**) ; **intégration checkpoint** (**R39**) — voir **`MCP_DECISIONS.md`** **R37** / **R38** / **R39**.
- **R36 — Fixture `ipc_nearby_v1`** : plateau **`qa/data/pcbnew/nearby/ipc_nearby_v1/`** + **`README_ipc_nearby_v1.md`** + **`r36_verify_ipc_nearby_fixture.ps1`** (validation par tools existants). Voir **`MCP_DECISIONS.md`** (**R36**).
