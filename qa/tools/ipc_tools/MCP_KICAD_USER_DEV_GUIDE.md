# MCP KiCad User/Dev Guide

## 1. Portee supportee

Chemin MCP supporte (gating QA):

- `kicad.exe <project>`
- `KICAD_IPC_AUTOMATION=1`
- ouverture editeur via `qa_open_editor_frame`

Chemin non-gating (diagnostic-only):

- `kicad.exe --frame sch <file.kicad_sch>`
- `qa/tools/ipc_tools/run_open_frame_ipc_smoke.ps1`
- usage reserve au diagnostic, hors validation MCP supportee

## 1 bis. Baseline MCP v2 reproductible (R24, pipeline CI **R28**)

Pour reproduire un **checkpoint MCP v2 vert** (build cohÃ©rent, smoke KIFACE, familles agrÃ©gÃ©es), suivre **`qa/tools/ipc_tools/README_IPC_QA_BASELINE.md`**.

**pcb_nearby_items_context v1** (**release R38**, gate checkpoint **R39**, impl. **R37**, QA **R37-Validation**) : **`get_pcb_nearby_items_context_session`** â€” **contexte de voisinage PCB** ; requÃªte V1 = **`item_id`** + **`radius_nm`** (**pas** de mode **point** en V1) ; **`distance_kind`** = **`bbox_center_to_query_nm`** ; scan **linÃ©aire** ; **`max_items`** / **`truncated`** ; **`include_same_net`**, **`item_types`**, **`layer`** optionnels ; **`is_true_drc=false`** â€” **pas** DRC, **pas** `RunTests`, **pas** verdict clearance. Famille **`pcb_nearby_items_context_v1`** **intÃ©grÃ©e** au checkpoint MCP v2 (**huit** familles officielles, **R39**). **Ne pas** lancer plusieurs scripts familles **PCB** lourds **en parallÃ¨le** sur la mÃªme machine (instances KiCad concurrentes â€” voir **`MCP_DECISIONS.md`** **R37-Validation** / **R38** / **R39**).

RÃ©sumÃ© :

- Build minimal : **`kicommon`**, **`pcbnew_kiface`**, **`kicad`**, **`qa_ipc_smoke`** (aprÃ¨s changements **`common/mcp`** ou **`pcbnew/api`**, relinker **`pcbnew_kiface`** pour Ã©viter **`_pcbnew.dll`** stale).
- **Fermer `kicad.exe`** avant link **`_pcbnew.dll`** ; en cas de **`LNK1104`**, tuer les processus restants puis supprimer lâ€™objet **`api_handler_pcb.cpp.obj`** et recompiler sous **VsDevCmd** / **`vcvars64`**.
- **`KICAD_IPC_KIFACE_SMOKE=1`** doit Ãªtre visible par le processus qui exÃ©cute **`run_mcp_v2_checkpoint.ps1`** (mÃªme session PowerShell ; **`run_mcp_v2_ci_baseline.ps1`** dÃ©finit lâ€™environnement pour lâ€™enfant lorsque **`EnableKifaceSmoke`** est activÃ©).
- PrÃ©flight local (**R34**, garde-fous **R66** + **R86**) : **`run_mcp_v2_baseline_preflight.ps1 -BuildDir <build>`** â€” **`test_ipc_qa_path_idempotence`** (PATH idempotent), fail-fast **`pcb_negative_contracts_v1`**, puis **`pcb_measure_distance`** + famille **`pcb_geometry_v1`**, puis **`run_mcp_v2_checkpoint.ps1`** (**8 familles** **R39**, inclut **`pcb_nearby_items_context_v1`** ; clearance et nearby exÃ©cutÃ©es **dans** le checkpoint ; **`qa_mcp_v2_baseline_preflight_summary.txt`**). **`-VerifyBinariesOnly`** : binaires seulement, sans garde-fou PATH. Pipeline CI complet : **`run_mcp_v2_ci_baseline.ps1`**.
- Pipeline CI baseline (**R28**, garde-fous **R65** + **R87**) : **`run_mcp_v2_ci_baseline.ps1`** â€” prÃ©-check processus, vÃ©rification **`cl`/`cmake`/`ninja`**, build **`-BuildMinimal`** optionnel, smoke **`Test-IpcKifaceDllLoad`**, **`test_ipc_qa_path_idempotence.ps1`** (PATH idempotent), **`targeted_pcb_measure_distance_probe.ps1`**, **`targeted_pcb_item_geometry_probe.ps1`**, **`run_pcb_geometry_v1_targeted_probes.ps1`**, **`run_pcb_negative_contracts_v1_targeted_probes.ps1`**, **`run_mcp_v2_checkpoint.ps1`** ; rÃ©sumÃ© **`qa_mcp_v2_ci_baseline_summary.txt`**. **`-VerifyOnly`** : toolchain + binaires + smoke KIFACE + garde-fou PATH (pas probes/checkpoint).
- Harness PATH (**R85**, standalone) : **`test_ipc_qa_path_idempotence.ps1 -BuildDir <build>`** â€” verrouille lâ€™idempotence **`Initialize-IpcQaEnvironment`** (rÃ©sumÃ© **`qa_ipc_path_idempotence_summary.txt`**) ; hors checkpoint MCP v2 par dÃ©faut.

## 2. Pre-requis Windows build-tree

Build tree attendu (exemple):

- `C:\dev\kicad-master\build\msvc-win64-release`

Binaires/fichiers requis:

- `common\kicommon.dll`
- `pcbnew\_pcbnew.dll` (doit correspondre aux derniers changements **`kicommon`** / MCP PCB ; sinon MCP incohÃ©rent)
- `kicad\kicad.exe`
- `qa\tools\ipc_tools\qa_ipc_smoke.exe`
- `KICAD_CONFIG_HOME` pointe vers un dossier de config isole du build tree
- `kicad_common.json` dans `KICAD_CONFIG_HOME\10.0\` contient `api.enable_server=true`

## 3. Variables d'environnement

```powershell
$BuildDir = "C:\dev\kicad-master\build\msvc-win64-release"
$RepoRoot = "C:\dev\kicad-master"
$CfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$PyHome   = Join-Path $BuildDir "vcpkg_installed\x64-windows\tools\python3"

$env:KICAD_CONFIG_HOME = $CfgRoot
$env:KICAD_RUN_FROM_BUILD_DIR = "1"
$env:KICAD_USE_EXTERNAL_PYTHONHOME = "1"
$env:PYTHONHOME = $PyHome
$env:KICAD_IPC_AUTOMATION = "1"

# Optionnel: active les logs de diagnostic startup IPC
# $env:KICAD_IPC_STARTUP_DIAG = "1"

$pathPrefix = @(
  (Join-Path $BuildDir "kicad"),
  (Join-Path $BuildDir "eeschema"),
  (Join-Path $BuildDir "pcbnew"),
  (Join-Path $BuildDir "common"),
  (Join-Path $BuildDir "common\gal"),
  (Join-Path $BuildDir "api"),
  (Join-Path $BuildDir "vcpkg_installed\x64-windows\bin"),
  $PyHome
) -join ";"

$env:PATH = $pathPrefix + ";" + $env:PATH
```

## 4. Lancer KiCad en mode MCP supporte

```powershell
$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$proj     = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_pro"
$proc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
```

## 5. Resoudre l'URL IPC

Strategie recommandee:

1. Candidat prioritaire: `ipc://%TEMP%\kicad\api-<pid>.sock`
2. Candidat fallback: `ipc://%TEMP%\kicad\api.sock`
3. Scan des sockets `api*.sock` dans `%TEMP%\kicad`
4. Validation de chaque candidat via `qa_ipc_smoke ... ping`

Exemple minimal:

```powershell
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$sockDir = Join-Path $env:TEMP "kicad"
$candidates = @(
  "ipc://$sockDir\api-$($proc.Id).sock",
  "ipc://$sockDir\api.sock"
)

$url = $null
foreach($u in $candidates){
  $out = & $smokeExe $u ping 2>&1 | Out-String
  if($out -match "status=1"){ $url = $u; break }
}

if(-not $url){
  Get-ChildItem -LiteralPath $sockDir -Filter "api*.sock" -ErrorAction SilentlyContinue | ForEach-Object {
    $u = "ipc://$($_.FullName)"
    $out = & $smokeExe $u ping 2>&1 | Out-String
    if($out -match "status=1"){ $script:url = $u }
  }
}

if(-not $url){ throw "IPC URL non resolue" }
```

## 6. Commandes de base qa_ipc_smoke

```powershell
& $smokeExe $url ping
& $smokeExe $url get_project_info
& $smokeExe $url workspace_overview
& $smokeExe $url mcp_list_tools
```

## 7. Ouvrir un editeur via MCP

Exemple SCH via outil QA-only `qa_open_editor_frame`:

```powershell
$schPath = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_sch"
$inputPath = Join-Path $env:TEMP "qa_open_editor_frame_input.json"

@{ type = "sch"; filename = $schPath } |
  ConvertTo-Json -Compress |
  Set-Content -Path $inputPath -Encoding UTF8

& $smokeExe $url mcp_call_tool_args qa_open_editor_frame ("@" + $inputPath)
```

## 8. Appeler un tool MCP avec args

Mode generique:

```powershell
& $smokeExe $url mcp_call_tool_args <tool_name> @<args_json_file>
```

Exemple:

```powershell
$argsPath = Join-Path $env:TEMP "library_symbol_search_args.json"
@{
  schematic_filename = (Join-Path $BuildDir "ipc-v1-proj\v1.kicad_sch")
  query = "GND"
  library_scope = "both"
  max_results = 20
} | ConvertTo-Json -Compress | Set-Content -Path $argsPath -Encoding UTF8

& $smokeExe $url mcp_call_tool_args library_symbol_search_session ("@" + $argsPath)
```

### 8bis. Sortie `qa_ipc_smoke` â€” library stdout (**R115â€“R117**)

- **Footprint** (**R115**, actif) : **`library_footprint_search_session`** et **`library_footprint_inspect_session`** â€” pas de dump complet **`ApiResponse.Utf8DebugString()`** ; header lÃ©ger **`status=`** / **`error_message=`** ; probes footprint parsent le Struct debug (regex) ou champs scalaires.
- **Symbol** (**R116** audit, **R117** clÃ´ture) : **`library_symbol_search_session`** Ã©met dÃ©jÃ  des lignes greppables (**`search_ok=`**, **`count=`**, â€¦) ; **`library_symbol_inspect_session`** via Struct debug â€” happy path **~3 KiB**, **sous** le seuil problÃ©matique R80 (~**4 KiB**). **Pas de patch symbol** pour lâ€™instant.
- **RÃ¨gle future** : patch symbol dans **`ipc_smoke.cpp`** seulement si stdout happy path **> ~4096 B** mesurÃ© **ou** timeout harness reproductible **prouvÃ©** liÃ© au dump stdout (aprÃ¨s exclusion flake IPC / **`Invoke-SmokeBounded`** R81).

## 9. Capabilities stables

Simulation v1.1 (9 tools):

1. `simulation_directive_inspect`
2. `simulation_netlist_preview_session`
3. `simulation_model_library_inspect`
4. `simulation_preflight_readiness_session`
5. `simulation_run_status_session`
6. `simulation_run_minimal_summary_session`
7. `simulation_run_vector_catalog_session`
8. `simulation_vector_scalar_probe_session`
9. `simulation_xaxis_probe_session`

Library v1 (4 tools):

1. `library_symbol_search_session`
2. `library_symbol_inspect_session`
3. `library_footprint_search_session`
4. `library_footprint_inspect_session`

SCH Connectivity v1 (2 tools, read-only) :

1. `schematic_net_inspect_session`
2. `schematic_symbol_connectivity_session`

SCH Actions v1 (4 tools, write/action contrÃ´lÃ©e, undoable, sans auto-save) :

1. `schematic_set_symbol_value_session`
2. `schematic_set_symbol_field_session`
3. `schematic_set_label_text_session`
4. `schematic_assign_footprint_session` (**R121** release officielle)

SCH Geometry v2 (3 tools):

1. `schematic_move_symbol_session`
2. `schematic_move_label_session`
3. `schematic_create_wire_segment_session`

SCH Authoring v2 (2 tools):

1. `schematic_create_label_session`
2. `schematic_place_symbol_session`

SCH Wiring v2 (2 tools):

1. `schematic_connect_symbol_pin_session`
2. `schematic_connect_pins_l_shape_session`

SCH Symbol Editing v2 : 3 tools stables (`schematic_set_symbol_field_session`, `schematic_set_symbol_value_session`, `schematic_set_symbol_reference_session`)

1. `schematic_set_symbol_field_session`
2. `schematic_set_symbol_value_session`
3. `schematic_set_symbol_reference_session`

PCB Inspection v2 : 3 tools stables (`pcb_get_board_items_session`, `pcb_get_board_item_by_id_session`, `pcb_get_net_items_session`) :

1. `pcb_get_board_items_session`
2. `pcb_get_board_item_by_id_session`
3. `pcb_get_net_items_session`

**pcb_clearance_context v1** (**release produit PCB Clearance Context v1**, **R32** ; implÃ©mentation **R31**) â€” **`get_pcb_clearance_context_session`** : **contexte clearance** **read-only**, **pas** une DRC KiCad complÃ¨te ; **`is_true_drc=false`** ; **pas** `RunTests` ; **pas** de rapport de violations.

- EntrÃ©e : **`board_filename`** (obligatoire), **`item_id`** (obligatoire), **`other_item_id`** optionnel, **`layer`** optionnel (**`BL_*`** ou alias **`F_Cu`**, comme **`pcb_measure_distance_session`**).
- Cuivre supportÃ© V1 : pad, track, arc, via ; nets / netclasses lus via **`BOARD_CONNECTED_ITEM`** / **`NETINFO_ITEM::GetNetClass()`** (**sans** dÃ©pendre du MCP **`get_net_classes`**).
- Si **`other_item_id`** : **`geometric_distance_nm`** = distance euclidienne entre **centres de bbox** (**nm**) â€” **contexte gÃ©omÃ©trique** ; **`geometric_distance_kind`** = **`bbox_center_to_bbox_center_context_only`** â€” **pas** clearance bord-Ã -bord.
- **`required_clearance_nm`** : clearance **nominale** netclass (**`NETCLASS::GetClearance()`**, IUÃ—100â†’nm, **max** sur une paire) si **`HasClearance()`** ; sinon indicateurs **`clearance_status`** (p.ex. **`rule_unavailable`**) â€” **disponibilitÃ© / contexte**, **pas** verdict DRC officiel.
- Probes : **`targeted_pcb_clearance_context_probe.ps1`** ; fixture **`ipc_clearance_v1`** ; runner **`run_pcb_clearance_context_v1_targeted_probes.ps1`** ; famille QA **`pcb_clearance_context_v1`** â€” **intÃ©grÃ©e** au checkpoint MCP v2 (**R33**) ; contrat **context-only** inchangÃ© (**`is_true_drc=false`**, pas verdict DRC officiel).

**pcb_nearby_items_context v1** (**release R38**) â€” **`get_pcb_nearby_items_context_session`** : **liste de voisins** (contexte PCB), **pas** une DRC ; V1 = **`item_id`** + **`radius_nm`** â€” **pas** mode **point** ; distance **centre-bbox â†’ centre-bbox query**, **`distance_kind`** = **`bbox_center_to_query_nm`** ; scan linÃ©aire bornÃ© ; **`is_true_drc=false`** ; **`truncated`** si **`max_items`** ; entrÃ©es obligatoires : `board_filename`, `item_id`, `radius_nm` ; optionnel : `layer` (`BL_*` / `F_Cu`), `max_items` (dÃ©faut **25**), `include_same_net` (dÃ©faut **`true`**), `item_types` ; **`query_kind`** = **`from_item`** ; pas dâ€™appel MCP interne au clearance.
- Probes : **`targeted_pcb_nearby_items_context_probe.ps1`** ; fixture **`ipc_nearby_v1`** ; runner **`run_pcb_nearby_items_context_v1_targeted_probes.ps1`** ; famille **`pcb_nearby_items_context_v1`** **requise** pour **`mcp_v2_checkpoint_green`** (**R39**, **huit** familles) ; sÃ©mantique **context-only** inchangÃ©e.

**pcb_drc_violations_context v1** (**release produit R125**, impl. **R124**) â€” **`get_pcb_drc_violations_context_session`** : **liste violations DRC bornÃ©e** read-only pour revue IA ; **complÃ¨te** clearance/nearby (context-only) **sans** les remplacer ; **hors** checkpoint MCP v2 (**candidate stable** R125).

**Objectif** : fournir Ã  lâ€™IA une liste structurÃ©e de violations DRC (pas un verdict clearance local ni un scan de voisinage).

**EntrÃ©e** :

- `board_filename` (obligatoire, absolu, PCB ouvert)
- `max_violations` (dÃ©faut **50**, 1â€“500)
- `run_drc` (dÃ©faut **`false`**)
- `include_items` (dÃ©faut **`true`**), `include_excluded`, `test_footprints` (dÃ©faut **`false`**), `report_all_track_errors`
- filtres optionnels : `severity`, `violation_types[]`, `layer` (`BL_*` / `F_Cu`)

**Sortie â€” champs sÃ©mantiques** :

- `is_official_drc` : **`true`** uniquement si **`run_drc=true`** (rÃ©sultat engine) ; **`false`** si lecture marqueurs session
- `is_true_drc` : alias de **`is_official_drc`** en V1
- `drc_was_run` : **`true`** si **`DRC_ENGINE::RunTests`** a Ã©tÃ© exÃ©cutÃ©
- `drc_source` : `existing_markers` | `engine_run`
- `violations_count` / `returned_count` / `truncated` ; `violations[]` (type, severity, message, layer, x_nm, y_nm, item_ids, net_names, rule_name)

**Modes** :

- **`run_drc=false`** : **`BOARD::Markers()`** / **`MARKER_DRC`** ; souvent **0 violation** (marqueurs **non** sauvegardÃ©s dans `.kicad_pcb`)
- **`run_drc=true`** : collecte mÃ©moire **`DRC_ENGINE::RunTests`** ; **sans** persistance marqueurs ; **sans** mutation board

**Blockers** : `missing_or_invalid_board_filename`, `board_filename_mismatch`, `pcb_context_unavailable`, `board_not_loaded`, `drc_engine_unavailable`, `drc_engine_not_initialized`, `drc_run_failed`, `invalid_max_violations`, `invalid_layer`, `N/A`.

**Limites v1** :

1. Pas de paritÃ© garantie avec le dialogue DRC GUI
2. Pas de persistance de marqueurs DRC
3. Pas de mutation board
4. **`run_drc=true`** plus lent sur grandes cartes
5. **`test_footprints=false`** par dÃ©faut
6. Fixture **`ipc_drc_v1`** : depuis **R127**, mono-type **`clearance`** (`violations_count=1`)

**QA** : **`targeted_pcb_drc_violations_context_probe.ps1`** ; fixture **`ipc_drc_v1`** ; runner **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`** ; famille **`pcb_drc_violations_context_v1`** â€” **requise** pour **`mcp_v2_checkpoint_green`** (**R126**, **neuf** familles).

**`get_project_context_snapshot`** â€” rÃ©sumÃ© DRC opt-in (**R129**) et navigation absolue (**R130â€“R133**) :

- Tool **composÃ©** workspace ; agrÃ¨ge project/workspace/open docs/netclasses/nets ; **ne remplace pas** **`get_pcb_drc_violations_context_session`**

#### Pattern IA officiel â€” Â« Project Snapshot navigation-ready Â» (**R133**)

1. **`CallTool("get_project_context_snapshot")`** â€” dÃ©faut sans DRC (`include_drc_summary=false`).
2. Lire le chemin PCB absolu :
   - **`snapshot_summary.open_pcb_board_absolute_path`** (recommandÃ©, premier board actif), **ou**
   - **`open_documents[]`** : entrÃ©e type PCB â†’ **`board_absolute_path`** (alias cohÃ©rent : **`absolute_path`**).
3. Appeler les tools PCB/DRC avec ce chemin en paramÃ¨tre **`board_filename`** (absolu acceptÃ©) â€” ex. **`get_pcb_drc_violations_context_session`**, **`pcb_get_board_items_session`**, **`get_pcb_item_geometry_session`**.

Pour SCH : **`open_documents[].schematic_absolute_path`** ou **`absolute_path`**.

**Ne pas** utiliser **`GetWorkspaceOverview`** / IPC natif pour obtenir un chemin absolu â€” protobuf **`DocumentSpecifier.board_filename`** reste **basename-only**.

#### SÃ©mantique champs

| Champ | RÃ´le | Recommandation agent |
|-------|------|----------------------|
| **`board_filename`** / **`filename`** | Basename legacy ; conservÃ© | RÃ©trocompat ; **ne pas** supposer chemin absolu |
| **`absolute_path`** | Chemin absolu gÃ©nÃ©rique MCP | OK navigation |
| **`board_absolute_path`** | PCB | **PrÃ©fÃ©rÃ©** pour tools PCB |
| **`schematic_absolute_path`** | SCH | **PrÃ©fÃ©rÃ©** pour tools SCH |
| **`DocumentSpecifier` (IPC proto)** | Basename dans `oneof` | Clients IPC natifs : **ancien contrat** |

#### Statut protobuf (**R132/R133**)

- Ajout futur **`absolute_path = 6`** possible (proto3 additif) â€” **non retenu**.
- RÃ©ouverture : ticket **client IPC natif explicite** hors MCP.
- DÃ©tail audit : **`MCP_DECISIONS.md`** R132.

#### DRC opt-in (**R129**)

- **DÃ©faut** : **`include_drc_summary=false`** â†’ **`drc_summary_present=0`** ; pas dâ€™appel DRC
- **`include_drc_summary=true`** + mode absent â†’ **`drc_run_mode=existing_markers`** (`run_drc=false` via dÃ©lÃ©gation interne)
- **`drc_run_mode=engine_run`** : rÃ©sumÃ© compact officiel (`is_official_drc=true`) ; **coÃ»teux** â€” jamais par dÃ©faut
- Sortie **`drc_summary`** : compteurs + **`types[]`** + **`has_clearance_violations`** â€” **sans** `violations[]` ; dÃ©tail â†’ **`get_pcb_drc_violations_context_session`**
- **`drc_board_filename`** : chemin **absolu** ; sinon rÃ©solution depuis board ouvert / sibling projet
- **`snapshot_summary.open_pcb_board_absolute_path`** (**R130**) : chemin PCB ouvert **absolu** fiable (liste **`open_pcb_board_absolute_paths`**) â€” **source navigation IA recommandÃ©e**
- **`open_documents[]`** (**R131**) : champs additifs **`absolute_path`**, **`board_absolute_path`** (PCB), **`schematic_absolute_path`** (SCH) â€” **`board_filename`** basename **conservÃ©**
- **Navigation absolue (R132/R133)** : pattern complet Â§ Â« Project Snapshot navigation-ready Â» ci-dessus ; **ne pas** attendre chemin absolu dans **`GetWorkspaceOverview`** / protobuf **`DocumentSpecifier`**
- **0 violation** en mode **`existing_markers`** **â‰ ** board clean
- QA : **`targeted_project_context_snapshot_paths_probe.ps1`**, **`targeted_project_context_snapshot_drc_probe.ps1`** â€” **hors** checkpoint MCP v2

**Audit R134 â€” `project_readiness_summary` (non implÃ©mentÃ©)** :

- Objectif produit : vue compacte IA (documents ouverts, PCB absolu, DRC lanÃ§able, violations, footprints manquants, prochaines actions).
- **Option retenue (audit)** : enrichir **`get_project_context_snapshot`** avec bloc **`project_readiness_summary`** (**Option A**, opt-in progressif).
- **DÃ©faut** : mÃ©tadonnÃ©es dÃ©rivÃ©es + **`recommended_next_actions`** / **`recommended_tools`** â€” **sans** DRC, **sans** scan footprints.
- **DRC** : rÃ©utiliser **`drc_summary`** opt-in existant â€” ne pas lancer **`engine_run`** par dÃ©faut.
- **Gap footprint** : rÃ©solu **R135b** via **`schematic_footprint_readiness_session`** (read-only) + opt-in **`include_footprints_summary`** dans snapshot.
- Tools workflow existants (**`workflow_handoff_readiness_session`**, **`simulation_preflight_readiness_session`**) : pÃ©rimÃ¨tres diffÃ©rents â€” ne remplacent pas le rÃ©sumÃ© projet.
- **`project_readiness_summary`** (**R135a**) : bloc toujours prÃ©sent dans **`get_project_context_snapshot`** â€” mÃ©tadonnÃ©es dÃ©rivÃ©es + **`recommended_next_actions`** / **`recommended_tools`** ; **sans** DRC par dÃ©faut ; footprints unknown par dÃ©faut (`footprints_summary_available=false`) ; opt-in **R135b** remplit compteurs + **`sch_to_pcb_ready`** ; pas de bool **`project_ready`**
- **`schematic_footprint_readiness_session`** (**R135b/R137/R138/R141 â€” Project Readiness V1 complet**) :
  - **`scope=current_sheet`** (dÃ©faut) : scan **Ã©cran courant** ; **power symbols exclus** ; comportement R135bâ€“R139 inchangÃ©
  - **`scope=hierarchy`** (**R141**) : scan **toute la hiÃ©rarchie SCH** via **`Hierarchy().GetSymbols`** ; footprint **`GetFootprintFieldText(true, &sheetPath, false)`** ; **`sheets_count`**, **`symbols_instances_count`**, **`sheet_path`**, **`instance_path`** si **`include_sheet_paths`**
  - **`validate_footprints=false`** par dÃ©faut
  - **`sch_to_pcb_ready`** = tous scannÃ©s ont footprint **non vide**
  - **`sch_to_pcb_ready_validated`** = **`null`** sans validation ; sinon present **et** valide librairie
  - validation **`KIFACE_TEST_FOOTPRINT_LINK`** + cache footprint/appel si **`validate_footprints=true`**
  - snapshot opt-in : **`include_footprints_summary`** + **`footprints_validate=false`** + **`footprints_scope=current_sheet|hierarchy`** (**R141**)
  - actions : **`assign_missing_footprints`**, **`fix_invalid_footprints`**, **`run_drc_detail`**
- **Non-rÃ©gression R137/R138** : **`schematic_set_symbol_field_session`** renvoie **`changed=true`** aprÃ¨s mutation rÃ©ussie (fix checkpoint **`sch_symbol_editing_v2`**)
- DÃ©tail : **`MCP_DECISIONS.md`** R134 / R135a / R135b / R137 / **R138**

pcb_geometry v1 (**release produit PCB Geometry v1.3**, **R27** ; mesure distance **v1.2** **R23** ; introspection **`get_pcb_item_geometry_session`** stable checkpoint-covered ; famille **`pcb_geometry_v1`**) :

- **`pcb_measure_distance_session`** (**mesure** â€” **ne pas** utiliser pour dÃ©crire la gÃ©omÃ©trie complÃ¨te dâ€™un item) : **pointâ€“point**, **pointâ€“item**, **itemâ€“item** **`bbox_center`**, **item `pad_anchor`** (**`PCB_PAD_T`**), **item `pad_shape_origin`** (**`PCB_PAD_T`**) ; **`bbox_center`** = **`BOARD_ITEM::GetBoundingBox().Centre()`** ; **`pad_anchor`** = **`PAD::GetPosition()`** ; **`pad_shape_origin`** = **`PAD::ShapePos(layer)`** avec **`layer`** obligatoire sur lâ€™endpoint **mesure** ; **`layer`** officiel = enum protobuf **`BoardLayer`** (**`BL_F_Cu`**, â€¦) ; **alias** court **`F_Cu`** acceptÃ© ; **pas** centroÃ¯de polygone, **pas** clearance, **pas** bord-Ã -bord sur la **mesure** ; **pas** net-aware / layer-aware gÃ©nÃ©rique hors **`pad_shape_origin`** ; **`layer`** sur point ou avec **`bbox_center`** / **`pad_anchor`** â†’ **`invalid_endpoint_*`** ; **V1** / **V2** comme **v1.2** ; **`bbox_edge`** hors pÃ©rimÃ¨tre ; `dx_nm` / `dy_nm` signÃ©s ; `distance_nm` = **`llround(hypot(dx_nm, dy_nm))`** ; 1 IU = 100 nm ; sortie **`qa_ipc_smoke` bornÃ©e**.

- **`get_pcb_item_geometry_session`** (**introspection gÃ©omÃ©trique** â€” **pas** une distance, **pas** de polygone, **pas** de mutation PCB) : **`board_filename`** + **`item_id`** ; champs : bbox min/max/centre (**nm**), position logique si disponible, **`layer`** / **`net_name`** si disponibles ; pads : **`pad_anchor_*`** toujours ; **`pad_shape_origin_*`** + **`layer_used`** **uniquement** si **`layer`** optionnel (**`BL_F_Cu`** ou alias **`F_Cu`**) ; bbox pad nominale = fast-path **`ShapePos` + `GetSize`** ; complÃ¨te **`pcb_measure_distance_session`** sans le remplacer ; sortie **`qa_ipc_smoke` bornÃ©e**.

Probes : **`targeted_pcb_measure_distance_probe.ps1`**, **`targeted_pcb_item_geometry_probe.ps1`** ; fixture **`ipc_geometry_v1`** ; runner **`run_pcb_geometry_v1_targeted_probes.ps1`** ; **flake** IPC **R16** ; checkpoint MCP v2.

Profil tool `schematic_move_symbol_session`:

- action gÃ©omÃ©trique SCH contrÃ´lÃ©e
- undoable
- `dry_run` supportÃ©
- readback explicite
- restore QA vÃ©rifiÃ©
- pas de sauvegarde automatique
- dÃ©placement absolu uniquement
- `symbol_id` obligatoire
- pas de dÃ©placement automatique des fils/labels
- output position plat : `old_position_x_nm`, `old_position_y_nm`, `new_position_x_nm`, `new_position_y_nm`, `readback_position_x_nm`, `readback_position_y_nm`

Profil tool `schematic_move_label_session`:

- action gÃ©omÃ©trique SCH contrÃ´lÃ©e
- local label uniquement
- dÃ©placement absolu uniquement
- undoable
- `dry_run` supportÃ©
- readback explicite
- restore QA vÃ©rifiÃ©
- output position plat : `old_position_x_nm`, `old_position_y_nm`, `new_position_x_nm`, `new_position_y_nm`, `readback_position_x_nm`, `readback_position_y_nm`
- pas de dÃ©placement automatique des fils
- pas de sauvegarde automatique

Profil tool `schematic_create_wire_segment_session`:

- action wiring SCH contrÃ´lÃ©e
- crÃ©e un segment de fil unique
- wire uniquement (pas bus)
- segment orthogonal par dÃ©faut (`require_orthogonal=true`)
- `start`/`end` absolus en `*_nm`
- undoable
- `dry_run` supportÃ©
- readback explicite
- restore QA par suppression du wire crÃ©Ã©
- output plat
- pas d'autorouting
- pas de multi-segment
- pas de sauvegarde automatique

Profil tool `schematic_create_label_session`:

- action authoring SCH contrÃ´lÃ©e
- label local uniquement
- texte requis, non vide, bornÃ©
- position absolue en `*_nm`
- undoable
- `dry_run` supportÃ©
- readback explicite
- restore QA par suppression du label crÃ©Ã©
- output plat
- pas de global label
- pas de hierarchical label
- pas de sauvegarde automatique

Profil tool `schematic_place_symbol_session`:

- action authoring SCH contrÃ´lÃ©e
- placement d'un symbole depuis `lib_id`
- entrÃ©es principales : `lib_id`, `position.x_nm/y_nm`, `reference`, `value`
- undoable
- `dry_run` supportÃ©
- readback explicite
- restore QA par suppression du symbole crÃ©Ã©
- output plat
- pas d'auto-annotation globale
- pas d'assignation footprint automatique
- pas de sauvegarde automatique
- validation QA officielle en deux phases :
  - phase A : dry-run dans process KiCad A
  - phase B : mutation/readback/restore dans process KiCad B

Profil tool `schematic_connect_symbol_pin_session`:

- action wiring SCH contrÃ´lÃ©e pin-to-pin
- connecte deux pins via `from.symbol_id`, `from.pin_number`, `to.symbol_id`, `to.pin_number`
- v1 strict : segment unique uniquement
- v1 strict : pins alignÃ©es uniquement
- v1 strict : pas de L-shape
- v1 strict : pas d'autorouting
- pins hidden/power rejetÃ©es par dÃ©faut
- pins ambiguÃ«s refusÃ©es
- output minimal stabilisÃ©
- readback via fallback endpoints si l'UUID post-commit diffÃ¨re
- `dry_run` supportÃ©
- restore QA validÃ©

Profil tool `schematic_connect_pins_l_shape_session`:

- action wiring SCH contrÃ´lÃ©e pin-to-pin non alignÃ©e
- connecte deux pins via `from.symbol_id`, `from.pin_number`, `to.symbol_id`, `to.pin_number`
- crÃ©e deux segments orthogonaux
- `elbow_mode` supportÃ© :
  - `horizontal_then_vertical`
  - `vertical_then_horizontal`
- refuse les pins dÃ©jÃ  alignÃ©es (`pins_already_aligned`)
- pas d'autorouting
- pas d'obstacle avoidance
- junction non crÃ©Ã©e en v1 (`junction_id` vide)
- pins hidden/power rejetÃ©es par dÃ©faut
- pins ambiguÃ«s refusÃ©es
- output minimal stabilisÃ©
- readback wire via UUID puis fallback endpoints
- `dry_run` supportÃ©
- restore QA validÃ© :
  - wire1 supprimÃ©
  - wire2 supprimÃ©
  - symbole dÃ©placÃ© restaurÃ©
- pas de sauvegarde automatique

Profil tool `schematic_set_symbol_field_session`:

- action d'Ã©dition de champ symbole SCH contrÃ´lÃ©e
- modifie un champ existant d'un symbole existant
- cible le symbole via `symbol_id`
- sÃ©lection du champ via `field_id` ou `field_name`
- champs autorisÃ©s v1 :
  - champs existants non `REFERENCE`
  - champs existants non `VALUE`
  - ex : `DATASHEET`, `FOOTPRINT`, `DESCRIPTION`, custom field existant
- `REFERENCE` refusÃ© : `reference_field_forbidden_in_v1`
- `VALUE` redirigÃ© : `use_schematic_set_symbol_value_session`
- crÃ©ation de champ non supportÃ©e : `field_creation_not_supported`
- champ absent : `field_not_found`
- `expected_old_value` supportÃ©
- `dry_run` supportÃ©
- readback explicite
- **`changed=true`** aprÃ¨s mutation rÃ©ussie si la valeur a changÃ© (**fix R137/R138** ; **`false`** si dry-run ou pas de changement)
- restore QA validÃ©
- pas de sauvegarde automatique

Profil tool `schematic_set_symbol_value_session`:

- modifie le champ `Value` d'un symbole existant
- cible le symbole via `symbol_id`
- input clÃ© : `schematic_filename`, `symbol_id`, `new_value`, `expected_old_value`, `dry_run`
- supporte `dry_run`, mutation, readback, restore QA et `expected_old_value`
- blockers importants :
  - `invalid_symbol_id`
  - `symbol_not_found`
  - `value_field_not_found`
  - `expected_old_value_mismatch`
  - `mutation_failed`
- `new_value` limitÃ© Ã  1024 caractÃ¨res
- pas de sauvegarde automatique

Profil tool `schematic_assign_footprint_session` (**R121** ; **R146/R147** Safe Assign Footprint Guard ; **R161** **`hierarchy_assign_instance_safe_enabled`**) :

- assigne un footprint qualifiÃ© (`LibNick:FootprintName`) au champ Footprint d'un symbole SCH existant
- complÃ¨te le flux post-**place_symbol** avec validation bibliothÃ¨que et signal **`pcb_ready`**
- **Safe Assign Footprint Guard (R147)** : aucune mutation globale silencieuse sur symbole partagÃ© ; opt-in explicite **`allow_shared_symbol_global_update`**
- input clÃ© : `schematic_filename`, `symbol_id`, `footprint`, `dry_run`, `allow_overwrite`, `expected_current_footprint`, `validate_footprint` (dÃ©faut `true`), **`allow_shared_symbol_global_update`** (dÃ©faut **`false`**, **R146/R147**), **`scope`** (dÃ©faut **`current_sheet`**, **R143/R144**), **`instance_path`** / **`sheet_path`** si **`scope=hierarchy`**
- **`scope=current_sheet`** : mutation sur Ã©cran courant ; symbole **non partagÃ©** â†’ mutation normale ; symbole **partagÃ©** (`affected_instances_count > 1`) â†’ **`shared_symbol_global_update_blocked`** sans opt-in
- **`scope=hierarchy`** (**R160/R161**) : mutation instance-safe via **`SetFootprintFieldTextForSheet`** ; **`instance_path`** obligatoire ; **`instance_safe_write_used=true`**, **`global_update=false`****
- mutation : **`SetFootprintFieldText`** + **`SCH_COMMIT`** (pas le chemin gÃ©nÃ©rique **`set_symbol_field`**)
- validation footprint : **`KIFACE_TEST_FOOTPRINT_LINK`** via **`FACE_PCB`** si `validate_footprint=true`
- `dry_run=true` : pas de mutation ; **`changed=true`** si le footprint changerait
- `allow_overwrite=false` (dÃ©faut) : refuse si footprint dÃ©jÃ  assignÃ© (`footprint_already_assigned`)
- `expected_current_footprint` : garde-fou optimistic locking (`expected_current_footprint_mismatch`)
- `validate_footprint=false` : assignation textuelle sans preuve library (`footprint_validated=false`, `pcb_ready=false`) â€” usage exceptionnel documentÃ©
- sorties clÃ©s : `old_footprint`, `new_footprint`, `footprint_validated`, `pcb_ready`, `exact_blocker_if_any` ; **`scope=current_sheet` (R146)** : `affected_instances_count`, `shared_sheet_detected`, `global_update` ; **`scope=hierarchy`** : `instance_path`, `sheet_path`, `instance_targeted`, `affected_instances_count`
- blockers importants :
  - `invalid_symbol_id`, `symbol_not_found`, `invalid_footprint_identifier`
  - `library_not_loadable:*`, `footprint_not_found_in_library:*`
  - `footprint_already_assigned`, `expected_current_footprint_mismatch`, `mutation_failed`
  - **`shared_symbol_global_update_blocked`** (**R146/R147** â€” Ã©cran partagÃ© multi-instance sans opt-in ; **changement comportemental intentionnel**)
  - hierarchy (**R161**) : `invalid_scope`, `missing_instance_selector`, `instance_path_not_found`, … ; **`hierarchy_instance_write_unsupported`** = fallback legacy uniquement
- limites v1 :
  - pas de mutation PCB
  - pas de placement footprint sur board
  - pas d'annotation automatique
  - pas de tool MCP public **save** / **undo** dédié footprint (voir **R161 save/undo**)
- diffÃ©rence vs **`schematic_set_symbol_field_session`** (champ FOOTPRINT) :
  - `set_symbol_field` = mutation texte gÃ©nÃ©rique sans validation library
  - `assign_footprint` = contrat produit IA + validation + **`pcb_ready`**
- restore QA : fixture **`issue19090`** ; restore avec **`validate_footprint=false`** car footprint initial absent de la lib QA **`qa_resistors`**
- pas de sauvegarde automatique

**Workflow IA officiel â€” Safe Assign Footprint Guard (R147)** :

```text
1. Appeler get_project_context_snapshot avec include_footprints_summary=true, footprints_scope=hierarchy.
2. Lire les symboles sans footprint et leur instance_path.
3. Appeler schematic_assign_footprint_session en dry_run=true.
4. VÃ©rifier affected_instances_count, shared_sheet_detected, global_update.
5. Si global_update=true :
   - ne pas muter par dÃ©faut ;
   - demander confirmation utilisateur ou stratÃ©gie explicite ;
   - utiliser allow_shared_symbol_global_update=true uniquement si la mutation globale est voulue.
6. Si scope=hierarchy mutation retourne hierarchy_instance_write_unsupported :
   - ne pas forcer ;
   - utiliser workflow feuille courante seulement dans un cas non partagÃ© (affected_instances_count â‰¤ 1).
```

**Notes workflow** :

- **`dry_run=true`** expose toujours les compteurs sans blocage â€” Ã©tape obligatoire avant toute mutation sur schÃ©ma hiÃ©rarchique.
- **`affected_instances_count`** peut sur-compter de maniÃ¨re conservatrice (â‰¥2 suffit Ã  activer le garde-fou).
- Ne **jamais** contourner **`shared_symbol_global_update_blocked`** en rÃ©essayant sans dry-run â€” utiliser l'opt-in explicite ou une intervention humaine.
- Alternative Ã©tape 1â€“2 : **`schematic_footprint_readiness_session`** avec **`scope=hierarchy`** pour lister les symboles sans footprint par **`instance_path`**.

**Changement comportemental (R146 â†’ officialisÃ© R147)** : les agents/scripts qui mutaient silencieusement un symbole sur Ã©cran partagÃ© via **`scope=current_sheet`** recevront dÃ©sormais **`shared_symbol_global_update_blocked`** â€” breaking safe intentionnel.

**Workflow IA consolidÃ© â€” post-R150 (R151 officialisÃ©)** :

```text
1. get_project_context_snapshot(include_footprints_summary=true, footprints_scope=hierarchy, footprints_validate=true)
2. schematic_footprint_readiness_session(scope=hierarchy, include_sheet_paths=true)
   â†’ retenir symbol_id, instance_path, sheet_path, footprint actuel
3. get_schematic_active_sheet_context_session
   â†’ vÃ©rifier que instance_path cible apparaÃ®t dans symbols[]
   â†’ vÃ©rifier que symbol_id concorde
4. SI instance_path absent de active sheet context :
   â†’ NE PAS muter ; demander navigation / ouverture feuille
5. schematic_assign_footprint_session(scope=current_sheet, dry_run=true)
   â†’ global_update=false, affected_instances_count â‰¤ 1, exact_blocker_if_any=N/A
6. Mutation current_sheet seulement si dry-run sÃ»r (+ garde-fou R146/R147)
```

**RÃ¨gles agent R151** :

| Condition | Action |
|-----------|--------|
| **`instance_path` absent** de active sheet context | **Ne pas muter** |
| **`active_sheet_is_shared=true`** | Dry-run obligatoire ; opt-in si mutation globale |
| Hierarchy mutation | **Autorisée (R161 Workflow A)** ou conservateur R156 |

**Workflow hiÃ©rarchique â€” R152 officialisÃ©** (fixture **`ipc_footprint_readiness_v1`**) :

```text
Si instance_path cible absent de active_sheet_context.symbols[] :
  ne pas appeler assign current_sheet en mutation.
  demander navigation / ouverture de la feuille cible.

Si instance_path cible prÃ©sent :
  faire dry-run current_sheet.
  muter seulement si dry-run safe :
    exact_blocker_if_any=N/A
    global_update=false
    shared_sheet_detected=false
```

| Cas | Contexte | Attendu agent |
|-----|----------|---------------|
| Root ouvert | Cible sous-feuille via readiness | **`instance_path`** absent â†’ **ne pas muter** ; assign dry-run â†’ **`symbol_not_found`** |
| Sous-feuille (navigation produit) | **`navigate_schematic_sheet_session`** (R154/R155) | **`instance_path`** prÃ©sent â†’ dry-run **`current_sheet`** puis mutation si sÃ»r |

Probe QA : **`targeted_schematic_active_sheet_context_probe.ps1`** â€” critÃ¨res **`hierarchy_*`**. Si navigation sous-feuille impossible sans tool produit : **`skipped_navigation_tool_missing`** â†’ **R153**.

**Workflow IA avancÃ© â€” Advanced Safe Footprint Assignment (R149 â€” officialisÃ©)** :

```text
A. Appeler get_project_context_snapshot avec :
   include_footprints_summary=true
   footprints_scope=hierarchy
   footprints_validate=true

B. Identifier les symboles incomplets :
   instance_path, sheet_path, symbol_id, reference,
   footprint actuel, sch_to_pcb_ready / sch_to_pcb_ready_validated

C. Faire un dry-run :
   schematic_assign_footprint_session
   scope=hierarchy, dry_run=true,
   instance_path=..., symbol_id=..., footprint=...

D. Si mutation hierarchy retourne hierarchy_instance_write_unsupported :
   ne pas forcer la mutation.

E. Pour utiliser current_sheet :
   vÃ©rifier que l'utilisateur ou l'agent a ouvert la bonne feuille / le bon fichier ;
   get_schematic_active_sheet_context_session â€” comparer instance_path cible vs symboles actifs (R150) ;
   relancer schematic_footprint_readiness_session(scope=current_sheet) ;
   faire un dry-run current_sheet ;
   vÃ©rifier :
     affected_instances_count <= 1
     shared_sheet_detected=false
     global_update=false
     exact_blocker_if_any=N/A

F. Muter seulement si le dry-run current_sheet est sÃ»r.
```

**RÃ¨gles de sÃ©curitÃ© (R149)** :

| RÃ¨gle | Action agent |
|-------|--------------|
| **`scope=hierarchy`** | **Mutation instance-safe (R161 Workflow A)** ou dry-run preview |
| **`scope=current_sheet`** | Mutation seulement aprÃ¨s Ã©tapes Eâ€“F |
| **`symbol_not_found`** | Contexte probablement incorrect â€” **ne pas** muter ; rouvrir/naviguer feuille |
| **`global_update=true`** | **Ne pas muter** â€” demander confirmation utilisateur |
| **`allow_shared_symbol_global_update`** | Opt-in volontaire â€” **jamais** automatique |
| **`affected_instances_count`** | Peut sur-compter (conservateur) â€” bloquer si **`> 1`** sans opt-in |
| Dry-run **`current_sheet`** | **Obligatoire** avant mutation hiÃ©rarchique |

**Limites actuelles (post-R154)** :

- **`navigate_schematic_sheet_session` (R154)** fournit navigation hiÃ©rarchique produit ; **`qa_open_editor_frame`** reste QA-only.
- **`scope=current_sheet`** reste global (**SetFootprintFieldText**) — R157 ; **`scope=hierarchy`** instance-safe depuis R160/R161

**Mutation hierarchy MCP (R160 validé ; R161 officialisé)** : statut **`hierarchy_assign_instance_safe_enabled`**.

### R161 — Hierarchy Footprint Assignment enabled (mini-release documentaire)

**Statut produit** : **`hierarchy_assign_instance_safe_enabled`** — mutation **`scope=hierarchy`** instance-safe via **`SetFootprintFieldTextForSheet`**.

**Workflow A — direct hierarchy (recommandé si `instance_path` connu)** :

```text
schematic_footprint_readiness_session(scope=hierarchy)
→ choisir symbols[].instance_path (+ symbol_id)
→ schematic_assign_footprint_session(scope=hierarchy, instance_path=..., dry_run=true)
→ si dry-run OK (ok=true, global_update=false, instance_safe_write_used=false en dry-run)
→ schematic_assign_footprint_session(scope=hierarchy, instance_path=..., dry_run=false)
→ relire readiness hierarchy — vérifier instance cible changed, autres instances inchangées
→ restore si besoin : assign scope=hierarchy avec ancien footprint (+ expected_current_footprint)
```

**Workflow B — conservateur R156 (navigation + current_sheet)** :

```text
schematic_footprint_readiness_session(scope=hierarchy) — pick missing footprint instance
→ navigate_schematic_sheet_session(instance_path=..., verify_instance_path=...)
→ get_schematic_active_sheet_context_session — comparer instance_path + symbol_id
→ schematic_assign_footprint_session(scope=current_sheet, dry_run=true)
→ mutation current_sheet seulement si global_update=false, blocker N/A, policy/utilisateur autorise
```

Script : **`r156_example_navigable_sch_footprint_workflow.ps1`**.

**Règles agent R161** :

| Règle | Action |
|-------|--------|
| Cible instance précise | **Préférer `scope=hierarchy`** (Workflow A) |
| Contexte feuille incertain | Workflow B (R156) |
| **`allow_shared_symbol_global_update`** | **Jamais** pour simuler instance-safe ; **`current_sheet` only** |
| Dry-run | **Toujours** avant mutation |
| Post-mutation | Relire **readiness hierarchy** |
| Restore | **`scope=hierarchy`** + ancien footprint (vide autorisé) |

**Save / undo — limites honnêtes (R161)** :

| Niveau | Prouvé | Non promis MCP public |
|--------|--------|------------------------|
| R159 prototype QA | save/reload, undo/redo via **`qa_spike_footprint_instance_api`** | — |
| R160 MCP public | isolation runtime, readback, restore via **`schematic_assign_footprint_session`** | — |
| MCP produit | — | **Pas** de tool public **`save_project`** / **`undo`** footprint ; l'agent ne doit **pas** promettre un workflow MCP complet save/undo sans tool dédié |

**Garantie R161** : isolation instance + readback + restore via MCP public ; persistance disque = comportement KiCad standard (save utilisateur / auto-save projet).

**Discovery runtime R162** : appeler **`get_project_context_snapshot`** (sans opt-in footprint) et lire **`project_readiness_summary`** :

| Champ | Usage agent |
|-------|-------------|
| **`hierarchy_assign_instance_safe_enabled=true`** | Workflow A disponible |
| **`recommended_footprint_assignment_workflow=hierarchy_instance_safe`** | Préférer assign **`scope=hierarchy`** |
| **`fallback_footprint_assignment_workflow=navigate_current_sheet_dry_run`** | Sinon Workflow B (R156) |

**Règle agent R163** : toujours lire **`project_readiness_summary`** avant de choisir le workflow footprint. Si **`hierarchy_assign_instance_safe_enabled=true`**, préférer **`scope=hierarchy`**. Si absent ou **`false`**, utiliser le fallback **`navigate_current_sheet_dry_run`** (Workflow B / script R156).

Script reproductible runtime-driven : **`qa/tools/ipc_tools/r163_example_runtime_selected_footprint_workflow.ps1`** — summary **`qa_r163_runtime_selected_footprint_workflow_summary.txt`**.

**CI observability R164** : step **G4** dans **`run_mcp_v2_ci_baseline.ps1`** — champs **`runtime_footprint_workflow_*`** ; **hors** **`ci_baseline_green`**.

**Echo readiness R165** : si l'agent appelle directement **`schematic_footprint_readiness_session`**, lire les mêmes champs workflow (**`hierarchy_assign_instance_safe_enabled`**, **`recommended_assign_scope`**, etc.) et préférer **`scope=hierarchy`** quand **`true`**. Helper partagé avec snapshot R162.

**Dual-source R166 — priorité agent** :

| Rôle | Source |
|------|--------|
| **Principale** | **`project_readiness_summary`** (`get_project_context_snapshot`) |
| **Locale** | **`schematic_footprint_readiness_session`** (echo R165) |

Pour choisir un workflow footprint, lire d'abord **`project_readiness_summary`**. Si l'agent travaille déjà dans **`schematic_footprint_readiness_session`**, il peut utiliser l'echo local. **Si les deux sources divergent** : ne pas muter, relire snapshot, puis signaler l'incohérence.

Script cohérence read-only : **`report_footprint_workflow_discovery_consistency.ps1`** — summary **`qa_footprint_workflow_discovery_consistency_summary.txt`**.

**CI advisory R167** : step **G5** dans **`run_mcp_v2_ci_baseline.ps1`** — champs **`footprint_workflow_discovery_consistency_*`** ; **hors** **`ci_baseline_green`**.

**Politique alerte R168 (clôture R162–R167)** :

- **`ci_baseline_green=true` ne suffit pas** pour ignorer G4/G5 rouges
- G4 : surveiller **`runtime_footprint_workflow_ok`**, **`runtime_footprint_workflow_green`**, **`runtime_footprint_workflow_mutation_performed`**
- G5 : surveiller **`footprint_workflow_discovery_consistency_ok`**, **`footprint_workflow_discovery_consistency_green`**, **`footprint_workflow_discovery_consistency_scope_match_ok`**
- Divergence snapshot/readiness : **ne pas muter** ; relire **`project_readiness_summary`** ; consulter G5

Détail niveaux d'alerte : **`MCP_DECISIONS.md`** R168 ; baseline : **`README_IPC_QA_BASELINE.md`** § Politique alerte R168.

**Workflow post-footprint R169 (audit — pas encore implémenté)** :

1. Vérifier **`sch_to_pcb_ready`** via snapshot / **`schematic_footprint_readiness_session`**.
2. Si prêt côté SCH → futur **`get_pcb_sync_readiness_session`** (missing/orphan counts).
3. Avant toute mutation PCB → futur **`pcb_update_from_schematic_preview_session`** (dry-run **`BOARD_NETLIST_UPDATER`**).
4. Ne pas confondre avec **`workflow_handoff_readiness_session`** (gate workflow générique).
5. Détail : **`MCP_DECISIONS.md`** R169.

**Workflow post-footprint R170 (implémenté — read-only)** :

1. Vérifier **`sch_to_pcb_ready`** via snapshot / readiness session.
2. Appeler **`get_pcb_sync_readiness_session`** (`board_filename` + `schematic_filename`, `match_mode=reference_or_path`).
3. Si **`needs_update_from_schematic=true`** → appeler **`pcb_update_from_schematic_preview_session`** (R171 dry-run) ; **ne pas muter** autonomement sans preview validée.
4. Si **`orphan_footprints_count>0`** → **`recommended_next_action=review_orphans`**.
5. Détail : **`MCP_DECISIONS.md`** R170.

**Workflow post-footprint R171 (implémenté — dry-run preview)** :

1. Après readiness R170, appeler **`pcb_update_from_schematic_preview_session`** (`board_filename` + `schematic_filename`, `match_mode=reference_or_path`).
2. Lire **`actions[]`** et compteurs `would_*` ; vérifier **`dry_run=true`**, **`board_mutation_detected=false`**.
3. Si **`pcb_update_needed=true`** → **`recommended_next_action=review_preview_before_mutation`** ; mutation réelle **non disponible** en V1 MCP.
4. Si orphelins avec `delete_orphans=true` → **`recommended_next_action=review_orphans`**.
5. Détail : **`MCP_DECISIONS.md`** R171.

**Workflow snapshot R172 (implémenté — pcb_sync_summary opt-in)** :

1. Appeler **`get_project_context_snapshot`** avec **`include_pcb_sync_summary=true`** (défaut `false` — aucun appel sync/preview).
2. Optionnel : **`pcb_sync_include_preview=true`** pour compteurs dry-run R171 (sans `actions[]`).
3. Chemins PCB/SCH : overrides explicites ou résolution depuis snapshot (`open_pcb_board_absolute_path` + SCH ouvert).
4. Lire **`pcb_sync_summary.recommended_next_tool`** / **`recommended_next_action`** pour orienter l'IA.
5. Détail : **`MCP_DECISIONS.md`** R172.

**Workflow apply R173 / officialisé R174 (`pcb_update_apply_controlled_v1`)** — voir aussi **R177** workflow complet ci-dessous.

1. **`get_project_context_snapshot`** avec **`include_pcb_sync_summary=true`** (orientation IA ; preview snapshot optionnelle).
2. **`get_pcb_sync_readiness_session`** — état SCH↔PCB read-only.
3. **`pcb_update_from_schematic_preview_session`** — dry-run obligatoire avant toute décision apply.
4. Si preview OK **et** politique / utilisateur confirme explicitement :
   **`pcb_update_from_schematic_session`** avec **`confirm_apply=true`** (jamais automatique sans policy).
   Option **R176** : **`include_post_apply_drc_summary=true`** pour résumé DRC advisory (non bloquant).
5. **`get_pcb_sync_readiness_session`** après apply — vérifier sync.
6. **`get_pcb_drc_violations_context_session`** avec **`run_drc=true`** — DRC post-apply (ou via opt-in R176 step 4).
7. **`save_board_session`** avec **`confirm_save=true`** — persistance disque (**R176** ; séparé de l'apply).

**Mini-release R177 — `pcb_apply_drc_save_workflow_v1` (workflow IA post-footprint complet)** :

```text
1. get_project_context_snapshot(include_pcb_sync_summary=true, pcb_sync_include_preview=true)
2. pcb_update_from_schematic_preview_session
3. pcb_update_from_schematic_session(confirm_apply=true, include_post_apply_drc_summary=true)
4. get_pcb_sync_readiness_session
5. save_board_session(confirm_save=true)
6. optionnel : get_pcb_drc_violations_context_session(run_drc=true)
7. post-save : relire snapshot ou sync readiness
```

Probe exemple QA : **`r177_example_pcb_apply_drc_save_workflow.ps1`** — **hors checkpoint MCP v2** ; observabilité CI step **G6** (**R178**, champs **`pcb_apply_drc_save_workflow_*`**, **hors** **`ci_baseline_green`**).

Après un run **`run_mcp_v2_ci_baseline.ps1`**, lire les advisories avec :

```powershell
& report_mcp_v2_ci_advisories.ps1 -BuildDir $BuildDir
```

Sortie : **`qa_mcp_v2_ci_advisory_report.txt`** — champs **`advisory_overall_status`**, **`g4_status`**, **`g5_status`**, **`g6_status`**, **`release_requires_manual_review`**. Détail **R180**.

Release go/no-go (wrapper **R181**) :

```powershell
& check_mcp_v2_release_advisories.ps1 -BuildDir $BuildDir
```

Sortie : **`qa_mcp_v2_release_advisory_check_summary.txt`** — **`release_advisory_check_green`**, **`release_decision`** (`go` | `manual_review_required` | `hold_release` | `incident_qa` | `unknown`). VerifyOnly **n'est pas** un `go` release. Détail **R181**.

Release pack export (**R182**) :

```powershell
& export_mcp_v2_release_pack.ps1 -BuildDir $BuildDir
& export_mcp_v2_release_pack.ps1 -BuildDir $BuildDir -IncludeCheckpointSummary
```

Sortie : **`qa_mcp_v2_release_pack.md`** + **`qa_mcp_v2_release_pack.json`**. **R183** : option **`-IncludeCheckpointSummary`** / **`-CheckpointSummaryPath`** pour détail checkpoint 9 familles. Détail **R182** · **R183**.

Pipeline release local (**R184**) — **commande officielle release MCP v2** (**R185**) :

```powershell
$BuildDir = "c:\dev\kicad-master\build\msvc-win64-release"
$tools = "c:\dev\kicad-master\qa\tools\ipc_tools"

& "$tools\run_mcp_v2_release_pack.ps1" -BuildDir $BuildDir -SkipBuild -KillRunningKiCad
```

**Artefacts** : `qa_mcp_v2_release_pack_pipeline_summary.txt`, `qa_mcp_v2_release_pack.md`, `qa_mcp_v2_release_pack.json`, plus summaries CI/advisory/release check.

**Exit** : **0** = `release_decision=go` ; **1** = release non-go ou erreur technique.

| Contexte | Commande | Gate |
|----------|----------|------|
| **CI dev** (merge) | `run_mcp_v2_ci_baseline.ps1` | `ci_baseline_green` — G4/G5/G6 **non bloquant** |
| **Release** (publication) | **`run_mcp_v2_release_pack.ps1`** | `release_decision` via R181 + pack R182/R183 |

Détail **R184** · **R185**.

**CI/CD (R186 audit)** : CI officiel = **GitLab** (`.gitlab-ci.yml`) ; job manuel Windows **R187** ; procédure mainteneur formalisée **R189** — détail **R186** / **R189**.

**GitLab release job (R187 / R189)** : **`.gitlab/MCP-Windows-Release.yml`** — job **`mcp_v2_release_pack_windows`** (stage `release`, tag `kicad-windows-ltsc2022`, namespace `kicad`). Handoff mainteneur : **`MAINTAINER_GITLAB_MCP_V2_RELEASE_HANDOFF.md`** ; détail **R189**. Premier run runner **en attente** (`blocked_by_gitlab_access=true`).

**Règles agent (R174 / R176 / R177)** :

| Règle | Action |
|-------|--------|
| Preview | Ne **jamais** appeler apply sans preview récente (R171). |
| Confirmation | Ne **jamais** mettre **`confirm_apply=true`** sans policy / confirmation utilisateur explicite. |
| Orphelins | **`delete_orphans=false`** par défaut — ne pas supprimer sans décision explicite. |
| Save | **`save_board_session`** séparé ; **`confirm_save=true`** obligatoire ; apply ne sauvegarde **pas** automatiquement. |
| DRC post-apply | Opt-in **`include_post_apply_drc_summary`** advisory ; **ne bloque pas** apply si apply OK ; **pas de rollback** si DRC advisory échoue. |
| Post-save | Relire snapshot ou sync readiness (**R177** step 7). |
| Save / restore | Save MCP = **`save_board_session`** (R176) ; restore QA = fixture fichier — undo produit non garanti. |
| Checkpoint | Apply **hors checkpoint MCP v2** — ne pas supposer couverture CI baseline. |

**Limite R175/R176** : undo post-apply **non garanti** (effets hors `BOARD_COMMIT`) ; DRC post-apply advisory — pas de gate bloquant ; save via **`save_board_session`** (R176).

Détail : **`MCP_DECISIONS.md`** R173 (impl.) · R174 (mini-release doc) · R175 (audit) · R176 (save + DRC advisory) · R177 (workflow complet) · R178 (CI G6 observability) · R179 (alerte G6 + clôture série) · R180 (rapport advisory local) · R181 (wrapper release advisory) · R182 (export release pack MD/JSON) · R183 (checkpoint detail dans release pack) · R184 (pipeline release local) · **R185 (commande officielle release MCP v2)** · **R186 (audit CI/CD release pack)** · **R187 (job GitLab manuel Windows)** · **R188 (premier run GitLab — bloqué accès)** · **R189 (procédure mainteneur GitLab)**.

**Limite produit officialisée (R158 → R161)** :

| RÃ¨gle | Action agent |
|-------|--------------|
| **`scope=current_sheet`** | Mutation autorisÃ©e si **`global_update=false`** et dry-run safe |
| Symbole partagÃ© | BloquÃ© par dÃ©faut â€” **`shared_symbol_global_update_blocked`** |
| **`allow_shared_symbol_global_update=true`** | Opt-in **global** explicite â€” **jamais** instance-safe automatique |
| **`scope=hierarchy` mutation** | **Autorisée (R160)** — **`instance_path`** + **`instance_safe_write_used=true`**, **`global_update=false`** |
| **`scope=hierarchy` dry-run** | Diagnostic / preview alignÃ© readiness |

**Workflow B — conservateur R156 (dupliqué ci-dessus R161)** :

```text
schematic_footprint_readiness_session(scope=hierarchy) â€” pick missing footprint instance
â†’ navigate_schematic_sheet_session(instance_path=..., verify_instance_path=...)
â†’ get_schematic_active_sheet_context_session â€” comparer instance_path + symbol_id
â†’ schematic_assign_footprint_session(scope=current_sheet, dry_run=true)
â†’ mutation seulement si global_update=false, blocker N/A, et policy/utilisateur autorise
```

Script reproductible : **`qa/tools/ipc_tools/r156_example_navigable_sch_footprint_workflow.ps1`** â€” summary **`qa_r156_navigable_sch_footprint_workflow_summary.txt`**.

**RÃ¨gles agent R155/R156** :

| Condition | Action |
|-----------|--------|
| Navigation produit | **`navigate_schematic_sheet_session`** uniquement |
| **`qa_open_editor_frame`** | Harness bootstrap root OK ; **interdit** navigation hiÃ©rarchique produit |
| Navigation Ã©chouÃ©e | **Ne pas muter** |
| **`instance_path` absent** post-nav | **Ne pas muter** |
| **`global_update=true`** | **Ne pas muter** sans opt-in explicite |
| **`allow_shared_symbol_global_update=true`** | **Jamais** automatique |
| **`scope=hierarchy` mutation** | **Autorisée (R161 Workflow A)** |
| **`global_update=false`** + dry-run safe | Mutation **`current_sheet`** autorisÃ©e **aprÃ¨s** confirmation policy/utilisateur |

Profil tool `get_schematic_active_sheet_context_session` (**R150 â€” read-only**) :

- expose le contexte feuille SCH **actif** : **`active_sheet_path`**, **`active_sheet_file`**, **`active_sheet_is_shared`**
- liste bornÃ©e symboles visibles sur Ã©cran courant (**power symbols exclus**) avec **`instance_path`** / **`sheet_path`**
- input : **`schematic_filename`** (optionnel), **`include_symbols`** (dÃ©faut true), **`max_symbols`** (dÃ©faut 50, 1..500)
- **read-only** â€” aucun commit, aucune navigation
- blockers : **`sch_context_unavailable`**, **`schematic_not_loaded`**, **`schematic_filename_mismatch`**, **`active_sheet_unavailable`**, **`invalid_max_symbols`**
- usage workflow R149 Ã©tape E : confirmer **`instance_path`** cible prÃ©sent avant assign **`current_sheet`**

Profil tool `navigate_schematic_sheet_session` (**R154 â€” UI navigation, no SCH commit**) :

- focalise lâ€™Ã©diteur SCH sur une feuille hiÃ©rarchique par **`sheet_path`** (`PathAsString`) ou **`instance_path`**
- navigation rÃ©elle via **`SCH_ACTIONS::changeSheet`** â€” Ã©quivalent navigateur hiÃ©rarchie
- input : **`sheet_path`** \| **`instance_path`** (obligatoire lâ€™un des deux), **`schematic_filename`** (optionnel), **`verify_instance_path`** (optionnel), **`dry_run`**
- **`dry_run=true`** : rÃ©solution seule, pas de changement de feuille active
- sorties : **`resolved_sheet_path`**, **`active_sheet_path_before/after`**, **`context_changed`**, **`post_navigation_verified`**
- blockers : **`missing_sheet_selector`**, **`sheet_path_not_found`**, **`instance_path_not_found`**, **`post_navigation_context_mismatch`**, etc.
- usage workflow : remplacer **`qa_open_editor_frame`** avant assign footprint **`current_sheet`**

Profil tool `schematic_set_symbol_reference_session`:

- modifie la rÃ©fÃ©rence `Reference` d'un symbole existant (`REFERENCE` reste refusÃ© dans `schematic_set_symbol_field_session`)
- cible le symbole via `symbol_id`
- pÃ©rimÃ¨tre v1 : feuille courante uniquement (`CurrentSheet()`), sans dÃ©tection globale de conflit de rÃ©fÃ©rence, sans annotation globale
- input clÃ© : `schematic_filename`, `symbol_id`, `new_reference`, `expected_old_reference`, `dry_run`
- mutation alignÃ©e KiCad : `SCH_SYMBOL::SetRef(...)`
- supporte `dry_run`, mutation, readback, restore QA et `expected_old_reference`
- validation officielle non interactive du designator
- blockers importants :
  - `invalid_symbol_id`
  - `symbol_not_found`
  - `invalid_reference`
  - `expected_old_reference_mismatch`
  - `mutation_failed`
  - `readback_failed`
- pas de sauvegarde automatique

Profil tool `pcb_get_board_items_session` :

- lecture PCB du document ouvert dans le cadre Ã©diteur ; `board_filename` requis (absolu, doit correspondre au fichier courant)
- read-only ; pas de mutation ; pas de sauvegarde automatique
- sort des compteurs : `footprint_count`, `pad_count`, `track_count`, `via_count`, `zone_count`, `drawing_count`
- `items_summary` bornÃ© par `max_items` ; `item_types` optionnel ; `items_summary_truncated=true` si la liste est tronquÃ©e
- nÃ©gatifs validÃ©s : `invalid_item_type`, `board_filename_mismatch`

Profil tool `pcb_get_board_item_by_id_session` :

- lecture dÃ©taillÃ©e ciblÃ©e dâ€™un item PCB existant (pas le payload brut `get_pcb_items_by_id`)
- cible via `board_filename` + `item_id`
- read-only ; pas de mutation ; pas de sauvegarde automatique
- rÃ©solution PCB : `BOARD::ResolveItem(KIID, true)` aprÃ¨s validation de forme (`KIID::SniffTest`) pour distinguer `invalid_item_id` vs absence
- distingue : `invalid_item_id`, `item_not_found`, `unsupported_item_type`
- types v1 supportÃ©s : footprint, pad, track, arc, via, zone, drawing (drawings au niveau board uniquement pour les primitives graphiques)
- retourne notamment : `item_type`, `layer`, `x_nm`, `y_nm`, `net_name`, `parent_footprint_id`, `reference`, `value`, `details`
- cas nÃ©gatifs validÃ©s : `invalid_item_id`, `item_not_found`, `board_filename_mismatch`

Profil tool `pcb_get_net_items_session` :

- lecture des items PCB associÃ©s Ã  un **net** du document ouvert ; `board_filename` requis (absolu)
- sÃ©lection du net par **`net_name`** et/ou **`net_code`** (au moins lâ€™un) ; si les deux sont fournis, **cohÃ©rence vÃ©rifiÃ©e** (`net_selector_mismatch` si conflit)
- **`net_name`** : correspondance **exacte** via `BOARD::FindNet(wxString)` ; code via `BOARD::FindNet(int)`
- **`net_code=0` refusÃ©** en v1 (`invalid_net_selector`)
- read-only ; pas de mutation ; pas de sauvegarde automatique
- pÃ©rimÃ¨tre : pads, tracks, arcs, vias, zones cuivre connectÃ©es au net (`track_count` agrÃ¨ge traces et arcs ; lignes `items_summary` avec `item_type` distinct `track` / `arc`)
- sort des compteurs : `pad_count`, `track_count`, `via_count`, `zone_count` ; `items_summary` bornÃ© par `max_items` ; `items_summary_truncated=true` si troncature
- blockers importants : `invalid_net_selector`, `net_not_found`, `net_selector_mismatch`, `board_filename_mismatch`, `invalid_max_items`, `read_failed`
- cas nÃ©gatifs validÃ©s : absence de sÃ©lecteur net, `net_code=0`, nom de net inventÃ©, mismatch `board_filename`
- cas `net_selector_mismatch` : peut rester **skipped** dans les probes contractuels sur la fixture IPC v1 actuelle faute de deux nets nommÃ©s exploitables

## 10. Preuves QA

Artefacts de preuve recommandes:

- `qa_library_v1_family_summary.txt`
- `qa_library_*_contract_summary.txt`
- `qa_simulation_*_contract_summary.txt`

Pour Library v1, les indicateurs cibles dans le summary famille sont:

- `library_v1_family_green=true`
- `exact_blocker_if_any=N/A`

Pour SCH Geometry v2, les indicateurs cibles dans le summary famille sont:

- `sch_geometry_v2_family_green=true`
- `schematic_move_symbol_ok=true`
- `schematic_move_label_ok=true`
- `schematic_create_wire_segment_ok=true`
- `exact_blocker_if_any=N/A`

Pour SCH Authoring v2, les indicateurs cibles dans le summary famille sont:

- `sch_authoring_v2_family_green=true`
- `schematic_create_label_ok=true`
- `schematic_place_symbol_ok=true`
- `exact_blocker_if_any=N/A`

TÃ©lÃ©mÃ©trie **advisory-only** (R103/R104) â€” **non gating** :

- Famille : **`schematic_place_symbol_kicad_exited_post_mutation_detected`**, **`schematic_place_symbol_cleanup_restart_*`**, **`schematic_place_symbol_*_retry_extra_total`**
- Checkpoint : **`sch_authoring_place_symbol_kicad_exited_post_mutation_detected`**, **`sch_authoring_place_symbol_cleanup_restart_*`**, **`sch_authoring_place_symbol_*_retry_extra_total`**

**`kicad_exited_post_mutation_detected=true`** signifie que KiCad a quittÃ© aprÃ¨s mutation rÃ©ussie ; le probe peut rester vert si R102 a rÃ©cupÃ©rÃ©. Seuils dâ€™alerte : ponctuel â†’ conserver artefacts ; **â‰¥ 2/10** runs place_symbol ou **â‰¥ 1** checkpoint CI/jour â†’ investiguer crash ; avec blocker **`restore_connection_refused`** / **`connection_refused`** â†’ incident QA. Voir **`README_IPC_QA_BASELINE.md`** Â§7bis.

Script read-only **R105** : **`report_sch_authoring_advisory.ps1`** â€” lit **`qa_mcp_v2_checkpoint_summary.txt`** (dernier checkpoint ou **`-CheckpointRunDir`**) ; **`Write-Warning`** si advisory/retry/cleanup selon R104 ; rÃ©sumÃ© **`qa_sch_authoring_advisory_report_summary.txt`** ; **exit 0** mÃªme avec warnings ; **hors** gate **`mcp_v2_checkpoint_green`**. **R106** : invoquÃ© automatiquement par **`run_mcp_v2_ci_baseline.ps1`** aprÃ¨s checkpoint enfant (champs **`sch_authoring_advisory_*`** dans **`qa_mcp_v2_ci_baseline_summary.txt`**, **hors** **`ci_baseline_green`** ; **`-VerifyOnly`** : **`N/A`**). **R107** : **`-ScanLastNCheckpoints N`** agrÃ¨ge les N derniers **`*-mcp-v2-checkpoint`** et applique les seuils frequence R104 (>=2 exits / >=1 exit jour UTC) ; incompatible avec **`-CheckpointRunDir`**. **R108** : le CI baseline appelle aussi **`-ScanLastNCheckpoints 10`** aprÃ¨s le report unique (champs **`sch_authoring_advisory_scan_*`**, **hors** gate green).

**ClÃ´ture R109 (sÃ©rie R100â€“R108)** : stabilisation harness **`schematic_place_symbol`** + observability advisory **non bloquante**. Champs CI : **`sch_authoring_advisory_report_*`** (checkpoint courant) et **`sch_authoring_advisory_scan_*`** (frequence sur N derniers checkpoints, **`sch_authoring_advisory_scan_last_n`**, dÃ©faut **10**, **R110** configurable). Advisory false/0 â†’ rien Ã  faire ; ponctuel vert â†’ artefacts ; frequence â†’ investiguer eeschema ; blocker IPC final â†’ incident QA. Limites : legacy pre-R103 en scan, VerifyOnly sans report. Voir **`README_IPC_QA_BASELINE.md`** Â§7ter.

Pour SCH Actions v1, les indicateurs cibles dans le summary famille sont :

- `sch_actions_v1_family_green=true`
- `schematic_set_symbol_value_ok=true`
- `schematic_set_symbol_field_ok=true`
- `schematic_set_label_text_ok=true`
- `schematic_assign_footprint_ok=true`
- `exact_blocker_if_any=N/A`

Runner/summary famille SCH Actions v1 :

- `qa/tools/ipc_tools/run_sch_actions_v1_targeted_probes.ps1`
- `qa_sch_actions_v1_family_summary.txt`
- probes contractuels :
  - `qa_schematic_set_symbol_value_session_contract_summary.txt`
  - `qa_schematic_set_symbol_field_session_contract_summary.txt`
  - `qa_schematic_set_label_text_session_contract_summary.txt`
  - `qa_schematic_assign_footprint_session_contract_summary.txt`

Pour SCH Wiring v2, les indicateurs cibles dans le summary famille sont:

- `sch_wiring_v2_family_green=true`
- `schematic_connect_symbol_pin_ok=true`
- `schematic_connect_pins_l_shape_ok=true`
- `exact_blocker_if_any=N/A`
- `schematic_connect_symbol_pin_exit_code=0`
- `schematic_connect_pins_l_shape_exit_code=0`

Pour SCH Symbol Editing v2, les indicateurs cibles dans le summary famille sont:

- `sch_symbol_editing_v2_family_green=true`
- `schematic_set_symbol_field_ok=true`
- `schematic_set_symbol_value_ok=true`
- `schematic_set_symbol_reference_ok=true`
- `exact_blocker_if_any=N/A`
- `schematic_set_symbol_field_exit_code=0`
- `schematic_set_symbol_value_exit_code=0`
- `schematic_set_symbol_reference_exit_code=0`

Pour PCB Inspection v2, les indicateurs cibles dans le summary famille sont :

- `pcb_inspection_v2_family_green=true`
- `pcb_get_board_items_ok=true`
- `pcb_get_board_item_by_id_ok=true`
- `pcb_get_net_items_ok=true`
- `exact_blocker_if_any=N/A`
- `pcb_get_board_items_exit_code=0`
- `pcb_get_board_item_by_id_exit_code=0`
- `pcb_get_net_items_exit_code=0`

Probes contractuels enfants :

- `qa_pcb_get_board_items_session_contract_summary.txt`
- `qa_pcb_get_board_item_by_id_session_contract_summary.txt`
- `qa_pcb_get_net_items_session_contract_summary.txt`

Runner/summary famille SCH Geometry v2:

- `qa/tools/ipc_tools/run_sch_geometry_v2_targeted_probes.ps1`
- `qa_sch_geometry_v2_family_summary.txt`

Runner/summary famille SCH Authoring v2:

- `qa/tools/ipc_tools/run_sch_authoring_v2_targeted_probes.ps1`
- `qa_sch_authoring_v2_family_summary.txt`

Runner/summary famille SCH Wiring v2:

- `qa/tools/ipc_tools/run_sch_wiring_v2_targeted_probes.ps1`
- `qa_sch_wiring_v2_family_summary.txt`

Runner/summary famille SCH Symbol Editing v2:

- `qa/tools/ipc_tools/run_sch_symbol_editing_v2_targeted_probes.ps1`
- `qa_sch_symbol_editing_v2_family_summary.txt`
- probes contractuels :
  - `qa_schematic_set_symbol_field_session_contract_summary.txt`
  - `qa_schematic_set_symbol_value_session_contract_summary.txt`
  - `qa_schematic_set_symbol_reference_session_contract_summary.txt`

Runner/summary famille PCB Inspection v2 :

- `qa/tools/ipc_tools/run_pcb_inspection_v2_targeted_probes.ps1`
- `qa_pcb_inspection_v2_family_summary.txt`
- probes contractuels :
  - `qa_pcb_get_board_items_session_contract_summary.txt`
  - `qa_pcb_get_board_item_by_id_session_contract_summary.txt`
  - `qa_pcb_get_net_items_session_contract_summary.txt`

## 11. Limitations

- `qa_open_editor_frame` est QA-only (orchestration de test, pas API produit publique).
- Le chemin `--frame` est diagnostic-only et non-gating pour MCP.
- Les tools beta ne sont pas garantis au meme niveau que les tools stables v1.
- Crash open-frame direct connu sur `kicad.exe --frame sch <file.kicad_sch>`; hors chemin supporte MCP, correction runtime differee.

## 12. Checkpoint MCP v1: profils `smoke|core|full`

Runner global:

- `qa/tools/ipc_tools/run_mcp_v1_checkpoint.ps1`

Summary global:

- `qa_mcp_v1_checkpoint_summary.txt`

Commandes:

```powershell
& "c:\dev\kicad-master\qa\tools\ipc_tools\run_mcp_v1_checkpoint.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release" -Profile smoke

& "c:\dev\kicad-master\qa\tools\ipc_tools\run_mcp_v1_checkpoint.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release" -Profile core

& "c:\dev\kicad-master\qa\tools\ipc_tools\run_mcp_v1_checkpoint.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release" -Profile full
```

Profils:

1. `smoke`
   - Inclus: Simulation v1.1, SCH Actions v1
   - Skipped: SCH Connectivity v1, Library v1
   - Usage recommande: gate rapide / PR / validation de base
2. `core`
   - Inclus: Simulation v1.1, SCH Connectivity v1, SCH Actions v1
   - Skipped: Library v1
   - Usage recommande: validation schematique plus complete
3. `full`
   - Inclus: Simulation v1.1, Library v1, SCH Connectivity v1, SCH Actions v1
   - Usage recommande: validation complete / release / diagnostic lourd

Etat terrain actuel:

- `smoke`: vert
- `core`: vert
- `full`: vert (`mcp_v1_full_checkpoint_green=true`, `mcp_v1_checkpoint_green=true`)

Precision `core`:

- Couverture effective: Simulation v1.1 + SCH Connectivity v1 + SCH Actions v1
- Library explicitement exclue:
  - `library_v1_included=false`
  - `library_v1_ok=skipped`

Stabilisation SCH Connectivity:

- Les probes cibles utilisent une fixture SCH stable `qa/data/pcbnew/issue19090/issue19090.kicad_sch`.

Library v1 gating actuel:

- Le gate Library v1 valide:
  - `symbol_search_ok=true`
  - `symbol_inspect_ok=true`
  - `footprint_search_ok=true` (**R82**)
  - `footprint_search_board_filename_negative_green=true` (**R83**)
  - `footprint_inspect_ok=true`
  - `footprint_inspect_board_filename_negative_green=true`
- Champs summary associes:
  - `footprint_search_ok`
  - `footprint_search_diagnostic_ok`
  - `footprint_search_gating=true`
  - `footprint_search_board_filename_negative_gating=true`
  - `exact_blocker_footprint_search_if_any`
  - `footprint_search_board_filename_invalid_path_ok` (advisory wx permissif)

Champs summary a lire en priorite:

1. `mcp_v1_checkpoint_profile`
2. `mcp_v1_smoke_checkpoint_green`
3. `mcp_v1_core_checkpoint_green`
4. `mcp_v1_full_checkpoint_green`
5. `mcp_v1_checkpoint_green`
6. `library_v1_included`
7. `sch_connectivity_v1_included`

Regle de compatibilite:

- `mcp_v1_checkpoint_green` depend du profil actif.
- `smoke`: `mcp_v1_checkpoint_green = mcp_v1_smoke_checkpoint_green`
- `core`: `mcp_v1_checkpoint_green = mcp_v1_core_checkpoint_green`
- `full`: `mcp_v1_checkpoint_green = mcp_v1_full_checkpoint_green`

Important:

- Un `smoke` vert ne signifie pas que Library ou SCH Connectivity sont vertes.
- Dans `smoke`, ces familles sont explicitement `skipped`, pas supprimees.
- `core` et `full` restent utiles pour les diagnostics lourds.

## 13. Frontiere checkpoint v1 vs familles v2

- `SCH Geometry v2` est une famille MCP v2 indÃ©pendante.
- `SCH Authoring v2` est une famille MCP v2 indÃ©pendante.
- `SCH Wiring v2` est une famille MCP v2 indÃ©pendante.
- `SCH Symbol Editing v2` est une famille MCP v2 indÃ©pendante.
- `PCB Inspection v2` est une famille MCP v2 indÃ©pendante : 3 tools stables (`pcb_get_board_items_session`, `pcb_get_board_item_by_id_session`, `pcb_get_net_items_session`).
- `pcb_geometry v1` est une famille MCP v2 indÃ©pendante : 2 tools stables (`pcb_measure_distance_session`, `get_pcb_item_geometry_session`).
- Elles ne sont pas incluses dans `run_mcp_v1_checkpoint.ps1`.
- Le checkpoint MCP v1 global reste sÃ©parÃ© tant qu'une intÃ©gration explicite n'est pas dÃ©cidÃ©e.
- Le checkpoint MCP v2 agrÃ¨ge les familles MCP v2 stables (`SCH Geometry v2`, `SCH Authoring v2`, `SCH Wiring v2`, `SCH Symbol Editing v2`, `PCB Inspection v2`, `pcb_geometry v1`). La famille `SCH Symbol Editing v2` compte 3 tools stables (`schematic_set_symbol_field_session`, `schematic_set_symbol_value_session`, `schematic_set_symbol_reference_session`). La famille `PCB Inspection v2` compte 3 tools stables (`pcb_get_board_items_session`, `pcb_get_board_item_by_id_session`, `pcb_get_net_items_session`). La famille `pcb_geometry v1` compte 2 tools stables (`pcb_measure_distance_session`, `get_pcb_item_geometry_session`).

## 14. Checkpoint MCP v2 (sÃ©parÃ©)

Runner global v2:

- `qa/tools/ipc_tools/run_mcp_v2_checkpoint.ps1`

Summary global v2:

- `qa_mcp_v2_checkpoint_summary.txt`

Commande:

```powershell
& "c:\dev\kicad-master\qa\tools\ipc_tools\run_mcp_v2_checkpoint.ps1" -BuildDir "c:\dev\kicad-master\build\msvc-win64-release"
```

Familles incluses actuellement :

- `SCH Geometry v2`
- `SCH Authoring v2`
- `SCH Wiring v2`
- `SCH Symbol Editing v2` (3 tools stables : field, value, reference)
- `PCB Inspection v2` (3 tools stables : `pcb_get_board_items_session`, `pcb_get_board_item_by_id_session`, `pcb_get_net_items_session`)
- `pcb_geometry v1` (2 tools stables : `pcb_measure_distance_session`, `get_pcb_item_geometry_session`) â€” release produit documentÃ©e **PCB Geometry v1.3** (**R27**)

Famille **PCB Clearance Context v1** (**release R32**, gate **R33**) : **`pcb_clearance_context v1`** â€” 1 tool stable (`get_pcb_clearance_context_session`) ; runner **`run_pcb_clearance_context_v1_targeted_probes.ps1`** ; **requise** pour **`mcp_v2_checkpoint_green`** ; sÃ©mantique **context-only** (**pas** DRC officielle).

Famille **PCB Nearby Items Context v1** (**release R38**, gate checkpoint **R39**) : **`pcb_nearby_items_context v1`** â€” 1 tool stable (`get_pcb_nearby_items_context_session`) ; probes **`targeted_pcb_nearby_items_context_probe.ps1`** + runner **`run_pcb_nearby_items_context_v1_targeted_probes.ps1`** ; **requise** pour **`mcp_v2_checkpoint_green`** avec les sept autres familles (**huit** au total, **R39**) ; sÃ©mantique **context-only** (**pas** DRC, **`is_true_drc=false`**) inchangÃ©e.

Famille **PCB DRC Violations Context v1** (**release R125**, gate **R126**) : **`pcb_drc_violations_context v1`** â€” 1 tool stable (`get_pcb_drc_violations_context_session`) ; runner **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`** ; **requise** pour **`mcp_v2_checkpoint_green`** (**neuf** familles, **R126**).

Champs summary Ã  lire :

1. `mcp_v2_checkpoint_green`
2. `sch_geometry_v2_ok`
3. `sch_authoring_v2_ok`
4. `sch_wiring_v2_ok`
5. `sch_symbol_editing_v2_ok`
6. `pcb_inspection_v2_ok`
7. `pcb_geometry_v1_ok`
8. `pcb_clearance_context_v1_ok` (**R33**)
9. `pcb_nearby_items_context_v1_ok` (**R39**)
10. `pcb_drc_violations_context_v1_ok` (**R126**)
11. `exact_blocker_if_any`
11. `sch_geometry_v2_target_run_dir`
12. `sch_authoring_v2_target_run_dir`
13. `sch_wiring_v2_target_run_dir`
14. `sch_symbol_editing_v2_target_run_dir`
15. `pcb_inspection_v2_target_run_dir`
16. `pcb_geometry_v1_target_run_dir`
17. `pcb_clearance_context_v1_target_run_dir` (**R33**)
18. `pcb_nearby_items_context_v1_target_run_dir` (**R39**)
19. `sch_geometry_v2_exit_code`
20. `sch_authoring_v2_exit_code`
21. `sch_wiring_v2_exit_code`
22. `sch_symbol_editing_v2_exit_code`
23. `pcb_inspection_v2_exit_code`
24. `pcb_geometry_v1_exit_code`
25. `pcb_clearance_context_v1_exit_code` (**R33**)
26. `pcb_nearby_items_context_v1_exit_code` (**R39**)

CritÃ¨res green :

- `mcp_v2_checkpoint_green=true`
- `sch_geometry_v2_ok=true`
- `sch_authoring_v2_ok=true`
- `sch_wiring_v2_ok=true`
- `sch_symbol_editing_v2_ok=true`
- `pcb_inspection_v2_ok=true`
- `pcb_geometry_v1_ok=true`
- `pcb_clearance_context_v1_ok=true` (**R33**)
- `pcb_nearby_items_context_v1_ok=true` (**R39**)
- `pcb_drc_violations_context_v1_ok=true` (**R126**)
- `exact_blocker_if_any=N/A`

Format blocker :

- prioritÃ© 1 (`SCH Geometry v2`) : `exact_blocker_if_any=sch_geometry_v2:<blocker>`
- prioritÃ© 2 (`SCH Authoring v2`) : `exact_blocker_if_any=sch_authoring_v2:<blocker>`
- prioritÃ© 3 (`SCH Wiring v2`) : `exact_blocker_if_any=sch_wiring_v2:<blocker>`
- prioritÃ© 4 (`SCH Symbol Editing v2`) : `exact_blocker_if_any=sch_symbol_editing_v2:<blocker>`
- prioritÃ© 5 (`PCB Inspection v2`) : `exact_blocker_if_any=pcb_inspection_v2:<blocker>`
- prioritÃ© 6 (`pcb_geometry v1`) : `exact_blocker_if_any=pcb_geometry_v1:<blocker>`
- prioritÃ© 7 (`pcb_clearance_context v1`, **R33**) : `exact_blocker_if_any=pcb_clearance_context_v1:<blocker>`
- prioritÃ© 8 (`pcb_nearby_items_context v1`, **R39**) : `exact_blocker_if_any=pcb_nearby_items_context_v1:<blocker>`
- prioritÃ© 9 (`pcb_drc_violations_context v1`, **R126**) : `exact_blocker_if_any=pcb_drc_violations_context_v1:<blocker>`
