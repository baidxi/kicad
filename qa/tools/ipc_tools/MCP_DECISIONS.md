# Décisions MCP KiCad (journal)

Ce fichier enregistre des décisions d’ingénierie et de gouvernance pour le travail MCP dans ce dépôt. Une entrée = problématique, choix, contexte minimal, date.

Format obligatoire par entrée :

- **Problématique** :
- **Choix retenu** :
- **Contexte / alternatives écartées** (optionnel) :
- **Date** :

---

## R1 — Baseline sans traçabilité Git locale

- **Problématique** : Comment reprendre le projet MCP KiCad alors que le workspace inspecté ne contient pas de dossier `.git` ?
- **Choix retenu** : On bloque l’ajout de nouvelles fonctionnalités tant que la traçabilité Git, le build et un checkpoint QA récent ne sont pas rétablis.
- **Contexte / alternatives écartées** : Continuer à coder sans historique ni diff amont ; décision refusée tant que la base n’est pas sécurisée.
- **Date** : 2026-05-02

## R3 — Stabilisation `qa_open_editor_frame` (harnais + outil QA)

- **Problématique** : Pourquoi le checkpoint MCP v2 est-il rouge alors que les handlers métier SCH et PCB ne semblent pas atteints ?
- **Choix retenu** : On stabilise d’abord `qa_open_editor_frame`, car les échecs observés viennent de l’ouverture des éditeurs et non des handlers métier.
- **Contexte / alternatives écartées** : Élargir les timeouts uniquement sans corriger la séquence projet/schéma ni attendre `get_project_info` ; décision refusée comme contournement fragile. Ne pas modifier `schematic_move_symbol_session` ni les sessions PCB d’inspection.
- **Date** : 2026-05-02

## R4 — Rebuild pour valider `qa_open_editor_frame` (PCB)

- **Problématique** : Comment valider le correctif R3 alors que le probe PCB échoue encore avec l’ancien binaire ?
- **Choix retenu** : On recompile d’abord `kicommon` et `kicad`, puis on relance les probes PCB avec diagnostics activés avant toute modification métier.
- **Contexte / alternatives écartées** : Interpreter les timeouts sans rebuild ; refusé car le message MCP dépend du code dans `kicommon.dll`.
- **Date** : 2026-05-02

## R5 — Diagnostic chargement pcbnew / `Kiway.Player` null

- **Problématique** : Pourquoi le probe PCB échoue-t-il encore après rebuild alors que le projet est chargé ?
- **Choix retenu** : On diagnostique d’abord le chargement pcbnew/KIFACE et `Kiway.Player` avant toute modification des handlers MCP PCB.
- **Contexte / alternatives écartées** : Modifier tout de suite les sessions `pcb_get_*` ; refusé tant que `KiFACE`/`Player` ne sont pas compris.
- **Date** : 2026-05-02

## R6 — Instrumentation KiFACE / `Player` (`KICAD_IPC_STARTUP_DIAG`)

- **Problématique** : Comment identifier précisément pourquoi `KiFACE(FACE_PCB)` échoue avant d’être mis en cache ?
- **Choix retenu** : On ajoute un diagnostic conditionnel dans `KIWAY::KiFACE` et `KIWAY::Player`, activé par `KICAD_IPC_STARTUP_DIAG`, avant toute modification des handlers MCP PCB.
- **Contexte / alternatives écartées** : Deviner à partir du seul message MCP « Player null » ; refusé après R5.
- **Date** : 2026-05-02

## R7 — Dépendances runtime `_pcbnew.dll` vs `_eeschema.dll`

- **Problématique** : Quelle dépendance ou condition runtime empêche Windows de charger `_pcbnew.dll` alors que `_eeschema.dll` charge correctement ?
- **Choix retenu** : On diagnostique les dépendances directes (dumpbin) et le chargement `LoadLibrary` dans le PATH réel du harness, les différences par rapport à eeschema, et l’absence de `Dependencies.exe` / analyse transitive, **avant** toute modification des handlers MCP PCB.
- **Contexte / alternatives écartées** : Corriger le code MCP ou le PATH « au hasard » sans `GetLastError` ni diff d’import ; refusé tant que l’échec n’est pas classé (126 vs 127, etc.).
- **Date** : 2026-05-02

## R8 — Symbole MCP manquant (`ERROR_PROC_NOT_FOUND` / 127) + rebuild pcbnew

- **Problématique** : Quel symbole importé par `_pcbnew.dll` ou par une dépendance provoque `ERROR_PROC_NOT_FOUND=127` ?
- **Choix retenu** : On identifie la DLL et le nom décoré exact via une passe **dumpbin /imports** vs **dumpbin /exports** (script automatisé ; `Dependencies.exe` indisponible), puis on reconstruit **`pcbnew_kiface`** (et les artefacts statiques bloquants le lien, p.ex. **altium2pcbnew.lib**) pour réaligner `_pcbnew.dll` sur `kicommon.dll` et les sources — **sans** modifier les handlers MCP PCB.
- **Contexte / alternatives écartées** : Réécrire les sessions `pcb_get_*` tant que le KIFACE ne charge pas ; refusé. Supposer une DLL système manquante alors que `GetLastError=127` ; écarté après preuve d’import décoré absent des exports de `kicommon.dll`.
- **Date** : 2026-05-02

## R9 — Revalidation QA MCP v2 après correction `_pcbnew.dll`

- **Problématique** : Comment vérifier que la correction de l’artefact `_pcbnew.dll` restaure réellement le baseline MCP v2 ?
- **Choix retenu** : On relance d’abord la famille **PCB Inspection v2**, puis les familles SCH **geometry / authoring / wiring / symbol_editing** v2, puis le **checkpoint MCP v2 complet** (`run_mcp_v2_checkpoint.ps1`) avant toute nouvelle fonctionnalité — sans modifier les handlers métier.
- **Contexte / alternatives écartées** : Déclarer le baseline « vert » sur la seule base des trois probes PCB ciblées R8 ; refusé — il faut la famille PCB Inspection v2 et le checkpoint agrégé.
- **Date** : 2026-05-02

## R10 — Smoke `LoadLibrary` KIFACE (garde-fou QA)

- **Problématique** : Comment éviter qu’un `_pcbnew.dll` désynchronisé fasse réapparaître un échec `Kiway.Player` null difficile à diagnostiquer ?
- **Choix retenu** : On ajoute **`Test-IpcKifaceDllLoad`** dans `ipc_qa_common.ps1`, activé uniquement par **`KICAD_IPC_KIFACE_SMOKE=1`**, et appelé au **début** de `run_mcp_v2_checkpoint.ps1` après **`Initialize-IpcQaEnvironment`** : `LoadLibraryW` sur `_pcbnew.dll` et `_eeschema.dll`, arrêt immédiat avec résumé **`exact_blocker_if_any=kiface_dll_loadlibrary_failed`** si échec — sans toucher aux handlers métier.
- **Contexte / alternatives écartées** : Exécuter le smoke à chaque probe ciblé (ralentissement, bruit) ; refusé. Remplacer les diagnostics `KICAD_IPC_STARTUP_DIAG` dans `kiway.cpp` ; refusé tant que le smoke ne couvre pas tous les scénarios de prod.
- **Date** : 2026-05-02

## R11 — Cadrage `pcb_measure_distance_session` (audit avant implémentation)

- **Problématique** : Quelle capacité PCB ajouter après restauration du baseline MCP v2 sans risquer de casser la base QA ?
- **Choix retenu** : On cadre d’abord **`pcb_measure_distance_session`** par **audit** des patterns PCB Inspection v2 existants et des **helpers KiCad** (`BOARD`, `ResolveItem`, `BOX2I`, `VECTOR2I::EuclideanNorm`, nm via IU) avant toute implémentation ; première version recommandée **point–point en nm** (Option A), puis extension **KIID + mode** si besoin métier.
- **Contexte / alternatives écartées** : Implémenter directement un tool hybride complexe (modes bbox/pad) avant un MVP ; refusé pour limiter les régressions QA et les ambiguïtés géométriques.
- **Date** : 2026-05-02

## R12 — `pcb_measure_distance_session` V1 point–point (avant agrégation checkpoint)

- **Problématique** : Comment ajouter une première capacité de mesure PCB sans fragiliser le checkpoint MCP v2 restauré ?
- **Choix retenu** : On implémente une V1 point–point de `pcb_measure_distance_session`, testée par un probe ciblé (`targeted_pcb_measure_distance_probe.ps1`) avant toute intégration au checkpoint global.
- **Contexte / alternatives écartées** : Greffer l’outil au checkpoint MCP v2 ou à la famille PCB Inspection v2 sans probe dédié ; refusé tant que le contrat sortie et la conversion nm/IU ne sont pas validés isolément.
- **Date** : 2026-05-02

## R13 — Promotion QA `pcb_geometry_v1` et agrégation checkpoint MCP v2

- **Problématique** : Comment intégrer `pcb_measure_distance_session` dans la QA officielle sans fragiliser le checkpoint MCP v2 ?
- **Choix retenu** : On crée une famille **`pcb_geometry_v1`** dédiée (`run_pcb_geometry_v1_targeted_probes.ps1`), on y valide la V1 point-point (probe renforcé), puis on l’ajoute au checkpoint MCP v2 lorsque la famille est verte.
- **Contexte / alternatives écartées** : Promouvoir l’outil sans runner famille ni critères `exact_blocker_if_any` alignés sur les autres familles ; refusé.
- **Date** : 2026-05-02

## R14 — `pcb_measure_distance_session` V2 item-to-item (centre bbox)

- **Problématique** : Comment rendre `pcb_measure_distance_session` plus utile à l’IA sans introduire une géométrie ambiguë ou fragile ?
- **Choix retenu** : On ajoute uniquement un mode **item-to-item** avec position = **centre de la bounding box** (`BOARD_ITEM::GetBoundingBox().Centre()`), tout en conservant la **V1 point-point** et la famille QA **pcb_geometry_v1**.
- **Contexte / alternatives écartées** : clearance, bord à bord, pad centre dédié, distance layer-aware ; refusé en V2 pour limiter la complexité et les faux positifs QA.
- **Note QA / harness** : Le probe `targeted_pcb_measure_distance_probe.ps1` utilise deux items **indépendants** (`pads` + `drawings` sur la fixture ipc-v1) pour un contrat léger et déterministe ; voir **R15** pour le diagnostic « timeout ~120 s » vs paires parent/enfant.
- **Date** : 2026-05-02

## R15 — Stabilisation PCB Geometry v1 (timeout harness vs paires item)

- **Problématique** : Comment sécuriser `pcb_measure_distance_session` V2 alors qu’une paire empreinte + pad enfant a été associée à un timeout smoke (~120 s) ?
- **Choix retenu** : **Pas de nouveau blocker handler** (`unsupported_parent_child_item_pair`, etc.) : une reproduction post-correction harness montre la paire **empreinte + pad enfant** **fonctionnelle** et rapide ; le blocage observé était **coherent avec le dump stdout massif** de `qa_ipc_smoke` (`printResponse` → `ApiResponse.Utf8DebugString`) avant la sortie allégée pour `pcb_measure_distance_session` (cf. R14 `ipc_smoke.cpp`). On **documente** la limite (boards très denses → bbox empreinte plus coûteuse ; automatiser de préférence avec des types légers ou point+item si besoin) sans interdire les KIID valides.
- **Contexte / alternatives écartées** : Option B (blocker explicite parent/enfant) — **refusé** : rejetterait des appels valides et utiles après correction harness ; Option C (whitelist de types en V2) — refusé comme garde-fou obligatoire (trop restrictif pour le même motif).
- **Date** : 2026-05-02

## R16 — Mini-release PCB Geometry v1 + durcissement flake `nng_dial`

- **Problématique** : Comment figer **pcb_geometry v1** comme capacité stable alors qu’un flake IPC occasionnel (`nng_dial: Connection refused`) a été observé sur le runner famille ?
- **Choix retenu** : On **documente** la capacité (sémantique 2D nm, bbox centre, limites, harness stdout, empreinte+pad) dans le manifest / guide / release note ; on **caractérise** le flake comme transitoire côté client IPC / smoke ; on ajoute un **retry court et borné** (`Invoke-SmokeBoundedIpcRetry` dans `ipc_qa_common.ps1`, utilisé par `Resolve-IpcUrlFromSockets`, `Wait-McpProjectLoaded`, et le probe `targeted_pcb_measure_distance_probe.ps1`) **uniquement** lorsque la sortie ressemble à « Connection refused », avec compteur **`ipc_smoke_ipc_retry_extra_total`** dans les résumés — **sans** nouvelle fonctionnalité géométrique (pas pad_center, clearance, etc.).
- **Contexte / alternatives écartées** : Masquer les échecs réels par des retries aveugles ; refusé — retry limité à 3 tentatives et au motif flake uniquement.
- **Date** : 2026-05-02

## R17 — Étude technique `pad_center` / ancrage pad pour `pcb_measure_distance_session` (sans implémentation)

- **Problématique** : Peut-on ajouter un mode **pad_center** à `pcb_measure_distance_session` sans créer d’ambiguïté géométrique ?
- **Choix retenu** : **Étude seulement** — analyser les définitions KiCad (`PAD::GetPosition`, `ShapePos`, bbox, polygone) et un schéma JSON avant toute implémentation ; privilégier une sémantique **nommée explicitement** (ancrage vs bbox vs centroïde couche) plutôt que le libellé ambigu « pad_center » seul.
- **Contexte / alternatives écartées** : Implémenter immédiatement un mode unique sans spécifier la définition physique ; refusé tant que la distinction **ancre / shape origin / centroïde polygone** n’est pas figée dans la doc et les erreurs MCP.
- **Date** : 2026-05-02

## R18 — `position` sur endpoints `item_id` : `bbox_center` (défaut) vs `pad_anchor` (`PAD::GetPosition`)

- **Problématique** : Comment ajouter une mesure spécifique aux pads sans confondre ancrage KiCad, bbox, centre de polygone et clearance ?
- **Choix retenu** : On ajoute une clé optionnelle **`position`** sur les objets endpoint **`a`** / **`b`** lorsque **`item_id`** est utilisé : **`bbox_center`** (comportement historique, défaut si absent) = centre **`BOARD_ITEM::GetBoundingBox().Centre()`** en nm ; **`pad_anchor`** (pads uniquement, `PCB_PAD_T`) = **`PAD::GetPosition()`** en nm (1 IU = 100 nm). Ce n’est **pas** une clearance, **pas** le centre du polygone de pastille, **pas** une distance bord-à-bord. **`position`** avec **`x_nm`/`y_nm`** sur le même endpoint reste **`invalid_endpoint_a` / `invalid_endpoint_b`**. Hors périmètre : `pad_shape_origin`, centroïde polygone, clearance, bbox_edge, layer-aware — inchangé vs R14–R16.
- **Contexte / alternatives écartées** : Réutiliser le libellé ambigu « pad_center » seul ; refusé (cf. R17). Exposer plusieurs origines pad sans les nommer ; refusé.
- **Date** : 2026-05-02

## R19 — Mini-release **PCB Geometry v1.1** (clôture documentation + cartographie avant extension)

- **Problématique** : Comment clôturer l’ajout de **`pad_anchor`** comme capacité stable sans introduire une nouvelle ambiguïté géométrique ?
- **Choix retenu** : On publie une **mini-release PCB Geometry v1.1** (famille QA **`pcb_geometry_v1`** inchangée) : documentation checkpoint / manifest / guide alignées sur les modes **point–point**, **point–item**, **item–item `bbox_center`**, **`pad_anchor`** (`PCB_PAD_T`) ; **`bbox_center`** = **`GetBoundingBox().Centre()`**, **`pad_anchor`** = **`PAD::GetPosition()`** ; rappel explicite que ce n’est **pas** clearance, **pas** bord-à-bord, **pas** centroïde polygone ; hors périmètre inchangé (**`pad_shape_origin`**, layer-aware, net-aware, etc.) ; régénération **`MCP_TOOL_MAP_R1.tsv`** ; pas de nouvelle fonctionnalité géométrique dans cette release.
- **Contexte / alternatives écartées** : Continuer à étendre sans baseline nommée ; refusé — figer **v1.1** avant toute **v1.2** ou nouveau mode.
- **Date** : 2026-05-02

## R20 — Étude **`pad_shape_origin`** / **`PAD::ShapePos(layer)`** (couche explicite ; pas d’implémentation)

- **Problématique** : Faut-il ajouter une position **`pad_shape_origin`** basée sur **`PAD::ShapePos(layer)`** à **`pcb_measure_distance_session`** ?
- **Choix retenu** : **Étude seulement** — analyser la sémantique de **`ShapePos`** (offset par couche via **`PADSTACK::Offset`**, rotation), l’alignement avec **`get_pad_shape_as_polygon`** / **`BoardLayer`**, la nécessité d’une **`layer`** obligatoire au JSON, les erreurs stables et une QA minimale (dont fixture avec **offset non nul** pour distinguer ancrage vs origine de forme) **avant** toute implémentation.
- **Contexte / alternatives écartées** : Exposer **`pad_shape_origin`** sans couche explicite ou sans règles pour couches absentes du padstack ; refusé en l’état — risque d’ambiguïté et de fallback silencieux (**`CopperLayer`** / **`CUSTOM`**).
- **Date** : 2026-05-02

## R21 — Fixture **`ipc_geometry_v1`** (pad à offset cuivre non nul pour QA future `pad_shape_origin`)

- **Problématique** : Comment tester **`pad_shape_origin`** de manière significative alors que les pads sans offset rendent **`ShapePos(layer)`** identique à **`pad_anchor`** ?
- **Choix retenu** : On ajoute une **fixture dédiée** sous **`qa/data/pcbnew/geometry/ipc_geometry_v1/`** (projet **`ipc_geometry_v1.kicad_pro`** + PCB avec pastille **ovale** et **`(drill (offset 1 0))`**, KIID pad fixe **`f0010001-0001-4001-8001-000000000001`**) ; on **ne modifie pas** la fixture **`ipc-v1`** utilisée par les probes existants. Copie miroir sous **`build/.../ipc-geometry-v1-proj/`** pour exécution harness. Vérification smoke : ouverture PCB, **`pcb_get_board_items_session`**, **`pcb_measure_distance_session`** (**`pad_anchor`**, **`bbox_center`**) — **sans** **`pad_shape_origin`** (non implémenté).
- **Contexte / alternatives écartées** : Enrichir **`v1.kicad_pcb`** (risque de régressions sur plusieurs familles QA) — **écarté** ; fixture minimale séparée (**option B** R20).
- **Date** : 2026-05-02

## R22 — `position` **`pad_shape_origin`** = **`PAD::ShapePos(layer)`** (couche obligatoire)

- **Problématique** : Comment ajouter **`pad_shape_origin`** de façon testable sans confondre **`ShapePos(layer)`**, **`pad_anchor`**, bbox et centroïde ?
- **Choix retenu** : On implémente **`position="pad_shape_origin"`** pour **`PCB_PAD_T`** uniquement, basé sur **`PAD::ShapePos(layer)`**, avec **`layer`** obligatoire (convention **`BoardLayer`** alignée sur **`get_pad_shape_as_polygon`** : chaîne enum protobuf ex. **`BL_F_Cu`**, ou jeton KiCad **`F_Cu`** accepté via préfixe **`BL_`** côté handler, ou valeur enum numérique), conversion **1 IU = 100 nm**, sources MCP **`item_pad_shape_origin`**, erreurs stables (**`pad_shape_origin_requires_pad_*`**, **`pad_shape_origin_requires_layer_*`**, **`invalid_layer_*`**, **`pad_shape_position_unavailable_*`**). Pas de centroïde polygone dédié, pas de clearance, pas de bord-à-bord. La fixture **`ipc_geometry_v1`** (pad à offset non nul, KIID stable) est **synchronisée** depuis **`qa/data/...`** vers **`build/.../ipc-geometry-v1-proj/`** au début de **`targeted_pcb_measure_distance_probe.ps1`** (pas de copie manuelle). Les couches « unsupported » au sens padstack ne sont **pas** exposées comme blockers dédiés tant que la sémantique des fallbacks KiCad reste ambiguë pour l’automatisation — **`ShapePos`** suit la logique du padstack.
- **Contexte / alternatives écartées** : Exposer **`pad_shape_origin`** sans **`layer`** ou confondre avec le centre de bbox ; refusé. Ajouter **`unsupported_layer_for_pad_*`** sans règle stable ; refusé pour l’instant (cf. R20).
- **Date** : 2026-05-02

## R22-Diag — Échecs **`pad_shape_origin`** / checkpoint MCP v2 après R22

- **Problématique** : Pourquoi **`pad_shape_origin`** et le checkpoint MCP v2 échouent-ils après l’implémentation R22 ?
- **Choix retenu** : On diagnostique séparément le contrat **PCB Geometry** et le blocker **SCH Wiring** avant de valider R22 ou de corriger le checkpoint. Pour PCB : les logs montrent **`invalid_layer_*`** avec payload **`layer":"F_Cu"`** ; **`magic_enum`** sur **`board::types::BoardLayer`** attend les **noms d’énumération protobuf** (ex. **`BL_F_Cu`**), pas le jeton KiCad seul — même convention que **`get_pad_shape_as_polygon`**. Le probe est corrigé (**`BL_F_Cu`**) ; le handler accepte aussi **`F_Cu`** via **`BL_` + jeton**. Les échecs **non-pad** venaient du même rejet de couche (**`invalid_layer_a`** au lieu de **`pad_shape_origin_requires_pad_*`**). Pour SCH : l’artefact **`wire_restore_failed`** coïncide avec **`nng_dial: Connection shutdown`** sur l’appel restore (**flake IPC**), pas avec un changement R22.
- **Contexte / alternatives écartées** : Modifier la fixture **`ipc_geometry_v1`** sous prétexte d’offset **`ShapePos`** — **non** : l’échec était **parsing de layer**, pas géométrie padstack.
- **Date** : 2026-05-02

## R23 — Mini-release **PCB Geometry v1.2** (**`pad_shape_origin`** checkpoint-covered)

- **Problématique** : Comment clôturer **`pad_shape_origin`** comme capacité stable après correction du parsing layer **`F_Cu` / `BL_F_Cu`** ?
- **Choix retenu** : On publie **PCB Geometry v1.2** (famille QA **`pcb_geometry_v1`** inchangée) : documentation manifeste / guide / release note alignées sur les modes **point–point**, **point–item**, **item `bbox_center`**, **`pad_anchor`**, **`pad_shape_origin`** ; **`layer`** officiel = enum protobuf **`BoardLayer`** (**`BL_F_Cu`**, …) ; **alias** KiCad court **`F_Cu`** accepté par le handler ; fixture **`ipc_geometry_v1`** ; négatifs layer / non-pad / layer invalide ; **pas** de nouvelle capacité hors périmètre (centroïde, clearance, bbox_edge, layer-aware générique, net-aware, mutation PCB). Cartographie **`MCP_TOOL_MAP_R1.tsv`** régénérée si nécessaire ; preuve : probe + famille + checkpoint MCP v2 **verts**.
- **Contexte / alternatives écartées** : Rester en **v1.1** documentaire alors que **`pad_shape_origin`** est stable — refusé ; la version produit doit refléter la baseline réelle.
- **Date** : 2026-05-02

## R24 — Durcissement **baseline MCP v2** (doc / préflight / checkpoint summary)

- **Problématique** : Comment rendre le baseline MCP v2 **reproductible** après PCB Geometry v1.2 et les correctifs harness ?
- **Choix retenu** : On ajoute **`README_IPC_QA_BASELINE.md`** (classification des scripts QA, build minimal **`kicommon`** / **`pcbnew_kiface`** / **`kicad`** / **`qa_ipc_smoke`**, **VsDevCmd**/**vcvars**, avertissement artefact **`_pcbnew.dll`** stale), le script **`run_mcp_v2_baseline_preflight.ps1`**, l’enrichissement du résumé checkpoint avec **`ipc_smoke_ipc_retry_extra_total_max_across_families`** et **`family_summary_files_in_checkpoint_dir`**, et des renvois dans **`MCP_KICAD_USER_DEV_GUIDE.md`**, **`MCP_V1_CHECKPOINT_RELEASE_NOTE.md`**, **`MCP_V1_OFFICIAL_MANIFEST.md`**. **Pas** de suppression d’artefacts historiques sans justification ; **`MCP_TOOL_MAP_R1.tsv`** reste l’outil de cartographie régénérable.
- **Contexte / alternatives écartées** : Nouvelle plateforme CI monolithique uniquement pour QA IPC ; refusé — documentation + script léger préféré.
- **Date** : 2026-05-02

## R25 — Étude **`get_pcb_item_geometry_session`** (lecture géométrique structurée, sans implémentation)

- **Problématique** : Faut-il ajouter un tool **`get_pcb_item_geometry_session`** pour exposer la géométrie d’un item plutôt que d’ajouter encore des modes à **`pcb_measure_distance_session`** ?
- **Choix retenu** : **Étude seulement** — inventorier les tools existants (**`pcb_get_board_item_by_id_session`**, **`get_bounding_box_pcb`**, **`get_pad_shape_as_polygon`**, **`pcb_measure_distance_session`**), comparer des périmètres V1 (minimal vs pad-aware vs polymorphe), proposer schémas JSON / erreurs / QA / fichiers impact ; **aucune implémentation** tant que le périmètre V1 n’est pas figé et validé.
- **Contexte / alternatives écartées** : Implémenter tout de suite un tool polymorphe large (Option C) ; refusé — risque de duplication lourde avec **`get_pad_shape_as_polygon`** et charge QA.
- **Date** : 2026-05-02

## R26 — **`get_pcb_item_geometry_session`** V1 (Option B allégée)

- **Problématique** : Comment exposer la géométrie d’un item PCB sans surcharger **`pcb_measure_distance_session`** ni dupliquer les tools existants ?
- **Choix retenu** : Nouveau tool **`get_pcb_item_geometry_session`** V1 : introspection **read-only** d’un **seul** item par **`item_id`** (KIID) avec **`board_filename`** obligatoire ; bbox en nm via **`GetBoundingBox`** (min/max/centre), **`position_x_nm`/`position_y_nm`** selon le type (aligné sur **`pcb_get_board_item_by_id_session`**), **`net_name`**, nom de couche KiCad ; pour **`PCB_PAD_T`** : **`pad_anchor_*`** = **`PAD::GetPosition()`** ; **`pad_shape_origin_*`** = **`PAD::ShapePos(layer)`** **uniquement** si l’argument optionnel **`layer`** est fourni (format **`BoardLayer`** `BL_*` + **alias** court `F_Cu` comme ailleurs) ; **`layer_used`** = nom canonique `BL_*` ; **pas** de polygone, **pas** de clearance, **pas** de mesure bord-à-bord, **pas** de mutation PCB ; callback **`mcp_pcb_item_geometry_callback`** ; QA **`targeted_pcb_item_geometry_probe.ps1`** + même famille **`pcb_geometry_v1`** que **`pcb_measure_distance_session`** (**`run_pcb_geometry_v1_targeted_probes.ps1`** agrège les deux).
- **Contexte / alternatives écartées** : N’ajouter que des modes à **`pcb_measure_distance_session`** (cf. R25 étude) ; refusé pour garder **mesure** vs **description géométrique** séparées ; tout fondre dans **`pcb_get_board_item_by_id_session`** ; refusé (réponse déjà chargée et différente du besoin bbox/pad_shape_origin explicite).
- **Date** : 2026-05-01

## R26-validation — Rebuild MSVC + QA terrain **`get_pcb_item_geometry_session`**

- **Problématique** : Comment valider **`get_pcb_item_geometry_session`** après implémentation alors que le premier build a échoué sur l’environnement MSVC (PCH / includes std) ?
- **Choix retenu** : On valide par **`vcvars64.bat`** puis **`cmake --build … --target kicommon pcbnew_kiface kicad qa_ipc_smoke`** ; smoke **`Test-IpcKifaceDllLoad`** avec **`Initialize-IpcQaEnvironment`** ; enchaînement probes / famille / checkpoint MCP v2. En cas d’échec **`LNK1104`** sur **`_pcbnew.dll`**, arrêter **`kicad.exe`** / **`qa_ipc_smoke.exe`** puis relinker ; si le link a échoué après compilation, **supprimer** l’objet **`api_handler_pcb.cpp.obj`** ou forcer la recompilation pour éviter un **`_pcbnew.dll`** repris sans le dernier handler. Les timeouts harness sur la fixture **`ipc_geometry_v1`** (pastille ovale + offset) avec l’ancien calcul **`PAD::GetBoundingBox()`** sont traités côté handler par une **bbox pad rapide** (`ShapePos` + `GetSize`, AABB) pour ne pas bloquer le thread IPC sur **`BuildEffectiveShapes` / mutex**.
- **Contexte / alternatives écartées** : Déclarer la QA verte sans relancer les probes après correction handler ; refusé. Ignorer les timeouts comme « environnement » sans vérifier **`pcbnew_kiface`** réellement à jour ; refusé.
- **Date** : 2026-05-03

## R26-Diag — Timeout **`item_geom_pad_no_layer`** / **`geometry_pad_anchor_contract_failed`**

- **Problématique** : Pourquoi **`get_pcb_item_geometry_session`** pouvait rester rouge sur **`targeted_pcb_item_geometry_probe.ps1`** (**timeout** sur **`item_geom_pad_no_layer`**, blocker **`geometry_pad_anchor_contract_failed`**) alors que l’implémentation R26 était en place ?
- **Choix retenu** : (1) Nettoyer **`kicad.exe`** / **`qa_ipc_smoke.exe`** avant link (**`LNK1104`** si **`_pcbnew.dll`** verrouillée). (2) Forcer un rebuild fiable : **`vcvars64.bat`**, suppression de **`pcbnew\CMakeFiles\pcbnew_kiface_objects.dir\api\api_handler_pcb.cpp.obj`**, puis **`cmake --build … --target kicommon pcbnew_kiface kicad qa_ipc_smoke`** — vérifier que **`_pcbnew.dll`** est **plus récent** que l’objet. (3) Confirmer en source que **`PCB_PAD_T`** n’emprunte **pas** **`BOARD_ITEM::GetBoundingBox()`** (bbox = **`ShapePos(lid)` + `GetSize(lid)`**). (4) Réduire le coût harness : **`qa_ipc_smoke`** n’appelle pas **`printResponse`** complet **`ApiResponse.Utf8DebugString()`** pour **`get_pcb_item_geometry_session`** (comme les autres tools à payload volumineux). (5) Instrumentation optionnelle **`KICAD_IPC_STARTUP_DIAG=1`** → **`pcb_item_geometry_diag.log`** sous **`PATHS::GetLogsPath()`** pour localiser l’étape si besoin. (6) Valider **d’abord** le cas minimal pad **sans** **`layer`** (**`ipc_geometry_v1.kicad_pcb`**, KIID fixe), puis **`targeted_pcb_item_geometry_probe.ps1`**, familles agrégées et **`run_mcp_v2_checkpoint.ps1`** avec **`KICAD_IPC_KIFACE_SMOKE=1`** (**définir la variable dans la même session PowerShell** ; `cmd set … && powershell` peut ne pas la transmettre au script).
- **Contexte / alternatives écartées** : Supposer un timeout « réel » dans le handler sans éliminer binary stale + dump protobuf géant ; refusé.
- **Date** : 2026-05-03

## R27 — Release **PCB Geometry v1.3** — clôture **`get_pcb_item_geometry_session`**

- **Problématique** : Comment clôturer **`get_pcb_item_geometry_session`** comme capacité **stable** après résolution des timeouts PAD (**R26-Diag**) ?
- **Choix retenu** : On publie **PCB Geometry v1.3** : **`get_pcb_item_geometry_session`** = introspection géométrique read-only (**pas** distance, **pas** polygone) ; complète **`pcb_measure_distance_session`** ; champs bbox nm / centre / position / net / layer ; **`pad_anchor`** ; **`pad_shape_origin`** si **`layer`** (**`BoardLayer`** **`BL_*`**, alias **`F_Cu`**) ; fast-path bbox pad (**`ShapePos` + `GetSize`**) ; **`qa_ipc_smoke` borné** ; exclusions inchangées (pas clearance, pas bord-à-bord générique, pas centroïde polygone, pas mutation PCB, pas net-aware distance). Documentation manifeste / guide / release note / baseline README alignées ; **`MCP_TOOL_MAP_R1.tsv`** régénéré si besoin. Preuve : probes / famille **`pcb_geometry_v1`** / checkpoint MCP v2 verts (**`KICAD_IPC_KIFACE_SMOKE=1`** dans la session PowerShell qui lance le checkpoint).
- **Contexte / alternatives écartées** : Porter **v1.3** **uniquement** sur la mesure sans documenter l’introspection ; refusé — **v1.3** couvre la famille **`pcb_geometry_v1`** au complet.
- **Date** : 2026-05-03

## R28 — Pipeline baseline MCP v2 (CI / local)

- **Problématique** : Comment rendre le baseline MCP v2 **reproductible** localement et **prêt pour une future CI** après **PCB Geometry v1.3** (**R27**) ?
- **Choix retenu** : On formalise **`run_mcp_v2_ci_baseline.ps1`** : pré-check **`kicad.exe` / `qa_ipc_smoke.exe`** (option **`-KillRunningKiCad`**), vérification MSVC (**`cl`**, **`cmake`**, **`ninja`**) via **VsDevCmd**, build minimal optionnel (**`-BuildMinimal`**, **`-SkipBuild`**), **`Test-IpcKifaceDllLoad`**, probes **`targeted_pcb_measure_distance_probe.ps1`** et **`targeted_pcb_item_geometry_probe.ps1`**, **`run_pcb_geometry_v1_targeted_probes.ps1`**, **`run_mcp_v2_checkpoint.ps1`** avec **`EnableKifaceSmoke`** (**`$true`** par défaut — définit **`KICAD_IPC_KIFACE_SMOKE`** pour le processus checkpoint), **`-VerifyOnly`** pour contrôle machine sans QA longue ; résumé **`qa_mcp_v2_ci_baseline_summary.txt`**. Documentation **`README_IPC_QA_BASELINE.md`**, **`MCP_KICAD_USER_DEV_GUIDE.md`**, **`MCP_V1_CHECKPOINT_RELEASE_NOTE.md`**. **Aucun** changement handlers MCP / registre outils.
- **Contexte / alternatives écartées** : Documenter seulement sans script ; refusé pour l’objectif CI — le script reste l’**Option B** demandée, en complément du préflight existant **`run_mcp_v2_baseline_preflight.ps1`**.
- **Date** : 2026-05-03

## R29 — Étude **`get_pcb_clearance_context_session`** (pas d’implémentation)

- **Problématique** : Faut-il ajouter **`get_pcb_clearance_context_session`** pour donner à l’IA un **contexte local** (nets, netclasses, couches, distances simples, règles utiles) **sans** prétendre exécuter une **vraie DRC** ?
- **Choix retenu** (**étude seulement**) : On **ne implémente pas** tant que le périmètre n’est pas figé. Analyse : les tools existants couvrent **géométrie** (**`pcb_measure_distance_session`**, **`get_pcb_item_geometry_session`**) et **données projet/plateau** (**`get_net_classes`**, **`set_net_classes`**, **`get_nets`**, **`get_board_stackup`**, **`pcb_get_board_item_by_id_session`**) ; la **clearance nominale** entre deux **`BOARD_ITEM`** peut s’appuyer sur **`DRC_ENGINE::EvalClearanceBatch`** / **`EvalRules`** (**`pcbnew/drc/drc_engine.h`**) une fois le moteur initialisé (**`DRC_TOOL::GetDRCEngine()->InitEngine`**, déjà au chargement carte dans **`PCB_EDIT_FRAME::OnBoardLoaded`**). **Option recommandée pour une future V1** : **A + touches B** — un ou deux **`item_id`**, **`layer`** optionnel (requiert une convention pour la couche d’évaluation des contraintes), sortie **`is_true_drc=false`** par défaut, **`clearance_status`** prudente (**`not_evaluated`** / **`unknown`** si pas de paire ou règle indisponible). **Éviter** d’enrichir **`pcb_measure_distance_session`** avec clearance (confusion **distance géométrique** vs **contrainte métier**). **Reporter** l’option **C** (RunTests / violation locale complète) — coût, fragilité UI-thread, risque de confusion « rapport DRC ».
- **Contexte / alternatives écartées** : Lancer une **DRC complète** comme préalable MCP ; refusé pour une première livraison contextuelle.
- **Date** : 2026-05-03

## R30 — Fixture QA **`ipc_clearance_v1`** avant **`get_pcb_clearance_context_session`**

- **Problématique** : Comment préparer une QA fiable pour un futur **`get_pcb_clearance_context_session`** sans confondre **contexte clearance** (lecture structurée + signaux prudents) et **vraie DRC** (RunTests, rapport officiel) ?
- **Choix retenu** : On crée une **fixture dédiée** (**`qa/data/pcbnew/clearance/ipc_clearance_v1/`**) avec **deux nets**, **deux netclasses** aux clearances projet distinctes (**0,20 mm** / **0,30 mm**), **deux pads SMD** sur **F.Cu** et **KIID fixes**, **avant** toute implémentation du tool. Documentation **`README_ipc_clearance_v1.md`** ; script optionnel **`r30_verify_ipc_clearance_fixture.ps1`** (tools existants uniquement : **`qa_open_editor_frame`**, **`pcb_get_board_item_by_id_session`**, **`get_pcb_item_geometry_session`**, **`pcb_measure_distance_session`**, **`get_net_classes`**, **`get_nets`**, **`get_board_stackup`**). **Aucune** modification des handlers **`pcb_measure_distance_session`**, **`get_pcb_item_geometry_session`**, netclass, DRC.
- **Contexte / alternatives écartées** : Modifier **`ipc_geometry_v1`** (risque pour les probes **PCB Geometry v1.3**) ; réutiliser **`ipc-v1-proj/v1.kicad_pcb`** seul (nets/netclasses moins contrôlés pour ce scénario). Option **B** (fixture neuve) privilégiée pour clarté et stabilité des KIID.
- **Date** : 2026-05-01

## R30-NetClasses-Diag — **`get_net_classes` / `get_board_stackup`** timeout harness (**qa_ipc_smoke**)

- **Problématique** : Pourquoi **`get_net_classes`** (et **`get_board_stackup`**) provoquent un **timeout** côté **`qa_ipc_smoke.exe`** alors que la fixture **`ipc_clearance_v1`** expose correctement pads, nets et distances (**`get_nets`**, **`pcb_measure_distance_session`**, etc.) ?
- **Choix retenu** : On diagnostique **avant** **`get_pcb_clearance_context_session`**. Le handler **`get_net_classes`** (**`common/mcp/mcp_executor.cpp`** → **`kicad::api::GetNetClassesReadonly()`** dans **`common/api/api_common_readonly.cpp`**) est **synchrone projet-only** (pas de frame PCB). **`get_board_stackup`** (**`pcbnew/api/api_handler_pcb.cpp`** **`GetBoardStackupToolInvoke`**, callback **`mcp_pcb_board_stackup_callback.cpp`**) est **GUI / PCB frame**. La cause probable du **timeout harness** n’est **pas** la fixture clearance : **`ipc_smoke.cpp`** appelait encore **`printResponse()`** = **`ApiResponse.Utf8DebugString()` complet** pour ces tools avant les branches résumé (**`result_json=`** / **`layer_count=`**). Des payloads **Struct** embarquant protos netclass / stackup peuvent rendre ce dump **extrêmement coûteux** et faire dépasser le **timeout du sous-processus** PowerShell. **Correctif** : traiter **`get_net_classes`** et **`get_board_stackup`** comme **`pcb_measure_distance_session`** — **ne pas** dumper le **`ApiResponse`** complet ; garder l’en-tête minimal **`=== MCP CallTool(maybe args) ===`** + **`status=`** + lignes résumé existantes.
- **Contexte / alternatives écartées** : Modifier **`GetNetClassesReadonly`** sans preuve ; refusé. Instrumentation **`net_classes_diag.log`** réservée si le défaut persistait après correction smoke — **patch smoke** prioritaire. Sonde **`r30_netclasses_minimal_probe.ps1`** (**`get_project_info`** + **`get_net_classes`**) pour comparer **`ipc-v1-proj`**, **`ipc-geometry-v1-proj`**, **`ipc-clearance-v1-proj`** après rebuild **`qa_ipc_smoke`**.
- **Date** : 2026-05-03

## R30-Smoke-Rebuild-Validation — Confirmation après **`qa_ipc_smoke`**

- **Problématique** : Comment confirmer que les timeouts **`get_net_classes`** venaient du dump **`qa_ipc_smoke`** (**`printResponse` / `Utf8DebugString`**) et non de la fixture **`ipc_clearance_v1`** ?
- **Choix retenu** : On **rebuild** **`qa_ipc_smoke`** après bornage des sorties **`get_net_classes`** / **`get_board_stackup`** dans **`ipc_smoke.cpp`**, puis on valide par **`r30_netclasses_minimal_probe.ps1`** (trois fixtures) et **`r30_verify_ipc_clearance_fixture.ps1`**. **Preuve** : probes minimales **OK** en **~0,5 s** pour **`get_net_classes`** sur chaque fixture ; **`ipc_clearance_v1`** expose **`QA_NETCLASS_A`** / **`QA_NETCLASS_B`** dans **`result_json`** ; **`r30_ipc_clearance_fixture_ok=true`** (**`get_board_stackup_status_ok=true`**).
- **Contexte / alternatives écartées** : Continuer à soupçonner uniquement les données projet sans rebuild smoke ; refusé après validation terrain.
- **Date** : 2026-05-03

## R31 — **`get_pcb_clearance_context_session`** V1 (contexte seulement)

- **Problématique** : Comment fournir un contexte clearance utile à l’IA sans faire croire à un verdict DRC officiel KiCad ?
- **Choix retenu** : On implémente **`get_pcb_clearance_context_session`** V1 comme **pré-analyse** : nets / netclasses / couche ; distance **bbox-centre ↔ bbox-centre** (nm) si paire ; clearance **nominale** netclass (IU×100→nm, **max** sur deux items) si **`NETCLASS::HasClearance()`** ; **`is_true_drc=false`** ; **pas** `RunTests` ; **pas** rapport violations ; lecture **`BOARD` / `NETINFO_ITEM::GetNetClass()`** sans passer par le MCP **`get_net_classes`** ; callback **`mcp_pcb_clearance_context_callback`** + handler **`PcbClearanceContextSessionToolInvoke`** ; **`qa_ipc_smoke`** sortie **bornée** ; QA **`targeted_pcb_clearance_context_probe.ps1`**, famille **`run_pcb_clearance_context_v1_targeted_probes.ps1`**, fixture **`ipc_clearance_v1`**. **Checkpoint MCP v2** : famille **candidat** (non agrégée au premier run) jusqu’à preuve de stabilité.
- **Contexte / alternatives écartées** : Exposer le résultat comme **conformité DRC** ou lancer **RunTests** complet ; refusé en V1.
- **Date** : 2026-05-01

## R31-Validation — QA terrain **`get_pcb_clearance_context_session`** (build + familles + checkpoint)

- **Problématique** : Comment valider **`get_pcb_clearance_context_session`** après implémentation sans le confondre avec une **vraie DRC**, tout en prouvant **build MSVC**, **smoke KIFACE**, probes **clearance context**, familles existantes et **checkpoint MCP v2** ?
- **Choix retenu** : On valide par **`vcvars64.bat`** puis **`cmake --build … --config Release --target kicommon pcbnew_kiface kicad qa_ipc_smoke`** ; correction compile handler si besoin (**`ToProtoEnum<PCB_LAYER_ID, board::types::BoardLayer>`** ; struct **`CLEARANCE_CTX_OUT`** + **`PackClearanceContextSessionStruct`** pour éviter les pièges d’arité MSVC sur le lambda **`makeOut`**) ; **`Test-IpcKifaceDllLoad`** avec **`Initialize-IpcQaEnvironment`** et **`KICAD_IPC_KIFACE_SMOKE=1`** ; **`targeted_pcb_clearance_context_probe.ps1`** + **`run_pcb_clearance_context_v1_targeted_probes.ps1`** ; **`run_pcb_geometry_v1_targeted_probes.ps1`** + **`run_pcb_inspection_v2_targeted_probes.ps1`** ; **`run_mcp_v2_checkpoint.ps1`** (même session, **`KICAD_IPC_KIFACE_SMOKE=1`**) — **sans** intégrer la famille **`pcb_clearance_context_v1`** au checkpoint global tant qu’elle reste **candidat**. Preuve : **`pcb_clearance_context_green=true`**, **`pcb_clearance_context_v1_family_green=true`**, **`mcp_v2_checkpoint_green=true`**, **`kiface_smoke_ok=true`**, **`is_true_drc=false`** inchangé côté contrat.
- **Contexte / alternatives écartées** : Déclarer la QA verte sans **relink** réel **`_pcbnew.dll`** après correction handler ; refusé. Mélanger **verdict DRC** et **contexte** dans le rapport de validation ; refusé.
- **Date** : 2026-05-03

## R32 — Release **PCB Clearance Context v1** (`get_pcb_clearance_context_session`) sans confusion DRC ni intégration checkpoint précipitée

- **Problématique** : Comment clôturer **`get_pcb_clearance_context_session`** comme capacité **stable** sans le confondre avec une **vraie DRC** ni l’intégrer trop vite au checkpoint global MCP v2 ?
- **Choix retenu** : On publie **PCB Clearance Context v1** comme **famille candidate verte** (**`pcb_clearance_context_v1`**), **documentée** (context-only : **`is_true_drc=false`**, **pas** `RunTests`, **pas** rapport violations ; distance géométrique = contexte, **pas** bord-à-bord ; **`required_clearance_nm`** / **`clearance_status`** = signaux nominaux / disponibilité, **pas** verdict officiel), **avant** de décider en **R33** de l’**intégration au checkpoint** agrégé ou du maintien **candidate** hors gate.
- **Contexte / alternatives écartées** : Ajouter **`pcb_clearance_context_v1`** à **`run_mcp_v2_checkpoint.ps1`** dans la même release que la doc — refusé en **R32** pour laisser une release « propre » documentée avant le gate global. Étendre vers RunTests / violations / nearby / mutation dans cette mission — refusé.
- **Date** : 2026-05-01

## R33 — Intégration **`pcb_clearance_context_v1`** au checkpoint MCP v2 global

- **Problématique** : Faut-il intégrer **`pcb_clearance_context_v1`** au checkpoint MCP v2 global après sa validation comme famille candidate verte ?
- **Choix retenu** : On l’**intègre** comme **famille officielle** (**sept** familles agrégées) : **`run_mcp_v2_checkpoint.ps1`** exécute **`run_pcb_clearance_context_v1_targeted_probes.ps1`** après **`pcb_geometry_v1`** ; résumé checkpoint enrichi (**`pcb_clearance_context_v1_ok`**, **`pcb_clearance_context_v1_target_run_dir`**, **`pcb_clearance_context_v1_exit_code`**) ; copie **`qa_pcb_clearance_context_v1_family_summary.txt`** ; blocker **`pcb_clearance_context_v1:<blocker>`** en **priorité 7** ; agrégation retry IPC élargie (**R33**). **Aucune** modification des handlers métier ; le tool reste **context-only** (**`is_true_drc=false`**, pas verdict DRC officiel).
- **Contexte / alternatives écartées** : Reporter indéfiniment en « candidate » après stabilité terrain documentée ; refusé. Étendre le tool vers DRC / RunTests dans la même mission — refusé.
- **Date** : 2026-05-01

## R34 — Alignement **`run_mcp_v2_baseline_preflight.ps1`** avec le checkpoint à **7 familles**

- **Problématique** : Comment aligner le préflight baseline avec le checkpoint MCP v2 enrichi à **7 familles** après **R33** ?
- **Choix retenu** : **Option A** (document + artefact de synthèse), **sans** dupliquer **`run_pcb_clearance_context_v1_targeted_probes.ps1`** avant le checkpoint (**Option B** écartée : travail déjà fait dans **`run_mcp_v2_checkpoint.ps1`**). Le préflight reste un **chemin local court** : fail-fast **`pcb_measure_distance`** + famille **`pcb_geometry_v1`**, puis **`run_mcp_v2_checkpoint.ps1`** ; en-tête de script et **`qa_mcp_v2_baseline_preflight_summary.txt`** précisent la délégation (**`pcb_clearance_context_v1_explicit_in_preflight=false`**). **Option C** (déprécier au profit du seul CI baseline) écartée : le préflight reste utile sans vérification toolchain MSVC. Documentation (**README**, guide, release note, manifest) clarifie les trois scripts : **checkpoint** = gate **7 familles** ; **CI baseline** = pipeline reproductible ; **préflight** = itération rapide.
- **Contexte / alternatives écartées** : Ajouter une passe clearance explicite avant checkpoint ; refusé (redondant, plus lent).
- **Date** : 2026-05-01

## R35 — Étude **`get_pcb_nearby_items_context_session`** (nearby spatial context-only)

- **Problématique** : Faut-il ajouter **`get_pcb_nearby_items_context_session`** pour aider l’IA à identifier les objets proches sans prétendre lancer une DRC locale ?
- **Choix retenu** : **Étude seule** pour cette mission — aucune implémentation. Synthèse dans la réponse Cursor **R35** : cartographie des tools MCP existants (**`pcb_get_board_items_session`** = liste globale bornée sans filtre spatial ; **`hit_test_pcb`** = test point ↔ **un** `item_id`, pas découverte spatiale ; géométrie / mesure / clearance = hors périmètre « liste de voisins »). Pistes KiCad : **`BOARD_ITEM::GetBoundingBox()`**, itération **`BOARD`** (footprints, tracks, …) comme **`PcbGetBoardItemsSessionToolInvoke`** ; RTREE cuivre (**`m_CopperItemRTreeCache`**) réservé au cache DRC — à ne pas réutiliser à la légère pour du MCP V1. **V1 recommandée** : mode **item + radius_nm** (filtre bbox / distance centre-bbox), **`max_items`** obligatoire avec borne sûre, **`is_true_drc=false`** ; mode **point** optionnel en phase 2 ; **pas** d’appel MCP à **`get_pcb_clearance_context_session`** depuis nearby — l’IA enchaîne les tools ; factorisation **code interne** C++ possible avec clearance si duplication net/netclass.
- **Contexte / alternatives écartées** : **Option D** (DRC local / collision engine) — **rejet V1**. Implémenter sans étude fixture/QA — refusé avant boucle **R36+**.
- **Date** : 2026-05-01

## R36 — Fixture QA **`ipc_nearby_v1`** (pré-implémentation nearby)

- **Problématique** : Comment préparer une QA fiable pour un futur **`get_pcb_nearby_items_context_session`** sans confondre **voisinage géométrique** et **DRC** ?
- **Choix retenu** : On crée une **fixture dédiée** (**Option B**) **`qa/data/pcbnew/nearby/ipc_nearby_v1/`** avec pads **QTGT** / **QSAM** / **QNIN** / **QXCL**, **KIID stables**, nets **QA_NET_MAIN** / **QA_NET_OTHER**, distances centre–centre **2 mm / 3 mm / 70 mm** vérifiables par **`pcb_measure_distance_session`** ; script **`r36_verify_ipc_nearby_fixture.ps1`** (tools existants uniquement). **Pas** de modification des fixtures **`ipc_clearance_v1`** / **`ipc_geometry_v1`** (évite régression probes).
- **Contexte / alternatives écartées** : Réutiliser **uniquement** **`ipc_clearance_v1`** (**Option A**) — limité pour **`include_same_net`** et rayons ; modifier une fixture existante (**Option C**) — risque pour le checkpoint.
- **Date** : 2026-05-01

## R37 — **`get_pcb_nearby_items_context_session`** V1 (liste de voisins context-only)

- **Problématique** : Comment fournir une liste d’items proches utile à l’IA sans transformer le tool en pseudo-DRC ni dépendre d’un index DRC complexe ?
- **Choix retenu** : On implémente **`get_pcb_nearby_items_context_session`** V1 avec **scan linéaire borné**, requête **par `item_id`** (mode **point** exclu), **`radius_nm`** obligatoire (plafonné serveur), **`max_items`** défaut **25** / plafond **500**, filtres optionnels **`layer`** (**`BL_*`** ou alias **`F_Cu`**), **`item_types`** (`pads` / `tracks` / `arcs` / `vias`), **`include_same_net`** défaut **`true`** ; distance **centre-bbox → centre-bbx** (`distance_kind` = **`bbox_center_to_query_nm`**), tri **distance croissante**, **`truncated`** si coupure ; **`is_true_drc=false`** ; pas d’appel MCP interne au clearance ; callback **`mcp_pcb_nearby_items_context_callback`** ; QA **`targeted_pcb_nearby_items_context_probe.ps1`**, famille **`run_pcb_nearby_items_context_v1_targeted_probes.ps1`**, fixture **`ipc_nearby_v1`** ; famille **candidate hors checkpoint MCP v2** (même stratégie que clearance avant **R33**).
- **Contexte / alternatives écartées** : R-TREE DRC, **`RunTests`**, verdict clearance, mode point-only, mutation PCB — refusés en **R37**.
- **Date** : 2026-05-01

## R37-Validation — QA terrain **`get_pcb_nearby_items_context_session`** sans intégration checkpoint globale

- **Problématique** : Comment valider **`get_pcb_nearby_items_context_session`** après implémentation sans l’intégrer prématurément au checkpoint global ?
- **Choix retenu** : On valide par **`vcvars64.bat`** puis **`cmake --build … --config Release --target kicommon pcbnew_kiface kicad qa_ipc_smoke`** ; **`Test-IpcKifaceDllLoad`** avec **`Initialize-IpcQaEnvironment`** et **`KICAD_IPC_KIFACE_SMOKE=1`** ; **`targeted_pcb_nearby_items_context_probe.ps1`** + **`run_pcb_nearby_items_context_v1_targeted_probes.ps1`** ; familles existantes **`pcb_geometry_v1`**, **`pcb_clearance_context_v1`**, **`pcb_inspection_v2`** ; **`run_mcp_v2_checkpoint.ps1`** dans la même session PowerShell avec **`$env:KICAD_IPC_KIFACE_SMOKE='1'`** — **sans** ajouter **`pcb_nearby_items_context_v1`** au checkpoint agrégé. Correctifs **probe** : ne collecter les **`item_id`** qu’à partir de **`key: "items"`** (le dump **`qa_ipc_smoke`** répète aussi l’**`item_id`** de requête au niveau racine) ; **`@(...)`** sur les résultats pour **`Set-StrictMode`**. Côté handler (**`PcbNearbyItemsContextSessionToolInvoke`**) : distance **IU nm** puis **`llround(dist_nm × 100)`** pour comparer **`radius_nm`** et remplir **`distance_nm`** comme les autres helpers PCB ; **`is_true_drc=false`** inchangé.
- **Contexte / alternatives écartées** : Intégrer **`pcb_nearby_items_context_v1`** au gate MCP v2 dans la même mission — refusé. Enchaîner deux familles PCB lourdement **en parallèle** — évité (**conflits KiCad** observés).
- **Date** : 2026-05-03

## R38 — Release **PCB Nearby Items Context v1** (`get_pcb_nearby_items_context_session`) — famille candidate stable hors agrégat checkpoint

- **Problématique** : Comment clôturer **`get_pcb_nearby_items_context_session`** comme capacité candidate stable sans le confondre avec une DRC ni l’intégrer trop vite au checkpoint global ?
- **Choix retenu** : On publie **PCB Nearby Items Context v1** comme **famille candidate verte** (**`pcb_nearby_items_context_v1`**), **context-only** (**`is_true_drc=false`**, **pas** `RunTests`, **pas** verdict clearance, scan linéaire V1, **`distance_kind`** = **`bbox_center_to_query_nm`**, **pas** mode **point** en V1), **documentée** dans le manifeste, le guide dev, la release note checkpoint et **`README_IPC_QA_BASELINE.md`** ; **`MCP_TOOL_MAP_R1.tsv`** régénéré si besoin. **Aucune** modification de **`run_mcp_v2_checkpoint.ps1`** dans **R38** — décision **intégration gate vs maintien candidate** = mission **R39+**. Avertissement QA : **pas** d’exécution **parallèle** de plusieurs familles PCB lourdes sur une même machine.
- **Contexte / alternatives écartées** : Agréger **`pcb_nearby_items_context_v1`** au checkpoint dans la même release doc — refusé (**R38**). Étendre vers DRC / `RunTests` / mode point / mutation PCB — refusé.
- **Date** : 2026-05-03

## R39 — Intégration **`pcb_nearby_items_context_v1`** au checkpoint MCP v2 global (**8e** famille)

- **Problématique** : Faut-il intégrer **`pcb_nearby_items_context_v1`** au checkpoint MCP v2 global après sa validation comme famille candidate verte ?
- **Choix retenu** : On l’**intègre** comme **8e** famille officielle : **`run_mcp_v2_checkpoint.ps1`** exécute **`run_pcb_nearby_items_context_v1_targeted_probes.ps1`** après **`pcb_clearance_context_v1`** (séquentiel, même session) ; résumé checkpoint enrichi (**`pcb_nearby_items_context_v1_ok`**, **`pcb_nearby_items_context_v1_target_run_dir`**, **`pcb_nearby_items_context_v1_exit_code`**) ; copie **`qa_pcb_nearby_items_context_v1_family_summary.txt`** ; agrégation retry IPC élargie ; blocker **`pcb_nearby_items_context_v1:<blocker>`** en **priorité 8** ; **`mcp_v2_checkpoint_green`** requiert désormais **huit** familles vertes. **Aucune** modification des handlers métier ; le tool reste **context-only** (**`is_true_drc=false`**, pas DRC, pas `RunTests`, pas verdict clearance).
- **Contexte / alternatives écartées** : Laisser la famille **candidate** indéfiniment après stabilité documentée — refusé. Modifier **`get_pcb_nearby_items_context_session`** pour cette mission — refusé.
- **Date** : 2026-05-03

## R40 — Audit synthèse architecture MCP / QA / dettes (post-**R39**)

- **Problématique** : Quelle priorité donner après intégration de **`pcb_nearby_items_context_v1`** comme **8e** famille du checkpoint MCP v2 ?
- **Choix retenu** : On réalise un **audit synthèse** (familles gate, tool map, pipeline registry/handlers/IPC, fixtures, scripts, risques, opportunités) **avant** d’ajouter de nouvelles capacités produit — document de référence mission **R40** (réponse Cursor) + alignement doc interne. La prochaine impulsion prioritaire recommandée : **Option B** — durcissement **CI/QA** (parser, durée checkpoint, politique d’artefacts, **CI Windows réelle**) plutôt que nouvelle fonctionnalité MCP immédiate (**Option C**) ou refonte helpers (**Option A**) sans mesure préalable.
- **Contexte / alternatives écartées** : Enchaîner tout de suite une capacité **point-mode nearby** ou clearance enrichie — **reporté** jusqu’après stabilité mesurable du gate à **8 familles**. **Option D** (release publique / CV) utile en parallèle documentation mais **non** substitut à la fiabilité QA.
- **Date** : 2026-05-03

## R41 — Observabilité checkpoint MCP v2 (**8 familles**) / CI baseline (**sans** nouveau tool ni handler)

- **Problématique** : Comment rendre le checkpoint MCP v2 à **8 familles** plus **observable** et **exploitable en CI** sans ajouter de nouvelles capacités MCP ?
- **Choix retenu** : On ajoute des **durées** (checkpoint total + **par famille**), des **horodatages** (**`checkpoint_started_at` / `checkpoint_finished_at`**), des **compteurs retry IPC par famille** (**`ipc_smoke_ipc_retry_extra_max_*`**) en complément du **max agrégé**, on enrichit **`qa_mcp_v2_ci_baseline_summary.txt`** (**`ci_elapsed_ms`**, **`build_elapsed_ms`**, **`checkpoint_elapsed_ms`** relu depuis l’enfant, **`checkpoint_family_count=8`**, **`checkpoint_target_run_dir`**, **`artifact_root`**) et on **documente** une politique minimale d’**archivage** des artefacts (**`README_IPC_QA_BASELINE.md`**). On **vérifie** que **`qa_ipc_smoke`** borne toujours la sortie pour les tools à gros payloads (**`get_net_classes`**, **`get_board_stackup`**, **`pcb_measure_distance_session`**, **`get_pcb_item_geometry_session`**, **`get_pcb_clearance_context_session`**, **`get_pcb_nearby_items_context_session`**) — **aucun** patch handler ni nouveau tool.
- **Contexte / alternatives écartées** : Refactor lourd des scripts ou des handlers pour la seule télémétrie — refusé. Ajouter un tool dédié « CI » — refusé.
- **Date** : 2026-05-01

## R42 — Étude factorisation helpers **session PCB** MCP (**sans** implémentation dans cette mission)

- **Problématique** : Comment réduire la **duplication** entre les handlers PCB MCP session (**inspection**, **mesure**, **géométrie**, **clearance context**, **nearby**) sans risquer de changer le comportement validé par le **checkpoint à 8 familles** ?
- **Choix retenu** : On **documente** une factorisation **future** et **incrémentale** : extraire d’abord des utilitaires à **faible risque** (chemin board, résolution frame/planche, conversion IU/nm, parsing layer canonique, bbox centre nm, **`KIID` + `ResolveItem`**), dans un fichier **`pcbnew/api`** dédié ; garder dans les handlers le **packaging `Struct`**, les **blocs métier** (clearance, nearby scan, endpoints **`pcb_measure_distance_session`**, bbox PAD spécifique **`get_pcb_item_geometry_session`**). Validation après chaque fusion : **familles PCB concernées** + **`run_mcp_v2_checkpoint.ps1`** — **`exact_blocker_if_any=N/A`** inchangé comme critère de non-régression.
- **Contexte / alternatives écartées** : Refactor **massif** ou migration vers **`common/mcp`** avant preuve de besoin — reporté (couplage **`PCB_EDIT_FRAME`** / **`BOARD`**). Unifier **`pcb_frame_not_open`** vs **`pcb_context_unavailable`** sans carte précise des probes — refusé tant que les chaînes **`exact_blocker_if_any`** sont contractuelles.
- **Date** : 2026-05-03

## R43 — Refactor helpers **pcb_session_helpers** (pilote clearance + nearby)

- **Problématique** : Comment commencer à factoriser les helpers PCB session sans changer les contrats des tools déjà couverts par le checkpoint ?
- **Choix retenu** : On ajoute **`pcbnew/api/api_handler_pcb_session_helpers.{h,cpp}`** avec uniquement **`IuToNm64`**, **`LayerTokenFromPcbLayerId`**, **`ParseBoardLayerFromValue`**, **`ParseOptionalBoardLayerFromStruct`**, **`BoundingBoxCentreNm`** (algorithmes alignés sur les lambdas existantes ; **`kiapi::board::types`** explicite dans le `.cpp` pour compilation hors alias **`board::`** du PCH). Premier usage pilote : **`PcbClearanceContextSessionToolInvoke`** et **`PcbNearbyItemsContextSessionToolInvoke`** — remplacement des lambdas **`layerToken`**, **`parseOptionalLayer`**, **`bboxCentreNm`** ; **aucun** changement de **`exact_blocker_if_any`** ni de chaînes contractuelles.
- **Contexte / alternatives écartées** : **`pcb_measure_distance_session`**, inspection v2, **`get_pcb_item_geometry_session`** dans la même mission — refusé (**R43** hors périmètre).
- **Date** : 2026-05-03

## R44 — Helpers **pcb_session_helpers** phase 2 (**get_pcb_item_geometry_session**)

- **Problématique** : Comment étendre les helpers PCB session à **`get_pcb_item_geometry_session`** sans changer les contrats **`pad_anchor`** / **`pad_shape_origin`** ?
- **Choix retenu** : On remplace la lambda **`parseBoardLayerArg`** par **`ParseBoardLayerFromValue`** (logique identique) ; la lambda locale **`iuToNm`** délègue à **`IuToNm64`** pour une seule définition de la règle IU→nm. On **ne** factorise **pas** **`BoundingBoxCentreNm`** sur le chemin PAD (AABB **`ShapePos`/`GetSize`**) ni **`layer_used`** via **`LayerTokenFromPcbLayerId`** (diff possible si nom d’énum vide : ancien code omettait **`layer_used`**, le helper renverrait **`BL_UNKNOWN`**).
- **Contexte / alternatives écartées** : **`pcb_measure_distance_session`**, **`wxTheApp`/`ResolveItem`**, **`board_filename`** — hors périmètre **R44**.
- **Date** : 2026-05-03

## R45 — **`OptionalLayerProtoName`** + **`IuToNm64`** direct dans **item geometry**

- **Problématique** : Comment finaliser la réutilisation des helpers dans **`get_pcb_item_geometry_session`** sans introduire **`BL_UNKNOWN`** dans **`layer_used`** ?
- **Choix retenu** : On ajoute **`OptionalLayerProtoName(PCB_LAYER_ID)`** → **`std::nullopt`** si **`magic_enum::enum_name`** est vide, sinon la chaîne proto ; **`layer_used`** l’utilise à la place du bloc **`ToProtoEnum` + `enum_name`** local. On supprime la lambda **`iuToNm`** et on appelle **`IuToNm64`** explicitement partout dans le handler ; pas de changement sur fast-path PAD, **`pad_anchor`**, **`pad_shape_origin`**, **`ResolveItem`**, **`board_filename`**.
- **Contexte / alternatives écartées** : **`LayerTokenFromPcbLayerId`** pour **`layer_used`** — refusé (**R44**). Clearance / nearby — inchangés sauf ajout du helper dans la bibliothèque.
- **Date** : 2026-05-03

## R46 — Warning MSVC **C5246** (`handleGetGraphicsDefaults` / **`classOrder`**)

- **Problématique** : Faut-il nettoyer le warning MSVC **C5246** dans **`api_handler_pcb.cpp`** pour réduire le bruit des builds avant de poursuivre les refactors ?
- **Choix retenu** : On applique uniquement une correction locale sûre sur l’initialisation **`std::array`** (**double accolade** `{{ … }}` + forme d’init sans **`=`**), sans toucher aux handlers MCP récents (**measure / item geometry / clearance / nearby**), puis revalidation **checkpoint MCP v2** (+ **`run_pcb_inspection_v2_targeted_probes.ps1`** car **`GetGraphicsDefaults`** est API PCB générale).
- **Contexte / alternatives écartées** : Refactor plus large de **`handleGetGraphicsDefaults`** — hors périmètre **R46**.
- **Date** : 2026-05-03

## R47 — Étude refactor **`pcb_measure_distance_session`** (layer + IU-nm)

- **Problématique** : Peut-on réutiliser les helpers layer / IU-nm dans **`pcb_measure_distance_session`** sans changer les contrats très sensibles de ses endpoints ?
- **Choix retenu** (étude, pas d’implémentation en **R47**) : **Oui, de façon minimale** — remplacer uniquement le bloc parsing **`layer`** dans **`parseEndpoint`** (branche **`pad_shape_origin`**) par **`ParseBoardLayerFromValue( itLayerField->second )`**, en **ignorant** le message d’erreur générique du helper et en conservant le mapping **`invalid_layer_a` / `invalid_layer_b`** (et le reste de **`parseEndpoint`** inchangé). **`IuToNm64`** peut remplacer les **`pos.x/y * 100LL`** et le centre bbox **`ctr * 100LL`** avec le même risque fonctionnel (**quasi nul**). **`BoundingBoxCentreNm`** est équivalent au centre bbox mais retourne **`item_bbox_unavailable`** sans suffixe **`_a`/`_b`** : réutilisable **uniquement** avec remapping explicite des erreurs. **`ParseOptionalBoardLayerFromStruct`** n’apporte rien ici (champ **`layer`** déjà ciblé ; absence gérée par **`pad_shape_origin_requires_layer_*`** avant parsing). **`LayerTokenFromPcbLayerId`** / **`OptionalLayerProtoName`** : **non** pertinents pour ce handler. **Ne pas** extraire tout **`parseEndpoint`** en un mega-helper (**option C** trop risquée pour le gain).
- **Contexte / alternatives écartées** : Refactor complet **`parseEndpoint`** (**C**) ; statu quo (**A**) si priorité ailleurs ; scripts QA / schémas — inchangés en **R47**.
- **Date** : 2026-05-03

## R48 — **`pcb_measure_distance_session`** : **`ParseBoardLayerFromValue`** + **`IuToNm64`**

- **Problématique** : Comment réutiliser les helpers layer / IU-nm dans **`pcb_measure_distance_session`** sans modifier les contrats des endpoints **`a`/`b`** ?
- **Choix retenu** : On remplace seulement le parsing **`layer`** (branche **`pad_shape_origin`**) par **`ParseBoardLayerFromValue`**, en mappant tout échec vers **`invalid_layer_a` / `invalid_layer_b`** (jamais le message générique **`invalid_layer`** du helper). On remplace les **`IU * 100LL`** dans **`resolveItemNm`** (**`pad_shape_origin`**, **`pad_anchor`**, centre bbox) par **`IuToNm64`**. Pas d’extraction de **`parseEndpoint`** ; pas de **`BoundingBoxCentreNm`** ; pas de changement clearance / nearby / item geometry.
- **Contexte / alternatives écartées** : Refactor élargi (**R47** option **C**) — refusé.
- **Date** : 2026-05-03

## R49 — QA explicite alias **`F_Cu`** (**`pcb_measure_distance_session`**)

- **Problématique** : Comment verrouiller l’alias KiCad **`F_Cu`** après centralisation du parsing layer dans **`pcb_measure_distance_session`** ?
- **Choix retenu** : On ajoute dans **`targeted_pcb_measure_distance_probe.ps1`** un appel **`pad_shape_origin`** avec **`layer = "F_Cu"`** sur la fixture **`ipc_geometry_v1`**, et on exige l’égalité (à tolérance nm) avec la réponse de référence **`BL_F_Cu`** (même pad, self-distance). Flag contractuel **`measure_pad_shape_origin_alias_f_cu_ok`** ; **`pcb_measure_distance_green`** ne passe que si ce flag est **true**. Relais dans **`run_pcb_geometry_v1_targeted_probes.ps1`** (**`qa_pcb_geometry_v1_family_summary.txt`**). Aucun changement handler / helper / checkpoint global.
- **Contexte / alternatives écartées** : Modifier **`ParseBoardLayerFromValue`** ou le schéma MCP — refusé.
- **Date** : 2026-05-03

## R50 — Étude helper commun **net / netclass** (clearance + nearby)

- **Problématique** : Peut-on factoriser la lecture net / netclass entre **`get_pcb_clearance_context_session`** et **`get_pcb_nearby_items_context_session`** sans changer les champs contractuels des réponses MCP ?
- **Choix retenu** (étude **R50**, pas d’implémentation) : **Oui, pour un sous-ensemble à faible risque** — les lambdas **`connectedFromBoardItem`** et **`netStringsFromConnected`** sont **quasi identiques** entre **`PcbClearanceContextSessionToolInvoke`** et **`PcbNearbyItemsContextSessionToolInvoke`** (même ordre : **`GetNetname()`** → UTF-8, **`GetNet()`**, sinon **`(netName, \"\")`**, **`GetNetClass()`**, sinon **`(netName, \"\")`**, sinon nom de classe). Les réponses utilisent toujours **`set_string_value`** pour **`net_name` / `netclass_name`** (y compris chaînes vides) dans **`PackNearbyContextSessionStruct`** / **`PackClearanceContextSessionStruct`** — pas d’omission conditionnelle de clé pour ces champs. **Ne pas** factoriser dans un premier pas la chaîne **`clearanceNmFromNetclass` / `nominalFromConnected` / `pairNominalNm` / `clearance_status` / `required_clearance_nm`** (spécifique clearance, **`std::optional`** + omission **`geometric_distance_nm`** / **`required_clearance_nm`** déjà codée). Pour **`same_net`** et le filtre **`include_same_net`** (nearby uniquement), la logique est déjà la même idée (**`wxString`** **`GetNetname`**, non vide, égalité) mais **dupliquée** inline vs **`netStringsFromConnected`** — une fonction **`SameNetByNetname( BOARD_CONNECTED_ITEM*, BOARD_CONNECTED_ITEM* )`** alignée sur le code actuel réduirait le risque de dérive **si** on la réutilise pour les **deux** sites nearby.
- **Contexte / alternatives écartées** : Unifier **`validateLayerForItem`** vs **`validateLayerForNearby`** (règle **via** différente : clearance **Top/Bottom** vs nearby **`GetLayerSet().Contains`**) — **hors** périmètre net/netclass **R50**. Introduire **`net_code`** dans un struct public sans besoin produit — évité.
- **Date** : 2026-05-03

## R51 — Helpers **`ConnectedItemFromBoardItem`** / **`NetNameAndNetclassUtf8`** / **`SameNetByBoardNetname`**

- **Problématique** : Comment factoriser la lecture net / netclass entre clearance et nearby sans changer les champs MCP contractuels ?
- **Choix retenu** : On extrait **`ConnectedItemFromBoardItem`**, **`NetNameAndNetclassUtf8`** et **`SameNetByBoardNetname`** dans **`pcbnew/api/api_handler_pcb_session_helpers.{h,cpp}`** ; **`PcbClearanceContextSessionToolInvoke`** et **`PcbNearbyItemsContextSessionToolInvoke`** les utilisent à la place des lambdas dupliquées. **`SameNetByBoardNetname`** remplace les comparaisons **`GetNetname()`** nearby (filtre **`include_same_net`** + bool **`same_net`**). La clearance nominale (**`clearanceNmFromNetclass`**, **`pairNominalNm`**, **`required_clearance_nm`**, **`clearance_status`**) reste dans le handler clearance ; **`PackClearanceContextSessionStruct`** / **`PackNearbyContextSessionStruct`** inchangés.
- **Contexte / alternatives écartées** : Factoriser la clearance IU→nm dans ce diff — refusé (**R50** option **D**).
- **Date** : 2026-05-03

## R52 — Étude factorisation session PCB / **`board_filename`**

- **Problématique** : Peut-on factoriser la résolution session PCB et **`board_filename`** sans changer les blockers contractuels des tools MCP PCB ?
- **Choix retenu** (étude **R52**, pas d’implémentation) : **Partiellement seulement** — la normalisation **`wxFileName`** depuis l’argument (**`FromUTF8`**, **`IsOk`**, **`MakeAbsolute`**, chemin absolu) est **quasi identique** entre les 7 handlers listés ; en revanche les **blockers `exact_blocker_if_any`** et les **chemins d’erreur** divergent nettement entre **(A)** **`pcb_measure_distance_session`**, **`get_pcb_item_geometry_session`**, **`get_pcb_clearance_context_session`**, **`get_pcb_nearby_items_context_session`** (souvent **`pcb_frame_not_open`** pour toute la chaîne **`wxTheApp` → `PCB_EDIT_FRAME`**, **`invalid_board_filename`** si fichier courant invalide) et **(B)** **`pcb_get_board_items_session`**, **`pcb_get_board_item_by_id_session`**, **`pcb_get_net_items_session`** (**`pcb_context_unavailable`** pour la plupart des étapes GUI/Kiway, y compris quand le **diagnostic** textuel vaut **`pcb_frame_not_open`** ; **`board_not_loaded`** pour fichier courant invalide ou **`board` null**). Les erreurs **pré-session** (champ manquant / type) : **(B)** utilise **`mkBad`** + **`ApiResponseStatus`** ; **(A)** utilise **`fail` / `errRule` / `errNearby`** avec messages légèrement différents. Une factorisation « toute la chaîne » sans **table de mapping par famille d’outil** risque d’unifier à tort les blockers.
- **Contexte / alternatives écartées** : Factoriser d’abord uniquement les handlers « récents geometry » en excluant inspection v2 — **fragile** (deux familles à maintenir, risque d’oubli). **D** (chaîne complète + table) — acceptable seulement avec QA négative renforcée par outil.
- **Date** : 2026-05-03

## R53 — **`ParseAbsoluteBoardWxFileNameFromArgs`** (pilote clearance + nearby)

- **Problématique** : Comment factoriser le parsing **`board_filename`** sans changer les blockers contractuels divergents entre handlers PCB MCP ?
- **Choix retenu** : On extrait uniquement **`ParseAbsoluteBoardWxFileNameFromArgs`** dans **`pcb_session_helpers`**, avec **`PcbBoardFilenameParseError`** (**Missing**, **NotString**, **Empty**, **InvalidPath**, **NotAbsolute**) — **jamais** exposé au client. Premier usage pilote : **`PcbClearanceContextSessionToolInvoke`** et **`PcbNearbyItemsContextSessionToolInvoke`** ; mapping local inchangé (**`errRule` / `errNearby`**, mêmes chaînes blocker + diagnostics). Pas de factorisation Kiway / **`GetBoard`** ; pas d’extension aux autres handlers en **R53**.
- **Contexte / alternatives écartées** : Unifier **`mkBad`** vs **`fail`** — refusé (**R52**).
- **Date** : 2026-05-03

## R54 — **`ParseAbsoluteBoardWxFileNameFromArgs`** : measure + item geometry

- **Problématique** : Comment étendre le helper **`board_filename`** à measure et item geometry sans changer les blockers visibles ?
- **Choix retenu** : On réutilise **`ParseAbsoluteBoardWxFileNameFromArgs`** dans **`PcbMeasureDistanceSessionToolInvoke`** et **`PcbItemGeometrySessionToolInvoke`** ; chaque handler conserve son **`fail(...)`** local (**3** args measure, **4** args item geometry) avec les mêmes chaînes blocker / diagnostics qu’avant. Clearance / nearby / inspection v2 / **`pcb_get_*`** inchangés en **R54**.
- **Contexte / alternatives écartées** : Factoriser Kiway dans le même diff — refusé.
- **Date** : 2026-05-11

## R55 — Étude **`ParseAbsoluteBoardWxFileNameFromArgs`** pour PCB Inspection v2 (**`mkBad`**)

- **Problématique** : Peut-on réutiliser **`ParseAbsoluteBoardWxFileNameFromArgs`** dans **`PcbGetBoardItemsSessionToolInvoke`**, **`PcbGetBoardItemByIdSessionToolInvoke`** et **`PcbGetNetItemsSessionToolInvoke`** sans modifier le transport **`mkBad`** (**`tl::unexpected( ApiResponseStatus )`**, **`AS_BAD_REQUEST`**, **`error_message`**) ni les messages visibles ?
- **Choix retenu** (étude **R55**, pas d’implémentation dans cette mission) : **Oui, à condition d’un mapping local strict** — le helper applique la **même séquence** que le code actuel (**`find` → type string → non vide → `wxFileName::FromUTF8` → `IsOk` → `MakeAbsolute` → `IsAbsolute`**) ; les cas **`PcbBoardFilenameParseError`** correspondent **un à un** aux branches actuelles. **`Missing`** et **`NotString`** se fondent déjà en **un seul** **`mkBad("missing or invalid 'board_filename' (string required)")`** dans les trois handlers ; le **`switch`** post-helper doit **fusionner** ces deux énums vers ce message unique. **`Empty`**, **`InvalidPath`**, **`NotAbsolute`** mappent respectivement vers **`'board_filename' must be non-empty`**, **`'board_filename' is not a valid filesystem path`**, **`'board_filename' must be an absolute path`** (libellés **identiques** byte à byte au code actuel). Le helper ne doit **pas** être exposé au client (**pas** de fuite de **`PcbBoardFilenameParseError`** dans la réponse).
- **Différences contractuelles à respecter** : Contrairement aux tools geometry / context, les erreurs **pré-board** ici ne passent **pas** par un **`Struct`** avec **`ok=false`** / **`exact_blocker_if_any`** — elles restent **`mkBad`** → **`ApiResponseStatus`** ; **aucun** champ **`board_filename`** dans le corps succès en cas d’échec parse (inchangé). Les chemins **`makeFlat`** (mismatch, contexte PCB, etc.) ne sont **pas** concernés par cette étude.
- **Recommandation refactor (R56)** : **Option D** en priorité — petit wrapper **Inspection v2** (ex. fonction **`static`** ou **`namespace` anonyme** dans **`api_handler_pcb.cpp`**, ou helper dédié si on veut le partager) qui transforme **`PcbBoardFilenameParseError`** → **`mkBad`** avec les **quatre** messages distincts ci-dessus (**Missing**/**NotString** partagés) — **une seule** table de vérité, puis **`ParseAbsoluteBoardWxFileNameFromArgs`** dans les **trois** handlers (**Option C**). **Option B** (pilote **`PcbGetBoardItemsSessionToolInvoke`**) reste valable si l’équipe veut un diff minimal avant généralisation. **Option A** acceptable si l’on privilégie zéro mouvement de code.
- **QA / lacunes** : Les sondes ciblées (**`targeted_pcb_get_*`**) et **`run_pcb_inspection_v2_targeted_probes.ps1`** couvrent surtout le **nominal** et **`board_filename_mismatch`** (**`makeFlat`**), pas systématiquement les négatifs **parse** (champ absent, type non string, chaîne vide, chemin invalide, chemin relatif). Pour **R56**, prévoir au besoin des cas IPC négatifs ou assertions sur **`error_message`** / statut **BAD_REQUEST** pour ces cinq branches, en plus de la liste QA déjà usuelle (targeted get * + inspection v2 + geometry v1 + clearance + nearby + checkpoint **`KICAD_IPC_KIFACE_SMOKE=1`**).
- **Date** : 2026-05-11

## R56 — **`ParseAbsoluteBoardWxFileNameFromArgs`** : PCB Inspection v2 (**`mkBad`**)

- **Problématique** : Comment réutiliser **`ParseAbsoluteBoardWxFileNameFromArgs`** dans PCB Inspection v2 sans modifier les réponses **`mkBad`** existantes ?
- **Choix retenu** : On appelle **`ParseAbsoluteBoardWxFileNameFromArgs( aArgs, "board_filename" )`** dans **`PcbGetBoardItemsSessionToolInvoke`**, **`PcbGetBoardItemByIdSessionToolInvoke`** et **`PcbGetNetItemsSessionToolInvoke`** ; en cas d’échec, **`mkBadBoardFilenameFromPcbSessionParseError`** (**`namespace` anonyme** dans **`api_handler_pcb.cpp`**) construit **`kiapi::common::ApiResponseStatus`** avec **`AS_BAD_REQUEST`** et les **cinq** branches de messages **identiques** à l’ancien code (**Missing**/**NotString** → une chaîne, puis **Empty**, **InvalidPath**, **NotAbsolute**). Succès : **`requestedFn`**, **`itBoard`**, **`boardFilenameUtf8`** comme auparavant. **`mkBad`** / **`makeFlat`** des handlers inchangés ; measure / clearance / nearby / item geometry inchangés.
- **Contexte / alternatives écartées** : Étendre le helper à **`GetPcbItemsToolInvoke`** (autre **`makeBadRequestResult`**) — **hors** périmètre **R56**.
- **Note QA checkpoint** : Un run **`run_mcp_v2_checkpoint.ps1`** peut rester **`mcp_v2_checkpoint_green=false`** pour cause **schématique** (**ex.** **`sch_authoring_v2_family_green=false`**, **`schematic_place_symbol:restore_connection_refused`**) alors que toutes les familles **PCB** du checkpoint sont vertes — **indépendant** du refactor **`board_filename`** Inspection v2.
- **Date** : 2026-05-11

## R57 — Audit final des duplications restantes dans **`api_handler_pcb.cpp`**

- **Problématique** : Après les refactors **layer / nm / net / `board_filename`**, quelles duplications restantes peuvent être réduites **sans** changer les contrats MCP ?
- **Choix retenu** (audit **R57**, pas d’implémentation) : **Prioriser uniquement** les extractions **à faible risque** (**ex.** lambdas **strictement identiques** entre **deux** handlers, ou parsing **`max_items`** identique avec **`mkBad`** inchangé). **Ne pas** factoriser en une seule passe la chaîne **`wxTheApp` → `PCB_EDIT_FRAME` → `BOARD*`** entre **(A)** tools session geometry / context (**`fail` / `errRule` / `errNearby`**, **`pcb_frame_not_open`**, **`invalid_board_filename`**) et **(B)** **`pcb_get_*`** (**`makeFlat`**, **`pcb_context_unavailable`**, **`board_not_loaded`**) — les **blockers** et **diagnostics** restent **volontairement divergents** (**R52**). **Ne pas** fusionner **`validateLayerForItem`** (clearance) et **`validateLayerForNearby`** (nearby) — règles **Top/Bottom** vs **`GetLayerSet().Contains`** (**R50**). Laisser **`parseEndpoint`** measure, **`PackClearanceContextSessionStruct` / `PackNearbyContextSessionStruct`**, **`clearanceNmFromNetclass` / `pairNominalNm`**, scan / tri nearby, et les gros blocs **`CLEARANCE_CTX_OUT`** partiels tels quels sauf micro-helper **local** si besoin produit.
- **Candidats R58 recommandés (ordre)** : **(1)** Extraire **`itemTypeLabel`** (**lambda identique** clearance vs nearby) vers **`pcb_session_helpers`** sous nom stable (**ex.** **`McpPcbItemTypeToken`**) — **un** diff, familles **clearance + nearby** + checkpoint. **(2)** Helper **`ParseMaxItemsInspectionV1`** (**défaut 500**, **1–5000**, messages **`mkBad`** **identiques**) partagé par **`PcbGetBoardItemsSessionToolInvoke`** et **`PcbGetNetItemsSessionToolInvoke`**. **(3)** Optionnel : aligner **`GetBoardStackupToolInvoke`** et **`GetPcbItemsToolInvoke`** sur **`ParseAbsoluteBoardWxFileNameFromArgs`** + mapping vers **`makeBadRequestResult`** (**mêmes** chaînes qu’aujourd’hui) — périmètre **hors** liste handlers **R56** ; QA **stackup / get_pcb_items** + checkpoint.
- **QA** : Pour chaque étape **R58** : sondes ciblés de la famille touchée ; **`run_mcp_v2_checkpoint.ps1`** avec **`KICAD_IPC_KIFACE_SMOKE=1`** ; compléter au besoin les **négatifs** **`board_filename`** / **`max_items`** pour inspection si absent des scripts.
- **Date** : 2026-05-11

## R58 — **`CopperItemTypeLabelMcp`** (clearance + nearby)

- **Problématique** : Comment réduire une duplication restante entre clearance et nearby **sans** toucher aux contrats MCP ?
- **Choix retenu** : On extrait **`CopperItemTypeLabelMcp( const BOARD_ITEM* )`** dans **`pcb_session_helpers`** ; **`PcbClearanceContextSessionToolInvoke`** et **`PcbNearbyItemsContextSessionToolInvoke`** appellent le helper à la place des lambdas **`itemTypeLabel`** locales. Sémantique inchangée : **`nullptr` → chaîne vide** ; **`PCB_PAD_T` / `PCB_TRACE_T` / `PCB_ARC_T` / `PCB_VIA_T`** → **`"pad"` / `"track"` / `"arc"` / `"via"`** ; sinon **`"unsupported"`**. **`PackClearanceContextSessionStruct`**, **`PackNearbyContextSessionStruct`**, **`validateLayer*`**, scan nearby, netclass / clearance — **inchangés**.
- **Contexte / alternatives écartées** : Renommer les champs proto **`item_type`** — refusé (contrat).
- **Date** : 2026-05-11

## R59 — **`ParseInspectionMaxItemsFromArgs`** (`pcb_get_board_items` + `pcb_get_net_items`)

- **Problématique** : Comment factoriser **`max_items`** entre **`PcbGetBoardItemsSessionToolInvoke`** et **`PcbGetNetItemsSessionToolInvoke`** sans modifier les messages **`mkBad`** existants ?
- **Choix retenu** : On ajoute **`ParseInspectionMaxItemsFromArgs( const google::protobuf::Struct& )`** dans **`pcb_session_helpers`**, retour **`tl::expected<int, std::string>`** : succès → entier (**défaut 500** si champ absent, sinon entier **`number_value`** avec cast identique à l’ancien code, plage **1–5000**) ; échec → **`error()`** est **exactement** la chaîne passée à **`mkBad`** auparavant (**`invalid 'max_items' (number required)`** ou **`invalid_max_items`**). Les deux handlers font **`if (!parse) return mkBad(parse.error());`** puis **`const int maxItems = *parse`**. Nearby **`max_items`** — **hors** périmètre **R59**.
- **Contexte / alternatives écartées** : Unifier avec nearby (**défaut 25**, **`errNearby`**) — refusé.
- **Date** : 2026-05-11

## R60 — Étude **`board_filename`** : **`GetPcbItemsToolInvoke`** / **`GetBoardStackupToolInvoke`**

- **Problématique** : Peut-on réutiliser **`ParseAbsoluteBoardWxFileNameFromArgs`** dans **`GetPcbItemsToolInvoke`** et **`GetBoardStackupToolInvoke`** sans modifier les réponses legacy visibles (**`makeBadRequestResult`** / **`makeNotReadyResult`**) ?
- **Choix retenu** (étude **R60**, pas d’implémentation) : **Oui** — les deux tools parsent **`board_filename`** avec la **même séquence** que le helper (**`find` → string → non vide → `wxFileName::FromUTF8` → `IsOk` → `MakeAbsolute` → `IsAbsolute`**) et les **cinq** messages **`makeBadRequestResult(...)`** sont **identiques** byte à byte à ceux de **`mkBad`** Inspection v2 pour ces cas. **`Missing`** / **`NotString`** → **`"missing or invalid 'board_filename' (string required)"`** ; **`Empty`** → **`"'board_filename' must be non-empty"`** ; **`InvalidPath`** / **`NotAbsolute`** → idem inspection. Ensuite la chaîne Kiway utilise **`makeNotReadyResult`** (messages longs en anglais, **`AS_NOT_READY`** selon le helper anonyme) ; le **mismatch** fichier courant vs demandé reste **`makeBadRequestResult("board_filename does not match currently open PCB editor document")`** — **hors** périmètre du helper **`board_filename`**. Pas de **`Struct`** avec **`exact_blocker_if_any`** sur erreurs parse : transport **`tl::unexpected( ApiResponseStatus )`** comme les autres tools legacy. **`GetPcbItemsToolInvoke`** est le hook MCP aligné sur **`API_HANDLER_PCB::handleGetItems`** (champ proto **`types`** vs session **`item_types`** — hors sujet **`board_filename`**).
- **Recommandation implémentation (R61)** : **Option D** — factoriser **les deux** tools avec **`ParseAbsoluteBoardWxFileNameFromArgs`** + mapping local (**`switch`** sur **`PcbBoardFilenameParseError`**) vers **`makeBadRequestResult`** avec les **cinq** chaînes ci-dessus (fusion **Missing**/**NotString**), ou petit wrapper **`makeBadRequestResultFromPcbBoardFilenameParseError`** dans le **`namespace` anonyme** de **`api_handler_pcb.cpp`** (miroir de **`mkBadBoardFilenameFromPcbSessionParseError`** mais **`makeBadRequestResult`**). **Option B/C** si l’équipe veut un pilote unique. **Option A** acceptable si priorité zéro-risque. **Option E** (wrapper dédié legacy) équivalent à **D** avec nom explicite.
- **QA future (R61)** : Couverture partielle dans **`run_ipc_v1_windows.ps1`** (**`get_pcb_items`**, **`get_board_stackup`**) ; relancer **`run_mcp_v2_checkpoint.ps1`** (**`KICAD_IPC_KIFACE_SMOKE=1`**) et familles PCB session (**geometry / clearance / nearby / inspection**) pour absence de régression croisée. **Manque probable** : sondes négatives **`board_filename`** dédiées (absent, non-string, vide, relatif, mismatch) **par tool** avec assertion sur **`error_message`** exact pour **`get_pcb_items`** / **`get_board_stackup`** (au-delà des cas déjà dans **`run_ipc_v1_windows.ps1`**). **`GetNetsPcbToolInvoke`** reprend le **même** bloc manuel **`board_filename`** — candidat **même** refactor si **R61** étend le périmètre.
- **Date** : 2026-05-11

## R61 — **`ParseAbsoluteBoardWxFileNameFromArgs`** : tools legacy PCB (**`makeBadRequestResult`**)

- **Problématique** : Comment réutiliser **`ParseAbsoluteBoardWxFileNameFromArgs`** dans les tools legacy PCB sans modifier les réponses **`makeBadRequestResult`** visibles ?
- **Choix retenu** : **`ParseAbsoluteBoardWxFileNameFromArgs`** + **`makeBadRequestResultFromPcbBoardFilenameParseError`** dans **`GetPcbItemsToolInvoke`**, **`GetBoardStackupToolInvoke`** et **`GetNetsPcbToolInvoke`** (bloc parse **`board_filename`** strictement identique). Messages **`makeBadRequestResult`** inchangés ; Kiway / mismatch / **`makeNotReadyResult`** inchangés.
- **Contexte / alternatives écartées** : Factoriser Kiway legacy — refusé (**R60**).
- **Date** : 2026-05-11

## R62 — **`ParseAbsoluteBoardWxFileNameFromArgs`** : **`GetPcbItemsByIdToolInvoke`**

- **Problématique** : Peut-on appliquer le refactor board_filename legacy à **`GetPcbItemsByIdToolInvoke`** sans changer les réponses **`makeBadRequestResult`** visibles ?
- **Choix retenu** : **Patch appliqué** — bloc parse **`board_filename`** strictement compatible (messages Missing/NotString fusionnés, Empty, InvalidPath, NotAbsolute identiques ; **`makeBadRequestResult`** via **`makeBadRequestResultFromPcbBoardFilenameParseError`**). **`requestedFn`** conservé ; **`boardFilenameUtf8`** supprimé (non utilisé après parse). Kiway / mismatch / **`ids`** / **`ResolveItem`** inchangés.
- **Contexte / alternatives écartées** : Bloc manuel lignes 489–502 miroir exact de **`ParseAbsoluteBoardWxFileNameFromArgs`** ; aucune divergence d’ordre de validation ni de transport d’erreur.
- **Date** : 2026-06-05

## R63 — **`ParseAbsoluteBoardWxFileNameFromArgs`** : **`GetHitTestPcbToolInvoke`** + **`GetBoundingBoxPcbToolInvoke`**

- **Problématique** : Peut-on appliquer le refactor board_filename legacy à HitTest et BoundingBox sans modifier les réponses **`makeBadRequestResult`** visibles ?
- **Choix retenu** : **Patch appliqué sur les deux tools** — blocs parse **`board_filename`** strictement compatibles (messages Missing/NotString fusionnés, Empty, InvalidPath, NotAbsolute identiques ; **`makeBadRequestResult`** via **`makeBadRequestResultFromPcbBoardFilenameParseError`**). **`requestedFn`** conservé ; **`boardFilenameUtf8`** supprimé (non utilisé après parse). Logique HitTest / BoundingBox métier, Kiway / mismatch inchangés.
- **Contexte / alternatives écartées** : Pas d’audit large des ~20 occurrences restantes ; **`run_ipc_v1_windows.ps1`** (**`stable_declared_vs_rows_mismatch`**) hors périmètre.
- **Date** : 2026-06-05

## R64 — QA négative dédiée **`board_filename`** / **`max_items`**

- **Problématique** : Comment verrouiller les contrats d’erreur **`board_filename`** / **`max_items`** après les refactors R53–R63 ?
- **Choix retenu** : Probe dédié **`targeted_pcb_negative_contracts_probe.ps1`** (hors **`run_mcp_v2_checkpoint.ps1`** dans un premier temps) — vérifie les **`error_message`** exacts pour legacy **`makeBadRequestResult`** (6 tools), session **`mkBad`** (3 tools) et **`max_items`** Inspection v2 (**`pcb_get_board_items_session`**, spot-check **`pcb_get_net_items_session`**). Cas **`board_filename` relatif** documenté **non bloquant** (**`MakeAbsolute()`** cwd-dependent).
- **Contexte / alternatives écartées** : Pas de nouveau refactor métier ; pas d’intégration checkpoint global en R64 ; extension **`run_ipc_v1_windows.ps1`** reportée.
- **Date** : 2026-06-05

## R65 — Famille **`pcb_negative_contracts_v1`** dans le **CI baseline** (hors checkpoint)

- **Problématique** : Comment exploiter le probe négatif R64 comme garde-fou QA reproductible sans ajouter une 9e famille au checkpoint MCP v2 ?
- **Choix retenu** : Runner **`run_pcb_negative_contracts_v1_targeted_probes.ps1`** (wrap **`targeted_pcb_negative_contracts_probe.ps1`**, résumé **`qa_pcb_negative_contracts_v1_family_summary.txt`**) intégré à **`run_mcp_v2_ci_baseline.ps1`** **avant** le checkpoint ; **`ci_baseline_green`** dépend de **`pcb_negative_contracts_v1_ok`**. **`run_mcp_v2_checkpoint.ps1`** inchangé (**8 familles**).
- **Contexte / alternatives écartées** : Pas d’intégration checkpoint global en R65 ; pas de refactor handlers ; cas **`board_filename` relatif** reste advisory dans le probe.
- **Date** : 2026-06-05

## R66 — **`pcb_negative_contracts_v1`** dans le **préflight local** (hors checkpoint)

- **Problématique** : Comment rendre les contrats **`board_filename`** / **`max_items`** détectables en itération locale rapide sans ajouter une 9e famille au checkpoint ?
- **Choix retenu** : **`run_mcp_v2_baseline_preflight.ps1`** lance **`run_pcb_negative_contracts_v1_targeted_probes.ps1`** après init / smoke KIFACE optionnel, **avant** **`targeted_pcb_measure_distance_probe.ps1`** ; **`preflight_ok`** dépend de **`pcb_negative_contracts_v1_family_green`** ; champs ajoutés dans **`qa_mcp_v2_baseline_preflight_summary.txt`**. Checkpoint **8 familles** inchangé.
- **Contexte / alternatives écartées** : Pas d’intégration checkpoint ; pas de refactor handlers.
- **Date** : 2026-06-05

## R67 — **`ParseAbsoluteBoardWxFileNameFromArgs`** : petit lot legacy 2 (pad polygon / layers)

- **Problématique** : Poursuivre le refactor legacy **`board_filename`** sans modifier les réponses **`makeBadRequestResult`** visibles ?
- **Choix retenu** : **Patch appliqué sur les trois tools** — **`GetPadShapeAsPolygonPcbToolInvoke`**, **`GetVisibleLayersPcbToolInvoke`**, **`GetBoardEnabledLayersPcbToolInvoke`** (blocs parse strictement identiques à R61–R63 ; **`makeBadRequestResultFromPcbBoardFilenameParseError`**). **`requestedFn`** conservé ; logique métier / Kiway / mismatch inchangés.
- **Contexte / alternatives écartées** : Pas d’audit large des autres occurrences ; checkpoint **8 familles** inchangé.
- **Date** : 2026-06-05

## R68 — QA négative : verrouillage contrats **`board_filename`** tools R67

- **Problématique** : Les trois tools refactorés en R67 n’étaient pas couverts par **`targeted_pcb_negative_contracts_probe.ps1`** (6 legacy + 3 session seulement).
- **Choix retenu** : Extension du probe — cas **absent / vide / non-string** pour **`get_visible_layers`**, **`get_board_enabled_layers`**, **`get_pad_shape_as_polygon`** ; flags dédiés **`legacy_*_board_filename_negative_ok`** ; **`pcb_negative_contracts_green`** dépend des nouveaux flags. Cas **relatif** inchangé (advisory, cwd-dependent). **Aucun** patch handler.
- **Contexte / alternatives écartées** : Pas d’intégration checkpoint **8 familles** ; pas de refactor métier supplémentaire.
- **Date** : 2026-06-05

## R69 — CI baseline : réparation toolchain MSVC sous shell minimal

- **Problématique** : **`run_mcp_v2_ci_baseline.ps1`** échouait sur **`Test-MsvcToolchainArtifacts`** (`where` introuvable) dans l’environnement Cursor / cmd minimal.
- **Cause** : subprocess **`cmd.exe /c`** sans **`%SystemRoot%\System32`** sur **`PATH`** ; **`where`** (sans **`.exe`**) non résolu après **`VsDevCmd.bat`** (invocation sans **`call`**).
- **Choix retenu** : **`set PATH=%SystemRoot%\System32;%PATH%`**, **`call VsDevCmd`**, **`where.exe`** explicite, échecs via **`(where.exe X || (echo FAIL:… & exit /b N))`** (pas **`if errorlevel`** dans une chaîne **`&&`** — coupe silencieuse), étapes **`[toolchain]`** dans **`toolchain_verify.log`** ; champ summary **`toolchain_missing_binary`**. Aucun patch handler / checkpoint.
- **Contexte / alternatives écartées** : Pas de déplacement vers helper commun (problème spécifique au gate CI baseline) ; préflight inchangé (pas de check MSVC).
- **Date** : 2026-06-05

## R70 — **`ParseAbsoluteBoardWxFileNameFromArgs`** : petit lot legacy 3 (layer tools)

- **Problématique** : Poursuivre le refactor legacy **`board_filename`** sur les tools layer GUI sans modifier les réponses **`makeBadRequestResult`** ?
- **Choix retenu** : **Patch sur quatre tools** — **`GetBoardLayerNamesPcbToolInvoke`**, **`GetActiveLayerPcbToolInvoke`**, **`SetActiveLayerPcbToolInvoke`**, **`SetVisibleLayersPcbToolInvoke`** (mapping strictement identique à R61–R67 ; **`makeBadRequestResultFromPcbBoardFilenameParseError`** ; **`requestedFn`** conservé). Logique métier / Kiway / mismatch inchangés.
- **Contexte / alternatives écartées** : Pas d’extension immédiate du probe négatif (hors scope) ; checkpoint **8 familles** inchangé.
- **Date** : 2026-06-05

## R71 — QA négative : verrouillage contrats **`board_filename`** tools R70

- **Problématique** : Les quatre tools refactorés en R70 n’étaient pas couverts par **`targeted_pcb_negative_contracts_probe.ps1`**.
- **Choix retenu** : Extension du probe — cas **absent / vide / non-string** pour **`get_board_layer_names`**, **`get_active_layer`**, **`set_active_layer`**, **`set_visible_layers`** (après **`qa_open_editor_frame`** — callbacks layer) ; flags **`legacy_*_board_filename_negative_ok`** ; **`pcb_negative_contracts_green`** dépend des nouveaux flags. **Aucun** patch handler ; checkpoint **8 familles** inchangé.
- **Contexte / alternatives écartées** : Pas d’intégration checkpoint ; pas de refactor métier.
- **Date** : 2026-06-05

## R72 — **`ParseAbsoluteBoardWxFileNameFromArgs`** : petit lot legacy 4 (appearance / origins)

- **Problématique** : Poursuivre le refactor legacy **`board_filename`** sur appearance / origins sans modifier les réponses **`makeBadRequestResult`** ?
- **Choix retenu** : **Patch sur quatre tools** — **`GetBoardEditorAppearanceSettingsPcbToolInvoke`**, **`SetBoardEditorAppearanceSettingsPcbToolInvoke`**, **`GetBoardOriginPcbToolInvoke`**, **`GetBoardExtentPcbToolInvoke`** (mapping strictement identique à R61–R70 ; **`makeBadRequestResultFromPcbBoardFilenameParseError`** ; **`requestedFn`** conservé). Logique métier / Kiway / mismatch inchangés.
- **Contexte / alternatives écartées** : Pas d’extension probe négatif (hors scope) ; scripts QA / checkpoint inchangés.
- **Date** : 2026-06-05

## R73 — QA négative : verrouillage contrats **`board_filename`** tools R72

- **Problématique** : Les quatre tools refactorés en R72 n’étaient pas couverts par **`targeted_pcb_negative_contracts_probe.ps1`** (13 legacy seulement).
- **Choix retenu** : Extension du probe — cas **absent / vide / non-string** pour **`get_board_editor_appearance_settings`**, **`set_board_editor_appearance_settings`**, **`get_board_origin`**, **`get_board_extent`** (après **`qa_open_editor_frame`**) ; flags **`legacy_*_board_filename_negative_ok`** ; **`pcb_negative_contracts_green`** dépend des nouveaux flags. **17 legacy tools** couverts au total. **Aucun** patch handler ; checkpoint **8 familles** inchangé.
- **Contexte / alternatives écartées** : Pas d’intégration checkpoint ; pas de refactor métier.
- **Date** : 2026-06-05

## R74 — **`ParseAbsoluteBoardWxFileNameFromArgs`** : petit lot legacy 5 (CRUD + SetBoardOrigin)

- **Problématique** : Poursuivre le refactor legacy **`board_filename`** sur CRUD PCB + **`SetBoardOrigin`** sans modifier les réponses **`makeBadRequestResult`** ?
- **Choix retenu** : **Patch sur quatre tools** — **`SetBoardOriginPcbToolInvoke`**, **`CreatePcbItemsToolInvoke`**, **`DeletePcbItemsToolInvoke`**, **`UpdatePcbItemsToolInvoke`** (mapping strictement identique à R61–R72 ; **`makeBadRequestResultFromPcbBoardFilenameParseError`** ; **`requestedFn`** conservé). Logique métier / Kiway / mismatch inchangés.
- **Contexte / alternatives écartées** : Pas d’extension probe négatif (hors scope) ; scripts QA / checkpoint inchangés.
- **Date** : 2026-06-05

## R75 — QA négative : verrouillage contrats **`board_filename`** tools R74

- **Problématique** : Les quatre tools refactorés en R74 n’étaient pas couverts par **`targeted_pcb_negative_contracts_probe.ps1`** (17 legacy seulement).
- **Choix retenu** : Extension du probe — cas **absent / vide / non-string** pour **`set_board_origin`**, **`create_pcb_items`**, **`delete_pcb_items`**, **`update_pcb_items`** (après **`qa_open_editor_frame`**) ; flags **`legacy_*_board_filename_negative_ok`** ; **`pcb_negative_contracts_green`** dépend des nouveaux flags. **21 legacy tools** couverts au total. **Aucun** patch handler ; checkpoint **8 familles** inchangé.
- **Contexte / alternatives écartées** : Pas d’intégration checkpoint ; pas de refactor métier ; pas d’arguments métier ajoutés pour les cas négatifs (parse **`board_filename`** reste le premier contrôle).
- **Date** : 2026-06-05

## R76 — Audit : 4 blocs legacy **`board_filename`** restants (sans patch)

- **Problématique** : Après R74/R75, il reste 4 blocs manuels **`itBoard`** — footprint library ×2, lambdas **`get_pcb_items`** ×2. Faut-il les refactorer avec **`ParseAbsoluteBoardWxFileNameFromArgs`** ?
- **Choix retenu** : **Audit seul, aucun patch.** Verdict : **lambdas `get_pcb_items`** — mapping **exact** (blocs strictement identiques entre eux, équivalents au pré-R61) ; patch R77 recommandé par **délégation à `GetPcbItemsToolInvoke`** (déduplication, risque **low**). **Footprint library** — mapping **non exact** vs R61–R74 : ordre de validation différent (Search : path après params métier ; Inspect : trim ASCII, pas de **`IsAbsolute`**, mismatch en **`makeInspectResult`** structuré, Kiway avant path) ; **pas de patch** avec le helper standard sans décision produit explicite.
- **Contexte / alternatives écartées** : Option D (4 blocs d’un coup) **rejetée** ; QA négative footprint library via probes dédiés **`targeted_library_footprint_*_probe.ps1`**, pas extension **`pcb_negative_contracts`** (21 tools).
- **Date** : 2026-06-05

## R77 — Déduplication lambdas inline **`get_pcb_items`**

- **Problématique** : Deux lambdas inline **`get_pcb_items`** (~220 lignes chacune) dupliquaient **`GetPcbItemsToolInvoke`** déjà refactoré (R61).
- **Choix retenu** : Remplacer le corps des deux lambdas par **`return kicad::mcp::GetPcbItemsToolInvoke( aArgs );`** — enregistrements MCP, descriptors, schemas et **`SetGetPcbItemsInvoke`** inchangés. Footprint library **non** touchée.
- **Contexte / alternatives écartées** : Pas de suppression d’un enregistrement (les deux chemins restent actifs) ; pas de modification **`GetPcbItemsToolInvoke`**.
- **Date** : 2026-06-05

## R78 — QA négative **`board_filename`** footprint library (probes dédiés)

- **Problématique** : Les deux handlers footprint library restants ont **`mapping_exact=false`** (R76) — ils ne doivent pas entrer dans **`targeted_pcb_negative_contracts_probe.ps1`** (21 legacy).
- **Choix retenu** : Extension **`targeted_library_footprint_search_probe.ps1`** et **`targeted_library_footprint_inspect_probe.ps1`** — négatifs **`board_filename`** avec contrats réels (Search : priorité **`query`** avant path ; Inspect : trim whitespace, mismatch structuré **`board_filename_mismatch`**). Probes **dédiés** — **hors** preflight / CI baseline MCP v2 / checkpoint **8 familles**.
- **Contexte / alternatives écartées** : Pas de patch handler ; pas d’intégration **`pcb_negative_contracts`** ; pas de refactor **`ParseAbsoluteBoardWxFileNameFromArgs`**.
- **Date** : 2026-06-05

## R79 — Runner **`run_library_v1_targeted_probes.ps1`** : remontée flags négatifs footprint

- **Problématique** : Les flags négatifs R78 dans les probes footprint n’étaient pas agrégés au résumé famille **`qa_library_v1_family_summary.txt`**.
- **Choix retenu** : Extension du runner — lecture **`footprint_search_board_filename_negative_green`** (report **non-gating**) et **`footprint_inspect_board_filename_negative_green`** (**gating** famille, avec détail mismatch/whitespace/advisory). **`footprint_search` happy-path timeout** reste diagnostic non-gating. Hors checkpoint MCP v2 / preflight / CI baseline.
- **Contexte / alternatives écartées** : Pas de patch handler ; pas de mélange avec **`pcb_negative_contracts_v1`**.
- **Date** : 2026-06-05

## R80 — Diagnostic timeout **`library_footprint_search_session`** happy path

- **Problématique** : Le happy path **`footprint_search_ok`** échouait avec **`footprint_search_tool_timeout`** (240 s) via **`Invoke-SmokeBounded`**, alors que les négatifs **`board_filename`** passaient en quelques secondes. Inspect footprint (même fixture) restait vert.
- **Cause retenue** : **Harness**, pas handler métier. Le handler répond en **~25–60 ms** en appel direct **`qa_ipc_smoke`** ; la sortie happy path fait **~4,8 KiB** (`printResponse` + `Utf8DebugString` du résultat). **`Invoke-SmokeBounded`** lit stdout **après** `HasExited` sans drainage asynchrone → **deadlock pipe Windows (~4 KiB)** → le processus smoke est tué au timeout harness ; artefact = littéral `timeout`, **aucune réponse partielle**.
- **Choix retenu** : Patch probe **`targeted_library_footprint_search_probe.ps1`** — appels search (négatifs + happy) en **`& qa_ipc_smoke` direct**, aligné sur **`targeted_library_footprint_inspect_probe.ps1`**. **`footprint_search_ok`** reste **non-gating** famille. Aucun patch handler C++ ; pas de modification **`run_mcp_v2_checkpoint.ps1`** ni intégration library v1 au checkpoint MCP v2.
- **Options R81** : **A** garder non-gating documenté ; **B** borner **`ipc_smoke.cpp`** pour `library_footprint_search_session` (défense en profondeur) ; **C** alléger requête happy path (non nécessaire — handler déjà rapide) ; **D** fixture minimale (déjà en place : `qa_resistors` / 6 footprints) ; **E** corriger **`Invoke-SmokeBounded`** (lecture stdout asynchrone, bénéfice transversal) ; **F** handler early-exit `max_results` (optimisation métier optionnelle).
- **Recommandation R81** : **E** (fix **`Invoke-SmokeBounded`** dans **`ipc_qa_common.ps1`**) + **B** optionnel (branche stdout bornée dans **`ipc_smoke.cpp`**, même pattern que `pcb_get_board_items_session`).
- **Date** : 2026-06-05

## R81 — Fix transversal **`Invoke-SmokeBounded`** (deadlock pipe stdout Windows)

- **Problématique** : R80 a montré que le timeout search happy path venait du harness : **`Invoke-SmokeBounded`** attendait **`HasExited`** avant **`ReadToEnd`**, provoquant un deadlock parent/enfant quand **`qa_ipc_smoke`** émettait **>~4 KiB** (ex. `library_footprint_search_session`).
- **Choix retenu** : Patch **`ipc_qa_common.ps1`** — drainage via **`StandardOutput.ReadToEndAsync`** / **`StandardError.ReadToEndAsync`** lancé juste après **`Start()`**, puis attente process + **`Task.Wait`** (évite deadlock pipe Windows **>~4 KiB**). Timeout/kill/`Text=timeout`/format de retour inchangés ; **`Invoke-SmokeBoundedIpcRetry`** inchangé (délègue toujours à **`Invoke-SmokeBounded`**). Revert workaround R80 : **`targeted_library_footprint_search_probe.ps1`** repasse sur **`Invoke-SmokeBounded`**. Aucun rebuild C++ ; **`footprint_search_ok`** reste **non-gating**.
- **Contexte / alternatives écartées** : Pas de patch handler ; pas de borne **`ipc_smoke.cpp`** dans cette mission (option B R80 reportée ultérieurement) ; pas de modification checkpoint MCP v2.
- **Date** : 2026-06-05

## R82 — Gating **`footprint_search_ok`** dans **`run_library_v1_targeted_probes.ps1`**

- **Problématique** : Après R81, le happy path **`library_footprint_search_session`** est stable via **`Invoke-SmokeBounded`** ; **`footprint_search_ok`** restait report-only (**`footprint_search_gating=false`**) dans le runner Library v1.
- **Choix retenu** : **`footprint_search_ok=true`** requis pour **`library_v1_family_green`** ; **`footprint_search_gating=true`** ; **`footprintSearch`** ajouté à la boucle **`$gatingResults`** ; **`exact_blocker_if_any`** via préfixe **`search_ok:`** (ou **`footprint_search:`** fallback). Négatifs **`footprint_search_board_filename_negative_green`** restent **report-only** ; négatifs inspect inchangés (**gating**). Hors checkpoint MCP v2 / preflight / CI baseline.
- **Contexte / alternatives écartées** : Pas de patch handler C++ ; pas d’intégration library v1 au checkpoint MCP v2 ; pas de gating des négatifs search **`board_filename`** (reporté R83).
- **Date** : 2026-06-05

## R83 — Gating négatifs **`board_filename`** Search footprint library

- **Problématique** : Après R82, **`footprint_search_ok`** est gating mais **`footprint_search_board_filename_negative_green`** restait report-only (**`footprint_search_board_filename_negative_gating=false`**).
- **Choix retenu** : **`footprint_search_board_filename_negative_green=true`** requis pour **`library_v1_family_green`** ; **`footprint_search_board_filename_negative_gating=true`** ; échec → **`exact_blocker_if_any=footprint_search_board_filename_negative_green:<valeur>`**. Champs advisory conservés (**`footprint_search_board_filename_invalid_path_ok`** skipped wx permissif, etc.). Hors checkpoint MCP v2.
- **Contexte / alternatives écartées** : Pas de patch handler ; pas de gating de l’advisory invalid-path seul.
- **Date** : 2026-06-05

## R84 — PATH idempotent dans **`Initialize-IpcQaEnvironment`**

- **Problématique** : Chaque appel à **`Initialize-IpcQaEnvironment`** préfixait **`$env:PATH`** sans déduplication ; sessions PowerShell longues (enchaînement probes / CI) dupliquaient les mêmes segments build → **`PATH` trop long** / échec CI baseline.
- **Choix retenu** : Helpers **`Get-IpcQaNormalizedPathKey`** + **`Set-IpcQaPathPrefixIdempotent`** dans **`ipc_qa_common.ps1`** — préfixes build ajoutés une seule fois, ordre de priorité inchangé (kicad → eeschema → pcbnew → common → gal → api → vcpkg bin → python) ; reste du PATH dédupliqué (comparaison Windows case-insensitive + slash normalisé, **`Resolve-Path`** quand le segment existe). Variables d’env KiCad inchangées.
- **Contexte / alternatives écartées** : Pas de patch handlers C++ ; pas de modification checkpoint MCP v2 ; pas de reset PATH global dans les runners.
- **Date** : 2026-06-05

## R85 — Test QA versionné idempotence PATH IPC

- **Problématique** : R84 a corrigé l’accumulation PATH mais sans test harness versionné pour éviter les régressions.
- **Choix retenu** : Script standalone **`test_ipc_qa_path_idempotence.ps1`** — mesure PATH avant/après **`Initialize-IpcQaEnvironment`** répété (défaut 10×), vérifie préfixes build requis + **`Test-IpcKifaceDllLoad`** ; résumé **`qa_ipc_path_idempotence_summary.txt`** (`ipc_path_idempotence_green`, `path_len_*`, `exact_blocker_if_any`). Documenté dans **`README_IPC_QA_BASELINE.md`**. **Hors** checkpoint MCP v2 / CI baseline par défaut.
- **Contexte / alternatives écartées** : Pas d’intégration obligatoire preflight/CI ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R86 — Garde-fou PATH dans **`run_mcp_v2_baseline_preflight.ps1`**

- **Problématique** : R85 fournit **`test_ipc_qa_path_idempotence.ps1`** en standalone ; les régressions PATH en session longue devaient être détectées plus tôt en préflight local.
- **Choix retenu** : Appel du script R85 dans **`run_mcp_v2_baseline_preflight.ps1`** après **`Initialize-IpcQaEnvironment`** / smoke KIFACE optionnel, **avant** **`pcb_negative_contracts_v1`** ; champs **`ipc_path_idempotence_ok`**, **`ipc_path_idempotence_target_run_dir`**, **`ipc_path_idempotence_exit_code`**, **`ipc_path_idempotence_summary_path`** dans **`qa_mcp_v2_baseline_preflight_summary.txt`** ; **`preflight_ok`** exige **`ipc_path_idempotence_green=true`**. **Hors** **`-VerifyBinariesOnly`** (quitte avant init env) et **hors** checkpoint MCP v2.
- **Contexte / alternatives écartées** : Pas de duplication logique PATH dans le préflight ; pas d’intégration checkpoint ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R87 — Garde-fou PATH dans **`run_mcp_v2_ci_baseline.ps1`**

- **Problématique** : R86 intègre le test PATH au préflight local ; le pipeline **`run_mcp_v2_ci_baseline.ps1`** ne le couvrait pas encore.
- **Choix retenu** : Helper **`Invoke-IpcPathIdempotenceGuard`** — appelle **`test_ipc_qa_path_idempotence.ps1`** (R85) après init env / smoke KIFACE, **avant** probes PCB ; champs **`ipc_path_idempotence_*`** dans **`qa_mcp_v2_ci_baseline_summary.txt`** ; **`ci_baseline_green`** exige **`ipc_path_idempotence_green=true`**. **`-VerifyOnly`** : inclut le garde-fou PATH après **`Initialize-IpcQaEnvironment`** (toolchain + binaires + smoke, sans probes/checkpoint). Hors checkpoint MCP v2.
- **Contexte / alternatives écartées** : Pas de duplication logique PATH ; pas d’intégration checkpoint ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R88 — Factorisation **`Invoke-IpcPathIdempotenceGuard`** dans **`ipc_qa_common.ps1`**

- **Problématique** : R86/R87 dupliquent le pattern d’appel de **`test_ipc_qa_path_idempotence.ps1`** entre préflight et CI baseline.
- **Choix retenu** : Helper commun **`Invoke-IpcPathIdempotenceGuard`** dans **`ipc_qa_common.ps1`** — lance le script R85, capture **`TARGET_RUN_DIR`**, lit **`qa_ipc_path_idempotence_summary.txt`**, retourne **`Ok`**, **`TargetRunDir`**, **`ExitCode`**, **`SummaryPath`** (+ **`Green`**). **`run_mcp_v2_baseline_preflight.ps1`** et **`run_mcp_v2_ci_baseline.ps1`** délèguent au helper ; champs summary **`ipc_path_idempotence_*`** inchangés ; **`Throw`** par défaut si non vert (switch **`-NoThrow`**). Comportement **`-VerifyBinariesOnly`** / **`-VerifyOnly`** inchangé.
- **Contexte / alternatives écartées** : Pas de duplication logique interne du test R85 ; pas d’intégration checkpoint ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R89 — Factorisation **`Invoke-IpcQaChildScript`** (exécution scripts enfants QA)

- **Problématique** : **`Invoke-PreflightChild`** et **`Invoke-ProbeCaptureTargetDir`** dupliquent le spawn PowerShell + capture stdout/stderr/exit/**`TARGET_RUN_DIR`** ; les runners **`run_*_targeted_probes.ps1`** ont une variante (parse **`TARGET_RUN_DIR`** depuis stdout+stderr + lecture green domain-specific).
- **Choix retenu** : Audit → mapping sûr pour préflight/CI baseline uniquement. Helper **`Invoke-IpcQaChildScript`** + **`Get-IpcQaChildTargetRunDir`** dans **`ipc_qa_common.ps1`** ; wrappers locaux **`Invoke-PreflightChild`** / **`Invoke-ProbeCaptureTargetDir`** conservés (délégation, **`TargetRunDirSource=StdoutOnly`**). **`Invoke-IpcPathIdempotenceGuard`** migré sur le helper commun. Runners familles / checkpoint **non** patchés (différences parse + green — mission R90).
- **Contexte / alternatives écartées** : Pas de méga-refonte ; pas de changement formats summary ; pas de patch handlers C++ / checkpoint.
- **Date** : 2026-06-05

## R90 — Migration pilote **`Invoke-TargetedProbe`** → **`Invoke-IpcQaChildScript`**

- **Problématique** : Les runners **`run_pcb_negative_contracts_v1_targeted_probes.ps1`** et **`run_library_v1_targeted_probes.ps1`** dupliquent encore le spawn PowerShell (variante stdout+stderr pour **`TARGET_RUN_DIR`**).
- **Choix retenu** : Audit → mapping sûr. **`run_pcb_negative_contracts_v1_targeted_probes.ps1`** : spawn inline remplacé par **`Invoke-IpcQaChildScript`** (**`TargetRunDirSource=StdoutAndStderr`**, **`LogBaseName`** = nom script complet pour logs `*.ps1.stdout.log`). **`run_library_v1_targeted_probes.ps1`** : wrapper **`Invoke-TargetedProbe`** conservé, délègue au helper ; lecture green / gating library v1 / champs summary inchangés. Autres runners famille / checkpoint **hors** périmètre.
- **Contexte / alternatives écartées** : Pas de changement critères **`family_green`** ; pas d’intégration library v1 au checkpoint ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R91 — Migration **`run_pcb_geometry_v1_targeted_probes.ps1`** → **`Invoke-IpcQaChildScript`**

- **Problématique** : **`run_pcb_geometry_v1_targeted_probes.ps1`** (préflight + CI baseline + checkpoint) dupliquait encore le spawn PowerShell dans **`Invoke-TargetedProbe`**.
- **Choix retenu** : Wrapper **`Invoke-TargetedProbe`** conservé — délègue à **`Invoke-IpcQaChildScript`** (**`TargetRunDirSource=StdoutAndStderr`**, logs `*.ps1.stdout.log`) ; lecture **`measure_*`** / **`item_geometry_*`**, **`pcb_geometry_v1_family_green`**, **`exact_blocker_if_any`**, copy summaries inchangés. Clearance / nearby / inspection / SCH **hors** périmètre.
- **Contexte / alternatives écartées** : Pas de changement champs summary ; pas de patch handlers C++ / checkpoint.
- **Date** : 2026-06-05

## R92 — Migration PCB context **`clearance`** + **`nearby`** → **`Invoke-IpcQaChildScript`**

- **Problématique** : **`run_pcb_clearance_context_v1_targeted_probes.ps1`** et **`run_pcb_nearby_items_context_v1_targeted_probes.ps1`** (familles checkpoint MCP v2) dupliquaient encore le spawn inline stdout+stderr.
- **Choix retenu** : Spawn remplacé par **`Invoke-IpcQaChildScript`** (**`TargetRunDirSource=StdoutAndStderr`**, logs `*.ps1.stdout.log`) ; lecture green, **`ipc_smoke_ipc_retry_extra_family`**, copy summary, **`exact_blocker_if_any`**, critères **`pcb_clearance_context_v1_family_green`** / **`pcb_nearby_items_context_v1_family_green`** inchangés. **`run_mcp_v2_checkpoint.ps1`**, inspection v2, SCH **hors** périmètre.
- **Contexte / alternatives écartées** : Pas de changement champs summary ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R93 — Migration **`run_pcb_inspection_v2_targeted_probes.ps1`** → **`Invoke-IpcQaChildScript`**

- **Problématique** : Dernière famille PCB checkpoint (**`pcb_inspection_v2`**, 3 probes) dupliquait encore le spawn dans **`Invoke-TargetedProbe`**.
- **Choix retenu** : Wrapper **`Invoke-TargetedProbe`** conservé — délègue à **`Invoke-IpcQaChildScript`** (**`TargetRunDirSource=StdoutAndStderr`**, **`LogBaseName`** = label court `pcb_get_board_items` / `pcb_get_board_item_by_id` / `pcb_get_net_items` pour logs inchangés) ; fonctions contrat **`Test-PcbGet*ContractOk`**, **`pcb_inspection_v2_family_green`**, **`probe_script_missing`**, champs summary inchangés. Familles SCH / **`run_mcp_v2_checkpoint.ps1`** **hors** périmètre.
- **Contexte / alternatives écartées** : Pas de changement critères green ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R94 — Migration pilote SCH **`run_sch_geometry_v2_targeted_probes.ps1`** → **`Invoke-IpcQaChildScript`**

- **Problématique** : Première famille SCH checkpoint (**`sch_geometry_v2`**, 3 probes) dupliquait encore le spawn dans **`Invoke-TargetedProbe`**.
- **Choix retenu** : Wrapper **`Invoke-TargetedProbe`** conservé — délègue à **`Invoke-IpcQaChildScript`** (**`TargetRunDirSource=StdoutAndStderr`**, logs `*.ps1.stdout.log`) ; critères **`schematic_move_symbol_ok`**, **`schematic_move_label_ok`**, **`schematic_create_wire_segment_ok`**, **`sch_geometry_v2_family_green`**, blockers familiaux inchangés. Autres familles SCH / **`run_mcp_v2_checkpoint.ps1`** **hors** périmètre. Flakes SCH connus (**`connection_refused`**, **`restore_connection_refused`**) documentés comme préexistants.
- **Contexte / alternatives écartées** : Pas de changement champs summary ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R95 — Migration SCH **`run_sch_authoring_v2_targeted_probes.ps1`** → **`Invoke-IpcQaChildScript`**

- **Problématique** : Famille **`sch_authoring_v2`** (2 probes : create_label, place_symbol) dupliquait encore le spawn ; famille historiquement sujette aux flakes **`restore_connection_refused`**.
- **Choix retenu** : Wrapper **`Invoke-TargetedProbe`** conservé — délègue à **`Invoke-IpcQaChildScript`** (**`TargetRunDirSource=StdoutAndStderr`**, logs `*.ps1.stdout.log`) ; critères **`schematic_create_label_ok`**, **`schematic_place_symbol_ok`**, **`sch_authoring_v2_family_green`**, blockers inchangés. Wiring / symbol_editing / checkpoint **hors** périmètre. Procédure flake : relancer authoring seul puis checkpoint.
- **Contexte / alternatives écartées** : Pas de changement champs summary ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R96 — Migration SCH **`run_sch_wiring_v2_targeted_probes.ps1`** → **`Invoke-IpcQaChildScript`**

- **Problématique** : Famille **`sch_wiring_v2`** (2 probes connect) dupliquait encore le spawn ; particularité **` -MinimalPayload`** passé aux scripts enfants.
- **Choix retenu** : Wrapper **`Invoke-TargetedProbe`** conservé — délègue à **`Invoke-IpcQaChildScript`** avec **`-ExtraArgs @('-MinimalPayload')`**, **`TargetRunDirSource=StdoutAndStderr`** ; critères **`schematic_connect_symbol_pin_ok`**, **`schematic_connect_pins_l_shape_ok`**, **`sch_wiring_v2_family_green`** inchangés. Symbol_editing / checkpoint **hors** périmètre.
- **Contexte / alternatives écartées** : Pas de changement champs summary ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R97 — Migration SCH **`run_sch_symbol_editing_v2_targeted_probes.ps1`** → **`Invoke-IpcQaChildScript`**

- **Problématique** : Dernière famille SCH checkpoint (**`sch_symbol_editing_v2`**, 3 probes set_field/value/reference) dupliquait encore le spawn.
- **Choix retenu** : Wrapper **`Invoke-TargetedProbe`** conservé — délègue à **`Invoke-IpcQaChildScript`** (**`TargetRunDirSource=StdoutAndStderr`**, pas d’**`ExtraArgs`**) ; critères **`schematic_set_symbol_*_ok`**, **`sch_symbol_editing_v2_family_green`**, priorité blockers field→value→reference inchangés. **Toutes les 8 familles checkpoint** utilisent désormais le helper commun (runners ciblés). **`run_mcp_v2_checkpoint.ps1`** (`Invoke-FamilyRunner`) **hors** périmètre R97.
- **Contexte / alternatives écartées** : Pas de changement champs summary ; pas de patch handlers C++.
- **Date** : 2026-06-05

## R98 — Audit **`Invoke-FamilyRunner`** (`run_mcp_v2_checkpoint.ps1`) → **`Invoke-IpcQaChildScript`**

- **Problématique** : Après **R97**, les **8** runners familles délèguent le spawn à **`Invoke-IpcQaChildScript`** ; seul **`run_mcp_v2_checkpoint.ps1`** conserve un bloc **`ProcessStartInfo`** local dans **`Invoke-FamilyRunner`** (~L117–219). Décider si une migration spawn-only est **comportementalement exacte** sans toucher aux champs **`qa_mcp_v2_checkpoint_summary.txt`**, aux critères **`mcp_v2_checkpoint_green`**, ni à l’ordre des familles.
- **Audit (14 points)** :
  1. **Structure** : fonction locale **`Invoke-FamilyRunner`** — params **`RunnerScriptName`**, **`SummaryName`** ; retour **`RunnerScript`**, **`ExitCode`**, **`TargetRunDir`**, **`Ok`**, **`ExactBlocker`**, **`ElapsedMs`**.
  2. **Spawn** : **`powershell.exe -NoProfile -ExecutionPolicy Bypass -File`** + **`-BuildDir`** uniquement (pas d’**`ExtraArgs`** — wiring **`-MinimalPayload`** reste **dans** le runner enfant).
  3. **Capture** : **`ReadToEnd`** stdout/stderr, **`WaitForExit`** — identique au helper.
  4. **`TARGET_RUN_DIR`** : regex sur **stdout + stderr** (**`StdoutAndStderr`**) — aligné familles **R90–R97**.
  5. **Logs** : **`$runDir\$RunnerScriptName.stdout.log`** / **`.stderr.log`** (nom complet **`*.ps1`**) — équivalent **`LogDir=$runDir`**, **`LogBaseName=$RunnerScriptName`**.
  6. **`ElapsedMs`** : **`Stopwatch`** arrêté **juste après** **`WaitForExit`** ( **avant** lecture/copie summary) — **non** fourni par le helper ; le wrapper doit conserver le chronomètre autour du spawn délégué.
  7. **Exit codes** : propagés tels quels ; si **≠ 0** et **`ExactBlocker=N/A`** → **`runner_exit_code_<n>`** (préfixe **`runner_`**, pas **`probe_`**).
  8. **Copie summary** : **`Copy-Item`** depuis **`Join-Path $targetRunDir $SummaryName`** vers **`$runDir`** — logique **hors** helper (inchangée post-migration spawn-only).
  9. **`*_family_green`** : **non lus** au checkpoint — **`Ok`** = **`exact_blocker_if_any=N/A`** **et** **`ExitCode=0`** après lecture du résumé familial copié (même sémantique que **`Invoke-TargetedProbe`** côté runners, sans relire la ligne **`sch_*_family_green`**).
  10. **`*_ok`** : dérivés de **`.Ok`** ; **`mcp_v2_checkpoint_green`** = AND des **8** **`*_ok`** + indépendant du smoke KIFACE (sauf branche early exit smoke).
  11. **`exact_blocker_if_any`** (checkpoint) : chaîne **`sch_*:`** / **`pcb_*:`** + priorité **`ExitCode≠0`** puis **`.ExactBlocker`** dans l’ordre familial fixe — logique **hors** spawn.
  12. **`ipc_smoke_ipc_retry_extra_total_max_across_families`** : max sur summaries **copiés** dans **`$runDir`** (patterns R24/R33/R39) + champs **`ipc_smoke_ipc_retry_extra_max_*`** via **`Get-MaxIpcRetryFromFamilySummary`** — **inchangés** si copie summary préservée.
  13. **Échecs** : script absent → retour anticipé **`runner_script_missing`**, **`ExitCode=-1`**, **`ElapsedMs=0`** (le helper **throw** si appelé sans garde — **pré-check obligatoire**) ; **`runner_target_run_dir_missing`**, **`summary_missing`**, **`exact_blocker_field_missing`** inchangés.
  14. **Smoke KIFACE** : branche **`KICAD_IPC_KIFACE_SMOKE=1`** **avant** les familles — **aucun** lien avec **`Invoke-FamilyRunner`** ; early exit **`kiface_dll_loadlibrary_failed`** sans appeler les runners.
- **Comparaison helper** :
  - **Couvert** : spawn, capture, logs UTF-8, **`TargetRunDir`** (**`StdoutAndStderr`**), **`SummaryPath`** déductible via **`-SummaryFileName`**.
  - **Non couvert (rester dans wrapper)** : **`ElapsedMs`**, garde script absent sans throw, préfixes blockers **`runner_*`**, lecture **`exact_blocker_if_any`**, **`Copy-Item`**, calcul **`.Ok`**, agrégation checkpoint.
- **Verdict audit** : **`mapping_exact=true`** pour une migration **spawn-only** (délégation **`Invoke-IpcQaChildScript`** + **`Stopwatch`** + garde **`Test-Path`**), en conservant **intégralement** la logique summary/blocker/agrégation existante.
- **Paramètres recommandés R99** : **`-ScriptPath $runnerPath`**, **`-BuildDir $BuildDir`**, **`-LogDir $runDir`**, **`-LogBaseName $RunnerScriptName`**, **`-TargetRunDirSource StdoutAndStderr`**, **`-SummaryFileName $SummaryName`** (optionnel ; **`SummaryPath`** du retour peut remplacer **`Join-Path`** manuel).
- **Risques** : (1) oublier le **`Test-Path`** avant le helper → throw au lieu de **`runner_script_missing`** ; (2) inclure lecture summary dans le **`Stopwatch`** → dérive **`sch_*_elapsed_ms`** ; (3) changer les préfixes **`runner_*`** → casse **`exact_blocker_if_any`** et télémetrie ; (4) lire **`*_family_green`** à la place de **`exact_blocker_if_any`** → changement critère **interdit**. Aucun impact attendu sur champs summary checkpoint ni **`mcp_v2_checkpoint_green`** si R99 respecte spawn-only.
- **Choix retenu (R98)** : **Audit-only** — **aucun patch** checkpoint ; **`Invoke-FamilyRunner`** audité.
- **Recommandation R99** : **Migration sûre** — remplacer le bloc **`ProcessStartInfo`** (~L143–161) par **`Invoke-IpcQaChildScript`** dans **`Invoke-FamilyRunner`** (même pattern que **R94–R97** **`Invoke-TargetedProbe`**), sans étendre le helper sauf besoin futur (**`ElapsedMs`** optionnel, non requis).
- **Contexte / alternatives écartées** : Extension helper **`ElapsedMs`** — possible mais **non nécessaire** (wrapper **`Stopwatch`** suffit) ; fusion **`Invoke-FamilyRunner`** dans le helper — **déconseillée** (logique agrégation checkpoint spécifique) ; maintien status quo — acceptable mais laisse la **dernière** duplication spawn.
- **Date** : 2026-06-05

## R99 — Migration **`Invoke-FamilyRunner`** (`run_mcp_v2_checkpoint.ps1`) → **`Invoke-IpcQaChildScript`**

- **Problématique** : Dernière duplication spawn PowerShell (**R98** audit **`mapping_exact=true`**) dans le gate MCP v2 — **`Invoke-FamilyRunner`** (~L117–219).
- **Choix retenu** : Migration **spawn-only** — bloc **`ProcessStartInfo`** remplacé par **`Invoke-IpcQaChildScript`** (**`TargetRunDirSource=StdoutAndStderr`**, **`LogBaseName=$RunnerScriptName`**, **`SummaryFileName=$SummaryName`**) ; **`Test-Path`** avant appel ( **`runner_script_missing`** ) ; **`Stopwatch`** autour du spawn uniquement ; blockers **`runner_*`**, copie summary via **`$child.SummaryPath`**, **`.Ok`** = **`exact_blocker_if_any=N/A`** + **`ExitCode=0`** inchangés ; ordre **8 familles**, champs **`qa_mcp_v2_checkpoint_summary.txt`**, critères **`mcp_v2_checkpoint_green`**, agrégation retry IPC, branche smoke KIFACE **inchangés**.
- **Contexte / alternatives écartées** : Pas d’extension **`ipc_qa_common.ps1`** ; pas de lecture **`*_family_green`** au checkpoint ; pas de patch handlers C++ / runners familles.
- **Date** : 2026-06-05

## R100 — Diagnostic flake SCH authoring **`schematic_place_symbol`**

- **Problématique** : Flake récurrent sur **`sch_authoring_v2`** / probe **`targeted_schematic_place_symbol_probe.ps1`** — blockers **`restore_connection_refused`**, **`connection_refused`**, **`dry_run_phase_not_ok`** ; visible en standalone, checkpoint et CI baseline ; **non** lié à R99 (spawn checkpoint).
- **Déroulé audité** :
  1. **`run_sch_authoring_v2_targeted_probes.ps1`** : séquentiel **`create_label`** puis **`place_symbol`** (spawn séparé, pas de retry familial).
  2. **`place_symbol`** : **`Stop-KiCadProcesses`** → KiCad → resolve URL → ping → open editor → **dry_run** → **restart KiCad** (two-phase) → mutation **`schematic_place_symbol_session`** → readback → **`delete_sch_items`** (restore) → readback.
  3. **`restore_connection_refused`** : phase **post-mutation** — appel **`delete_sch_items`** (L479–490) ; log typique **`nng_dial: Connection refused`** ; souvent **`post_mutation_ping_ok=false`** (ping **`timeout`**) alors que **mutation OK** (`mutation_ok=true`, `symbol_created_ok=true`).
  4. **`dry_run_phase_not_ok`** : échec **dry_run** avant restart (L572) ; log typique **`schematic_place_symbol_dry_run_output.log` = `timeout`** ( **`Invoke-SmokeBounded`** ), pas mutation atteinte.
  5. **`connection_refused`** : échec direct **`schematic_place_symbol_session`** mutation (L404–421) ; retry **local 2 tentatives** + ping intermédiaire déjà présent.
  6. **Standalone vs checkpoint vs CI** : même probe / mêmes blockers ; checkpoint/CI ajoutent **charge amont** (géométrie SCH, **`create_label`**, familles PCB) → flake **plus fréquent observé** mais **non déterministe** (caractérisation R100 : **5/5** authoring + **3/3** checkpoint verts avec nettoyage process entre checkpoints).
  7. **Retry IPC borné famille** : **absent** sur authoring (pas de champ **`ipc_smoke_ipc_retry_extra_*`** dans summaries place_symbol / family authoring, contrairement clearance/nearby).
  8. **Socket IPC** : **`Clear-IpcSockets`** ( **`api*.sock`** temp) + **`Resolve-IpcUrlFromSockets`** au démarrage et après restart two-phase ; **pas** de re-resolve avant appels post-mutation.
  9. **Processus résiduels** : probe appelle **`Stop-KiCadProcesses`** en entrée ; flake persiste avec artefact **`restore_connection_refused`** malgré mutation réussie → cause **IPC indisponible après mutation**, pas script absent au démarrage.
  10. **Timing** : **`Start-Sleep 1000ms`** avant post-mutation ping ; retries post-mutation **2×500ms** — insuffisant quand serveur MCP transient down après **`ensure_fresh_connectivity`**.
- **Artefacts rouges analysés** (ex.) :
  - **`…190650419-schematic-place-symbol-targeted`** : `restore_connection_refused` ; `delete_sch_items_symbol_restore_output.log` = `nng_dial: Connection refused` ; `post_mutation_ping.log` = `timeout` ; mutation log **AS_OK**.
  - **`…185051031-schematic-place-symbol-targeted`** : `dry_run_phase_not_ok` ; `schematic_place_symbol_dry_run_output.log` = `timeout`.
- **Verdict diagnostic** :
  - **Hypothèse principale** : **instabilité / indisponibilité transitoire du socket IPC MCP après mutation réussie** (connectivity refresh) — échec **`delete_sch_items`** / readback ; classé **IPC socket not ready / timing harness post-mutation**.
  - **Hypothèse secondaire** : **timeout harness dry_run** (socket pas prêt ou KiCad lent) → **`dry_run_phase_not_ok`** ; classé **timeout / timing harness phase dry_run**.
  - **Écartées** : restore projet/schéma ( **`restore_*`** = cleanup **`delete_sch_items`**, pas reload schéma) ; schéma MCP ; régression R99 checkpoint ; processus résiduel **seul** (nettoyage améliore mais n’explique pas mutation OK + IPC down après).
- **Choix retenu (R100)** : **Diagnostic-only** — **aucun patch** handlers ni retry permanent checkpoint.
- **Recommandation R101** : **Retry borné ciblé + attente socket plus stricte dans le probe harness uniquement** — ex. après mutation réussie : re-**`Resolve-IpcUrlFromSockets`** / boucle ping renforcée avant **`delete_sch_items`** et readbacks ; retry **`Test-IsRetriableIpcOutput`** avec backoff **>500ms** (3–5 tentatives) ; retry dry_run sur **`timeout`**/**`connection_refused`** avant **`dry_run_phase_not_ok`** final ; champs summary probe **inchangés** ou champs **advisory** optionnels (`post_mutation_resolve_ok`, `restore_retry_count`) ; **pas** de retry checkpoint global tant que le probe n’est pas durci.
- **Contexte / alternatives écartées** : Patch handlers C++ (hors périmètre) ; retry permanent **`run_mcp_v2_checkpoint.ps1`** (masquerait la cause) ; **`SkipDryRun`** systématique (affaiblirait le contrat two-phase).
- **Date** : 2026-06-05

## R101 — Durcissement harness **`targeted_schematic_place_symbol_probe.ps1`** (flake IPC post-mutation)

- **Problématique** : Flake **`sch_authoring_v2:schematic_place_symbol`** (**R100**) — IPC indisponible après mutation réussie (`restore_connection_refused`, `dry_run_phase_not_ok`, `connection_refused`).
- **Choix retenu** : Retry **borné probe-local** (**`ipcPostMutationMaxExtraAttempts=3`**, backoff 500/1000/2000 ms) — **`Resolve-AndPingCurrentKiCad`** + **`Sync-ProbeIpcUrl`** avant relances dry-run / ping post-mutation / readback / **`delete_sch_items`** ; mutation retry existant enrichi d’un re-resolve ; **blockers finaux inchangés** (`dry_run_phase_not_ok`, `connection_refused`, `restore_connection_refused`, …) ; champs **advisory** ajoutés au summary probe : **`place_symbol_ipc_reconnect_extra_total`**, **`place_symbol_dry_run_retry_extra_total`**, **`place_symbol_restore_retry_extra_total`**, **`post_mutation_ping_retry_extra_total`** ; **`run_sch_authoring_v2_targeted_probes.ps1`** / checkpoint / handlers C++ **inchangés**.
- **Contexte / alternatives écartées** : Pas d’extension **`ipc_qa_common.ps1`** ; pas de retry checkpoint global ; pas de masquage échec métier (contrat dry-run / création symbole inchangé).
- **Date** : 2026-06-05

## R102 — Restart cleanup-only post-mutation KiCad exit (`targeted_schematic_place_symbol_probe.ps1`)

- **Problématique** : Artefact R101 **`…193519774-*`** — mutation **AS_OK** puis **`kicad_alive_at_post_mutation=false`** (process **`HasExited`**, pas simple perte socket sur process vivant) ; restore/readback impossibles → **`restore_connection_refused`** malgré retries R101.
- **Audit** :
  1. Logs **`ipc_reconnect_*`** → **`kicad_not_alive`** (process mort, re-resolve inutile sur PID stale).
  2. Mutation tool **AS_OK** + **`symbol_id`** retourné — création session confirmée ; persistance disque **non garantie** si exit avant save.
  3. Restart cleanup-only **sûr** : relire **`get_sch_items_by_id`** après reopen ; si trouvé → **`delete_sch_items`** + vérif ; si absent → cleanup **non requis** (schéma propre, documenté **`post_mutation_cleanup_not_required.txt`**).
- **Choix retenu** : **`Invoke-PostMutationCleanupRestart`** (1 restart borné) si **`kicad_exited_post_mutation_detected=true`** et readback/restore incomplets ; champs advisory **`kicad_exited_post_mutation_detected`**, **`post_mutation_cleanup_restart_*`**, **`post_mutation_created_item_found_after_restart`**, **`post_mutation_cleanup_after_restart_ok`** ; retry mutation renforcé (**`place_symbol_mutation_retry_extra_total`**, max **1+3** avec re-resolve) ; **blockers finaux inchangés** si échec définitif (`restore_connection_refused`, `connection_refused`, `dry_run_phase_not_ok`) ; checkpoint / handlers C++ **inchangés**.
- **Contexte / alternatives écartées** : Patch handlers C++ ; retry checkpoint global ; succès silencieux sans vérif readback/delete ou sans constat « non persisté ».
- **Date** : 2026-06-05

## R103 — Télémétrie advisory R102 → **`sch_authoring_v2`** + checkpoint MCP v2

- **Problématique** : Champs advisory R102 (**`kicad_exited_post_mutation_detected`**, cleanup restart, retries) visibles uniquement dans **`qa_schematic_place_symbol_session_contract_summary.txt`** — pas agrégés au niveau famille / gate.
- **Choix retenu** : Relais **non bloquant** — **`run_sch_authoring_v2_targeted_probes.ps1`** lit le summary probe copié et écrit **`schematic_place_symbol_*`** (9 champs) dans **`qa_sch_authoring_v2_family_summary.txt`** ; **`run_mcp_v2_checkpoint.ps1`** relaie **6** champs **`sch_authoring_place_symbol_*`** depuis le summary familial copié. **`sch_authoring_v2_family_green`**, **`mcp_v2_checkpoint_green`**, **`exact_blocker_if_any`**, agrégation retry IPC **inchangés** ; handlers C++ / probe place_symbol **inchangés**.
- **Contexte / alternatives écartées** : Critères green basés sur advisory — rejeté ; retry checkpoint global — hors périmètre.
- **Date** : 2026-06-05

## R104 — Politique d’alerte advisory SCH authoring post-mutation

- **Problématique** : Télémétrie R103 (**`sch_authoring_place_symbol_*`**) disponible au checkpoint sans règle explicite de lecture vs critères green — risque de confusion avec un gate ou d’ignorer des exits KiCad récupérés.
- **Choix retenu** : **Documentation-only** — politique advisory dans **`README_IPC_QA_BASELINE.md`** §7bis, **`MCP_V1_CHECKPOINT_RELEASE_NOTE.md`** (SCH Authoring), **`MCP_KICAD_USER_DEV_GUIDE.md`** ; **aucun** changement scripts QA / critères **`mcp_v2_checkpoint_green`**. **`sch_authoring_place_symbol_kicad_exited_post_mutation_detected`** = exit process post-mutation AS_OK ; **non bloquant**. Seuils : ponctuel → artefacts ; **≥ 2/10** runs place_symbol ou **≥ 1/jour** checkpoint CI → investiguer crash ; advisory + **`restore_connection_refused`** / **`connection_refused`** → incident QA. Liste artefacts à conserver documentée.
- **Contexte / alternatives écartées** : Gate CI sur advisory — rejeté ; patch handlers — hors périmètre ; retry supplémentaire — rejeté.
- **Date** : 2026-06-05

## R105 — Script read-only **`report_sch_authoring_advisory.ps1`**

- **Problématique** : Politique R104 documentée mais sans outil automatisé de lecture des champs **`sch_authoring_place_symbol_*`** au checkpoint — risque d’ignorer les warnings ou de confondre advisory et gate green.
- **Choix retenu** : Script **read-only** **`report_sch_authoring_advisory.ps1`** — lit **`qa_mcp_v2_checkpoint_summary.txt`** (dernier **`*-mcp-v2-checkpoint`** sous **`BuildDir\ipc-v1-artifacts`** ou **`-CheckpointRunDir`**) et **`qa_sch_authoring_v2_family_summary.txt`** si présent ; **`Write-Warning`** selon seuils R104 (**`kicad_exited_post_mutation_detected`**, cleanup restart, retries > 0, incident QA si blockers IPC) ; résumé **`qa_sch_authoring_advisory_report_summary.txt`** sous **`…-sch-authoring-advisory-report`** ; **exit 0** même avec warnings ; **exit 1** seulement si summary introuvable / parsing impossible. **Non intégré** à **`run_mcp_v2_checkpoint.ps1`** ; critères **`mcp_v2_checkpoint_green`** inchangés.
- **Contexte / alternatives écartées** : Intégration au checkpoint (fail gate) — rejeté ; modification handlers / probe — hors périmètre.
- **Date** : 2026-06-05

## R106 — Report advisory SCH authoring dans **`run_mcp_v2_ci_baseline.ps1`** (observability-only)

- **Problématique** : **`report_sch_authoring_advisory.ps1`** (R105) utile en standalone mais absent du pipeline CI baseline — télémétrie advisory non archivée automatiquement après checkpoint.
- **Choix retenu** : **`run_mcp_v2_ci_baseline.ps1`** appelle **`report_sch_authoring_advisory.ps1 -CheckpointRunDir $checkpoint_target_run_dir`** **après** checkpoint enfant vert ; champs **`sch_authoring_advisory_report_ok`**, **`sch_authoring_advisory_report_exit_code`**, **`sch_authoring_advisory_report_target_run_dir`**, **`sch_authoring_advisory_report_summary_path`**, **`sch_authoring_advisory_warning_emitted`**, **`sch_authoring_advisory_recommended_action`**, **`sch_authoring_advisory_report_blocker`** dans **`qa_mcp_v2_ci_baseline_summary.txt`** — **hors** calcul **`ci_baseline_green`**. Échec structurel report → **`sch_authoring_advisory_report_ok=false`** sans fail gate CI (**R106**). **`-VerifyOnly`** : report **non** lancé (**`N/A`**). Checkpoint MCP v2 / handlers C++ **inchangés**.
- **Contexte / alternatives écartées** : Fail gate CI sur advisory — rejeté ; intégration au checkpoint MCP v2 — rejeté.
- **Date** : 2026-06-05

## R107 — **`-ScanLastNCheckpoints`** sur **`report_sch_authoring_advisory.ps1`**

- **Problématique** : Report R105/R106 ne couvre qu’un seul checkpoint — le seuil frequence R104 (**>= 2** exits sur serie, **>= 1** checkpoint CI/jour) reste manuel.
- **Choix retenu** : Paramètre **`-ScanLastNCheckpoints N`** — scan des N derniers **`*-mcp-v2-checkpoint`** sous **`ArtifactRoot`** / **`BuildDir\ipc-v1-artifacts`** ; agrégation compteurs/sommes advisory ; checkpoints **legacy** sans champs R103 traités comme **`false`/`0`** en mode scan ; **`Write-Warning`** si signal ponctuel, retries, **`kicad_exited_post_mutation_detected_count >= 2`**, ou **>= 1** exit sur checkpoint du jour UTC ; summary **`scan_mode=last_n_checkpoints`** + champs agrégés ; **incompatible** avec **`-CheckpointRunDir`** ; **exit 0** avec warnings ; mode checkpoint unique **strict** inchangé. **`run_mcp_v2_ci_baseline.ps1`** / critères green **inchangés** (R108 intègre le scan).
- **Contexte / alternatives écartées** : Fail gate sur frequence — rejeté ; modification CI baseline pour scan auto — hors périmètre R107.
- **Date** : 2026-06-05

## R108 — Scan advisory multi-checkpoint dans **`run_mcp_v2_ci_baseline.ps1`**

- **Problématique** : **`-ScanLastNCheckpoints`** (R107) disponible en standalone mais absent du pipeline CI baseline — seuils frequence R104 non archivés automatiquement après un run CI complet.
- **Choix retenu** : **`run_mcp_v2_ci_baseline.ps1`** appelle **`report_sch_authoring_advisory.ps1 -ScanLastNCheckpoints 10`** **après** le report checkpoint unique (R106) ; champs **`sch_authoring_advisory_scan_*`** (ok, exit, target_run_dir, summary_path, warning, recommended_action, compteurs agrégés, blocker) dans **`qa_mcp_v2_ci_baseline_summary.txt`** — **hors** **`ci_baseline_green`**. Échec structurel scan → **`sch_authoring_advisory_scan_ok=false`** sans fail gate. **`-VerifyOnly`** / **`-SkipCheckpoint`** : scan **non** lancé (**`N/A`** / **`skipped`**). Report unique R106 **inchangé**.
- **Contexte / alternatives écartées** : Fail gate CI sur frequence advisory — rejeté ; modification **`report_sch_authoring_advisory.ps1`** — fix minimal checkpoints legacy sans champs R103 en mode scan uniquement.
- **Date** : 2026-06-05

## R109 — Clôture release QA harness SCH authoring observability (**R100–R108**)

- **Problématique** : Série R100–R108 (flake **`schematic_place_symbol`**, harness R101–R102, advisory R103–R108) dispersée dans plusieurs docs — besoin d’une **synthèse de clôture** sans rouvrir le gate MCP v2 / CI baseline.
- **Choix retenu** : **Documentation-only** — synthèse dans **`README_IPC_QA_BASELINE.md`** §7ter, **`MCP_V1_CHECKPOINT_RELEASE_NOTE.md`** (SCH Authoring), **`MCP_KICAD_USER_DEV_GUIDE.md`** ; récap missions R100–R108, statut final (gate inchangé, advisory observability-only, CI enrichi G2/G3), champs **`sch_authoring_place_symbol_*`**, **`sch_authoring_advisory_report_*`**, **`sch_authoring_advisory_scan_*`**, politique de lecture, limites (legacy pre-R103, N=10 fixe, VerifyOnly), pistes R110+ (paramètre scan N, job CI séparé, eeschema, dette v1). **Aucun** changement scripts gate / handlers C++ / critères green.
- **Contexte / alternatives écartées** : Fail gate advisory — rejeté ; patch probe/checkpoint — série considérée **clôturée** côté observability.
- **Date** : 2026-06-06

## R110 — Paramètre **`-SchAuthoringAdvisoryScanLastN`** sur **`run_mcp_v2_ci_baseline.ps1`**

- **Problématique** : Scan advisory G3 utilisait **`N=10`** codé en dur (**R108**) — pas de réglage CI sans patch script.
- **Choix retenu** : Paramètre CLI **`[int]$SchAuthoringAdvisoryScanLastN = 10`** — passé à **`report_sch_authoring_advisory.ps1 -ScanLastNCheckpoints`** ; validation **`<= 0`** → **`throw`** (pas de « scan désactivé ») ; summary CI **`sch_authoring_advisory_scan_last_n`** ; **`-VerifyOnly`** / **`-SkipCheckpoint`** : **`N/A`** / **`skipped`** ; **hors** **`ci_baseline_green`**. Comportement défaut **N=10** inchangé ; handlers / checkpoint / **`report_sch_authoring_advisory.ps1`** inchangés.
- **Contexte / alternatives écartées** : **`N=0`** pour désactiver le scan — rejeté (ambiguïté) ; fail gate sur scan — rejeté.
- **Date** : 2026-06-06

## R111 — Audit **`Invoke-FamilyRunner`** (`run_mcp_v1_checkpoint.ps1`) → **`Invoke-IpcQaChildScript`**

- **Problématique** : **`run_mcp_v1_checkpoint.ps1`** conserve le **dernier** gate checkpoint avec spawn **`ProcessStartInfo`** local (**`Invoke-FamilyRunner`**, ~L71–166) — duplication déjà éliminée sur MCP v2 (**R99**). Décider si migration spawn-only est **comportementalement exacte** sans toucher **`qa_mcp_v1_checkpoint_summary.txt`**, profils **smoke/core/full**, critères **`mcp_v1_*_checkpoint_green`**, blockers préfixés, ni ordre des familles.
- **Constats audit** :
  1. **Une** fonction spawn : **`Invoke-FamilyRunner`** — params **`RunnerScriptName`**, **`SummaryName`** ; retour **`Runner`**, **`ExitCode`**, **`TargetRunDir`**, **`Ok`**, **`ExactBlocker`** (pas d’**`ElapsedMs`** — absent du summary v1).
  2. **Scripts enfants** (ordre) : toujours **`run_simulation_v1_1_targeted_probes.ps1`** ; **`run_library_v1_targeted_probes.ps1`** (profil **full**) ; **`run_sch_connectivity_v1_targeted_probes.ps1`** (**core/full**) ; toujours **`run_sch_actions_v1_targeted_probes.ps1`**. Profils **`-Profile smoke|core|full`**, **`-IncludeLibrary`** force **full**.
  3. **`TARGET_RUN_DIR`** : regex sur **stdout + stderr** → **`TargetRunDirSource=StdoutAndStderr`** (aligné familles v1 et **R99**).
  4. **Logs** : **`$RunnerScriptName.stdout.log`** / **`.stderr.log`** → **`-LogDir $runDir`**, **`-LogBaseName $RunnerScriptName`**.
  5. **Summary** : copie **`Join-Path $targetRunDir $SummaryName`** → checkpoint ; lecture **`exact_blocker_if_any`** ; **`.Ok`** si **`N/A`** + exit 0 — **identique** logique **R99** (ne lit **pas** **`*_family_green`**).
  6. **Blockers checkpoint** : **`Get-SmokeCheckpointBlocker`** / **`Get-CoreCheckpointBlocker`** / **`Get-FullCheckpointBlocker`** — priorité simulation → library → connectivity → actions ; préfixes **`simulation_v1_1:`**, etc.
  7. **Champs summary** : **`mcp_v1_checkpoint_profile`**, **`mcp_v1_smoke/core/full_checkpoint_green`**, **`mcp_v1_checkpoint_green`**, **`exact_blocker_*`**, **`_*_ok`**, **`_*_target_run_dir`**, **`_*_exit_code`**, **`library_v1_included`**, **`sch_connectivity_v1_included`** — **inchangés** si spawn-only.
  8. **Pas** de smoke KIFACE, pas d’agrégation retry IPC, pas de dot-source **`ipc_qa_common.ps1`** aujourd’hui — **R112** ajoutera **`. $ipc_qa_common.ps1`** uniquement.
  9. **Runners enfants** : **`run_library_v1_targeted_probes.ps1`** déjà sur **`Invoke-IpcQaChildScript`** (R90) ; **`run_sch_actions_v1`** / **`run_sch_connectivity_v1`** conservent spawn probe local — **hors** périmètre R112 checkpoint.
- **Mapping `Invoke-IpcQaChildScript`** : **`-ScriptPath`**, **`-BuildDir`**, **`-LogDir $runDir`**, **`-LogBaseName $RunnerScriptName`**, **`-TargetRunDirSource StdoutAndStderr`**, **`-SummaryFileName $SummaryName`** — **`ExtraArgs`** : aucun. **`mapping_exact=true`**, **`patch_recommended=true`**, **`risk_level=low`** (mêmes garde-fous **R99** : **`Test-Path`** avant helper → **`runner_script_missing`**, ne pas fusionner logique blocker/green dans le helper).
- **QA R111** : profil **smoke** exécuté (~107 s) — **`mcp_v1_checkpoint_green=true`**, **`exact_blocker_if_any=N/A`**. Profil **full** non lancé (long, simulation 9 contrats + 3 familles ; **hors** gate MCP v2 / CI baseline).
- **Choix retenu (R111)** : **Audit-only** — **aucun patch** ; recommandation **R112** migration spawn-only **`Invoke-FamilyRunner`**.
- **Contexte / alternatives écartées** : Migration runners **`sch_actions`** / **`sch_connectivity`** en même temps — reporté ; extension helper — **non nécessaire** ; maintien status quo — acceptable mais dernière duplication spawn checkpoint.
- **Date** : 2026-06-06

## R112 — Migration **`Invoke-FamilyRunner`** (`run_mcp_v1_checkpoint.ps1`) → **`Invoke-IpcQaChildScript`**

- **Problématique** : Dernière duplication spawn checkpoint (**R111** **`mapping_exact=true`**) — **`Invoke-FamilyRunner`** (~L71–166) bloc **`ProcessStartInfo`** local.
- **Choix retenu** : Migration **spawn-only** (pattern **R99** MCP v2) — **`. ipc_qa_common.ps1`** ; **`Test-Path`** avant helper (**`runner_script_missing`**) ; **`Invoke-IpcQaChildScript`** (**`-LogDir $runDir`**, **`-LogBaseName $RunnerScriptName`**, **`-TargetRunDirSource StdoutAndStderr`**, **`-SummaryFileName $SummaryName`**) ; logique **`.Ok`**, blockers **`runner_*`**, copie summaries, profils **smoke/core/full**, ordre familles, champs **`qa_mcp_v1_checkpoint_summary.txt`**, critères **`mcp_v1_*_checkpoint_green`** **inchangés**. Runners enfants v1 **non** modifiés.
- **Contexte / alternatives écartées** : Lecture **`*_family_green`** pour **`.Ok`** — rejeté ; fusion logique blocker dans le helper — rejeté.
- **Date** : 2026-06-06

## R113 — Migration **`Invoke-TargetedProbe`** (runners v1 **`sch_actions`** / **`sch_connectivity`**) → **`Invoke-IpcQaChildScript`**

- **Problématique** : Dernière duplication spawn côté runners v1 MCP (**R111** §9) — **`run_sch_actions_v1_targeted_probes.ps1`** et **`run_sch_connectivity_v1_targeted_probes.ps1`** conservent **`ProcessStartInfo`** local dans **`Invoke-TargetedProbe`** (~L14–109 chacun).
- **Constats audit (identiques structurellement, pattern R94 v2)** :
  1. **Fonction spawn** : **`Invoke-TargetedProbe`** — params **`ProbeScriptName`**, **`SummaryName`** ; retour **`ProbeScript`**, **`ExitCode`**, **`TargetRunDir`**, **`Ok`**, **`ExactBlocker`**.
  2. **Probes enfants** — **`sch_actions`** (ordre) : **`targeted_schematic_set_symbol_value_probe.ps1`**, **`targeted_schematic_set_symbol_field_probe.ps1`**, **`targeted_schematic_set_label_text_probe.ps1`**. **`sch_connectivity`** : **`targeted_schematic_net_inspect_probe.ps1`**, **`targeted_schematic_symbol_connectivity_probe.ps1`**.
  3. **`TARGET_RUN_DIR`** : regex **stdout + stderr** → **`TargetRunDirSource=StdoutAndStderr`**.
  4. **Logs** : **`$ProbeScriptName.stdout.log`** / **`.stderr.log`** → **`-LogDir $runDir`**, **`-LogBaseName $ProbeScriptName`**.
  5. **`ExtraArgs`** : aucun (seul **`-BuildDir`** passé aux probes).
  6. **`probe_script_missing`** : **`Test-Path`** avant helper (helper **throw** si script absent — garde-fou identique R94/R112).
  7. **Summaries** : copie vers **`$runDir`** ; lecture **`exact_blocker_if_any`** ; **`.Ok`** si **`N/A`** + exit 0 ; **`sch_*_v1_family_green`** = AND des **`*_ok`** per-probe — **inchangé**.
  8. **Blockers** : **`probe_target_run_dir_missing`**, **`summary_missing`**, **`exact_blocker_field_missing`**, **`probe_exit_code_<n>`** ; **`firstBlocker`** préfixé (**`schematic_set_symbol_value:`**, etc.).
- **Mapping** : **`mapping_exact=true`**, **`patch_recommended=true`**, **`risk_level=low`** (les deux runners).
- **Choix retenu** : Migration **spawn-only** — **`. ipc_qa_common.ps1`** ; **`Invoke-IpcQaChildScript`** avec **`SummaryFileName`** ; critères / champs family summary **inchangés**. **`run_mcp_v1_checkpoint.ps1`** **non** modifié.
- **Contexte / alternatives écartées** : Fusion logique green/blocker dans le helper — rejeté ; migration **`simulation`** (pas de spawn probe familial) — hors scope.
- **Date** : 2026-06-06

## R114 — Audit clôture arc spawn QA **R89–R113** + **`run_simulation_v1_1_targeted_probes.ps1`**

- **Problématique** : Dernier cas mentionné post-R113 — **`run_simulation_v1_1_targeted_probes.ps1`** : migration spawn restante ou clôture de l’arc factorisation **`Invoke-IpcQaChildScript`** ?
- **Constats audit simulation v1.1** :
  1. **Aucun spawn enfant** — pas de **`ProcessStartInfo`**, pas de **`Invoke-TargetedProbe`**, pas d’appel probe PowerShell.
  2. **Modèle agrégation** — boucle sur **9** contrats canoniques (**`directive`**, **`netlist_preview`**, **`model_library`**, **`preflight`**, **`run_status`**, **`run_minimal_summary`**, **`run_vector_catalog`**, **`vector_scalar_probe`**, **`xaxis_probe`**) ; copie **`qa_simulation_*_contract_summary.txt`** depuis **`ExistingRunDir`** (mode strict) ou **`Resolve-LatestArtifactPath`** (mode **`multi_runs`** par défaut).
  3. **`TARGET_RUN_DIR`** — création dossier familial local + **`Write-Output`** stdout ; **pas** de parse stdout/stderr enfant.
  4. **Logs** — aucun **`*.stdout.log`** probe sous le dossier familial.
  5. **Summaries** — produit **`qa_simulation_v1_1_family_summary.txt`** (copie + agrégation champs **`*_ok`**, **`exact_blocker_if_any`**, **`source_run_dir`**, **`*_artifact_path`**).
  6. **Green** — **`simulation_v1_1_family_green`** = AND des 9 **`*_ok`** (**`true`** si **`exact_blocker_if_any=N/A`** sur artefact source).
  7. **Blockers** — **`missing_artifact`**, **`exact_blocker_missing`**, préfixés label contrat ; **`firstBlocker`** ordre fixe contrats.
  8. **Checkpoint v1** — familial simulation invoqué via **`Invoke-FamilyRunner`** (**R112**, spawn checkpoint → runner agrégateur) ; critères checkpoint **inchangés**.
- **Compatibilité `Invoke-IpcQaChildScript`** : **non applicable** — helper conçu pour spawn script enfant + capture exit/logs/**`TARGET_RUN_DIR`** ; simulation résout/copie des fichiers existants.
- **Verdict** : **`mapping_exact=false`**, **`patch_recommended=false`**, **`risk_level=low`** (statu quo recommandé).
- **Synthèse arc R89–R113** :
  - **R89** : helper **`Invoke-IpcQaChildScript`** ; préflight + CI baseline.
  - **R90–R97** : runners familles checkpoint (PCB + SCH v2).
  - **R98–R99** : **`run_mcp_v2_checkpoint.ps1`** **`Invoke-FamilyRunner`**.
  - **R111–R112** : **`run_mcp_v1_checkpoint.ps1`** **`Invoke-FamilyRunner`**.
  - **R113** : runners v1 **`sch_actions`** / **`sch_connectivity`**.
  - Critères green / contrats summary **inchangés** ; handlers C++ **non** modifiés.
- **Choix retenu (R114)** : **Audit-only** — **clôture arc spawn QA** ; **pas** de mission R115 migration simulation. Reste hors arc : dette **`ipc_smoke.cpp`** footprint library ; spawn diag (**`diag_*`**, runtime diag) intentionnel.
- **QA R114** : simulation standalone + checkpoint v1 **full** + CI **VerifyOnly** — non-régression attendue.
- **Date** : 2026-06-06

## R115 — Bornage stdout **`qa_ipc_smoke`** footprint library (**`library_footprint_search_session`** / **`library_footprint_inspect_session`**)

- **Problématique** : R80 a diagnostiqué timeout harness search happy path (~**4,8 KiB** stdout, deadlock pipe corrigé R81 **`Invoke-SmokeBounded`**) ; R80 option **B** recommandait défense **`ipc_smoke.cpp`** (même pattern tools à gros payloads). Post-R114 : dernière dette QA harness footprint library.
- **Constats audit `ipc_smoke.cpp`** :
  1. Tools déjà bornés : **`get_net_classes`**, **`get_board_stackup`**, **`pcb_measure_distance_session`**, **`get_pcb_item_geometry_session`**, **`get_pcb_clearance_context_session`**, **`get_pcb_nearby_items_context_session`**, **`pcb_get_board_items_session`** (trim **`items_summary`**) ; **`library_symbol_search_session`** (lignes **`search_ok=`** greppables).
  2. **Footprint search / inspect** passaient encore par **`printResponse()`** → **`ApiResponse.Utf8DebugString()` complet** + **`out.result().Utf8DebugString()`** (double dump).
  3. **Probes** (**`targeted_library_footprint_search_probe.ps1`**, **`targeted_library_footprint_inspect_probe.ps1`**) parsent **`status=`**, **`error_message=`**, champs Struct via regex **`key: "…"`** / **`number_value`** / **`string_value`** — **pas** les lignes **`search_ok=`** du symbol search.
  4. **Inspect** : résultat handler déjà **scalaire compact** (**`inspect_ok`**, **`pads_count`**, …) — risque principal = dump **`ApiResponse`**.
  5. **`run_library_v1_targeted_probes.ps1`** : agrège summaries contrat ; **ne parse pas** stdout smoke directement.
- **Mapping** : **`mapping_exact=true`**, **`patch_recommended=true`**, **`risk_level=low`** (search + inspect).
- **Choix retenu** : Patch **`ipc_smoke.cpp` uniquement** — header léger (**`status`**, **`error_message`**, **`kicad_token`**) sans **`printResponse`** ; **search** : **`trimmed.Utf8DebugString()`** avec trim **`items[]`** si **>5** entrées ; **inspect** : **`out.result().Utf8DebugString()`** seul. **Handlers MCP** / schémas / critères **`library_v1_family_green`** **inchangés**.
- **Contexte / alternatives écartées** : Lignes **`search_ok=`** greppables (pattern symbol) — rejeté (casserait regex probes) ; patch probes — hors périmètre ; handler early-exit — hors périmètre.
- **QA R115** : rebuild **`qa_ipc_smoke`** ; probes search + inspect ; **`run_library_v1_targeted_probes.ps1`** ; checkpoint v1 **smoke/core** (+ **full** si temps) ; CI **VerifyOnly**.
- **Date** : 2026-06-06

## R116 — Audit stdout **`qa_ipc_smoke`** library symbol (**`library_symbol_search_session`** / **`library_symbol_inspect_session`**)

- **Problématique** : Post-R115 footprint borné — faut-il borner symbol search / inspect ? Le flake **`symbol_search_tool_timeout`** observé une fois dans **`run_library_v1`** (R115) est-il lié au stdout smoke ?
- **Constats audit** :
  1. **Tools** : **`library_symbol_search_session`**, **`library_symbol_inspect_session`** (probes **`targeted_library_symbol_*_probe.ps1`**, agrégés par **`run_library_v1_targeted_probes.ps1`**).
  2. **Search** : branche dédiée **`ipc_smoke.cpp`** ~L997–1071 — lignes greppables **`search_ok=`**, **`count=`**, **`items_present=`**, **`first_items=`**, **`diagnostics=`**, **`exact_blocker_if_any=`**, **`session_log=`** ; **mais** header via **`printResponse()`** (dump **`ApiResponse`** complet avant greppable). **Pas** dans la liste header léger R115.
  3. **Inspect** : **aucune** branche dédiée — **`printResponse()`** + **`out.result().Utf8DebugString()`** (double dump). Probe : lignes **`inspect_ok=`**, **`symbol_found=`**, **`pin_count=`**, … + fallback regex Struct debug.
  4. **Tailles happy path mesurées (QA R116)** : search **~3030 B**, inspect **~3209 B** — **inférieures** au ~**4800 B** footprint pré-R115 / seuil pipe R80.
  5. **Flake timeout** : **non attribué** au volume stdout post-R81 ; corrélé plutôt à charge IPC / enchaînement **4 probes** library (KiCad SCH+PCB) — **`library_v1_family_green=true`** en run isolé ou retry.
- **Mapping patch R117 (si souhaité)** : **`mapping_exact=true`** — search : ajouter header léger (supprimer **`printResponse`** uniquement) ; inspect : header léger + branche greppable (**`inspect_ok=`**, **`symbol_found=`**, **`pin_count=`**, **`fields_summary=`**, **`diagnostics=`**, **`exact_blocker_if_any=`**). **`patch_recommended=false`** en urgence (défense optionnelle symétrique R115).
- **Choix retenu (R116)** : **Audit-only** — **pas de patch** **`ipc_smoke.cpp`** ; **clôture dette harness library acceptable** (footprint R115 + symbol déjà < 4 KiB + greppable search). **R117 optionnel** : alignement symétrique symbol (skip **`printResponse`** + greppable inspect).
- **QA R116** : probes symbol + library v1 + footprint R115 + checkpoint v1 smoke/core + CI **VerifyOnly** — non-régression.
- **Date** : 2026-06-06

## R117 — Clôture dette harness library stdout (**R115–R116**)

- **Problématique** : Formaliser la fin de la série footprint/symbol **`qa_ipc_smoke`** après **R115** (patch footprint) et **R116** (audit symbol sans patch).
- **Synthèse clôture** :
  1. **R115** : **`library_footprint_search_session`** + **`library_footprint_inspect_session`** bornés dans **`ipc_smoke.cpp`** — header léger, trim **`items[]`** search ; rebuild **`qa_ipc_smoke`** ; probes / **`library_v1_family_green`** inchangés.
  2. **R116** : symbol search/inspect audités — stdout **~3030 B / ~3209 B** ; **`mapping_exact=true`**, **`patch_recommended=false`** urgence ; flake timeout **non** attribué au dump.
  3. **R117** : **Dette harness library stdout clôturée** — **pas** de patch symbol ; handlers / critères green / checkpoints / probes **inchangés**.
- **Règle future (patch symbol optionnel)** : rouvrir **`ipc_smoke.cpp`** **uniquement si** (a) stdout happy path symbol **> ~4096 B** mesuré sur probe library, **ou** (b) **`symbol_*_tool_timeout`** reproductible **prouvé** lié au dump stdout (hors flake IPC / post-R81 pipe). Patch connu : skip **`printResponse`** + greppable inspect (symétrie R115).
- **Choix retenu (R117)** : **Documentation-only** — README § clôture, release note, dev guide §8bis, entrée **`MCP_DECISIONS.md`** ; **aucun** code, **aucun** rebuild.
- **QA R117** : CI **`VerifyOnly`** courte (sanity docs-only).
- **Date** : 2026-06-06

## R118 — Audit prochaine capacité produit MCP KiCad

- **Problématique** : Après clôture des arcs QA (**R89–R117**), quelle **capacité MCP produit** prioriser pour accroître l’autonomie IA (lire → analyser → modifier progressivement SCH / PCB / library / simulation / projet) **sans** implémentation dans cette mission ?
- **Inventaire tools pertinents (registre `mcp_executor.cpp`, ~90+ tools)** :
  - **Projet** : **`get_project_info`**, **`get_workspace_overview`**, **`get_open_documents`**, **`get_project_context_snapshot`**, **`qa_open_editor_frame`**.
  - **SCH read** : **`get_sch_items`**, **`get_sch_items_by_id`**, **`schematic_net_inspect_session`**, **`schematic_symbol_connectivity_session`**, **`hit_test_sch`**, **`get_selection_sch`**.
  - **SCH write (familles gate v1/v2)** : placement (**`schematic_place_symbol_session`** — manifeste : **pas d’assignation footprint auto**), wiring, geometry, labels, champs (**`schematic_set_symbol_*`**, **`schematic_set_label_text_session`**).
  - **PCB read (gate MCP v2, 8 familles)** : inspection v2, geometry v1, clearance context v1, nearby context v1 ; plus **`get_nets`**, **`get_net_classes`**, **`get_board_stackup`**, **`hit_test_pcb`**.
  - **PCB write (hors gate)** : **`create_pcb_items`**, **`update_pcb_items`**, **`delete_pcb_items`**, sessions composées **`pcb_graphic_annotation_session`**, etc. — API bas niveau, **sans** pattern session QA gate.
  - **Library** : **`library_symbol_search_session`**, **`library_symbol_inspect_session`**, **`library_footprint_search_session`**, **`library_footprint_inspect_session`** — runner **`library_v1`**, **hors** checkpoint MCP v2.
  - **Simulation (hors gate MCP v2)** : 9 tools (**`simulation_directive_inspect`** … **`simulation_xaxis_probe_session`**) agrégés **`run_simulation_v1_1_targeted_probes.ps1`** ; exécution runtime ngspice / frame SCH requise.
- **Manques les plus bloquants pour une IA** :
  1. **Lien symbol → footprint validé** (library search/inspect séparés ; **`schematic_place_symbol_session`** sans assignation footprint — manifeste §15).
  2. **Vue DRC violations PCB** (clearance/nearby = contexte ; **pas** liste violations ; **`is_true_drc=false`** documenté).
  3. **Mutation PCB contrôlée session** (read riche, write raw protobuf risqué, pas de dry-run gate).
  4. **Orchestration simulation IA** (tools atomiques, pas de session « run + résumé pass/fail » intégrée au gate).
  5. **Contexte projet unifié** partiel (**`get_project_context_snapshot`** existe mais navigation cross-documents encore composée côté client).
- **Trois candidats comparés** :
  | Candidat | Bénéfice IA | Fichiers probables | Complexité | Risque mutation | QA / gate |
  |----------|-------------|-------------------|------------|-----------------|-----------|
  | **C1 — `schematic_assign_footprint_session` V1** | Assigner un footprint validé à un symbole SCH (lib_id + footprint qualifié) ; comble lacune manifeste post-**place_symbol** | **`eeschema/api/api_handler_sch.cpp`**, callback MCP SCH, **`mcp_executor.cpp`**, probe **`targeted_schematic_assign_footprint_probe.ps1`**, runner (extension **`sch_actions`** ou famille dédiée) | **Moyenne** | **Modéré** (champ Footprint, dry-run, undo) | Pattern session SCH éprouvé ; **hors** gate v2 initial ; non-régression **`run_library_v1`**, v1 checkpoint |
  | **C2 — `get_pcb_drc_violations_context_session` V1** | Lister violations DRC **bornées** read-only pour revue IA (complète clearance/nearby) | **`pcbnew/api/api_handler_pcb.cpp`**, **`drc_engine`**, callback, probe + fixture **`ipc_drc_v1`**, famille candidate checkpoint v2 | **Moyenne–élevée** | **Faible** (read-only) | Risque confusion **`RunTests`** / DRC officielle → **`is_official_drc=false`**, messages prudents ; schéma Struct nouveau |
  | **C3 — `simulation_assistant_session` V1** | Orchestrer preflight + run minimal + résumé structuré pour IA | **`mcp_executor.cpp`** (compose tools existants), probe, extension agrégation simulation v1.1 | **Moyenne** | **Faible–modéré** (run SPICE) | Runtime GUI/ngspice, flaky CI ; **hors** gate MCP v2 ; long |
- **Recommandation R119** : **C1 — `schematic_assign_footprint_session` V1**.
- **Pourquoi C1** : gap **documenté produit** (manifeste §15 — pas d’assignation footprint sur place_symbol) ; **réutilise** patterns et QA library + SCH actions ; **débloque** le flux IA « composant choisi → footprint validé → PCB-ready » sans ouvrir mutation PCB bas niveau ; intégration **réaliste** (handler SCH + session dry-run comme **`schematic_set_symbol_field_session`** + validation **`library_footprint_inspect_session`** en interne ou préflight). C2 utile mais redondant partiellement avec clearance + revue manuelle ; risque sémantique DRC. C3 moins universel et plus lourd runtime.
- **QA proposée R119** : étude d’implémentation + fixture SCH (**sym-lib-table** + **`fp-lib-table`**) ; probe dry-run/mutation/restore ; **`sch_actions_v1`** ou runner dédié ; non-régression **`run_library_v1`**, checkpoint v1 **smoke/core** ; **pas** intégration gate MCP v2 dans un premier temps.
- **Choix retenu (R118)** : **Audit-only** — **aucun** code ; entrée décision + synthèse README § produit.
- **Date** : 2026-06-06

## R119 — Cadrage technique `schematic_assign_footprint_session` V1

- **Problématique** : Cadrer précisément l’implémentation V1 recommandée en R118 — assignation footprint validée sur symbole SCH existant — **sans** patch handler / schéma / QA dans cette mission.
- **Patterns SCH write existants (audit code)** :
  - **`schematic_set_symbol_field_session`** (`api_handler_sch.cpp` ~6677–7095) : session contrôlée ; `schematic_filename` + `symbol_id` + `new_value` ; résolution symbole par KIID sur écran courant ; `dry_run` / `expected_old_value` ; mutation via **`SCH_COMMIT`** + **`field->SetText(..., &currentSheet, currentVariant)`** + **`schFrame->Refresh()`** ; readback strict ; **`FOOTPRINT`** déjà autorisé comme `field_id` (probe **`targeted_schematic_set_symbol_field_probe.ps1`** le teste) mais **sans validation library** ni sémantique `pcb_ready`.
  - **`schematic_set_symbol_value_session`** / **`schematic_set_symbol_reference_session`** : même squelette session (contexte SCH, filename match, blockers `invalid_symbol_id`, `symbol_not_found`, `expected_*_mismatch`, `mutation_failed`).
  - **`schematic_place_symbol_session`** (~4893+) : placement depuis `lib_id` ; **ne touche pas** au champ Footprint (manifeste §15 l.792) ; dry-run/mutation/restore QA en 2 processus (authoring v2).
  - **Probes QA** : dry-run → verify no change → mutation → readback (`expected_old_value`) → restore → verify ; fixture préférée **`qa/data/pcbnew/issue19090/issue19090.kicad_sch`** (symboles R1/R2 stables, footprint **`Resistor_SMD:R_0805_2012Metric`**).
- **Représentation champ Footprint** :
  - Nom canonique : property **`Footprint`** → **`FIELD_T::FOOTPRINT`** ; `field_id` MCP = **`"FOOTPRINT"`**.
  - Stockage SCH typique : identifiant qualifié **`LibraryNick:FootprintName`** (ex. fixture issue19090 : **`Resistor_SMD:R_0805_2012Metric`** ; ERC parse via **`LIB_ID::Parse( footprint, true )`** — `erc.cpp` ~1791+).
  - Lecture recommandée handler : **`symbol->GetFootprintFieldText( false, &currentSheet, false )`** (aligné netlist/assign UI).
  - Écriture recommandée handler : **`symbol->SetFootprintFieldText( wxString )`** puis **`SCH_COMMIT`** (aligné **`assign_footprints.cpp`**, **`backannotate.cpp`**) — **pas** réutiliser le chemin générique `set_symbol_field` seul (pas de validation ; sémantique produit diluée).
  - Champ absent sur instance : **`GetField( FIELD_T::FOOTPRINT )` const** → `nullptr` → traiter **`old_footprint=""`** ; **`SetFootprintFieldText`** crée le champ si absent (non-const **`GetField`**).
  - Champ déjà présent non vide : mutation autorisée seulement si **`allow_overwrite=true`** (défaut **false**) ou footprint identique (no-op, `changed=false`).
  - Undo/save/refresh : **`SCH_COMMIT::Push("Assign footprint via MCP")`** ; **`schFrame->Refresh()`** ; **pas** d’auto-save (convention sessions SCH) ; **pas** de refresh connectivité requis (footprint n’est pas un item wire/net).
- **Validation footprint (audit `library_footprint_inspect_session`)** :
  - Tool actuel (`api_handler_pcb.cpp` ~4931+) : requiert **frame PCB** + `board_filename` match ; args **`library`** + **`footprint_name`** → **`qualified_name = library + ":" + footprint_name`** ; validation **`FOOTPRINT_LIBRARY_ADAPTER::LoadOne`** + **`LoadFootprint`** ; blockers **`library_not_loadable:*`**, **`footprint_not_found_in_library:*`**, **`board_filename_mismatch`**.
  - **Recommandation R120 handler SCH** : validation **interne** via **`PROJECT::FootprintLibAdapter( schFrame->Kiway() )`** (mécanisme KIFACE — `project.cpp` ~420, **sans** frame PCB obligatoire) ; **ne pas** appeler le tool MCP inspect depuis le handler.
  - Format V1 entrée : champ unique **`footprint`** (string qualifiée `LibNick:Name`, max 1024) ; parser avec **`LIB_ID::Parse`** → blocker **`invalid_footprint_identifier`** si échec.
  - **`validate_footprint`** (bool, défaut **`true`**) : si `true`, exiger **`LoadOne` + `LoadFootprint`** avant mutation ; si `false`, assigner sans load (diagnostics **`validate_footprint=false`** — usage QA / IA ayant déjà inspecté).
- **Contrat JSON V1 proposé (entrée)** :
  ```json
  {
    "type": "object",
    "required": ["schematic_filename", "symbol_id", "footprint"],
    "properties": {
      "schematic_filename": { "type": "string" },
      "symbol_id": { "type": "string", "description": "KIID UUID du symbole" },
      "footprint": { "type": "string", "description": "Identifiant qualifié LibNick:FootprintName" },
      "dry_run": { "type": "boolean", "default": false },
      "allow_overwrite": { "type": "boolean", "default": false },
      "expected_current_footprint": { "type": "string" },
      "validate_footprint": { "type": "boolean", "default": true }
    },
    "additionalProperties": false
  }
  ```
- **Sortie JSON V1 proposée (Struct MCP)** :
  - **`ok`**, **`changed`**, **`dry_run`**, **`symbol_id`**, **`symbol_ref`**, **`old_footprint`**, **`new_footprint`**, **`footprint_validated`**, **`pcb_ready`**, **`exact_blocker_if_any`**, **`diagnostics`**, **`session_log`**.
  - **`pcb_ready`** : `true` si footprint résultant non vide **et** (`footprint_validated` ou validation désactivée avec assignation réussie).
  - Bad requests HTTP-style ( **`makeBadRequestResult`** ) : args manquants / types invalides — **hors** `exact_blocker_if_any` structuré.
- **Blockers exacts proposés (V1)** :
  | Blocker | Condition |
  |---------|-----------|
  | *(bad request)* | `schematic_filename` / `symbol_id` / `footprint` manquant ou type invalide |
  | `invalid_symbol_id` | KIID mal formé |
  | `schematic_filename_mismatch` | SCH ouvert ≠ `schematic_filename` |
  | `sch_context_unavailable` | pas de frame SCH / écran |
  | `symbol_not_found` | KIID absent de l’écran courant |
  | `invalid_footprint_identifier` | `LIB_ID::Parse` échoue |
  | `footprint_not_found_in_library:Lib:Name` | `LoadFootprint` null (si `validate_footprint=true`) |
  | `library_not_loadable:Lib` | `LoadOne` échoue (si `validate_footprint=true`) |
  | `footprint_already_assigned` | `old_footprint` non vide, ≠ nouveau, `allow_overwrite=false` |
  | `expected_current_footprint_mismatch` | `expected_current_footprint` fourni ≠ `old_footprint` |
  | `mutation_failed` | readback ≠ `new_footprint` après commit |
  | `N/A` | succès (dry-run ou mutation) |
  - **Non retenus V1** : `footprint_field_missing` (création implicite OK) ; `restore_failed` (restauration = seconde mutation QA, pas blocker handler dédié).
- **Comportement mutation V1** :
  - **`dry_run=true`** : aucun commit ; retourne `old_footprint`, `new_footprint`, `footprint_validated` (validation exécutée si activée), `changed` si valeur diffère.
  - **`dry_run=false`** : modifie **uniquement** le champ Footprint ; **pas** PCB, **pas** annotation auto, **pas** placement footprint board.
  - **Restore QA** : second appel avec `footprint=old_footprint`, `expected_current_footprint=new_footprint`, `allow_overwrite=true`.
- **Fichiers à modifier (R120 implémentation)** :
  - Handler : **`eeschema/api/api_handler_sch.cpp`** (`SchematicAssignFootprintSessionToolInvoke` + enregistrement callback).
  - Callback : **`common/mcp/mcp_sch_assign_footprint_callback.cpp`**, **`include/mcp/mcp_sch_assign_footprint_callback.h`** (pattern **`mcp_sch_set_symbol_field_callback.*`**).
  - Registre : **`common/mcp/mcp_executor.cpp`** (descriptor + `input_schema_json`).
  - QA : **`targeted_schematic_assign_footprint_probe.ps1`** ; extension **`run_sch_actions_v1_targeted_probes.ps1`** (4e probe).
  - Docs produit (R120) : **`MCP_V1_OFFICIAL_MANIFEST.md`**, **`MCP_KICAD_USER_DEV_GUIDE.md`**.
- **Fixture QA R120 proposée** :
  - SCH : **`qa/data/pcbnew/issue19090/issue19090.kicad_sch`** (+ projet associé si ouverture projet requise).
  - **`fp-lib-table`** projet : lib **`qa_resistors`** → **`qa/data/libraries/Resistor_SMD.pretty`** (pattern **`targeted_library_footprint_inspect_probe.ps1`**).
  - Cas happy : symbole R1/R2 ; dry-run assign **`qa_resistors:R_0201_0603Metric`** ; mutation + readback ; restore vers **`Resistor_SMD:R_0805_2012Metric`**.
  - Cas négatifs : footprint invalide, lib introuvable, `allow_overwrite=false` sur footprint existant, `expected_current_footprint_mismatch`.
- **Probe QA R120** : **`targeted_schematic_assign_footprint_probe.ps1`** — même orchestration que **`targeted_schematic_set_symbol_field_probe.ps1`** ; summary **`qa_schematic_assign_footprint_session_contract_summary.txt`** ; clés green : `dry_run_ok`, `mutation_ok`, `readback_after_mutation_ok`, `restore_ok`, `footprint_validated_ok`, `pcb_ready_ok`, négatifs.
- **Runner recommandé** : **extension `run_sch_actions_v1_targeted_probes.ps1`** (4e tool write SCH actions, cohérent avec field/value/label ; **hors** gate MCP v2 ; critères green famille à étendre **en R120 uniquement**).
- **Risques** :
  - Footprint stock KiCad vs lib QA selon config CI (mitigation : fp-lib-table projet déterministe).
  - **`validate_footprint=false`** : assignation non vérifiée (documenter ; défaut true).
  - Confusion avec **`schematic_set_symbol_field_session` field_id=FOOTPRINT** : tool dédié = contrat IA + validation + `pcb_ready`.
- **Recommandation R120** : **Implémenter** — cadrage complet, patterns handler/probe/runner identifiés, validation library faisable depuis SCH via **`PROJECT::FootprintLibAdapter`**, pas d’audit ciblé supplémentaire requis sauf blocage runtime R120.
- **QA baseline R119 (audit)** : **`run_mcp_v2_ci_baseline.ps1 -VerifyOnly`** → `MCP_V2_CI_BASELINE_OK=1` ; **`run_library_v1_targeted_probes.ps1`** green ; **`run_sch_actions_v1_targeted_probes.ps1`** green.
- **Choix retenu (R119)** : **Audit-only** — entrée décision ; **aucun** handler / schéma / script QA / critère green modifié.
- **Date** : 2026-06-06

## R120 — Implémenter `schematic_assign_footprint_session` V1

- **Problématique** : Implémenter le tool MCP recommandé R118/R119 pour assigner un footprint validé à un symbole SCH (dry-run, mutation contrôlée, restore QA).
- **Implémentation** :
  - Handler **`SchematicAssignFootprintSessionToolInvoke`** dans **`eeschema/api/api_handler_sch.cpp`**.
  - Callback **`mcp_sch_assign_footprint_callback.{h,cpp}`** ; enregistrement **`mcp_executor.cpp`**.
  - Validation : **`LIB_ID::Parse(..., false)`** + **`KIFACE_TEST_FOOTPRINT_LINK`** via **`FACE_PCB`** (export ajouté **`pcbnew.cpp`**) ; évite lien direct **`pcbcommon`** depuis eeschema.
  - Mutation : **`SetFootprintFieldText`** + **`SCH_COMMIT`** + refresh ; sorties **`footprint_validated`**, **`pcb_ready`**.
- **QA** :
  - Probe **`targeted_schematic_assign_footprint_probe.ps1`** ; summary **`qa_schematic_assign_footprint_session_contract_summary.txt`**.
  - Extension **`run_sch_actions_v1_targeted_probes.ps1`** (4e probe) ; critère **`schematic_assign_footprint_ok=true`** ajouté à **`sch_actions_v1_family_green`**.
  - Fixture **`issue19090`** + **`qa_resistors`** fp-lib-table.
- **Hors périmètre respecté** : pas PCB ; pas gate MCP v2 ; **`schematic_set_symbol_field_session`** inchangé.
- **Choix retenu (R120)** : implémentation produit + QA v1 ; docs manifest / tool map / README / release note.
- **Date** : 2026-06-06

## R121 — Mini-release `schematic_assign_footprint_session` V1

- **Problématique** : Clôturer proprement R120 par documentation officielle + QA checkpoint v1 **full** ; stabiliser **`schematic_assign_footprint_session`** comme 4e tool SCH Actions v1 sans nouvelle feature.
- **Documentation enrichie** :
  - **`MCP_V1_OFFICIAL_MANIFEST.md`** §12 : objectif, contrat entrée/sortie complet, blockers, sémantique `dry_run` / `allow_overwrite` / `expected_current_footprint` / `validate_footprint` / `pcb_ready`, limites (pas PCB, pas placement board, pas annotation auto), diff vs **`schematic_set_symbol_field_session`**, limite QA restore **`validate_footprint=false`**.
  - **`MCP_TOOL_MAP_R1.tsv`** : entrée **`schematic_assign_footprint_session`** alignée (`manifest_mentioned`).
  - **`MCP_V1_CHECKPOINT_RELEASE_NOTE.md`** : SCH Actions v1 = 4 tools ; artefacts QA assign footprint.
  - **`MCP_KICAD_USER_DEV_GUIDE.md`** : section capabilities SCH Actions v1 + profil tool assign footprint + indicateurs **`sch_actions_v1_family_green`**.
  - **`README_IPC_QA_BASELINE.md`** : section mini-release R121.
- **QA clôture** :
  - Probe **`targeted_schematic_assign_footprint_probe.ps1`** ; runner **`run_sch_actions_v1_targeted_probes.ps1`** (summary **`schematic_assign_footprint_ok`** relayé).
  - Checkpoint v1 **smoke / core / full** ; **`run_library_v1_targeted_probes.ps1`** ; **`run_mcp_v2_ci_baseline.ps1 -VerifyOnly`**.
- **Hors périmètre respecté** : aucun handler C++ ; aucun schéma MCP ; gate MCP v2 inchangé ; pas d’ouverture **`get_pcb_drc_violations_context_session`**.
- **Recommandation R122** : **`get_pcb_drc_violations_context_session`** (read-only context DRC) ou enrichissement **`get_project_context_snapshot`** — audit produit MCP suivant.
- **Choix retenu (R121)** : mini-release documentation + QA full ; capacité officielle SCH Actions v1.
- **Date** : 2026-06-06

## R122 — Audit `get_pcb_drc_violations_context_session` V1

- **Problématique** : Auditer la faisabilité d’un tool MCP **read-only** listant des violations DRC PCB **bornées** pour l’IA, après clôture du flux SCH « symbole → footprint validé → **`pcb_ready`** » (R121), **sans** implémentation ni confusion avec clearance/nearby (**`is_true_drc=false`**).
- **Hors périmètre respecté** : aucun handler C++ ; aucun schéma MCP ; aucun script QA ; aucun critère green ; pas d’intégration checkpoint MCP v2.

### 1. Source des violations — options tranchées

| Option | Faisabilité | Verdict R122 |
|--------|-------------|--------------|
| **A — Lire marqueurs existants** (`BOARD::Markers()`, `MARKER_DRC`) | **Élevée** — pattern **`DRC_ITEMS_PROVIDER`** + **`RC_ITEM::GetJsonViolation`** (`drc_report.cpp`, `rc_item.cpp`) | **V1 par défaut** si `run_drc=false` ; liste **souvent vide** après ouverture board (marqueurs **non persistés** — `pcb_io_kicad_sexpr.cpp` : « Do not save PCB_MARKERs ») |
| **B — Lancer DRC KiCad bornée** | **Moyenne** — pas de sous-ensemble officiel « un test » ; **`DRC_ENGINE::RunTests`** exécute **tous** les providers (`drc_engine.cpp`) | **V1 optionnelle** via `run_drc=true` avec handler **en mémoire** (pattern **`WriteDRCReport`** — `pcbnew_scripting_helpers.cpp` ~537–621) : collecte **`DRC_ITEM`** **sans** `BOARD_COMMIT` |
| **C — DRC complète GUI** (`DRC_TOOL::RunTests`) | **Élevée** techniquement | **Hors V1 MCP** — ajoute des **`PCB_MARKER`** au board (`drc_tool.cpp` ~185–208, `SKIP_UNDO` mais mutation graphe) ; zone refill + netlist schéma optionnels ; **flake CI** |
| **D — Contexte DRC-like non officiel** | Déjà couvert | **Refusé** — redondant avec **`get_pcb_clearance_context_session`** / **`get_pcb_nearby_items_context_session`** |

**Choix V1 recommandé** : **deux modes explicites** dans **un** tool — (1) **`run_drc=false`** : snapshot marqueurs session courante ; (2) **`run_drc=true`** : **`DRC_ENGINE::RunTests`** + handler collecteur **sans persistance** des marqueurs (aligné export JSON **`RC_JSON::DRC_REPORT`**, **`is_official_drc=true`**, **`drc_was_run=true`**). **Ne pas** réutiliser **`DRC_TOOL::RunTests`** depuis MCP V1.

### 2. API KiCad pertinente (audit code)

- **Stockage runtime** : **`BOARD::Markers()`** (`board.h`) — **`PCB_MARKER`** (`pcb_marker.h`) hérite **`MARKER_BASE`** ; types **`MARKER_DRC`**, **`MARKER_PARITY`**, **`MARKER_RATSNEST`**, **`MARKER_DRAWING_SHEET`** (`marker_base.h`).
- **Item violation** : **`DRC_ITEM`** extends **`RC_ITEM`** (`drc_item.h`) — codes **`PCB_DRC_CODE`** / **`DRCE_*`** ; **`GetSettingsKey()`**, **`GetErrorMessage()`**, **`GetViolatingRule()`** / **`GetViolatingRuleDesc()`**, **`GetIDs()`** (jusqu’à 4 KIID).
- **Export structuré** : **`RC_ITEM::GetJsonViolation`** → **`RC_JSON::VIOLATION`** (`rc_json_schema.h`) : `type`, `description`, `severity`, `items[]` (uuid, description, pos), `excluded`, `comment`.
- **Moteur** : **`DRC_ENGINE`** (`drc_engine.h/.cpp`) — **`InitEngine`** (règles `.kicad_dru`, déjà appelé **`PCB_EDIT_FRAME::OnBoardLoaded`** via **`DRC_TOOL`**) ; **`RunTests(units, reportAllTrackErrors, testFootprints, commit)`** ; **`ReportViolation`** → handler configurable ; **`SetViolationHandler`** / **`ClearViolationHandler`**.
- **Rapport officiel existant** : **`DRC_REPORT::writeJsonReport`** (`drc_report.cpp`) — lit marqueurs via **`DRC_ITEMS_PROVIDER`** ; schéma **`https://schemas.kicad.org/drc.v1.json`**.
- **API protobuf (mutation, hors MCP tool actuel)** : **`InjectDrcError`** (`api_handler_pcb.cpp` ~8177) — crée un marqueur via commit ; utile QA interne, **pas** modèle read-only produit.
- **Contraintes frame** : handler MCP PCB = callback **`mcp_pcb_*_callback`** + **`PCB_EDIT_FRAME`** ouvert, **`board_filename`** match (pattern **`PcbClearanceContextSessionToolInvoke`**) ; **`DRC_TOOL::RunTests`** exige **`TOOL_MANAGER`** / frame — **ne pas** appeler depuis MCP ; préférer **`board->GetDesignSettings().m_DRCEngine`** déjà initialisé.
- **Tests unitaires headless** : **`drc_test_utils`**, **`test_drc_*.cpp`** — **`RunTests`** sans GUI avec board chargé ; confirme faisabilité moteur sans dialog.

### 3. Nom tool recommandé

- **Retenu** : **`get_pcb_drc_violations_context_session`** — cohérent avec **`get_pcb_clearance_context_session`** / **`get_pcb_nearby_items_context_session`** ; suffixe **`_context_session`** signale **liste structurée pour IA**, pas verdict unique.
- **Champs sémantiques obligatoires** : **`is_official_drc`** + **`drc_source`** (`existing_markers` | `engine_run`) pour éviter confusion avec clearance (**`is_true_drc=false`**).
- **Écartés** : **`get_pcb_drc_context_session`** (trop vague) ; **`get_pcb_existing_drc_markers_session`** (exact pour mode A seul, trop restrictif si `run_drc=true` ajouté).

### 4. Contrat entrée V1 proposé

```json
{
  "type": "object",
  "required": ["board_filename"],
  "properties": {
    "board_filename": { "type": "string", "description": "Chemin absolu, doit correspondre au PCB ouvert" },
    "max_violations": { "type": "integer", "default": 25, "minimum": 1, "maximum": 500 },
    "run_drc": { "type": "boolean", "default": false,
      "description": "false = lire marqueurs MARKER_DRC existants ; true = exécuter DRC_ENGINE::RunTests (collecte mémoire, sans persister marqueurs)" },
    "severity": { "type": "string", "enum": ["error", "warning", "exclusion", "all"], "default": "all" },
    "violation_types": { "type": "array", "items": { "type": "string" },
      "description": "Filtre sur DRC_ITEM settings keys (ex. clearance, unconnected_items)" },
    "layer": { "type": "string", "description": "BL_* ou alias F_Cu — filtre couche marqueur si disponible" },
    "include_excluded": { "type": "boolean", "default": false },
    "include_items": { "type": "boolean", "default": true,
      "description": "Inclure item_ids / net_names dérivés des KIID RC_ITEM" },
    "report_all_track_errors": { "type": "boolean", "default": false, "description": "Uniquement si run_drc=true ; passe-through RunTests" },
    "test_footprints": { "type": "boolean", "default": false, "description": "Uniquement si run_drc=true ; parity schéma — déconseillé V1 CI" }
  },
  "additionalProperties": false
}
```

### 5. Contrat sortie V1 proposé

- **`ok`**, **`is_official_drc`** (`false` si `run_drc=false` ; `true` si `run_drc=true` et RunTests OK), **`is_true_drc`** (alias documentaire = **`is_official_drc`** pour parité clearance), **`drc_was_run`**, **`drc_source`**, **`violations_count`**, **`returned_count`**, **`truncated`**, **`violations[]`**, **`exact_blocker_if_any`**, **`diagnostics`**, **`session_log`** (optionnel).
- **Chaque violation** : **`type`** (settings key), **`severity`**, **`message`**, **`layer`** (protobuf **`BoardLayer`** si mappable), **`x_nm`**, **`y_nm`** (position marqueur ou violation handler), **`item_ids[]`**, **`net_names[]`** (via **`BOARD::ResolveItem`** + **`NETINFO_ITEM`**), **`rule_name`** (si **`GetViolatingRule()`** non null), **`excluded`**, **`marker_id`** (KIID marqueur si mode existing_markers).
- **Séparer** (option V1.1) : **`unconnected_items[]`**, **`schematic_parity[]`** comme **`DRC_REPORT` JSON — hors scope minimal V1** (documenter **`violations[]` = MARKER_DRC errors/warnings** seulement).

### 6. Blockers exacts proposés

| Blocker | Condition |
|---------|-----------|
| `missing_or_invalid_board_filename` | absent / non string / vide |
| `board_filename_mismatch` | PCB ouvert ≠ argument |
| `pcb_context_unavailable` | pas de frame PCB / callback |
| `board_not_loaded` | **`GetBoard()`** null |
| `drc_engine_unavailable` | **`m_DRCEngine`** null |
| `drc_engine_not_initialized` | règles non compilées (catch **`PARSE_ERROR`** InitEngine) |
| `drc_run_failed` | exception / annulation progress pendant **`run_drc=true`** |
| `drc_markers_unavailable` | échec lecture provider (interne) |
| `invalid_max_violations` | hors bornes |
| `invalid_layer` | alias/couche non résolu |
| `invalid_severity` | enum inconnu |
| `run_drc_already_running` | garde si réentrance détectée (optionnel) |
| `N/A` | succès |

### 7. QA V1 proposée (R123)

- **Fixture dédiée recommandée** : **`qa/data/pcbnew/drc/ipc_drc_v1/`** — **ne pas** réutiliser **`ipc_clearance_v1`** (pads trop espacés, **aucune** violation garantie) ni **`ipc_nearby_v1`** (géométrie voisinage, pas DRC).
  - Plateau minimal : 2 pads **nets différents**, centres **< clearance min** (ex. **0,15 mm** avec règle **`clearance (min 0,20 mm)`** dans **`ipc_drc_v1.kicad_dru`**) ; KIID figés ; empreintes inline comme **`ipc_clearance_v1`**.
  - Référence technique : **`qa/tools/drc_proto/test_cases/basic_clearance`** + tests **`test_drc_physical_clearance.cpp`**.
- **Probe** : **`targeted_pcb_drc_violations_context_probe.ps1`** — cas **`run_drc=true`** → **`violations_count >= 1`**, type **`clearance`** (ou settings key stable), **`is_official_drc=true`** ; cas **`run_drc=false`** sur board fraîchement ouvert → **`violations_count=0`**, **`is_official_drc=false`**, **`ok=true`** (contrat explicite « pas encore de DRC session ») ; négatifs : **`board_filename_mismatch`**, **`invalid_max_violations`**.
- **Runner** : **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`** ; summary **`qa_pcb_drc_violations_context_v1_family_summary.txt`** ; clé **`pcb_drc_violations_context_v1_family_green=true`**.
- **Gate** : **hors checkpoint MCP v2 initial** (comme clearance avant R33) ; non-régression séquentielle : **`run_pcb_geometry_v1`**, **`run_pcb_clearance_context_v1`**, **`run_pcb_nearby_items_context_v1`**, **`run_pcb_inspection_v2`**, **`run_mcp_v2_ci_baseline.ps1 -VerifyOnly`**.
- **Timeout probe** : **`run_drc=true`** sur petit plateau — prévoir **120–180 s** ; **`Invoke-SmokeBounded`** ; stdout borné (**`ipc_smoke.cpp`**, leçon R115).

### 8. Risques

| Risque | Niveau | Mitigation |
|--------|--------|------------|
| Coût runtime DRC complète | **Élevé** sur grands boards | `max_violations` + **`run_drc=false`** par défaut ; timeout strict ; doc « IA : d’abord inspection / nearby » |
| Flakiness GUI / zone fill | **Moyen** | V1 sans refill zones (comme **`WriteDRCReport`**) ; accepter écart vs GUI DRC dialog ; fixture minuscule |
| Confusion DRC officielle vs contexte | **Élevé** | **`is_official_drc`** explicite ; doc manifeste : clearance/nearby restent **`is_true_drc=false`** ; ce tool **liste** des violations, ne **calcule pas** une clearance locale |
| Marqueurs non persistés | **Élevé** | Documenter ; **`run_drc=false`** souvent vide ; prod IA doit passer **`run_drc=true`** pour liste fraîche |
| Board modifié si mauvais chemin | **Moyen** | Interdire **`DRC_TOOL::RunTests`** ; handler collecteur sans commit |
| **`test_footprints=true`** | **Élevé** en CI | défaut **`false`** ; requiert schéma annoté + fetch netlist |
| Parallèle familles PCB | **Moyen** | runner séquentiel (leçon R37) |

### 9. Recommandation R123 (une seule option)

**Option retenue** : **Créer d’abord la fixture dédiée `ipc_drc_v1` + script verify**, **puis** implémenter V1 read-only avec **`run_drc=true`** sur petit plateau (collecte **`DRC_ENGINE`** en mémoire) **et** mode **`run_drc=false`** (snapshot marqueurs) ; **hors** gate MCP v2.

**Justification** : marqueurs **non sauvegardés** dans `.kicad_pcb` → QA et valeur IA exigent **`run_drc=true`** prouvé sur fixture stable ; implémenter sans fixture = probe flaky. Ne pas lancer une « DRC complète GUI » ni promettre parité totale dialog DRC en V1.

**Alternatives écartées** : implémenter direct sans fixture ; renommer en tool markers-only ; intégrer au checkpoint v2 dans la même mission.

- **Choix retenu (R122)** : **Audit-only** — faisabilité **confirmée** avec garde-fous ; contrat et QA R123 cadrés ; **aucun** code.
- **Date** : 2026-06-06

## R123 — Fixture QA `ipc_drc_v1` pour futur `get_pcb_drc_violations_context_session`

- **Problématique** : Créer le plateau PCB minimal stable (violation clearance DRC attendue) et un script verify harness **sans** implémenter le handler MCP DRC.
- **Fixture** : **`qa/data/pcbnew/drc/ipc_drc_v1/`**
  - **`ipc_drc_v1.kicad_pro`** : netclasses **`QA_DRC_NETCLASS_A`** (clearance **0,20 mm**), **`QA_DRC_NETCLASS_B`** (clearance **0,30 mm**).
  - **`ipc_drc_v1.kicad_pcb`** : 2 pads SMD **1×1 mm** inline **`QA:R_FIX`** sur **F.Cu**.
  - **`ipc_drc_v1.kicad_sch`** : labels **QA_DRC_NET_A** / **QA_DRC_NET_B**.
  - **`README_ipc_drc_v1.md`** : contrat fixture documenté.
- **Géométrie figée** :
  - Pad **RA** centre **(10, 10)** net **QA_DRC_NET_A** KIID **`f0d1a001-0001-4001-8001-0000000000a1`**.
  - Pad **RB** centre **(11,15, 10)** net **QA_DRC_NET_B** KIID **`f0d1b002-0002-4002-8002-0000000000b2`**.
  - Distance centre–centre **1,15 mm** (**`distance_nm` ≈ 115_000_000**) ; écart bord–bord **0,15 mm** **<** clearance paire **0,30 mm** → violation **`DRCE_CLEARANCE`** attendue pour futur **`run_drc=true`**.
- **Script verify** : **`qa/tools/ipc_tools/r123_verify_ipc_drc_fixture.ps1`**
  - Copie vers **`build/.../ipc-drc-v1-proj/`** ; **`qa_open_editor_frame`** ; **`pcb_get_board_items_session`**, **`pcb_get_board_item_by_id_session`**, **`get_pcb_item_geometry_session`**, **`pcb_measure_distance_session`**, **`get_pcb_clearance_context_session`** (**`is_true_drc=false`**, **`required_clearance_nm` ≈ 30_000_000**), **`get_pcb_nearby_items_context_session`**, **`get_net_classes`**, **`get_nets`**.
  - Summary **`r123_ipc_drc_v1_fixture_summary.txt`** ; clé **`ipc_drc_v1_fixture_ok=true`**.
  - **Ne valide pas** une DRC officielle MCP (pas de handler).
- **Hors périmètre respecté** : aucun handler C++ ; aucun schéma MCP ; aucun critère green modifié ; pas d’intégration checkpoint MCP v2.
- **Recommandation R124** : implémenter **`get_pcb_drc_violations_context_session` V1** avec probe **`targeted_pcb_drc_violations_context_probe.ps1`** sur **`ipc_drc_v1`**, mode **`run_drc=true`** (collecte **`DRC_ENGINE`** mémoire) + **`run_drc=false`** ; famille candidate **`pcb_drc_violations_context_v1`** hors gate v2 initial.
- **Choix retenu (R123)** : fixture + verify harness uniquement.
- **Date** : 2026-06-06

## R124 — Implémentation `get_pcb_drc_violations_context_session` V1

- **Problématique** : Exposer à l’IA une liste **bornée** de violations DRC PCB read-only, avec **`run_drc=false`** (marqueurs runtime) et **`run_drc=true`** (collecte **`DRC_ENGINE::RunTests`** en mémoire), **sans** mutation board ni persistance marqueurs.
- **Handler** : **`pcbnew/api/api_handler_pcb_drc_violations_context.cpp`** — **`PcbDrcViolationsContextSessionToolInvoke`** ; callback **`common/mcp/mcp_pcb_drc_violations_context_callback.cpp`** ; registry **`get_pcb_drc_violations_context_session`** dans **`mcp_executor.cpp`**.
- **Contrat entrée V1** : `board_filename` (obligatoire, absolu) ; `max_violations` défaut **50**, plage **1–500** ; `run_drc` défaut **`false`** ; `include_items` défaut **`true`** ; `include_excluded` défaut **`false`** ; `test_footprints` défaut **`false`** ; `report_all_track_errors` défaut **`false`** ; filtres optionnels : `severity` (`error` / `warning` / `exclusion` / `all`), `violation_types[]`, `layer` (**`BL_*`** / alias **`F_Cu`**).
- **Contrat sortie V1** : `ok`, `is_official_drc`, `is_true_drc` (= `is_official_drc`), `drc_was_run`, `drc_source` (`existing_markers` | `engine_run`), `violations_count`, `returned_count`, `truncated`, `violations[]` (type, severity, message, layer, x_nm, y_nm, item_ids, net_names, rule_name si dispo), `exact_blocker_if_any`, `diagnostics`.
- **Modes** :
  - **`run_drc=false`** : lit **`BOARD::Markers()`** (`MARKER_DRC`) ; `is_official_drc=false` ; `drc_source=existing_markers` ; **0 violation** = cas normal (marqueurs non persistés dans `.kicad_pcb`).
  - **`run_drc=true`** : **`DRC_ENGINE::InitEngine`** + **`RunTests`** avec handler collecteur ; **pas** **`BOARD_COMMIT`** ; **pas** persistance marqueurs ; `is_official_drc=true` ; `drc_was_run=true` ; `drc_source=engine_run`.
- **Blockers finaux** : `missing_or_invalid_board_filename`, `board_filename_mismatch`, `pcb_context_unavailable`, `board_not_loaded`, `drc_engine_unavailable`, `drc_engine_not_initialized`, `drc_run_failed`, `invalid_max_violations`, `invalid_layer`, `N/A`.
- **QA** :
  - Fixture **`qa/data/pcbnew/drc/ipc_drc_v1/`** (R123).
  - Probe **`targeted_pcb_drc_violations_context_probe.ps1`** ; summary **`qa_pcb_drc_violations_context_summary.txt`**.
  - Runner **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`** ; summary **`qa_pcb_drc_violations_context_v1_family_summary.txt`** ; clé **`pcb_drc_violations_context_v1_family_green=true`**.
  - **`ipc_smoke.cpp`** : dump **`violations[]`** omis ; lignes greppables **`first_violation_type`**, **`drc_violation_types`**, **`drc_violation_net_names`** (leçon R115).
- **Gate** : **hors checkpoint MCP v2 initial** (comme **`schematic_assign_footprint_session`** R120) ; non-régression séquentielle : R123 fixture, familles PCB existantes, **`run_mcp_v2_ci_baseline.ps1 -VerifyOnly`** vert.
- **Résultats observés (2026-06-06)** : build OK ; probe + famille green ; fixture R123 green ; inspection / geometry / clearance / nearby green ; CI VerifyOnly **`MCP_V2_CI_BASELINE_OK=1`** ; MCP v2 checkpoint **inchangé**.
- **Limites V1** : pas parité garantie avec dialogue DRC GUI ; **`run_drc=true`** peut être coûteux sur grands boards ; marqueurs session souvent vides sans **`run_drc=true`**.
- **Recommandation R125** : intégrer **`pcb_drc_violations_context_v1`** au checkpoint MCP v2 après stabilité observée, ou enrichir **`get_project_context_snapshot`** avec compteur violations si **`run_drc=true`** demandé explicitement.
- **Choix retenu (R124)** : implémentation read-only dual-mode + QA famille dédiée hors gate v2.
- **Date** : 2026-06-06

## R125 — Mini-release PCB DRC Violations Context v1

- **Problématique** : Clôturer **R124** par une mini-release documentation + observation de stabilité, **sans** intégrer la famille au checkpoint MCP v2.
- **Périmètre respecté** : documentation uniquement (`MCP_V1_OFFICIAL_MANIFEST.md`, `MCP_TOOL_MAP_R1.tsv`, `MCP_V1_CHECKPOINT_RELEASE_NOTE.md`, `MCP_KICAD_USER_DEV_GUIDE.md`, `README_IPC_QA_BASELINE.md`, **`MCP_DECISIONS.md`** R125) ; **aucun** handler C++ ; **aucun** changement contrat MCP ; **aucune** modification **`run_mcp_v2_checkpoint.ps1`** ni critères green MCP v2.
- **Documentation complétée** :
  - objectif, contrat entrée/sortie, blockers, diff **`run_drc=false`** / **`run_drc=true`**
  - sémantique **`is_official_drc`**, **`is_true_drc`**, **`drc_was_run`**, **`drc_source`**
  - limites : pas parité GUI ; pas persistance marqueurs ; pas mutation board ; lenteur **`run_drc=true`** ; **`test_footprints=false`** ; types multiples sur **`ipc_drc_v1`**
- **Tool map** : **`get_pcb_drc_violations_context_session`** — catégorie **`PCB_DRC_context`**, statut **`candidate_family_hors_mcp_v2_checkpoint`**
- **Manifeste** : famille **`pcb_drc_violations_context_v1`** — **release R125**, **candidate stable**, **hors** gate v2 ; section **§19** contrat complet
- **Observation stabilité** : 3× **`targeted_pcb_drc_violations_context_probe.ps1`** ; 3× **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`** ; 1× R123 ; familles PCB existantes ; CI VerifyOnly ; checkpoint MCP v2 optionnel KIFACE smoke (non-régression **huit** familles — **sans** ajout DRC v1)
- **Résultats stabilité (2026-06-06, log `build/.../ipc-v1-artifacts/r125-stability-20260606-122140/`)** :
  - probes **3/3** green (`exit=0`, ~8–17 s chacun)
  - familles **3/3** green (`exit=0`, ~12–18 s chacun)
  - R123, inspection, geometry, clearance, nearby : **green**
  - CI VerifyOnly : **`MCP_V2_CI_BASELINE_OK=1`**
  - checkpoint v2 optionnel : **`mcp_v2_checkpoint_green=true`** (~316 s) — **huit** familles inchangées, **sans** `pcb_drc_violations_context_v1`
- **Types violations observés (fixture `ipc_drc_v1`, `run_drc=true`)** : **`clearance`**, **`invalid_outline`**, **`lib_footprint_issues`** (`violations_count=4`) ; nets **`QA_DRC_NET_A`**, **`QA_DRC_NET_B`** présents
- **Recommandation R126** : décision produit — intégrer **`pcb_drc_violations_context_v1`** comme **neuvième** famille MCP v2 **ou** enrichir snapshot projet ; **ou** durcir fixture (outline/footprint) si parité types souhaitée en QA
- **Choix retenu (R125)** : mini-release doc + stabilité multi-run ; **pas** d’intégration checkpoint v2
- **Date** : 2026-06-06

## R126 — Intégration `pcb_drc_violations_context_v1` au checkpoint MCP v2 (9e famille)

- **Problématique** : Promouvoir **`pcb_drc_violations_context_v1`** dans le gate MCP v2 officiel après stabilité candidate **R125**.
- **Modifications** :
  - **`run_mcp_v2_checkpoint.ps1`** : exécution **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`** **après** **`pcb_nearby_items_context_v1`** (9e famille) ; copie **`qa_pcb_drc_violations_context_v1_family_summary.txt`**
  - Champs summary : **`pcb_drc_violations_context_v1_ok`**, **`pcb_drc_violations_context_v1_target_run_dir`**, **`pcb_drc_violations_context_v1_exit_code`**, **`pcb_drc_violations_context_v1_elapsed_ms`**, **`ipc_smoke_ipc_retry_extra_max_pcb_drc_violations_context_v1`**, **`checkpoint_family_count=9`**
  - Blocker priorité **9** : **`pcb_drc_violations_context_v1:<exact_blocker>`**
  - **`run_mcp_v2_ci_baseline.ps1`** : **`checkpoint_family_count=9`**
  - Docs : manifest (**checkpoint-covered**), tool map (**`checkpoint_covered_mcp_v2`**), release note, README, guide dev
- **Hors périmètre respecté** : aucun handler C++ ; aucun changement contrat MCP ; probe/famille/critères internes DRC inchangés ; familles existantes inchangées
- **`mcp_v2_checkpoint_green`** exige désormais **9** familles vertes
- **Résultats validation (2026-06-06)** : checkpoint **`mcp_v2_checkpoint_green=true`**, **`pcb_drc_violations_context_v1_ok=true`**, **`checkpoint_family_count=9`**, **`checkpoint_elapsed_ms≈164000`**, **`pcb_drc_violations_context_v1_elapsed_ms≈9963`** ; CI VerifyOnly + CI baseline complet **`ci_baseline_green=true`** ; preflight **`MCP_V2_BASELINE_PREFLIGHT_OK=1`**
- **Recommandation R127** : durcir fixture **`ipc_drc_v1`** (réduire **`invalid_outline`** / **`lib_footprint_issues`**) **ou** enrichir **`get_project_context_snapshot`** avec résumé violations opt-in
- **Choix retenu (R126)** : intégration gate v2 séquentielle post-nearby
- **Date** : 2026-06-06

## R127 — Durcissement fixture `ipc_drc_v1` (mono-signal clearance)

- **Problématique** : La fixture gate DRC retournait **4** violations engine (`clearance`, `invalid_outline`, `lib_footprint_issues`) — signal QA non mono-type.
- **Audit causes** :
  - **`invalid_outline`** (`DRCE_INVALID_OUTLINE`) : un seul segment **`gr_line`** sur **Edge.Cuts** (0,0)–(25,0) — contour **non fermé** (`DRC_TEST_PROVIDER_MISC::testOutline`).
  - **`lib_footprint_issues`** (`DRCE_LIB_FOOTPRINT_ISSUES`) : FPID **`QA:R_FIX`** — bibliothèque **`QA`** absente de la config projet (`DRC_TEST_PROVIDER_LIBRARY_PARITY` ; 2 footprints → 2 violations).
- **Option retenue** : **A** — corriger la fixture pour viser **`drc_violation_types=clearance`** uniquement.
- **Patch fixture** (`ipc_drc_v1.kicad_pcb`) :
  - Contour **`gr_rect`** fermé **(0,0)–(25,20) mm** sur **Edge.Cuts**.
  - Empreintes **`(footprint "" …)`** inline board-only (nickname vide → skip library parity).
  - Pads, nets, KIID, géométrie clearance **inchangés**.
- **Probe** : assertion **`clearance_only_types_ok`** (`violations_count=1`, `drc_violation_types=clearance`).
- **Hors périmètre respecté** : aucun handler MCP ; checkpoint v2 inchangé (9 familles) ; critères familles PCB existantes inchangés.
- **Recommandation R128** : enrichir **`get_project_context_snapshot`** avec compteur violations opt-in, ou documenter usage **`violation_types=["clearance"]`** pour filtrage IA.
- **Choix retenu (R127)** : Option A — fixture mono-signal clearance.
- **Date** : 2026-06-06

## R128 — Audit enrichissement `get_project_context_snapshot` (résumé DRC optionnel)

- **Objectif** : auditer la faisabilité d’un **`drc_summary`** opt-in dans **`get_project_context_snapshot`**, sans implémentation ni changement de contrat MCP dans R128.
- **Implémentation actuelle** (`common/mcp/mcp_executor.cpp`, **`invokeGetProjectContextSnapshot`**) :
  - Tool **composé lecture** (pas de handler C++ dédié) agrégeant **`get_project_info`**, **`get_workspace_overview`**, **`get_open_documents`**, **`get_net_classes`**, **`get_nets`**.
  - **Entrées V1** : `board_filename?`, `include_nets?` (défaut **true**), `open_documents_type?` (`all`|`pcb`|`sch`).
  - **Sortie** : blocs nested + **`snapshot_summary`** (`project_loaded`, compteurs docs/netclasses/nets) + flags `*_present`.
  - **Nature** : **mixte project + workspace + PCB conditionnel** — project/netclasses **sans frame** ; open docs / nets **GUI + frame PCB** requis pour contenu utile (headless **`kicad-mcp`** : listes vides, nets **`skipped=no_board_target`**).
  - **Filenames** : workspace retourne **basename** (`GetFullName`) dans `board_filename` des docs ouverts — **`get_nets`** exige chemin **absolu** → échec silencieux possible dans le bloc `nets.error` (QA ne requiert pas **`nets_present`**).
- **Tool DRC source** (`get_pcb_drc_violations_context_session`, `pcbnew/api/api_handler_pcb_drc_violations_context.cpp`) :
  - **GUI-only** frame PCB + **`board_filename` absolu** + match éditeur.
  - **`run_drc=false`** → marqueurs session (`existing_markers`) ; souvent **0** violation (marqueurs non persistés `.kicad_pcb`).
  - **`run_drc=true`** → **`DRC_ENGINE::RunTests`** mémoire ; **`is_official_drc=true`** ; fixture **`ipc_drc_v1`** post-R127 : **`violations_count=1`**, type **`clearance`** uniquement.
- **Options comparées** :
  - **A — Metadata-only** (défaut snapshot) : jamais de DRC ; expose `drc_tool_available`, `pcb_board_open`, `recommended_next_tool` — **coût nul**, **zéro flake engine**.
  - **B — Existing markers** (`drc_run_mode=existing_markers`) : délégation **`run_drc=false`** — **lecture cheap** mais **souvent trompeuse** (0 violation sur board fraîchement ouvert malgré problèmes réels).
  - **C — Engine run opt-in** (`drc_run_mode=engine_run`) : délégation **`run_drc=true`** — **officiel**, aligné gate **`ipc_drc_v1`**, **coûteux** (~10–18 s probe) ; **jamais par défaut**.
- **Option recommandée R129** : **progressive opt-in A → B/C** — défaut **metadata-only** ; si **`include_drc_summary=true`** : mode par défaut **`existing_markers`** (B) ; **`engine_run`** (C) **uniquement** si demandé explicitement ; **pas de tableau `violations[]`** dans le snapshot (résumé compact + renvoi vers tool DRC pour le détail).
- **Contrat entrée proposé (R129)** :
  - `include_drc_summary` (bool, défaut **false**)
  - `drc_run_mode` : `none` | `existing_markers` | `engine_run` (défaut **`none`** si `include_drc_summary=false`, sinon **`existing_markers`**)
  - `drc_board_filename?` (absolu ; défaut = résolution board comme **`get_nets`** mais **corriger chemin absolu**)
  - `drc_max_violations` (1–500, défaut **10** dans snapshot vs **50** tool DRC)
  - `drc_violation_types?` (filtre types, ex. `["clearance"]`)
- **Contrat sortie proposé** : bloc **`drc_summary`** compact (champs user : `available`, `requested`, `run_mode`, `is_official_drc`, `drc_was_run`, `violations_count`, `returned_count`, `truncated`, `types[]`, `has_clearance_violations`, `exact_blocker_if_any`, `recommended_detail_tool=get_pcb_drc_violations_context_session`) — **sans** liste violations complète.
- **Sémantique IA** : snapshot = **orientation + compteurs/types** ; détail items/nets/positions → **`get_pcb_drc_violations_context_session`** avec `include_items=true`.
- **Risques** : latence **`engine_run`** sur appels fréquents ; confusion **`is_official_drc`** vs clearance context (**`is_true_drc=false`**) ; mode B **false-negative** ; duplication si logique DRC recodée (préférer **`CallTool` interne** comme **`get_nets`**) ; résolution chemin board à durcir.
- **QA R129 proposée** : probe **`targeted_project_context_snapshot_drc_probe.ps1`** (ou extension harness IPC v1) sur fixture **`ipc_drc_v1`** — cas : défaut sans DRC ; `existing_markers` (0 attendu) ; `engine_run` (1× **`clearance`**) ; non-régression **`run_ipc_v1_windows.ps1`** snapshot flags ; checkpoint v2 **inchangé** (pas de famille snapshot).
- **Hors périmètre R128 respecté** : aucun handler, schéma MCP, script QA, critère green, checkpoint modifié.
- **Choix retenu (R128)** : audit only — implémentation **R129** avec opt-in progressif **A + B/C**.
- **Recommandation R129** : implémenter **`drc_summary`** composé via **`CallTool("get_pcb_drc_violations_context_session")`** + probe dédiée + doc manifest/tool map.
- **Date** : 2026-06-06

## R129 — Implémentation `drc_summary` opt-in dans `get_project_context_snapshot`

- **Objectif** : enrichir **`get_project_context_snapshot`** avec bloc **`drc_summary`** optionnel via délégation **`CallTool("get_pcb_drc_violations_context_session")`**, sans DRC par défaut, sans violations détaillées dans le snapshot.
- **Implémentation** (`common/mcp/mcp_executor.cpp`, **`invokeGetProjectContextSnapshot`**) :
  - Entrées ajoutées : **`include_drc_summary`** (défaut **false**), **`drc_run_mode`** (`none`|`existing_markers`|`engine_run`), **`drc_board_filename?`**, **`drc_max_violations`** (1–100, défaut **10**), **`drc_violation_types?`**
  - Résolution board **absolu** : override → `project_directory` + basename ouvert → sibling `.kicad_pcb` depuis `.kicad_pro`
  - Sortie : **`drc_summary_present`** + bloc **`drc_summary`** compact (compteurs, **`types[]`**, **`has_clearance_violations`**, **`recommended_detail_tool`**) — **sans** `violations[]`
  - Erreurs DRC : snapshot global **ok** ; blocker dans **`drc_summary.exact_blocker_if_any`**
  - Validation structurelle : **`invalid_drc_run_mode`** / **`invalid_drc_max_violations`** → **`AS_BAD_REQUEST`** snapshot entier
- **QA** : **`targeted_project_context_snapshot_drc_probe.ps1`** — fixture **`ipc_drc_v1`** ; cas default / existing_markers / engine_run / invalid mode / invalid max
- **Hors périmètre respecté** : handler DRC inchangé ; checkpoint MCP v2 inchangé (9 familles) ; critères green MCP v2 inchangés ; pas de nouvelle famille checkpoint
- **Choix retenu (R129)** : opt-in progressif R128 — défaut sans DRC ; **`existing_markers`** si summary demandé ; **`engine_run`** explicite seulement
- **Recommandation R130** : documenter pattern IA « snapshot orientation → tool DRC détail » dans le guide utilisateur ; éventuellement enrichir **`snapshot_summary`** avec **`open_pcb_board_absolute_path`** pour éviter ambiguïté basename
- **Date** : 2026-06-06

## R130 — Chemin PCB absolu dans `snapshot_summary`

- **Problématique** : `workspace_overview` / `open_documents` exposent le PCB ouvert en **basename** (`GetFullName` dans `workspace_state.cpp`) — ambigu pour agents IA et tools exigeant un chemin **absolu**.
- **Audit** :
  - Source basename : **`maybeAddDocumentFromFrame`** → `doc.filename = fn.GetFullName()`
  - Chemin absolu **disponible** côté frame (`GetCurrentFileName()`) mais **non propagé** dans l’API workspace protobuf/MCP historique
  - Meilleur emplacement sortie : **`snapshot_summary`** (champs dédiés, sans casser `open_documents`)
- **Implémentation** (`invokeGetProjectContextSnapshot`) :
  - Helpers **`resolveOpenPcbBoardAbsolutePaths`** : basename ouvert + `project_directory` → absolu ; fallback sibling `.kicad_pcb`
  - Champs **`snapshot_summary`** : **`open_pcb_board_absolute_path_present`**, **`open_pcb_board_absolute_path`** (premier/actif), **`open_pcb_board_absolute_paths`** (liste)
  - **`resolveAbsoluteBoardPathForSnapshot`** (DRC R129) réutilise la même résolution
  - **`workspace_state.cpp` inchangé** — compatibilité IPC existante préservée
- **QA** : **`targeted_project_context_snapshot_paths_probe.ps1`** — résumé **`qa_project_context_snapshot_paths_summary.txt`** ; vérifie suffix `.kicad_pcb`, correspondance fixture, **`engine_run`** sans `drc_board_filename`, non-régression DRC défaut
- **Hors périmètre respecté** : handler DRC ; checkpoint v2 ; critères green MCP v2 ; comportement `drc_summary` par défaut inchangé
- **Choix retenu (R130)** : enrichissement **`snapshot_summary`** uniquement — pas de rename champs existants
- **Recommandation R131** : propager **`board_absolute_path`** non destructif dans `open_documents[]` si parité IPC le permet ; documenter pattern IA « lire `snapshot_summary.open_pcb_board_absolute_path` → appeler tools PCB/DRC »
- **Date** : 2026-06-06

## R131 — Chemins absolus dans `open_documents[]`

- **Problématique** : post-R130, **`open_documents[]`** / **`workspace_overview`** exposaient encore **`board_filename`** = basename — ambigu pour agents IA lisant directement ce bloc.
- **Audit** :
  - Construction : **`maybeAddDocumentFromFrame`** (`workspace_state.cpp`) — chemin absolu disponible via **`GetCurrentFileName()`** / **`wxFileName::GetFullPath()`**
  - Champs MCP existants : **`type`**, **`board_filename`** (basename), **`project`** (PCB)
  - Protobuf **`DocumentSpecifier`** inchangé — propagation MCP via **`openDocumentToMcpStruct`** sans modifier le schéma proto IPC
- **Implémentation** :
  - **`OPEN_DOCUMENT.absolute_path`** ajouté (`workspace_state.h/.cpp`)
  - **`openDocumentToMcpStruct`** (`mcp_executor.cpp`) : conserve **`board_filename`** ; ajoute **`absolute_path`** + **`board_absolute_path`** (PCB) / **`schematic_absolute_path`** (SCH)
  - **`get_workspace_overview`** / **`get_open_documents`** : sérialisation depuis **`OPEN_DOCUMENT`** (plus via protobuf pour les chemins)
  - **`resolveOpenPcbBoardAbsolutePaths`** : priorité chemins depuis **`open_pcb_documents[]`** MCP ; cohérence avec **`snapshot_summary.open_pcb_board_absolute_path`**
- **QA** : probe paths étendu — **`open_documents_pcb_absolute_path_ok`**, **`open_documents_matches_snapshot_summary_ok`**
- **Hors périmètre respecté** : handler DRC ; `drc_summary` ; checkpoint v2 ; critères green MCP v2 ; basename **`board_filename`** conservé
- **Choix retenu (R131)** : enrichissement additif **`open_documents[]`** + cohérence R130
- **Recommandation R132** : documenter pattern IA unifié ; éventuelle promotion au contrat protobuf **`DocumentSpecifier`** si parité IPC officielle requise
- **Date** : 2026-06-06

## R132 — Audit promotion `absolute_path` dans `DocumentSpecifier` protobuf

- **Problématique** : post-R131, les chemins absolus sont exposés côté MCP (`open_documents[]`, `snapshot_summary`) mais **`GetWorkspaceOverview`** / IPC natif sérialise encore **`DocumentSpecifier`** sans chemin absolu — faut-il promouvoir un champ protobuf ?
- **Localisation protobuf** :
  - Définition : **`api/proto/common/types/base_types.proto`** — message **`DocumentSpecifier`**
  - Champs actuels : **`type`** (1) ; **`oneof identifier`** — **`lib_id`** (2), **`sheet_path`** (3), **`board_filename`** (4) ; **`project`** (5)
  - Sémantique **`board_filename`** : commentaire proto « e.g. `board.kicad_pcb` » = **basename** ; code **`maybeAddDocumentFromFrame`** → `fn.GetFullName()` ; **`validateDocumentInternal`** PCB compare à **`GetFullName()`** ; SCH stocke aussi le basename dans **`board_filename`** (workaround documenté **`workspace_state.h`**) — **pas ambigu côté implémentation**, mais le nom du champ est générique/historique
  - Consommateurs : **`ItemHeader.document`** (requêtes PCB/SCH) ; **`GetWorkspaceOverviewResponse.open_*_documents`** ; handlers **`api_handler_pcb.cpp`**, **`api_handler_sch.cpp`**, **`api_handler_editor.cpp`**
- **Compatibilité ajout champ** :
  - Proto3 : nouveau champ scalaire (ex. **`string absolute_path = 6`**) = **backward-compatible** (clients anciens ignorent ; serveur peut ne pas le remplir)
  - Numérotation : **6** libre (champs 1–5 occupés) ; **ne pas réutiliser** un numéro déprécié
  - Fichiers générés : **`build/.../cpp/api/common/types/base_types.pb.h`** / **`.pb.cc`** via **`api/CMakeLists.txt`** (`protoc --cpp_out`)
  - Risque clients IPC : **faible** si additif — clients qui ne lisent pas le nouveau champ continuent avec **`board_filename`** ; clients qui l’exigent sans patch serveur ne verront rien (comportement actuel)
  - **Sans** patch **`ToProto`** + handlers, ajouter le champ proto seul **n’apporte aucune valeur** — **`workspace_state.cpp`** ne le remplit pas aujourd’hui
- **Comparaison design** :
  | Option | Avantages | Inconvénients |
  |--------|-----------|---------------|
  | **`absolute_path`** (proto) | Parité nommage MCP ; générique PCB+SCH | Ne distingue pas rôle board vs sch ; redondant avec **`project.path` + basename** |
  | **`document_absolute_path`** | Plus explicite | Verbeux ; diverge des champs MCP R131 |
  | **`board_absolute_path` / `schematic_absolute_path`** (proto) | Aligné MCP R131 | Deux champs ou sémantique conditionnelle sur un message partagé ; lourde pour **`oneof identifier`** existant |
  | **MCP seulement (statu quo)** | Déjà livré R130/R131 ; QA green ; zéro rupture IPC | Écart contrat protobuf ↔ MCP ; **`ipc_smoke workspace_overview`** ne voit pas les chemins absolus |
- **Impact QA si patch R133 (Option A)** :
  - Build + regen protobuf obligatoires
  - Étendre **`ipc_smoke.cpp`** commande **`workspace_overview`** : lignes greppables **`open_pcb_documents0_absolute_path=`** (miroir MCP)
  - Étendre **`targeted_project_context_snapshot_paths_probe.ps1`** ou créer probe IPC dédié comparant **`GetWorkspaceOverview`** vs fixture
  - Non-régression : **`targeted_project_context_snapshot_drc_probe.ps1`**, **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`**, **`run_mcp_v2_checkpoint.ps1`**, **`run_mcp_v2_ci_baseline.ps1 -VerifyOnly`**
  - Checkpoint MCP v2 et critères green : **inchangés** (chemins absolus hors familles checkpoint)
- **Options tranchées** :
  - **Option A** (R133 proto) : faisable techniquement ; coût modéré (proto + regen + **`ToProto`** + QA IPC) ; utile **seulement** si clients IPC natifs (non-MCP) doivent naviguer sans couche MCP
  - **Option B** (retenue) : **protobuf inchangé** ; documenter MCP comme **source officielle navigation IA** — pattern : **`get_project_context_snapshot`** → **`snapshot_summary.open_pcb_board_absolute_path`** et/ou **`open_documents[].board_absolute_path`** → tools PCB/DRC ; **`board_filename`** reste identifiant legacy IPC
  - **Option C** : sous-ensemble de B — reporter sans documentation ; **rejeté** car R130/R131 ont déjà formalisé le besoin agent ; la documentation manquait
- **Choix retenu (R132)** : **Option B** — pas de promotion protobuf tant qu’aucun client IPC natif ne l’exige ; MCP R130/R131 couvre le cas d’usage agent/QA actuel
- **Recommandation R133** : **documentation uniquement** (guide IA + baseline) sauf demande explicite parité IPC — alors Option A minimaliste : **`absolute_path = 6`** + **`ToProto`** + assertion **`ipc_smoke workspace_overview`**
- **Hors périmètre respecté** : aucun `.proto`, regen, handler C++, checkpoint, critères green, champs MCP R131 modifiés
- **Date** : 2026-06-06

## R133 — Mini-release « Project Snapshot navigation-ready »

- **Problématique** : clôturer la série R128–R132 — formaliser le contrat navigation IA sans promotion protobuf (Option B R132).
- **Pattern IA officiel** :
  1. Appeler **`get_project_context_snapshot`** (sans DRC par défaut).
  2. Lire **`snapshot_summary.open_pcb_board_absolute_path`** (PCB actif) **ou** **`open_documents[]`** filtré type PCB → **`board_absolute_path`** (SCH → **`schematic_absolute_path`**).
  3. Passer ce chemin **absolu** aux tools PCB/DRC (`board_filename` param) — ex. **`get_pcb_drc_violations_context_session`**, **`pcb_get_board_items_session`**, **`get_pcb_item_geometry_session`**.
- **Sémantique champs** :
  - **`board_filename`** / **`filename`** : **conservés** ; souvent **basename** — identifiant legacy IPC et rétrocompat MCP.
  - **`absolute_path`**, **`board_absolute_path`**, **`schematic_absolute_path`** : champs MCP **recommandés** pour navigation agent (R131).
  - Protobuf **`DocumentSpecifier`** : **basename-only** (`board_filename` dans `oneof identifier`) — clients IPC natifs (**`GetWorkspaceOverview`**, requêtes **`ItemHeader`**) : **ancien contrat inchangé**.
- **Statut protobuf** :
  - Ajout futur **`absolute_path = 6`** techniquement possible (proto3 additif, R132).
  - **Non retenu** R132/R133 — pas de patch `.proto` / regen / **`ToProto`**.
  - **Réouverture** : besoin **explicite** d’un client IPC natif (hors MCP) nécessitant chemins absolus dans **`GetWorkspaceOverview`** ou **`DocumentSpecifier`** entrant.
- **Implémentation** : **documentation uniquement** — manifest, guide, release note, baseline, présente décision.
- **QA non-régression** (série R133) :
  - **`targeted_project_context_snapshot_paths_probe.ps1`**
  - **`targeted_project_context_snapshot_drc_probe.ps1`**
  - **`run_pcb_drc_violations_context_v1_targeted_probes.ps1`**
  - **`run_mcp_v2_ci_baseline.ps1 -VerifyOnly`**
  - Checkpoint v2 optionnel : **`run_mcp_v2_checkpoint.ps1`**
- **Hors périmètre respecté** : `.proto`, regen, `workspace_state.cpp`, `mcp_executor.cpp`, checkpoint critères green, logique DRC
- **Choix retenu (R133)** : mini-release documentaire **« Project Snapshot navigation-ready »** — MCP = source navigation IA ; protobuf inchangé
- **QA R133 observée** (2026-06-06) : paths **`project_context_snapshot_paths_green=true`** ; DRC snapshot **`project_context_snapshot_drc_green=true`** ; famille DRC v1 **`pcb_drc_violations_context_v1_family_green=true`** ; CI **`ci_baseline_green=true`** (VerifyOnly) ; checkpoint v2 optionnel **`mcp_v2_checkpoint_green=true`** (9 familles)
- **Recommandation R134** : dette open-frame / factorisation QA mineure **ou** nouvelle capability contrôlée — **ne pas** rouvrir protobuf sans ticket client IPC natif
- **Date** : 2026-06-06

## R134 — Audit `project_readiness_summary` pour IA KiCad

- **Problématique** : après R133 (snapshot navigation-ready), l’IA a besoin d’une **vue compacte** : documents ouverts, PCB accessible, DRC lançable, violations, footprints manquants, prochaines actions — sans lancer DRC par défaut ni rouvrir protobuf.
- **Capacités existantes auditées** :
  - **`get_project_context_snapshot`** (`mcp_executor.cpp` ~895–1153) : compose **`get_project_info`**, **`get_workspace_overview`**, **`get_open_documents`**, **`get_net_classes`**, **`get_nets`** ; **`snapshot_summary`** (counts, chemins absolus PCB) ; **`drc_summary`** opt-in (**R129**, délégation **`get_pcb_drc_violations_context_session`**, défaut **`include_drc_summary=false`**, jamais **`engine_run`** par défaut).
  - **`workflow_handoff_readiness_session`** : readiness **workflow** (`sch_to_pcb` / `pcb_to_release`) via **`workflow_next_best_action_session`** — **pas** un résumé projet compact ; orienté exécution/rollback.
  - **`simulation_preflight_readiness_session`** : readiness **simulation** isolée — hors périmètre DRC/footprint projet.
  - **`schematic_assign_footprint_session`** : **write** + validation library + **`pcb_ready`** — **pas** agrégat read-only.
  - **`get_symbol_fields_by_id`** : retourne **`reference`**, **`value`**, **`lib_id`**, flags power — **sans champ footprint** (`api_handler_sch.cpp` ~1981–1991).
  - **`get_sch_items`** : défaut **`SCH_LINE_T`** seulement ; avec **`types=[SCH_SYMBOL_T]`** → **`count`** + KIID par symbole — **sans footprint** (sérialisation proto symbole vide, patch `id` seulement).
- **Signaux readiness fiables aujourd’hui** (sans nouveau handler) :
  | Signal | Source | Fiabilité |
  |--------|--------|-----------|
  | `project_loaded` | `snapshot_summary.project_loaded` | **Fiable** |
  | `schematic_open` | `open_schematic_documents_count > 0` | **Fiable** (frame ouvert) |
  | `pcb_open` | `open_pcb_documents_count > 0` | **Fiable** |
  | `pcb_absolute_path_present` | `snapshot_summary.open_pcb_board_absolute_path_present` | **Fiable** (R130/R131) |
  | `net_classes_available` | `net_classes_present` + `net_classes_count` | **Fiable** si projet chargé |
  | `nets_available` | `nets_present` + `nets_entry_count` | **Fiable** si PCB cible + `include_nets=true` |
  | `drc_available` | dérivable : `pcb_absolute_path_present` | **Fiable** (tool DRC exige board absolu) |
  | `drc_summary_present` | flag + bloc **`drc_summary`** si opt-in | **Fiable** |
  | `drc_has_violations` | **`drc_summary.violations_count > 0`** si opt-in **`existing_markers`** ou **`engine_run`** | **Partiel** — **`existing_markers`** souvent **0** (marqueurs non persistés) ; **`engine_run`** coûteux |
  | `drc_has_clearance_violations` | **`drc_summary.has_clearance_violations`** | **Partiel** (même caveat mode) |
  | `drc_recommended_next_tool` | déjà **`drc_summary.recommended_detail_tool`** | **Fiable** |
  | Simulation | **`simulation_preflight_readiness_session`** séparé | **Hors snapshot** |
- **Signaux manquants / non fiables sans extension** :
  | Signal | Gap |
  |--------|-----|
  | `symbols_count` / `symbols_without_footprint_count` | **Aucun read aggregate** ; footprint non exposé par **`get_symbol_fields_by_id`** |
  | Footprint validé vs texte seul | Validation library uniquement dans **`assign_footprint`** (write) |
  | `sch_to_pcb_ready` bool compact | **`workflow_handoff_readiness_session`** ≠ compteur ; pas de scan footprint |
  | « Projet prêt » global | Risque sémantique — **interdit** comme verdict unique V1 |
- **Footprint readiness (audit)** :
  - Détection propre **impossible** côté MCP executor seul sans handler SCH read.
  - **`get_sch_items` + boucle `get_symbol_fields_by_id`** : **insuffisant** (pas de footprint en sortie).
  - **Recommandation** : nouveau tool read-only **`schematic_footprint_readiness_session`** (handler **`api_handler_sch.cpp`**) — scan écran courant, compteurs **`symbols_count`**, **`symbols_without_footprint_count`**, **`symbols_with_footprint_count`**, option **`validate_footprints`** (coût library) ; **ou** extension **`get_symbol_fields_by_id`** avec champ **`footprint`** (plus petit patch, pas d’agrégat).
- **DRC readiness (contrat proposé)** — réutiliser **`drc_summary`** existant, **ne pas dupliquer** :
  - **`drc_available`** : bool dérivé (`pcb_absolute_path_present`)
  - **`drc_summary_requested`** : `include_drc_summary`
  - **`drc_summary_present`** : flag existant
  - **`drc_has_violations`** / **`drc_has_clearance_violations`** : miroir **`drc_summary`** si présent ; sinon **`null`/absent** (pas d’inférence)
  - **`drc_recommended_next_tool`** : **`get_pcb_drc_violations_context_session`**
  - **Règle** : défaut snapshot **sans** DRC ; **`existing_markers`** seulement si IA demande résumé ; **`engine_run`** jamais par défaut readiness
- **Contrat sortie proposé (R135)** — bloc **`project_readiness_summary`** dans snapshot :
  ```json
  {
    "available": true,
    "project_loaded": true,
    "schematic_open": true,
    "pcb_open": true,
    "pcb_absolute_path_present": true,
    "pcb_absolute_path": "<from snapshot_summary>",
    "sch_absolute_path": "<from open_documents sch>",
    "net_classes_available": true,
    "nets_available": true,
    "drc_available": true,
    "drc_summary_requested": false,
    "drc_summary_present": false,
    "drc_has_violations": null,
    "drc_has_clearance_violations": null,
    "drc_recommended_next_tool": "get_pcb_drc_violations_context_session",
    "footprints_summary_available": false,
    "footprints_summary_requested": false,
    "symbols_count": null,
    "symbols_without_footprint_count": null,
    "sch_to_pcb_ready": null,
    "recommended_next_actions": ["open_pcb_for_drc"],
    "recommended_tools": ["get_pcb_drc_violations_context_session"],
    "exact_blocker_if_any": "N/A"
  }
  ```
  - **`recommended_next_actions`** / **`recommended_tools`** : règles **déterministes** bornées (pas de ML) — ex. si `symbols_without_footprint_count > 0` → **`assign_missing_footprints`** + **`schematic_assign_footprint_session`** ; si DRC opt-in avec violations → **`run_drc_detail`**.
  - **`sch_to_pcb_ready`** : **`null`** tant que footprints_summary absent ; si présent : `symbols_without_footprint_count == 0` **ET** `pcb_absolute_path_present` (heuristique V1, documentée).
- **Comparaison emplacement produit** :
  | Option | Avantages | Inconvénients |
  |--------|-----------|---------------|
  | **A** — enrichir **`get_project_context_snapshot`** | Hub IA R133 ; pattern **`drc_summary`** opt-in éprouvé ; un seul appel | Snapshot déjà composé ; risque lourdeur si scans activés |
  | **B** — tool **`get_project_readiness_summary`** | Séparation concerns | Fragmente entrée IA ; duplication compose snapshot |
  | **C** — pattern client documenté | Zéro code | Footprint gap ; orchestration lourde ; objectif produit non atteint |
- **Risques** :
  - Résumé **trop interprétatif** → limiter aux règles explicites + **`exact_blocker_if_any`**
  - **Faux « projet prêt »** → pas de bool **`project_ready`** global V1
  - **Coût runtime** → métadonnées dérivées par défaut ; footprints + DRC opt-in séparés
  - **Flakiness DRC** → ne jamais **`engine_run`** par défaut ; documenter limite **`existing_markers`**
  - **Duplication client** → Option C insuffisante
- **QA R135 proposée** (si implémentation) :
  - Fixture : **`ipc_drc_v1`** (PCB + DRC) + **`issue19090`** (SCH footprints) ou fixture dédiée **`ipc_readiness_v1`**
  - Probe : **`targeted_project_context_snapshot_readiness_probe.ps1`**
  - Cas : défaut (readiness metadata, pas DRC, pas footprints) ; `include_drc_summary` + `existing_markers` ; `include_footprints_summary=true` (post tool SCH) ; cohérence chemins R130/R131
  - Non-régression : paths probe, DRC snapshot probe, checkpoint v2 **inchangé**
  - **`ipc_smoke`** : lignes greppables **`readiness_*`**
- **Besoin nouveau tool** : **oui** pour footprint aggregate — **`schematic_footprint_readiness_session`** (read-only) recommandé en R135b ou extension **`get_symbol_fields_by_id`**
- **Choix retenu (R134 audit)** : **Option A** — bloc **`project_readiness_summary`** opt-in progressif dans **`get_project_context_snapshot`** :
  1. **R135a** : métadonnées dérivées + **`recommended_*`** (sans handler)
  2. **R135b** : tool SCH read footprint aggregate + **`include_footprints_summary`**
  3. DRC : réutiliser **`drc_summary`** existant (pas de 3e bloc redondant)
- **Recommandation R135** : implémenter **R135a** dans **`mcp_executor.cpp`** uniquement ; planifier **R135b** handler SCH ; ne pas créer tool composé séparé (Option B rejetée) ; ne pas rester Option C
- **Hors périmètre respecté** : handlers, schémas MCP, scripts QA, checkpoint, critères green, protobuf, DRC par défaut
- **Date** : 2026-06-06

## R135a — `project_readiness_summary` dérivé dans `get_project_context_snapshot`

- **Objectif** : bloc **`project_readiness_summary`** compact, dérivé des signaux snapshot existants — sans handler SCH, sans scan footprint, sans DRC par défaut.
- **Implémentation** (`mcp_executor.cpp`) :
  - **`buildProjectReadinessSummary`** : dérive project/docs/DRC/footprints-unknown + **`recommended_next_actions`** / **`recommended_tools`**
  - Flags top-level : **`project_readiness_summary_present`**
  - Champs null protobuf pour **`drc_has_violations`**, **`symbols_count`**, etc. si indisponibles
  - **`footprints_summary_diagnostic`** : indique **`schematic_footprint_readiness_session`** requis (R135b)
  - Règles actions : `open_or_create_pcb` ; `request_drc_summary_engine_run` ; `run_drc_detail` ; `inspect_schematic_footprints`
  - **`existing_markers`** + `violations_count=0` → **`request_drc_summary_engine_run`** (ne prétend pas board clean)
  - Pas de bool global **`project_ready`**
- **QA** :
  - **`targeted_project_context_snapshot_readiness_probe.ps1`** — résumé **`qa_project_context_snapshot_readiness_summary.txt`**
  - **`ipc_smoke.cpp`** : lignes greppables **`readiness_*`**
- **Hors périmètre respecté** : handlers SCH/PCB DRC ; checkpoint v2 ; critères green ; **`schematic_footprint_readiness_session`**
- **Choix retenu (R135a)** : enrichissement snapshot hub IA — footprint aggregate reporté **R135b**
- **QA R135a observée** (2026-06-06) : readiness **`project_context_snapshot_readiness_green=true`** ; paths **`project_context_snapshot_paths_green=true`** ; DRC snapshot **`project_context_snapshot_drc_green=true`** ; famille DRC v1 green ; checkpoint v2 **`mcp_v2_checkpoint_green=true`** (9 familles) ; CI **`ci_baseline_green=true`** (VerifyOnly)
- **Recommandation R135b** : **`schematic_footprint_readiness_session`** read-only + **`include_footprints_summary`** opt-in
- **Date** : 2026-06-06

## R135b — `schematic_footprint_readiness_session` + opt-in `include_footprints_summary`

- **Objectif** : source read-only fiable pour compteurs footprint SCH + **`sch_to_pcb_ready`** ; intégration opt-in dans **`get_project_context_snapshot`** sans changer le défaut R135a.
- **Nouveau tool** : **`schematic_footprint_readiness_session`** (read-only, GUI SCH)
  - Handler : **`SchematicFootprintReadinessSessionToolInvoke`** — **`eeschema/api/api_handler_sch.cpp`**
  - Callback : **`common/mcp/mcp_sch_footprint_readiness_callback.cpp`**
  - Registry : **`common/mcp/mcp_executor.cpp`**
- **Entrée** : `schematic_filename` (obligatoire, doit matcher schéma ouvert) ; `include_symbols` (défaut true) ; `max_symbols` (défaut 50, bornes 1–500) ; `validate_footprints` (défaut false)
- **Sortie** : `symbols_count`, `symbols_with_footprint_count`, `symbols_without_footprint_count`, `symbols_with_invalid_footprint_count` (null si pas de validation), `sch_to_pcb_ready`, `symbols[]` borné, `truncated`, `exact_blocker_if_any`, `diagnostics`
- **Règles scan** :
  - Parcourt **`SCH_SYMBOL_T`** sur écran courant
  - **Power symbols exclus** (`IsPower()`) — comptés dans `diagnostics` (`power_symbols_excluded=N`)
  - Footprint via **`GetFootprintFieldText(false, &currentSheet, false)`**
  - **`sch_to_pcb_ready`** = `symbols_count > 0 && symbols_without_footprint_count == 0`
  - **`validate_footprints=true`** → blocker **`footprint_validation_unavailable`** (reporté **R136**)
- **Blockers** : `missing_or_invalid_schematic_filename`, `schematic_filename_mismatch`, `sch_context_unavailable`, `schematic_not_loaded`, `invalid_max_symbols`, `footprint_validation_unavailable`, `N/A`
- **Snapshot opt-in** (`get_project_context_snapshot`) :
  - Entrées : `include_footprints_summary` (défaut false), `footprints_max_symbols` (1–500, défaut 50)
  - Si false : comportement R135a inchangé (`footprints_summary_available=false`, champs null)
  - Si true : **`CallTool("schematic_footprint_readiness_session")`** ; remplit **`project_readiness_summary`** + bloc compact **`footprints_summary`**
  - Action **`assign_missing_footprints`** si `symbols_without_footprint_count > 0` ; sinon pas d’assignation recommandée
  - SCH non ouvert : `footprints_summary_available=false` (pas d’erreur snapshot globale)
- **QA** :
  - **`targeted_schematic_footprint_readiness_probe.ps1`** — fixture **`issue19090`** ; négatifs filename absent / mismatch
  - **`targeted_project_context_snapshot_readiness_probe.ps1`** étendu — opt-in sans SCH ; opt-in happy **`issue19090`**
  - **`ipc_smoke.cpp`** : lignes greppables **`footprint_readiness_*`**, **`readiness_symbols_*`**, **`footprints_summary_*`**
- **Hors périmètre respecté** : **`schematic_assign_footprint_session`** inchangé ; checkpoint MCP v2 ; critères green ; validation library par défaut ; pas de mutation SCH ; pas de bool **`project_ready`**
- **Choix retenu (R135b)** : tool SCH read-only dédié + opt-in snapshot (pas d’extension **`get_symbol_fields_by_id`** seule)
- **QA R135b observée** (2026-06-06) : footprint readiness **`schematic_footprint_readiness_green=true`** (`symbols_count=2`, `symbols_without_footprint_count=0`, `sch_to_pcb_ready=true` sur **`issue19090`**) ; snapshot readiness **`project_context_snapshot_readiness_green=true`** (défaut footprints unknown ; opt-in sans SCH ; opt-in happy **`issue19090`**) ; paths **`project_context_snapshot_paths_green=true`** ; DRC snapshot **`project_context_snapshot_drc_green=true`** ; SCH actions v1 **`sch_actions_v1_family_green=true`** ; famille DRC v1 green ; checkpoint v2 **`mcp_v2_checkpoint_green=true`** (9 familles) ; CI **`ci_baseline_green=true`** (VerifyOnly)
- **Recommandation R136** : **`validate_footprints=true`** — audit validation footprint library depuis SCH si coût/fiabilite acceptable
- **Date** : 2026-06-06

## R136 — Audit `validate_footprints=true` pour `schematic_footprint_readiness_session`

- **Objectif** : auditer faisabilite, cout runtime et risques d activer **`validate_footprints=true`** — **sans** patch handler, callback, schema MCP, QA fonctionnelle, checkpoint v2 ni criteres green.
- **Contexte R135b** : tool read-only livre ; **`validate_footprints=true`** retourne **`footprint_validation_unavailable`** ; **`symbols_with_invalid_footprint_count=null`** ; **`sch_to_pcb_ready`** = presence footprint non vide uniquement.

### 1. Reutilisation validation R120

| Question | Conclusion audit |
|----------|------------------|
| **`KIFACE_TEST_FOOTPRINT_LINK` reutilisable ?** | **Oui** — export stable **`pcbnew.cpp`** (`testFootprintLink` ~234) et duplicate **`cvpcb.cpp`** ; signature **`int (*)( const wxString&, PROJECT* )`** ; codes **`KIFACE_TEST_FOOTPRINT_LINK_*`** dans **`kiface_ids.h`**. |
| **Accessible depuis readiness sans dependance nouvelle ?** | **Oui** — meme pattern que **`SchematicAssignFootprintSessionToolInvoke`** (~7642) : **`KIWAY_HOLDER`** → **`KiFACE( FACE_PCB )`** → **`IfaceOrAddress( KIFACE_TEST_FOOTPRINT_LINK )`** → appel avec **`&schFrame->Prj()`**. Aucun frame PCB ouvert requis ; adapter via **`PROJECT_PCB::FootprintLibAdapter( aProject )`**. |
| **Factorisation assign / readiness ?** | **Recommandee (R137)** — extraire helper local (ex. parse **`LIB_ID`**, appel link tester, map result → `{ ok, reason }`) pour eviter derive entre assign (echec tool entier) et readiness (agregat multi-symboles). **Non bloquant** pour faisabilite. |

**Alignement ERC** : **`ERC_TESTER::TestFootprintLinkIssues`** (`erc.cpp` ~1768) utilise le meme link tester + pre-check **`LIB_ID::Parse( footprint, true )`** pour identifiant invalide. Readiness R137 devrait reprendre **les deux** etapes (parse + link) par symbole.

**Ecart assign R120 vs readiness R135b** : assign normalise via **`LIB_ID::Parse(..., false)`** puis **`fpID.Format()`** avant test ; readiness lit le texte brut **`GetFootprintFieldText`** — R137 doit appliquer la **meme normalisation** avant **`KIFACE_TEST_FOOTPRINT_LINK`**.

### 2. Cout runtime (estimation audit, sans benchmark R136)

| Scenario | Comportement attendu | Ordre de grandeur |
|----------|---------------------|-------------------|
| **2 symboles, meme lib** (ex. **`issue19090`**) | 1× **`LoadOne`** (si lib froide) + 2× **`FootprintExists`** | **~10–200 ms** selon cache disque / preload |
| **10 symboles, 2 libs uniques** | 2× **`LoadOne`** + 10× **`FootprintExists`** | **~20–400 ms** |
| **50 symboles, ~10 libs uniques** | 10× **`LoadOne`** + 50× **`FootprintExists`** | **~100 ms–2 s** (borne **`max_symbols`**) |

**`testFootprintLink`** (`pcbnew.cpp`) : **`LoadOne( libName )`** si lib non chargee — **cout dominant** ; post-charge **`FootprintExists`** ~ O(1) index.

| Question | Recommandation |
|----------|----------------|
| **`max_symbols` borne la validation ?** | **Oui** — valider uniquement les symboles comptes / emis (meme boucle, meme plafond 1–500). |
| **Cache local par footprint unique ?** | **Oui (R137)** — map **`qualified_fp → link_result`** dans l appel tool ; gain fort si N symboles partagent peu de libs (BOM typique). |

**Flakiness** : variance **`LoadOne`** ; dependance **`fp-lib-table`** projet/global (voir ci-dessous) ; pas de frame PCB requis mais **config lib identique** a R120.

### 3. Semantique `sch_to_pcb_ready` — options A / B / C

| Option | Description | Avantages | Inconvenients |
|--------|-------------|-----------|---------------|
| **A** | **`sch_to_pcb_ready`** = tous footprints **present** (non vide) | Stable R135b ; snapshot / probes inchanges | Ne distingue pas lib invalide |
| **B** | Si **`validate_footprints=true`**, **`sch_to_pcb_ready`** = present **et** valide | Un seul bool | **Breaking** : meme champ, sens different selon entree ; casse QA R135b / snapshot opt-in |
| **C** | **`sch_to_pcb_ready`** (presence) + **`sch_to_pcb_ready_validated`** (null si pas de validation) | Retro-compatible ; IA lit le bon signal selon mode | Deux champs a documenter |

**Option retenue (R136 audit → R137 impl)** : **Option C**.

- **`sch_to_pcb_ready`** : inchange (**presence**, R135b).
- **`sch_to_pcb_ready_validated`** : **`null`** si **`validate_footprints=false`** ; si **`true`** et validation OK : **`symbols_count > 0 && invalid == 0 && unparseable == 0`** (symboles scannes non-power avec footprint non vide).

### 4. Contrat sortie propose R137

**Entree** : inchangee R135b ; snapshot **`include_footprints_summary`** garde **`validate_footprints=false`** par defaut (ajout opt-in **`include_footprints_validation`** ou param **`footprints_validate=false`** en R137 — a trancher en impl).

**Sortie additive** ( **`validate_footprints=false`** → champs validation **`null`** / **`false`** selon table ) :

```json
{
  "footprint_validation_available": true,
  "footprint_validation_performed": false,
  "symbols_with_invalid_footprint_count": null,
  "symbols_with_unvalidated_footprint_count": 0,
  "sch_to_pcb_ready": true,
  "sch_to_pcb_ready_validated": null,
  "invalid_footprints": []
}
```

Avec **`validate_footprints=true`** (exemple) :

```json
{
  "footprint_validation_available": true,
  "footprint_validation_performed": true,
  "symbols_with_invalid_footprint_count": 1,
  "symbols_with_unvalidated_footprint_count": null,
  "sch_to_pcb_ready": true,
  "sch_to_pcb_ready_validated": false,
  "invalid_footprints": [
    {
      "symbol_id": "...",
      "reference": "R2",
      "footprint": "qa_resistors:missing_fp",
      "reason": "footprint_not_found_in_library"
    }
  ]
}
```

**`symbols_with_unvalidated_footprint_count`** : nombre de symboles avec footprint non vide **non** re-valides quand **`validate_footprints=false`** (diagnostic IA) ; **`null`** quand validation performed.

**Reasons `invalid_footprints[].reason`** (alignees R120 / ERC) : **`invalid_footprint_identifier`**, **`library_not_loadable`**, **`library_not_enabled`**, **`footprint_not_found_in_library`**, **`footprint_empty`** (deja compte **`without`**, pas liste invalid).

**Blockers tool** : retirer **`footprint_validation_unavailable`** global ; conserver **`pcb_kiface_unavailable`** / **`footprint_link_tester_unavailable`** seulement si **`validate_footprints=true`** et infra absente — retour **`ok=true`** avec **`footprint_validation_available=false`**, counts **`null`**, diagnostic explicite (readiness = lecture, pas mutation).

### 5. QA proposee R137

| Cas | Fixture / setup | Attendu |
|-----|-----------------|--------|
| **`validate_footprints=false`** | **`issue19090`** (R135b) | Comportement R135b **inchange** ; invalid count **`null`** |
| **Validation OK** | **`issue19090`** + **`fp-lib-table`** QA (**pattern R120** : alias **`Resistor_SMD`** → **`qa/data/libraries/Resistor_SMD.pretty`**) | **`validate_footprints=true`** ; **`symbols_with_invalid_footprint_count=0`** ; **`sch_to_pcb_ready_validated=true`** |
| **Validation KO** | SCH derivee ou champ footprint **`qa_resistors:missing_fp`** / identifiant invalide | **`invalid_footprint_count >= 1`** ; entree **`invalid_footprints[]`** bornee |
| **Snapshot opt-in** | **`include_footprints_summary=true`** sans validation snapshot | Non-regression R135b |
| **Checkpoint MCP v2** | — | **Inchange** (9 familles) |

**Note fixture R135b vs R137** : probe readiness R135b **n injecte pas** **`fp-lib-table`** (contrairement assign R120) — presence-only green ; validation R137 **exige** setup lib explicite (comme **`targeted_schematic_assign_footprint_probe.ps1`**).

**Extension probe** : **`targeted_schematic_footprint_readiness_probe.ps1`** — cas opt-in validation ; summary keys **`footprint_validation_*`**, **`invalid_footprint_count_ok`**.

### 6. Risques

- **Dependance config lib** : sans **`fp-lib-table`**, footprints presents deviennent « invalid » — faux positifs IA si non documente.
- **Surcharge `LoadOne`** : schemas grands + **`validate_footprints=true`** + pas de cache → latence IPC ; mitiger cache + **`max_symbols`**.
- **Semantique snapshot** : **`assign_missing_footprints`** base sur **`without_count`** aujourd hui ; R137 ajouter action **`fix_invalid_footprints`** si **`invalid_count > 0`** (snapshot R137, hors R136).
- **Ecran courant seulement** : meme limite R135b ; validation multi-feuilles = mission ulterieure.

### Choix retenu (R136)

- **Faisabilite** : **oui** — reutiliser **`KIFACE_TEST_FOOTPRINT_LINK`** / pattern R120 sans nouvelle dependance.
- **Factorisation** : **oui, recommandee** en R137 (helper partage parse + test).
- **Semantique** : **Option C** (deux champs readiness).
- **Cache footprint unique** : **oui** en R137.
- **Impl** : mission **R137** ; R136 = audit documentaire uniquement.

- **Date** : 2026-06-06

## R137 — `validate_footprints=true` dans `schematic_footprint_readiness_session`

- **Objectif** : activer validation library read-only via **`KIFACE_TEST_FOOTPRINT_LINK`** (pattern R120) ; cache footprint unique par appel ; **Option C** sémantique.
- **Handler** (`api_handler_sch.cpp`) :
  - Helpers **`mcpEvaluateFootprintLink`** / **`mcpFpLinkInvalidReason`**
  - **`validate_footprints=false`** : comportement R135b inchangé ; **`sch_to_pcb_ready_validated=null`**
  - **`validate_footprints=true`** : validation + **`invalid_footprints[]`** borné ; champs **`footprint_validation_*`**, **`symbols_with_invalid_footprint_count`**, **`sch_to_pcb_ready_validated`**
  - **`sch_to_pcb_ready`** inchangé (presence non vide)
  - Cache **`std::map`** clé footprint trimé ; diagnostics **`validation_cache_hits`**
- **Snapshot** (`mcp_executor.cpp`) : entrée **`footprints_validate=false`** ; relay vers tool ; champs readiness **`symbols_with_invalid_footprint_count`**, **`sch_to_pcb_ready_validated`**, **`footprint_validation_performed`** ; action **`fix_invalid_footprints`**
- **QA** : probes footprint + snapshot readiness étendus ; **`ipc_smoke.cpp`** lignes greppables validation
- **Hors périmètre respecté** : checkpoint v2 ; critères green ; **`schematic_assign_footprint_session`** inchangé ; pas de mutation auto
- **QA R137 observée** (2026-06-06) : footprint readiness **`schematic_footprint_readiness_green=true`** (validate happy/invalid) ; snapshot readiness **`project_context_snapshot_readiness_green=true`** (opt-in validate + **`fix_invalid_footprints`**) ; paths/DRC snapshot green ; SCH actions v1 green ; DRC v1 green ; checkpoint v2 **`mcp_v2_checkpoint_green=true`** (9 familles) après correction **`changed=true`** post-mutation dans **`SchematicSetSymbolFieldSessionToolInvoke`** (régression **`makeResult(true,false)`** → **`makeResult(true,true)`**) ; CI **`ci_baseline_green=true`** (VerifyOnly)
- **Recommandation R138** : validation multi-feuilles hiérarchiques ; promotion **`footprints_validate=true`** snapshot si besoin produit ; factorisation helper assign/readiness si dette technique
- **Date** : 2026-06-06

## R138 — Mini-release « Project Readiness Summary V1 complet » + audit isolation QA SCH

- **Objectif** : clôturer **R135a / R135b / R137** par une mini-release documentaire ; auditer stabilité QA autour de **`issue19090`** ; **sans** impl multi-feuilles.
- **Statut V1 « complet »** :
  - **`project_readiness_summary`** (R135a) : métadonnées dérivées + **`recommended_next_actions`** / **`recommended_tools`**
  - **`schematic_footprint_readiness_session`** (R135b/R137) : scan écran courant ; power symbols exclus ; validation library opt-in
  - Snapshot opt-in : **`include_footprints_summary`** + **`footprints_validate=false`** par défaut
  - Checkpoint MCP v2 : **9 familles inchangées** ; critères green **non modifiés**
- **Sémantique officielle (Option C, R137)** :
  - **`sch_to_pcb_ready`** : tous les symboles **scannés** (non-power, écran courant) ont un footprint **non vide**
  - **`sch_to_pcb_ready_validated`** : **`null`** si **`validate_footprints=false`** ; si **`true`** : tous scannés ont footprint non vide **ET** valide en librairie (`symbols_with_invalid_footprint_count == 0`)
  - **`symbols_with_unvalidated_footprint_count`** : renseigné si validation non demandée (compteur symboles avec footprint non vide mais non validés)
- **Champs validation R137** : `footprint_validation_available`, `footprint_validation_performed`, `symbols_with_invalid_footprint_count`, `invalid_footprints[]`, champs symbole `footprint_validated` / `footprint_valid` / `footprint_validation_status`
- **Cache** : map footprint qualifié normalisé → résultat link test ; **par appel tool uniquement** (pas global)
- **Recommended actions snapshot** :
  - **`assign_missing_footprints`** : `symbols_without_footprint_count > 0`
  - **`fix_invalid_footprints`** : validation effectuée et `symbols_with_invalid_footprint_count > 0` → tool **`schematic_assign_footprint_session`**
  - **`run_drc_detail`** : DRC engine run avec violations
- **Limites V1 documentées** : écran courant uniquement ; pas scan hiérarchique multi-feuilles ; validation opt-in ; pas de mutation auto
- **Non-régression SCH actions (R137)** : **`SchematicSetSymbolFieldSessionToolInvoke`** renvoyait **`changed=false`** après mutation réussie (`makeResult(true,false)` au lieu de **`makeResult(true,true)`**) — faisait échouer **`targeted_schematic_set_symbol_field_probe.ps1`** / famille **`sch_symbol_editing_v2`** en checkpoint ; **corrigé R137** ; documenté R138 comme fix contractuel **`changed`** post-mutation
- **Audit isolation QA `issue19090`** :

| Probe / runner | Mutations | Restore | Stop-KiCad | Risque ordre |
|--------------|-----------|---------|------------|--------------|
| **`targeted_schematic_footprint_readiness_probe.ps1`** | FOOTPRINT temporaire (`set_symbol_field`) ; **`fp-lib-table`** projet | restore **`R_0805_2012Metric`** ; Stop-KiCad fin | début + fin | moyen si KiCad réutilisé sans stop |
| **`targeted_project_context_snapshot_readiness_probe.ps1`** | idem + bloc validate invalid | restore footprints ; Stop-KiCad | début + fin + entre blocs | moyen |
| **`targeted_schematic_set_symbol_field_probe.ps1`** | DATASHEET **`mcp-qa-datasheet`** | restore **`""`** | **non** en début (ping KiCad existant) | **élevé** si session polluée (idempotent → `changed=false` avant fix R137) |
| **`run_sch_actions_v1_targeted_probes.ps1`** | value/field/label/assign sur **`issue19090`** | restore par probe enfant | dépend enfant | enchaînement 4 probes même fixture |
| **Familles checkpoint v2** (geometry/authoring/wiring/symbol-editing) | wires, labels, symboles, champs sur **`issue19090`** | restore par probe | **non** entre familles (KiCad partagé) | **élevé** sans teardown |
| Probes lecture seule (library search, net inspect, …) | non | N/A | variable | faible |

- **Recommandation isolation QA (R138)** :
  1. **`Stop-KiCadProcesses`** (+ optionnel **`Clear-IpcSockets`**) **avant** checkpoint MCP v2 et entre suites lourdes mutantes
  2. Ne pas enchaîner readiness mutante + checkpoint sans redémarrage KiCad
  3. **`fp-lib-table`** dans **`issue19090/`** : effet disque toléré QA (alias lib Resistor_SMD) ; pas de backup aujourd’hui
  4. Teardown commun **`Stop-KiCadProcesses`** suffisant en R138 ; refactor harness **`Reset-Issue19090Session`** = **R139+** si besoin
- **Date** : 2026-06-06

## R139 — Factorisation helper validation footprint SCH

- **Objectif** : factoriser **`KIFACE_TEST_FOOTPRINT_LINK`** partagé entre **`schematic_assign_footprint_session`** et **`schematic_footprint_readiness_session`** ; **aucun** changement contrat MCP / comportement runtime.
- **Helper** (`api_handler_sch.cpp`, statique local) :
  - **`MCP_FOOTPRINT_LINK_EVAL`** (+ alias **`MCP_FP_LINK_EVAL`**) : `status`, `qualifiedFootprint`, `libraryNickname`, `rawLinkResult`
  - **`mcpResolveFootprintLinkTester( KIWAY& )`** : `FACE_PCB` + `KIFACE_TEST_FOOTPRINT_LINK`
  - **`evaluateFootprintLinkForSchMcp( TEST_FOOTPRINT_LINK_FN, PROJECT&, footprint )`** : parse **`LIB_ID::Parse(..., false)`**, normalise **`fpID.Format()`**, link test (point d’entrée unique assign + readiness)
  - **`mcpFpLinkInvalidReason( MCP_FP_LINK_STATUS )`** : mapping readiness inchangé
- **Mappings préservés** :
  - **assign** : `invalid_footprint_identifier` (parse précoce) ; `library_not_loadable:*` ; `footprint_not_found_in_library:*` ; `footprint_link_test_failed` + `link_result=`
  - **readiness** : `valid`, `invalid_footprint_identifier`, `library_not_loadable`, `library_not_enabled`, `footprint_not_found`, `validation_error`
- **Cache readiness R137** : **`std::map`** par appel dans **`SchematicFootprintReadinessSessionToolInvoke`** ; helper **sans** cache global
- **Hors périmètre respecté** : schémas MCP ; callbacks ; checkpoint v2 ; critères green ; multi-feuilles ; sémantique **`sch_to_pcb_ready`** / **`sch_to_pcb_ready_validated`**
- **QA R139 observée** (2026-06-06) : build OK ; assign footprint green ; footprint readiness green ; snapshot readiness green ; SCH actions v1 green ; DRC v1 green ; checkpoint v2 **`mcp_v2_checkpoint_green=true`** (9 familles) ; CI **`ci_baseline_green=true`** (VerifyOnly)
- **Recommandation R140** : validation footprint **multi-feuilles hiérarchiques** ; ou harness teardown **`issue19090`** si pollution session réapparaît
- **Date** : 2026-06-06

## R140 — Audit multi-feuilles pour `schematic_footprint_readiness_session`

- **Objectif** : auditer faisabilité scan hiérarchique **sans implémentation** ; recommander design R141.
- **État actuel** (`SchematicFootprintReadinessSessionToolInvoke`, ~L7356+) :
  - Parcours **`schFrame->GetScreen()->Items()`** — écran **courant** uniquement
  - Footprint : **`GetFootprintFieldText( false, &currentSheet, false )`** — avec **`aResolve=false`**, le **`SCH_SHEET_PATH`** est **ignoré** (`GetField(FOOTPRINT)->GetText()` brut) ; pas de résolution instance hiérarchique
  - Power symbols exclus via **`IsPower()`**
  - Validation : **`evaluateFootprintLinkForSchMcp`** + cache local R137

### 1. APIs hiérarchiques KiCad identifiées

| API | Rôle | Usage KiCad existant |
|-----|------|----------------------|
| **`SCHEMATIC::BuildSheetListSortedByPageNumbers()`** | Liste aplatie de **`SCH_SHEET_PATH`** (toutes top-level roots) | **`ERC_TESTER`** (`erc.h` L58), ERC footprint link (`erc.cpp` L1779) |
| **`SCHEMATIC::Hierarchy()`** | Cache **`m_hierarchy`** (alias sheet list) | Assign footprints GUI (`assign_footprints.cpp` L53), netlist SPICE |
| **`SCHEMATIC::RefreshHierarchy()`** | Rebuild cache hiérarchie | Avant parcours si stale |
| **`SCH_SHEET_PATH::LastScreen()`** | **`SCH_SCREEN*`** de la feuille cible | ERC, netlist exporters |
| **`SCH_SHEET_PATH::PathHumanReadable()`** | Chemin lisible `/ROOT_UUID/...` | Affichage / QA |
| **`SCH_SHEET_PATH::GetSymbols( SCH_REFERENCE_LIST&, … )`** | Symboles **par instance** sur ce path | Annotation, éditeur |
| **`SCH_SHEET_LIST::GetSymbols( refs, includePower )`** | Tous symboles projet (flat instances) | **`assign_footprints.cpp`**, **`sch_editor_control.cpp`** |
| **`SCH_REFERENCE`** | `(symbol, sheetPath, ref, footprint)` résolus | Annotation / CvPcb |

**Pattern recommandé R141** (aligné ERC footprint + assign footprints) :

```cpp
SCH_SHEET_LIST sheets = schematic.BuildSheetListSortedByPageNumbers();
// ou: schematic.Hierarchy().GetSymbols( refs, false /* no power */ );

for( const SCH_SHEET_PATH& sheet : sheets )
    for( SCH_SYMBOL* on sheet.LastScreen() … )
        footprint = symbol->GetFootprintFieldText( true, &sheet, false );
```

Alternative **`Hierarchy().GetSymbols( refs, false )`** : une entrée **`SCH_REFERENCE`** par **instance** — évite double comptage écran vs path.

### 2. Lecture footprint et sheet path

| Question | Réponse audit |
|----------|---------------|
| **`GetFootprintFieldText` dépend du path ?** | **Oui si `aResolve=true`** → **`GetShownText( aPath, … )`** → **`GetFieldText( name, aPath, variant )`** avec données **`SCH_SYMBOL_INSTANCE`** |
| **Footprint variable par instance ?** | **Oui** — hiérarchie complexe : même **`SCH_SCREEN`** partagé, refs/footprints par **`m_instances`** / path |
| **API R141 recommandée** | **`GetFootprintFieldText( true, &sheetPath, false )`** (pattern ERC L1786) — **pas** `aResolve=false` |
| **Limite actuelle V1** | Même sur feuille courante, **`aResolve=false`** ignore instances → écart vs ERC si footprints instance-specific |

### 3. Comptage — ambiguïtés

| Stratégie | Verdict R141 |
|-----------|--------------|
| Symboles par définition (UUID unique écran) | **Inadapté** — sous-compte instances hiérarchiques |
| Symboles par instance (**`SCH_REFERENCE`**) | **Recommandé** — aligné BOM / PCB / assign footprints |
| Ignorer feuilles répétées | **Non** — chaque **`SCH_SHEET_PATH`** distinct = instance board distincte |
| Inclure feuilles répétées | **Oui** — **`BuildSheetListSortedByPageNumbers`** énumère chaque path |
| **Risque double comptage** | **Oui** si parcours **`SCH_SCREEN`** seul ou dédup par **`symbol_id`** seul sur écran partagé |

**Clé instance MCP future** : **`symbol_id` + `sheet_path`** (ou `instance_path` KIID) — **`symbol_id` seul ambigu** en hiérarchie complexe.

### 4. Options A / B / C

| Option | Description | Pour | Contre |
|--------|-------------|------|--------|
| **A** | Étendre tool existant : **`scope=current_sheet \| hierarchy`**, **`include_sheet_paths`** | Rétro-compat ; snapshot relay ; un seul tool | Contrat **`symbols[]`** enrichi ; doc sémantique |
| **B** | Nouveau tool **`schematic_hierarchy_footprint_readiness_session`** | Isole breaking change | Duplication ; snapshot double appel |
| **C** | Rester current-sheet V1 | Défaut inchangé | Ne résout pas multi-feuilles |

**Option retenue (R140 audit → R141 impl)** : **Option A**.

- **`scope=current_sheet`** (défaut) : comportement R135b–R139 **inchangé** (même boucle écran courant)
- **`scope=hierarchy`** : parcours **`BuildSheetListSortedByPageNumbers`** ou **`GetSymbols`**, footprint path-aware, champs sheet dans **`symbols[]`**

### 5. Contrat MCP proposé R141 (Option A)

**Entrées additifs** :

```json
{
  "scope": "current_sheet",
  "include_sheet_paths": false
}
```

| Valeur | Comportement |
|--------|--------------|
| **`scope=current_sheet`** | Inchangé R139 |
| **`scope=hierarchy`** | Toutes feuilles ; comptage instances |
| **`include_sheet_paths=true`** | Remplit **`sheet_path`**, **`sheet_name`**, **`instance_path`** par symbole |

**Sorties additifs (scope=hierarchy)** :

| Champ | Description |
|-------|-------------|
| **`scope`** | `"current_sheet"` \| `"hierarchy"` |
| **`sheets_count`** | Nombre de **`SCH_SHEET_PATH`** parcourus |
| **`symbols_instances_count`** | Instances non-power (alias sémantique de **`symbols_count`** en mode hierarchy) |
| **`symbols[].sheet_path`** | **`PathHumanReadable()`** ou KIID path |
| **`symbols[].sheet_name`** | Nom feuille |
| **`symbols[].instance_path`** | Identifiant instance stable (KIID path recommandé) |
| **`truncated`**, **`sch_to_pcb_ready`**, **`sch_to_pcb_ready_validated`** | Inchangés sémantiquement — appliqués au ** périmètre scanné** |

Snapshot : relay **`footprints_scope`** / **`scope`** vers tool (défaut **`current_sheet`**).

### 6. QA proposée R141

| Cas | Fixture / attendu |
|-----|-------------------|
| Non-régression **`scope=current_sheet`** | **`issue19090`** (existant) — green inchangé |
| Hiérarchie 2 feuilles | **`qa/data/eeschema/netlists/complex_hierarchy/complex_hierarchy.kicad_pro`** (existe) |
| Footprint manquant sous-feuille | Fixture dédiée minimale **`qa/data/eeschema/hierarchy/ipc_footprint_readiness_v1/`** (à créer R141) |
| **`validate_footprints=true`** | Pattern fp-lib-table R120 sur fixture hiérarchique |
| Feuille répétée (complex hierarchy) | **`complex_hierarchy`** — 2 instances même subsheet si présent |
| Snapshot opt-in **`scope=hierarchy`** | Extension **`targeted_project_context_snapshot_readiness_probe.ps1`** |
| Checkpoint MCP v2 | **Inchangé** (9 familles) |

### 7. Risques R141

1. **`aResolve=false` actuel** — corriger en même temps que hierarchy (au minimum utiliser path-aware sur feuille courante si instances)
2. **`symbol_id` non unique** — documenter clé composite ; ne pas casser clients v1
3. **Hiérarchie stale** — appeler **`RefreshHierarchy()`** si nécessaire avant parcours
4. **Performance** — cache footprint R137 reste par appel ; coût scan N feuilles acceptable read-only
5. **GUI SCH ouvert** — même contrainte qu’aujourd’hui ; pas de scan hors projet chargé

### 8. Hors périmètre R140 respecté

Aucun handler, callback, schéma MCP, script QA fonctionnel, checkpoint v2, critère green modifié.

- **Recommandation R141** : implémenter **Option A** ; parcours **`SCH_SHEET_LIST::GetSymbols`** ou boucle ERC ; **`GetFootprintFieldText(true, &path, false)`** ; fixture **`ipc_footprint_readiness_v1`** ; probe dédié ou extension footprint readiness probe
- **Date** : 2026-06-06

## R141 — `scope=hierarchy` dans `schematic_footprint_readiness_session`

- **Objectif** : étendre **`schematic_footprint_readiness_session`** avec **`scope=current_sheet|hierarchy`** (défaut **`current_sheet`**) et relay snapshot **`footprints_scope`** ; **`current_sheet` inchangé** R135b–R139.
- **Handler** (`api_handler_sch.cpp`, **`SchematicFootprintReadinessSessionToolInvoke`**) :
  - Entrées : **`scope`**, **`include_sheet_paths`** (défaut **`true`** si **`hierarchy`**, optionnel **`current_sheet`**)
  - **`current_sheet`** : boucle écran courant + **`GetFootprintFieldText(false, &currentSheet, false)`** (strict R139)
  - **`hierarchy`** : **`RefreshHierarchy()`** + **`Hierarchy().GetSymbols(refs, false)`** ; footprint **`GetFootprintFieldText(true, &sheetPath, false)`** ; comptage par instance (pas dédup UUID seul)
  - Sorties additifs : **`scope`**, **`sheets_count`**, **`symbols_instances_count`**, **`symbols[].sheet_path`**, **`sheet_name`**, **`instance_path`** si **`include_sheet_paths`**
  - Validation : helper **`evaluateFootprintLinkForSchMcp`** + cache par appel inchangé
- **Snapshot** (`mcp_executor.cpp`) : **`footprints_scope`** (défaut **`current_sheet`**) relayé ; **`buildFootprintsSummaryCompact`** + **`buildProjectReadinessSummary`** enrichis (**`scope`**, **`symbols_instances_count`**, **`footprints_scope`**)
- **Fixture QA** : **`qa/data/eeschema/hierarchy/ipc_footprint_readiness_v1/`** (Root + SubSheet ; 2 footprints valides + 1 manquant)
- **Probes** : extension **`targeted_schematic_footprint_readiness_probe.ps1`** + **`targeted_project_context_snapshot_readiness_probe.ps1`**
- **Hors périmètre respecté** : pas nouveau tool ; assign footprint / DRC / checkpoint v2 / critères green inchangés ; protobuf **`DocumentSpecifier`** non rouvert
- **QA R141 observée** (2026-06-06) : build OK ; **`scope=current_sheet`** non régressé (**`issue19090`**) ; **`scope=hierarchy`** opérationnel (fixture **`ipc_footprint_readiness_v1`**, 3 instances / 2 sheets) ; snapshot **`footprints_scope=hierarchy`** OK ; footprint readiness probe green ; snapshot readiness green ; paths/DRC green ; SCH actions v1 green ; DRC v1 green ; checkpoint v2 **`mcp_v2_checkpoint_green=true`** (9 familles) ; CI **`ci_baseline_green=true`** (VerifyOnly)
- **Recommandation R142** : assign footprint **`scope=hierarchy`** ou clé composite **`symbol_id+instance_path`** pour écriture instance-aware
- **Date** : 2026-06-06

## R142 — Audit assignation footprint instance-aware (hiérarchie SCH)

- **Objectif** : auditer faisabilité d’étendre **`schematic_assign_footprint_session`** pour cibler une instance hiérarchique détectée par readiness **`scope=hierarchy`** ; **aucune implémentation** R142.
- **Hors périmètre respecté** : handler, callbacks, schémas MCP, scripts QA fonctionnels, checkpoint v2, critères green — **inchangés**.

### 1. État actuel `SchematicAssignFootprintSessionToolInvoke` (~L7719+)

| Aspect | Comportement observé |
|--------|----------------------|
| **Résolution symbole** | Boucle **`schFrame->GetScreen()->Items()`** — **écran courant uniquement** |
| **Clé** | **`symbol_id`** (UUID **`SCH_SYMBOL::m_Uuid`**) — pas de **`sheet_path`** / **`instance_path`** |
| **Lecture footprint actuel** | **`GetField(FOOTPRINT)->GetText(&currentSheet, variant)`** — résolution via **`currentSheet`** (path-aware en lecture) |
| **Référence retournée** | **`GetRefProp()`** — texte brut écran, **pas** **`GetRef(&sheetPath)`** |
| **Écriture** | **`SetFootprintFieldText(qualifiedFp)`** → **`FOOTPRINT` field `SetText` global** (sans path) |
| **Readback** | **`GetText(&currentSheet, variant)`** — valide vs feuille **courante** seulement |
| **Commit** | **`commit.Modify(symbol, screen, NO_RECURSE)`** sur écran courant |

**Conséquences** :
- Symbole sur sous-feuille **non trouvé** si l’éditeur est sur root → **`symbol_not_found`**.
- Même **`symbol_id`** sur écran partagé (hiérarchie complexe) : **aucune** désambiguïsation.
- **`SetFootprintFieldText`** peut affecter **toutes les instances** partageant le même objet **`SCH_SYMBOL`** sur un **`SCH_SCREEN`** commun.

**Alignement readiness R141** : readiness hierarchy lit via **`Hierarchy().GetSymbols`** + **`GetFootprintFieldText(true, &sheetPath, false)`** ; assign reste **désaligné** (lookup écran + write global).

### 2. Sémantique KiCad footprint / instance

| Question | Réponse audit |
|----------|---------------|
| Stockage footprint | **Deux niveaux** : (1) texte champ **`SCH_FIELD::FOOTPRINT`** sur **`SCH_SYMBOL`** ; (2) persistance fichier **`(symbol_instances … (footprint …))`** / legacy **`SCH_SYMBOL_INSTANCE::m_Footprint`** |
| **`GetFootprintFieldText(true, &path)`** | **`GetShownText(path)`** → résolution affichage ; pour footprint sans variant, **`getUnescapedText`** retombe sur texte **global** du champ sauf variants |
| **`SetFootprintFieldText`** | **`GetField(FOOTPRINT)->SetText(fp)`** — **global**, pas path |
| **`SCH_FIELD::SetText(text, &path, variant)`** | Délègue **`SCH_SYMBOL::SetFieldText`** ; pour **FOOTPRINT** + variant vide → **`field->SetText(text)` global** si différent du défaut |
| **`schematic_set_symbol_field_session`** | Même lookup écran courant ; mutation **`field->SetText(newValue, &currentSheet, variant)`** — footprint Footprint = **global** en pratique |
| Pattern GUI **`assign_footprints.cpp`** | **`Hierarchy().GetSymbols`** pour lister ; écriture **`footprintField->SetText(footprint)`** sans path (commentaire CvPcb : parfois **toutes instances**) |
| **`UpdateSymbolInstanceData`** | Au load : applique **`symbol_instances`** du fichier via **`SetFootprintFieldText`** par path (écrasement séquentiel du champ global) |
| API path-aware fiable lecture | **`GetFootprintFieldText(true, &SCH_SHEET_PATH, false)`** ou **`SCH_REFERENCE::GetFootprint()`** après **`GetSymbols`** |
| API path-aware fiable écriture | **Pas d’API publique claire « set footprint for instance only »** ; R143 devra combiner **`SCH_REFERENCE` + target `SCH_SHEET_PATH` + commit sur `LastScreen()`** et mettre à jour données instance ( **`m_instances` / `m_symbolInstances`** ) — **spike R143 requis** sur écran partagé |

**Risque mutation toutes instances** : **oui** si seul **`SetFootprintFieldText`** / **`SetText` global** est utilisé sur symbole dont le **`SCH_SCREEN`** est instancié plusieurs fois.

### 3. Options de clé de ciblage (A/B/C/D)

| Option | Clé | Pour | Contre | Verdict R143 |
|--------|-----|------|--------|--------------|
| **A** | **`symbol_id` seul** | Rétro-compat **`current_sheet`** ; probe **`issue19090`** inchangé | Échec cross-sheet ; ambiguïté écran partagé ; désaligné readiness | **Garder défaut `scope=current_sheet` uniquement** |
| **B** | **`symbol_id + sheet_path`** | Lisible (`ipc_footprint_readiness_v1/SubSheet`) ; proche UX | **`sheet_path`** = **`PathHumanReadable`** (pas préfixe `/`) ; parsing / collision noms | **Secondaire** (validation croisée) |
| **C** | **`instance_path`** (KIID) | **Aligné readiness R141** (`PathAsString()+symbolUuid`) ; stable ; match **`SCH_SYMBOL_INSTANCE::m_Path`** | Clients doivent copier depuis readiness | **Recommandé primaire** |
| **D** | **`reference + sheet_path`** | Intuitif métier | Ref **non unique** (multi-unit, feuilles répétées, même subsheet) | **Non recommandé seul** |

**Option retenue R143** : **C + garde-fous** — **`instance_path` obligatoire** si **`scope=hierarchy`** (ou si **`symbol_id`** matche **>1** instance dans **`Hierarchy().GetSymbols`**) ; **`sheet_path` optionnel** pour validation ; défaut **`scope=current_sheet`** = option **A** inchangée.

**Résolution proposée R143** :
1. **`RefreshHierarchy()`** + **`Hierarchy().GetSymbols(refs, false)`**
2. Filtrer **`ref.GetSymbol()->m_Uuid == symbol_id`**
3. Si **`instance_path`** : parser **`KIID_PATH`** (= readiness format) ; match **`ref.GetSheetPath().PathAsString() + ref.GetSymbol()->m_Uuid.AsString()`** ou équivalent **`m_Path`**
4. Si une seule instance après filtre → OK ; si 0 → **`symbol_instance_not_found`** ; si >1 → **`ambiguous_symbol_id`**
5. Cible : **`SCH_REFERENCE`** retenu → **`targetSheet = ref.GetSheetPath()`**, **`symbol = ref.GetSymbol()`**, **`screen = targetSheet.LastScreen()`**

### 4. Contrat MCP proposé R143

**Entrées additifs** (défauts = comportement actuel) :

```json
{
  "scope": "current_sheet",
  "sheet_path": "ipc_footprint_readiness_v1/SubSheet",
  "instance_path": "/a1000003-0003-4003-8003-000000000003/a1000005-0005-4005-8005-000000000005"
}
```

| Paramètre | Règle |
|-----------|--------|
| **`scope=current_sheet`** | Comportement R135–R141 assign **strictement inchangé** |
| **`scope=hierarchy`** | Résolution via **`Hierarchy().GetSymbols`** ; **`instance_path` requis** sauf unicité prouvée |
| **`sheet_path`** | Optionnel ; si présent, doit matcher **`PathHumanReadable(false,true)`** de la cible |
| **`expected_current_footprint`** | Lire via **`GetFootprintFieldText(true, &targetSheet, false)`** en mode hierarchy |

**Sorties additifs** :

| Champ | Description |
|-------|-------------|
| **`scope`** | `"current_sheet"` \| `"hierarchy"` |
| **`sheet_path`**, **`instance_path`** | Cible effective |
| **`instance_targeted`** | `true` si mutation instance explicite |
| **`ambiguous_symbol_id`** | `true` si plusieurs instances sans sélecteur |
| **`affected_instances_count`** | `1` cible ; `>1` si fallback global documenté (à éviter) |
| **`old_footprint`**, **`new_footprint`**, **`changed`**, **`exact_blocker_if_any`** | Inchangés sémantiquement |

### 5. Blockers R143 proposés

| Blocker | Cas |
|---------|-----|
| **`invalid_scope`** | Valeur hors enum |
| **`missing_instance_selector`** | **`scope=hierarchy`** sans **`instance_path`** et **`symbol_id`** ambigu |
| **`instance_path_not_found`** | **`instance_path`** ne matche aucun **`SCH_REFERENCE`** |
| **`sheet_path_not_found`** | **`sheet_path`** fourni mais ne matche pas la cible |
| **`sheet_path_mismatch`** | **`instance_path`** + **`sheet_path`** incohérents |
| **`ambiguous_symbol_id`** | Plusieurs instances pour **`symbol_id`** sans **`instance_path`** |
| **`symbol_instance_not_found`** | Aucune instance après filtre |
| *existants* | **`symbol_not_found`**, **`invalid_symbol_id`**, **`footprint_already_assigned`**, **`expected_current_footprint_mismatch`**, validation library, etc. — **mappings R139 préservés** |

### 6. QA R143 proposée

| Cas | Fixture / attendu |
|-----|-------------------|
| Non-régression **`scope=current_sheet`** | **`issue19090`** + **`targeted_schematic_assign_footprint_probe.ps1`** inchangé |
| Dry-run hierarchy | **`ipc_footprint_readiness_v1`** : readiness → **`instance_path`** R2 sans FP → assign dry-run |
| Mutation ciblée | Assign FP sur instance sub **`R2`** seulement |
| Readback | **`schematic_footprint_readiness_session scope=hierarchy`** : **`symbols_without_footprint_count`** −1 ; root **`R1`** inchangé |
| Ambiguïté | **`symbol_id` seul + scope=hierarchy** sur fixture à écran partagé (**`complex_hierarchy`**) → **`ambiguous_symbol_id`** |
| Restore | Remettre footprint / vide ; **`Stop-KiCadProcesses`** entre suites |
| Checkpoint MCP v2 | **Inchangé** (9 familles) |

### 7. Risques R143

1. **Écriture KiCad** — **`SetFootprintFieldText` insuffisant** pour écran partagé ; spike **`m_symbolInstances` / `m_instances`** obligatoire avant impl.
2. **Désalignement read/write** — readiness lit path-aware ; assign doit utiliser **même path** pour **`expected_current_footprint`** et readback.
3. **Feuille courante vs cible** — commit sur **`targetSheet.LastScreen()`**, pas **`GetScreen()`** courant.
4. **Référence sortie** — utiliser **`ref.GetRef()`** / **`GetRef(&targetSheet)`**, pas **`GetRefProp()`**.
5. **Probes existants** — extension **nouvelle section** assign hierarchy ; ne pas casser green **`current_sheet`**.

- **Recommandation R143** : implémenter **`scope=current_sheet|hierarchy`** sur **`schematic_assign_footprint_session`** ; clé **`instance_path`** (Option **C**) ; résolution **`Hierarchy().GetSymbols`** ; lecture **`GetFootprintFieldText(true, &targetSheet, false)`** ; écriture instance-aware (spike KiCad) ; QA **`ipc_footprint_readiness_v1`** + ambiguïté **`complex_hierarchy`**
- **Date** : 2026-06-06

## R143 — Spike + `scope=hierarchy` prudent dans `schematic_assign_footprint_session`

### Spike instance-write (obligatoire)

| Question spike | Résultat |
|----------------|----------|
| **`instance_write_supported`** | **`false`** (variant vide / défaut projet) |
| Stockage fichier | **`(instances … (path …))`** pour ref/unit ; footprint = **`property "Footprint"`** sur **`SCH_SYMBOL`** (global par objet symbole écran) ; legacy **`symbol_instances (footprint …)`** / **`SCH_SYMBOL_INSTANCE::m_Footprint`** marqué « do not use » |
| **`SetFootprintFieldText`** | **`GetField(FOOTPRINT)->SetText(fp)`** — **global**, affecte toutes instances partageant le même **`SCH_SYMBOL`** sur un **`SCH_SCREEN`** |
| **`SCH_FIELD::SetText(text, &path, variant)`** | Délègue **`SetFieldText`** ; FOOTPRINT + variant vide → **`field->SetText(text)`** global (`sch_symbol.cpp` L741–748) |
| **`SetValueFieldText(value, &path, variant)`** | Pattern instance via **`m_Variants`** si variant non vide — **pas d’équivalent public pour FOOTPRINT** |
| Readback path-aware | **`GetFootprintFieldText(true, &sheetPath, false)`** — OK (aligné readiness R141) |
| Preuve non-régression autre instance | **`SetFootprintFieldText`** modifierait le champ global → **risque toutes instances** sur écran partagé |
| Risques | Mutation approximative interdite ; variant-only write non aligné readiness défaut ; **`UpdateSymbolInstanceData`** legacy écrase global au load |

**API d’écriture instance retenue** : **aucune** (blocker **`hierarchy_instance_write_unsupported`**).

### Implémentation R143

- **Handler** (`api_handler_sch.cpp`, **`SchematicAssignFootprintSessionToolInvoke`**) :
  - Entrées : **`scope`** (défaut **`current_sheet`**), **`instance_path`**, **`sheet_path`** optionnel
  - **`current_sheet`** : chemin code **strictement inchangé** R135–R142
  - **`hierarchy`** : **`RefreshHierarchy()`** + **`GetSymbols`** ; résolution **`instance_path`** (= **`PathAsString()+symbolUuid`**) ; validation **`symbol_id`** cohérent ; lecture **`GetFootprintFieldText(true, &targetSheet, false)`**
  - **Mutation** : si écriture instance-safe absente → **`hierarchy_instance_write_unsupported`**, **`ok=false`**, **aucune mutation**
  - **Dry-run hierarchy** : **`ok=true`**, preview **`old_footprint`/`new_footprint`/`changed`**, **`instance_targeted=false`**, **`exact_blocker_if_any=N/A`**
  - Blockers ajoutés : **`invalid_scope`**, **`missing_instance_selector`**, **`instance_path_not_found`**, **`sheet_path_not_found`**, **`symbol_instance_not_found`**, **`symbol_instance_mismatch`**, **`ambiguous_symbol_id`**, **`hierarchy_instance_write_unsupported`**
- **Schéma MCP** (`mcp_executor.cpp`) : propriétés **`scope`**, **`instance_path`**, **`sheet_path`**
- **ipc_smoke** : lignes **`assign_footprint_*`** greppables
- **QA** : extension **`targeted_schematic_assign_footprint_probe.ps1`** — non-régression **`issue19090`** + hierarchy dry-run / unsupported / négatifs sur **`ipc_footprint_readiness_v1`**

### Résultat QA attendu

- Build OK ; assign probe green ; readiness / snapshot / sch_actions / DRC / checkpoint MCP v2 / CI VerifyOnly green
- **`scope=hierarchy`** exposé mais **mutation bloquée** tant que KiCad n’offre pas d’API FOOTPRINT instance-safe (variant vide)

- **Recommandation R144** : spike écriture via **`SCH_SYMBOL_INSTANCE::m_Variants`** + variant projet explicite **ou** contribution upstream KiCad **`SetFootprintFieldText(value, &path, variant)`** ; fixture **`complex_hierarchy_shared`** pour preuve isolation ; ne pas activer mutation hierarchy sans test multi-instance même écran
- **Date** : 2026-06-05

## R144 — Mini-release « Hierarchical Assign Footprint V1 safe-limited »

### Problématique

Comment exposer **`scope=hierarchy`** sur **`schematic_assign_footprint_session`** (R141–R143) **sans** risquer une mutation globale dangereuse sur symboles partageant un **`SCH_SCREEN`** ?

### Choix retenu

**Officialiser un mode hierarchy safe-limited** : résolution + dry-run + diagnostic **supportés** ; **mutation hierarchy volontairement bloquée** via **`hierarchy_instance_write_unsupported`**.

| Mode | Mutation | Diagnostic |
|------|----------|------------|
| **`scope=current_sheet`** (défaut) | **Oui** — chemin R121 inchangé (`SetFootprintFieldText` sur écran courant) | standard |
| **`scope=hierarchy`** + **`dry_run=true`** | **Non** — preview `old_footprint` / `new_footprint` / `changed` | **Oui** |
| **`scope=hierarchy`** + **`dry_run=false`** | **Non** — blocker **`hierarchy_instance_write_unsupported`**, **`ok=false`**, zéro mutation | résolution effectuée avant refus |

### Raison

Spike R143 : **`SetFootprintFieldText`** écrit **globalement** ; aucune API publique KiCad instance-safe identifiée pour FOOTPRINT (variant vide). Pas de hack **`symbol_instances`** / **`m_Variants`**.

### Documentation livrée (R144)

- **`MCP_V1_OFFICIAL_MANIFEST.md`** : statut safe-limited + sémantique blockers hierarchy
- **`MCP_KICAD_USER_DEV_GUIDE.md`** : workflow IA recommandé (snapshot → readiness → dry-run → fallback)
- **`README_IPC_QA_BASELINE.md`** : comportement volontairement safe
- **`MCP_V1_CHECKPOINT_RELEASE_NOTE.md`** : entrée mini-release R144
- **`MCP_TOOL_MAP_R1.tsv`** : tag **`R144_safe_limited`**

### QA R144 (non-régression courte)

- **`targeted_schematic_assign_footprint_probe.ps1`**
- **`targeted_schematic_footprint_readiness_probe.ps1`**
- **`targeted_project_context_snapshot_readiness_probe.ps1`**
- **`run_mcp_v2_checkpoint.ps1`** (9 familles)
- **`run_mcp_v2_ci_baseline.ps1 -VerifyOnly`**

**Aucun handler, schéma MCP, probe fonctionnelle, critère green modifié.**

### Prochaine piste (R145+)

- Spike upstream KiCad : **`SetFootprintFieldText(value, &sheetPath, variant)`** ou API dédiée instance FOOTPRINT
- Fixture **`complex_hierarchy_shared`** pour preuve isolation multi-instance même écran
- Workflow client : ouvrir feuille cible + **`scope=current_sheet`** uniquement si symbole non partagé

- **Date** : 2026-06-06

## R145 — Spike écriture footprint instance-safe + fixture multi-instance

### Objectif

Prouver ou invalider une écriture footprint **instance-safe** dans une hiérarchie KiCad, **sans** activer la mutation hierarchy dans **`schematic_assign_footprint_session`**.

### Fixture

**`qa/data/eeschema/hierarchy/ipc_footprint_instance_write_spike/`**

| Élément | Détail |
|---------|--------|
| Root | **`ipc_footprint_instance_write_spike.kicad_sch`** — feuilles **InstA** / **InstB** |
| Sub partagé | **`sub_shared.kicad_sch`** — un **`Device:R`** UUID **`c1000005-0005-4005-8005-000000000005`** |
| Instances | **R1** (InstA) et **R2** (InstB) — **`instance_path`** distincts, symbole logique partagé |
| Footprint initial | vide sur les deux instances (readiness hierarchy) |

### Audit voies d’écriture (C++)

| Voie | Résultat |
|------|----------|
| **`SCH_SYMBOL::SetFootprintFieldText`** | **Global** — `GetField(FOOTPRINT)->SetText` sans `sheetPath` |
| **`SCH_SYMBOL::SetFieldText`** (FOOTPRINT, variant vide) | **Global** — `field->SetText(aFieldText)` |
| **`SCH_FIELD::SetText(text, &path, variant)`** | Instance via **`m_Variants`** **uniquement** si **`aVariantName` non vide** |
| **`GetFootprintFieldText(true, &path, false)`** | Lit champ global ; variant FOOTPRINT seulement si variant projet explicite |
| **`symbol_instances (footprint …)`** legacy | Parsé mais « do not use » ; **`UpdateSymbolInstanceData`** → **`SetFootprintFieldText`** global |
| Save moderne **`(instances … (path …))`** | ref/unit ; footprint = **`(property "Footprint" …)`** unique sur symbole **`SCH_SCREEN`** |
| GUI **`assign_footprints.cpp`** | Met à jour **toutes** les instances (`footprintField->SetText`) |

### Spike QA contrôlé

Script : **`qa/tools/ipc_tools/r145_spike_schematic_footprint_instance_write.ps1`**

1. Readiness **before** (`scope=hierarchy`) — 2 instances, footprint vide
2. Assign hierarchy **`dry_run=false`** — **`hierarchy_instance_write_unsupported`** (inchangé)
3. Écriture expérimentale **`scope=current_sheet`** sur **`sub_shared.kicad_sch`** (chemin **`SetFootprintFieldText`**)
4. Readback **`current_sheet` dry-run** — footprint écrit visible
5. Hierarchy dry-run **before** A/B — footprint vide par instance
6. Inférence isolation négative : une seule propriété **`Footprint`** sur écran partagé → mutation globale attendue sur R1 **et** R2

**Limites harness** : pas d’outil MCP save ; **`qa_open_editor_frame` root-after-sub** timeout Windows → readback hierarchy **after** non mesuré in-session ; reload = **skipped** (format fichier = propriété unique partagée documentée).

### Conclusion R145

| Critère | Valeur |
|---------|--------|
| **`instance_write_supported`** | **`false`** |
| API testée (spike) | **`SetFootprintFieldText`** via assign **`scope=current_sheet`** |
| Preuve readback cible | **oui** — dry-run sub-screen |
| Preuve non-régression autre instance | **non** (isolation absente — mutation globale inférée) |
| Preuve reload | **skipped** — pas de save MCP |
| Mutation MCP hierarchy activée | **non** |

**Raison** : aucune voie publique sûre pour FOOTPRINT par instance sans variant projet ; écriture **`current_sheet`** sur écran partagé mute le champ global.

### QA non-régression

- **`targeted_schematic_assign_footprint_probe.ps1`**
- **`targeted_schematic_footprint_readiness_probe.ps1`**
- **`targeted_project_context_snapshot_readiness_probe.ps1`**
- **`run_mcp_v2_checkpoint.ps1`** (9 familles)
- **`run_mcp_v2_ci_baseline.ps1 -VerifyOnly`**
- **`r145_spike_schematic_footprint_instance_write.ps1`**

**Aucun handler C++, schéma MCP, critère green modifié.**

### Recommandation R146

- Contribution upstream : **`SetFootprintFieldText(value, &sheetPath, variant)`** ou FOOTPRINT dans **`SCH_SYMBOL_INSTANCE`** hors variant-only
- Ne pas activer mutation **`scope=hierarchy`** tant qu’une preuve d’isolation multi-instance **mesurée** (readiness after + save/reload) n’existe pas
- Workflow client : **`scope=current_sheet`** seulement si symbole **non partagé** entre instances hiérarchiques

- **Date** : 2026-06-06

## R146 — Garde-fou `current_sheet` sur feuilles hiérarchiques partagées

### Problématique

R145 : **`SetFootprintFieldText`** est global ; **`scope=hierarchy`** mutation déjà bloquée. **`scope=current_sheet`** sur un **`SCH_SCREEN`** partagé (sous-feuille instanciée ≥2 fois) mute **toutes** les instances du symbole sans signal explicite.

### Audit détection (fiable)

| Signal | Source | Usage R146 |
|--------|--------|------------|
| **`affected_instances_count`** | `Hierarchy().GetSymbols` — compter refs où `GetSymbol() == symbol` ; repli `symbol->GetInstances()` projet courant | **≥2** ⇒ mutation globale |
| **`shared_sheet_detected`** | `SCH_SCREEN::GetRefCount() > 1` | écran référencé par plusieurs feuilles |
| **`global_update`** | `affected_instances_count > 1` | risque mutation multi-instance |

### Politique retenue — **Option C**

| Cas | Comportement |
|-----|--------------|
| **`affected_instances_count ≤ 1`** | inchangé (ex. **`issue19090`**) |
| **`affected_instances_count > 1`** + mutation + sans opt-in | **`ok=false`**, **`changed=false`**, blocker **`shared_symbol_global_update_blocked`** |
| **`allow_shared_symbol_global_update=true`** | mutation globale volontaire autorisée (chemin **`SetFootprintFieldText`**) |
| **`dry_run=true`** | preview autorisé — expose compteurs, pas de blocage |
| **`scope=hierarchy`** mutation | inchangé — **`hierarchy_instance_write_unsupported`** |

### Contrat MCP (champs ajoutés `scope=current_sheet`)

- **`allow_shared_symbol_global_update`** (bool, défaut **`false`**, optionnel)
- **`affected_instances_count`**, **`shared_sheet_detected`**, **`global_update`**, **`scope=current_sheet`**

### Implémentation

- **`eeschema/api/api_handler_sch.cpp`** — garde-fou avant **`SetFootprintFieldText`**
- **`common/mcp/mcp_executor.cpp`** — schéma JSON
- **`targeted_schematic_assign_footprint_probe.ps1`** — cas **`ipc_footprint_instance_write_spike`**
- **`r145_spike_schematic_footprint_instance_write.ps1`** — opt-in explicite pour write expérimental

### QA non-régression

- **`issue19090`** : mutation **`current_sheet`** inchangée
- **`ipc_footprint_instance_write_spike`** : blocage sans opt-in ; opt-in + restore
- hierarchy mutation toujours bloquée
- assign / readiness / snapshot / checkpoint MCP v2 / CI VerifyOnly green

### Recommandation R147

- Documenter workflow IA : readiness hierarchy → détecter **`affected_instances_count`** avant assign
- Upstream KiCad : API instance-safe FOOTPRINT (R145) avant d’assouplir le garde-fou

- **Date** : 2026-06-06

## R147 — Mini-release « Safe Assign Footprint Guard »

### Problématique

Comment **officialiser** le comportement de sécurité introduit en R146 sans modifier le code produit ni les critères green MCP v2 ?

### Choix retenu

**Documentation + workflow IA officiel** — mini-release documentaire clôturant R146 :

| Livrable | Contenu |
|----------|---------|
| Manifeste officiel | statut **`Safe Assign Footprint Guard`** sur **`schematic_assign_footprint_session`** |
| Guide développeur / IA | workflow 6 étapes (snapshot → dry-run → compteurs → opt-in) |
| README QA | garde-fou comportemental R146 + critères probe + sur-comptage conservateur |
| Release note | R147 « Safe Assign Footprint Guard officialisé » |

### Pourquoi

- Protéger les **agents IA** contre les mutations globales **involontaires** sur écrans hiérarchiques partagés.
- Rendre explicite le **changement comportemental intentionnel** R146 (breaking safe pour scripts non opt-in).
- Séparer clairement : **mutation normale** (symbole non partagé) vs **opt-in volontaire** vs **hierarchy dry-run only**.

### Contrat officialisé (rappel R146, sans changement code)

| `scope` | Mutation | Garde-fou |
|---------|----------|-----------|
| **`current_sheet`**, symbole non partagé (`affected_instances_count ≤ 1`) | **oui** | aucun — comportement historique (**`issue19090`**) |
| **`current_sheet`**, symbole partagé (`affected_instances_count > 1`) | **non** par défaut | **`shared_symbol_global_update_blocked`** |
| **`current_sheet`**, partagé + **`allow_shared_symbol_global_update=true`** | **oui** (global volontaire) | opt-in explicite |
| **`scope=hierarchy`** | **non** | **`hierarchy_instance_write_unsupported`** (inchangé R144) |

Champs diagnostic exposés (**`scope=current_sheet`**) : **`affected_instances_count`**, **`shared_sheet_detected`**, **`global_update`**.

### Changement comportemental intentionnel (R146 → officialisé R147)

Avant R146 : **`scope=current_sheet`** sur sous-feuille partagée pouvait muter **silencieusement** toutes les instances du symbole via **`SetFootprintFieldText`** global.

Après R146 / officialisé R147 : mutation refusée par défaut ; l’agent **doit** lire les compteurs en dry-run et n’utiliser **`allow_shared_symbol_global_update=true`** qu’avec confirmation utilisateur ou stratégie explicite.

### QA non-régression (R147 — docs only)

Probe **`targeted_schematic_assign_footprint_probe.ps1`** (inchangée) vérifie :

- blocage par défaut (**`shared_symbol_global_update_blocked`**) ;
- readback inchangé après blocage ;
- opt-in accepté ;
- hierarchy mutation toujours bloquée ;
- **`issue19090`** inchangé.

Note : **`affected_instances_count`** peut **sur-compter** de manière conservatrice (ex. 3 au lieu de 2 sur fixture spike) — le garde-fou reste actif.

### Prochaine piste (R148+)

- Upstream KiCad : API FOOTPRINT **instance-safe** (**`SetFootprintFieldText(value, &sheetPath, variant)`** ou **`SCH_SYMBOL_INSTANCE`**) avant d’assouplir le garde-fou.
- Workflow client avancé : stratégies par instance sans mutation globale (hors périmètre tant que moteur global).

- **Date** : 2026-06-07

## R148 — Audit workflow IA avancé (assign footprint sûr sans mutation hierarchy)

### Problématique

Peut-on corriger des footprints détectés en **`scope=hierarchy`** **sans** activer la mutation hierarchy, en s’appuyant sur le **Safe Assign Footprint Guard (R147)** et **`scope=current_sheet`** uniquement quand le contexte éditeur est sûr ?

### Audit — navigation / contexte feuille

| Question | Réponse audit |
|----------|---------------|
| Tool MCP pour **ouvrir / cibler** une feuille SCH hiérarchique ? | **Non (produit)**. **`qa_open_editor_frame`** (QA-only) ouvre un **fichier** `.kicad_sch` absolu via **`OpenProjectFiles`** — pas de navigation intra-hiérarchie vers un **`instance_path`**. |
| Vérifier que la feuille courante = **`sheet_path`** / **`instance_path`** ? | **Non**. Aucun champ MCP n’expose **`schematic.CurrentSheet()`** (PathHumanReadable / PathAsString). Code commenté dans **`validateDocumentInternal`** : TODO serdes **`SCH_SHEET_PATH`**. |
| **`get_open_documents`** / snapshot suffisants ? | **Partiellement**. Exposent **`schematic_absolute_path`** / **`absolute_path`** du **fichier éditeur ouvert** — **pas** la position hiérarchique active ni le **`sheet_path`**. |

### Audit — workflow `current_sheet` sécurisé

**Préconditions handler** (`api_handler_sch.cpp`) :

1. **`schematic_filename`** = **`GetCurrentFileName()`** (fichier éditeur ouvert — racine **ou** sous-feuille ouverte directement).
2. **`scope=current_sheet`** : symbole résolu sur **`GetScreen()->Items()`** uniquement — **`symbol_not_found`** si mauvaise feuille / mauvais fichier.
3. Garde-fou R146 : **`affected_instances_count`**, **`shared_sheet_detected`**, **`global_update`** calculés **avant** mutation ; blocage **`shared_symbol_global_update_blocked`** si **`global_update=true`** sans opt-in.

| Question | Réponse |
|----------|---------|
| Assign **`current_sheet`** après navigation manuelle ? | **Oui**, si fichier + écran courant contiennent le symbole. |
| Champs à vérifier avant mutation | **`ok`**, **`global_update=false`**, **`affected_instances_count ≤ 1`**, **`shared_sheet_detected=false`** (ou opt-in explicite), **`symbol_not_found` absent**, dry-run **`changed`** attendu. |
| **`affected_instances_count`** suffit pour bloquer partagés ? | **Oui** (conservateur, peut sur-compter). |
| Dry-run **`current_sheet`** obligatoire avant mutation ? | **Oui** — workflow officiel R147/R148. |

**Limite structurelle** : même avec **`affected_instances_count=1`**, **`SetFootprintFieldText`** reste **global** (R145) — le garde-fou empêche le cas multi-instance, pas une écriture instance-safe prouvée.

### Gaps tools MCP identifiés

| Gap | Impact IA | Contournement actuel |
|-----|-----------|----------------------|
| **Pas de navigation hiérarchique SCH (produit)** | L’IA ne peut pas se placer sur la feuille cible | Utilisateur navigue manuellement ; ou ouvrir le `.kicad_sch` sous-feuille (harness **`qa_open_editor_frame`** QA-only) |
| **Pas de vérification active sheet** | Impossible de confirmer **`sheet_path`** avant mutation | Heuristique : dry-run → **`symbol_not_found`** = mauvais contexte ; pas de preuve positive |
| **`get_open_documents` sans `current_sheet_path`** | Snapshot ne confirme pas l’instance active | Lire **`schematic_absolute_path`** seulement |
| **Readiness hierarchy vs assign current_sheet** | Diagnostic global, mutation locale | Workflow en deux temps (voir ci-dessous) |

**Candidats tool R149+ (non implémentés R148)** :

| Nom proposé | Rôle | Priorité audit |
|-------------|------|----------------|
| **`get_schematic_active_sheet_context_session`** | Read-only : **`schematic_filename`**, **`current_sheet_path`**, **`current_sheet_human`**, **`screen_file`**, aligné **`CurrentSheet()`** | **Haute** — comble gap vérification |
| **`open_schematic_sheet_by_path`** | Navigation programmatique vers **`sheet_path`** / sous-fichier | Moyenne — complexité GUI KiCad |
| **`verify_schematic_instance_context`** | Compare **`instance_path`** cible vs contexte actif | Basse si **`get_schematic_active_sheet_context`** + comparaison client |

### Workflow IA avancé retenu (déterministe)

```text
A. get_project_context_snapshot(
     include_footprints_summary=true,
     footprints_scope=hierarchy,
     footprints_validate=true)
B. Sélectionner une entrée sans footprint : symbol_id, instance_path, sheet_path
C. schematic_assign_footprint_session(
     scope=hierarchy, dry_run=true, instance_path=..., symbol_id=..., footprint=...)
D. Si hierarchy_instance_write_unsupported (mutation) :
   D1. Lire affected_instances_count du dry-run hierarchy (diagnostic)
   D2. S'assurer que le bon fichier SCH est ouvert (schematic_absolute_path ou sous-fichier)
       → navigation manuelle utilisateur si nécessaire
   D3. schematic_footprint_readiness_session(scope=current_sheet) — cible visible ?
   D4. schematic_assign_footprint_session(scope=current_sheet, dry_run=true)
       → vérifier global_update, shared_sheet_detected, affected_instances_count
   D5. Mutation current_sheet SEULEMENT SI :
       - dry-run ok=true
       - global_update=false ET affected_instances_count ≤ 1
       - OU allow_shared_symbol_global_update=true avec confirmation utilisateur explicite
E. Si symbol_not_found en D4 : contexte feuille incorrect → retour D2 (ne pas forcer)
F. Ne jamais forcer scope=hierarchy mutation
```

**Faisabilité globale** : **oui avec supervision** — diagnostic hiérarchique **fiable** ; mutation **fiable** sur symboles **non partagés** si contexte éditeur correct ; **gap navigation** impose intervention humaine ou harness QA.

### Options R149 comparées

| Option | Description | Avantages | Inconvénients |
|--------|-------------|-----------|---------------|
| **A** — Documentation workflow uniquement | Officialiser workflow R148 + limites | Zéro risque régression ; immédiat | Navigation / vérification sheet restent manuelles |
| **B** — Tool vérification active sheet | **`get_schematic_active_sheet_context_session`** read-only | Comble gap principal ; faible surface | Ne résout pas navigation |
| **C** — Tool navigation sheet | Ouvrir / naviguer vers feuille cible | Automatisation complète IA | Complexité GUI ; risque régression ; hors pattern read-only |
| **D** — Spike upstream instance-safe | API KiCad instance FOOTPRINT | Résout cause racine | Long terme ; hors MCP court terme |

### Recommandation R149

**Option A + B (séquencées)** :

1. **R149** : mini-release documentaire — workflow avancé R148 + limites explicites (**Option A**).
2. **R150** : implémenter **`get_schematic_active_sheet_context_session`** read-only (**Option B**) — contrat minimal :

```json
{
  "schematic_filename": "<absolu>",
  "current_sheet_path": "<PathAsString>",
  "current_sheet_human": "<PathHumanReadable>",
  "screen_filename": "<GetScreen()->GetFileName()>",
  "schematic_filename_matches_editor": true
}
```

Blockers proposés : **`schematic_filename_mismatch`**, **`sch_context_unavailable`**.

**Ne pas** prioriser **Option C** avant preuve de besoin produit ; **Option D** reste piste upstream parallèle.

### QA proposée R149 / R150

| Mission | QA |
|---------|-----|
| **R149 (doc)** | Extension guide + **`MCP_DECISIONS`** ; probes existantes inchangées green |
| **R150 (tool B)** | Probe **`targeted_schematic_active_sheet_context_probe.ps1`** sur **`ipc_footprint_readiness_v1`** : root vs sub ouvert ; **`current_sheet_path`** cohérent |

### Implémentation R148

- **Aucun code produit modifié** — audit + documentation uniquement.

- **Date** : 2026-06-07

## R149 — Mini-release « Advanced Safe Footprint Assignment Workflow »

### Problématique

Comment **officialiser** le workflow IA avancé audité en R148 (correction footprints hiérarchiques **sans** mutation **`scope=hierarchy`**) et cadrer R150 sans modifier le code produit ?

### Choix retenu — **Option A** (R148)

Mini-release **documentaire** clôturant R148 : workflow A–F, règles de sécurité, limites actuelles, contrat prévisionnel R150.

### Workflow IA officiel — Advanced Safe Footprint Assignment

```text
A. Appeler get_project_context_snapshot avec :
   include_footprints_summary=true
   footprints_scope=hierarchy
   footprints_validate=true

B. Identifier les symboles incomplets :
   instance_path
   sheet_path
   symbol_id
   reference
   footprint actuel
   sch_to_pcb_ready / sch_to_pcb_ready_validated

C. Faire un dry-run :
   schematic_assign_footprint_session
   scope=hierarchy
   dry_run=true
   instance_path=...
   symbol_id=...
   footprint=...

D. Si mutation hierarchy retourne hierarchy_instance_write_unsupported :
   ne pas forcer la mutation.

E. Pour utiliser current_sheet :
   vérifier que l'utilisateur ou l'agent a ouvert la bonne feuille / le bon fichier ;
   relancer readiness current_sheet ;
   faire un dry-run current_sheet ;
   vérifier :
     affected_instances_count <= 1
     shared_sheet_detected=false
     global_update=false
     exact_blocker_if_any=N/A

F. Muter seulement si le dry-run current_sheet est sûr.
```

Alternative étapes A–B : **`schematic_footprint_readiness_session`** **`scope=hierarchy`**, **`include_sheet_paths=true`**.

### Règles de sécurité (officialisées)

| Règle | Sémantique |
|-------|------------|
| **`scope=hierarchy`** | Diagnostic / dry-run **uniquement** — mutation → **`hierarchy_instance_write_unsupported`** |
| **`scope=current_sheet`** | Mutation **seulement** si contexte actif sûr (étapes E–F) |
| **`symbol_not_found`** | Mauvais contexte **probable** — heuristique **négative**, pas preuve positive |
| **`global_update=true`** | **Ne pas muter** sans confirmation utilisateur explicite |
| **`allow_shared_symbol_global_update=true`** | Opt-in volontaire — **jamais** automatique |
| **`affected_instances_count`** | Peut **sur-compter** (conservateur) — garde-fou actif dès **`> 1`** |
| Dry-run **`current_sheet`** | **Obligatoire** avant toute mutation sur schéma hiérarchique |

### Limites actuelles (jusqu’à R150)

| Limite | Impact |
|--------|--------|
| Pas de tool produit navigation instance hiérarchique | Workflow **partiellement supervisé** — ouverture feuille manuelle |
| **`qa_open_editor_frame`** QA-only | Ouvre un **fichier** `.kicad_sch`, pas un **`instance_path`** |
| Pas de tool exposant **`CurrentSheet()`** | Impossible de confirmer **`sheet_path`** avant mutation |
| **`get_project_context_snapshot`** | Chemins **fichiers** — pas position hiérarchique active |
| Pas de mutation footprint **instance-safe** KiCad | **`SetFootprintFieldText`** global (R145) même si **`count ≤ 1`** |

### Prochaine brique recommandée — R150

Tool read-only proposé : **`get_schematic_active_sheet_context_session`**.

**Input prévisionnel** :

```json
{
  "schematic_filename": "optional",
  "include_symbols": true,
  "max_symbols": 50
}
```

**Output prévisionnel** :

```json
{
  "ok": true,
  "schematic_filename": "...",
  "schematic_absolute_path": "...",
  "active_sheet_available": true,
  "active_sheet_path": "...",
  "active_sheet_name": "...",
  "active_sheet_file": "...",
  "active_sheet_is_shared": false,
  "symbols_count": 12,
  "symbols": [
    {
      "symbol_id": "...",
      "reference": "R1",
      "instance_path": "...",
      "footprint": "..."
    }
  ],
  "exact_blocker_if_any": "N/A"
}
```

Blockers prévisionnels : **`schematic_filename_mismatch`**, **`sch_context_unavailable`**.

QA R150 proposée : probe **`targeted_schematic_active_sheet_context_probe.ps1`** sur **`ipc_footprint_readiness_v1`**.

### Implémentation R149

- **Aucun code produit modifié** — documentation uniquement.
- CI VerifyOnly green.

### Recommandation R150

Implémenter **`get_schematic_active_sheet_context_session`** (**Option B** R148) — comble le gap vérification active sheet ; ne remplace pas la navigation hiérarchique.

- **Date** : 2026-06-07

## R150 — `get_schematic_active_sheet_context_session` (read-only)

### Problématique

R149 : workflow IA avancé **supervisé** faute d’exposition MCP de **`schematic.CurrentSheet()`**. Besoin d’une **preuve positive** du contexte actif avant mutation **`scope=current_sheet`**.

### Choix retenu

Tool **read-only** GUI-only exposant :

| Champ | Source |
|-------|--------|
| **`schematic_filename`** / **`schematic_absolute_path`** | **`GetCurrentFileName()`** (optionnel mismatch si param fourni) |
| **`active_sheet_path`** | **`CurrentSheet().PathAsString()`** |
| **`active_sheet_name`** | **`CurrentSheet().Last()->GetName()`** |
| **`active_sheet_file`** | **`GetScreen()->GetFileName()`** absolu |
| **`active_sheet_is_root`** | **`CurrentSheet().size() <= 1`** |
| **`active_sheet_is_shared`** | **`SCH_SCREEN::GetRefCount() > 1`** ou références **`SCH_SHEET`** projet |
| **`symbols[]`** | **`GetScreen()->Items()`** — power symbols exclus ; footprint path-aware **`GetFootprintFieldText(false, &currentSheet, false)`** ; **`instance_path`** aligné readiness |

### Garde-fous

- **Aucun** **`SCH_COMMIT`** ; aucune mutation ; aucun changement de feuille.
- **`schematic_filename`** optionnel — si fourni : match strict éditeur.

### Blockers

| Blocker | Cas |
|---------|-----|
| **`sch_context_unavailable`** | pas de frame SCH / Kiway |
| **`schematic_not_loaded`** | pas de fichier courant |
| **`schematic_filename_mismatch`** | param ≠ éditeur |
| **`active_sheet_unavailable`** | écran / sheet path vide |
| **`invalid_max_symbols`** | hors **1..500** |

### Implémentation

- **`eeschema/api/api_handler_sch.cpp`** — **`SchematicActiveSheetContextSessionToolInvoke`**
- **`common/mcp/mcp_sch_active_sheet_context_callback.cpp`**
- **`common/mcp/mcp_executor.cpp`** — registry + JSON schema
- **`qa/tools/ipc_tools/targeted_schematic_active_sheet_context_probe.ps1`**
- **`qa/tools/ipc_tools/ipc_smoke.cpp`** — lignes greppables

### QA

Fixture **`issue19090`** : **`ok=true`**, symboles visibles, négatifs **`invalid_max_symbols`**, **`schematic_filename_mismatch`**. Non-régression assign / readiness / snapshot / checkpoint v2 / CI VerifyOnly.

### Recommandation R151

- Intégrer **`get_schematic_active_sheet_context_session`** dans workflow R149 étape E (comparer **`instance_path`** cible vs symboles actifs).
- Navigation hiérarchique produit reste hors scope — upstream ou tool dédié ultérieur.

- **Date** : 2026-06-07

## R151 — Workflow IA consolidé avec `active_sheet_context`

### Problématique

R150 a ajouté **`get_schematic_active_sheet_context_session`**. R149 documentait un workflow supervisé sans preuve positive du contexte. R151 **valide et officialise** la chaîne complète post-R150.

### Workflow officiel consolidé (post-R150)

```text
1. get_project_context_snapshot(include_footprints_summary=true, footprints_scope=hierarchy, footprints_validate=true)
2. schematic_footprint_readiness_session(scope=hierarchy, include_sheet_paths=true) — cibler symbol_id + instance_path
3. get_schematic_active_sheet_context_session — comparer instance_path cible ∈ symbols[]
4. SI instance absente → ne pas muter ; demander navigation / ouverture feuille
5. schematic_assign_footprint_session(scope=current_sheet, dry_run=true) — vérifier global_update=false, affected_instances_count ≤ 1
6. Mutation current_sheet seulement si dry-run sûr (+ garde-fou R146/R147)
```

### Règles agent (officialisées)

| Règle | Action |
|-------|--------|
| Correspondance **`instance_path`** | **`scope=current_sheet`** **interdit** tant que l’`instance_path` readiness n’apparaît pas dans **`active_sheet_context.symbols[]`** |
| **`symbol_id`** | Doit concorder entre readiness et active sheet context pour la même **`instance_path`** |
| **`active_sheet_is_shared=true`** | Traiter comme risque global — dry-run assign obligatoire ; pas de mutation sans opt-in |
| Hierarchy mutation | **Toujours interdite** — **`hierarchy_instance_write_unsupported`** |

### Champs comparables (audit validé)

| Source | Champ | Usage |
|--------|-------|-------|
| Readiness hierarchy | **`symbols[].instance_path`** | Cible workflow |
| Active sheet context | **`symbols[].instance_path`** | Preuve positive contexte |
| Active sheet context | **`active_sheet_path`**, **`active_sheet_file`** | Diagnostic navigation |
| Assign dry-run | **`affected_instances_count`**, **`global_update`**, **`shared_sheet_detected`** | Garde-fou R146 |

### QA R151

Extension **`targeted_schematic_active_sheet_context_probe.ps1`** (fixture **`issue19090`**) :

| Critère | Attendu |
|---------|---------|
| **`workflow_instance_path_match_ok`** | readiness hierarchy **`instance_path`** ∈ active sheet **`symbols[]`** |
| **`workflow_symbol_id_match_ok`** | **`symbol_id`** concordant |
| **`workflow_assign_dry_run_ok`** | dry-run **`current_sheet`** : **`ok=true`**, blocker **`N/A`**, **`global_update=false`**, count **≤ 1** |

### Implémentation R151

- **Aucun handler produit modifié** — probe + documentation uniquement.

### Recommandation R152

- Fixture hiérarchique **`ipc_footprint_readiness_v1`** : workflow root vs sub-sheet (navigation manuelle documentée).
- Navigation sheet produit : reste hors scope ; upstream KiCad ou tool dédié.

- **Date** : 2026-06-07

## R152 — Workflow active sheet sur fixture hiérarchique

### Problématique

R151 a validé le workflow consolidé sur **`issue19090`** (schéma root simple). R152 **étend la preuve QA** à la fixture hiérarchique **`ipc_footprint_readiness_v1`** pour démontrer que l’IA peut distinguer un contexte actif correct d’un contexte actif incorrect avant **`schematic_assign_footprint_session scope=current_sheet`**.

### Règle agent (hiérarchique — officialisée)

```text
Si instance_path cible absent de active_sheet_context.symbols[] :
  ne pas appeler assign current_sheet en mutation.
  demander navigation / ouverture de la feuille cible.

Si instance_path cible présent :
  faire dry-run current_sheet.
  muter seulement si dry-run safe :
    exact_blocker_if_any=N/A
    global_update=false
    shared_sheet_detected=false
```

### Cas validés (fixture `ipc_footprint_readiness_v1`)

| Cas | Contexte | Attendu | Critère QA |
|-----|----------|---------|------------|
| **A — root ouvert** | Sous-feuille ciblée via readiness hierarchy | **`instance_path`** sous-feuille **absent** de **`active_sheet_context.symbols[]`** ; assign dry-run root → **`symbol_not_found`** | **`hierarchy_wrong_context_instance_absent_ok`**, **`hierarchy_wrong_context_blocks_assignment_ok`** |
| **B — sous-feuille ouverte** | **`qa_open_editor_frame`** sur **`sub_ipc_fp.kicad_sch`** | **`instance_path`** (ou **`symbol_id`** fallback) **présent** ; dry-run **`current_sheet`** sûr ou **`shared_sheet_global_update_detected`** (R146) | **`hierarchy_active_context_instance_match_ok`**, **`hierarchy_active_context_symbol_id_match_ok`**, **`hierarchy_current_sheet_dry_run_safe_ok`** |

### Champs summary QA (R152)

| Champ | Signification |
|-------|---------------|
| **`workflow_hierarchy_fixture_checked`** | Fixture hiérarchique évaluée |
| **`hierarchy_wrong_context_instance_absent_ok`** | Cas A : instance sous-feuille absente en contexte root |
| **`hierarchy_wrong_context_blocks_assignment_ok`** | Cas A : assign dry-run root bloqué (**`symbol_not_found`**) |
| **`hierarchy_active_context_instance_match_ok`** | Cas B : match positif (**`true`** ou **`skipped_navigation_tool_missing`**) |
| **`hierarchy_active_context_symbol_id_match_ok`** | Cas B : **`symbol_id`** concordant |
| **`hierarchy_current_sheet_dry_run_safe_ok`** | Cas B : dry-run **`current_sheet`** sûr (**`true`**) ou risque global détecté (**`shared_sheet_global_update_detected`**) |

### Limite / skip navigation

Si l’ouverture sous-feuille via **`qa_open_editor_frame`** échoue (pas de navigation sheet produit) :

- **`hierarchy_active_context_instance_match_ok=skipped_navigation_tool_missing`**
- Cas A (root / instance absente) reste **obligatoire** pour green
- Recommandation **R153** : tool navigation sheet produit **ou** fixture ouvrable directement en contexte hiérarchique natif

### Implémentation R152

- **Aucun handler produit modifié** — extension probe **`targeted_schematic_active_sheet_context_probe.ps1`** + documentation uniquement.
- Navigation sous-feuille : **`qa_open_editor_frame`** (outil QA existant, hors checkpoint MCP v2).

### Recommandation R153

- Tool navigation sheet produit (**`navigate_schematic_sheet`** ou équivalent upstream KiCad).
- Alternative : API FOOTPRINT instance-safe upstream ; ou fixture ouvrable sans navigation manuelle.

- **Date** : 2026-06-07

## R153 — Audit navigation SCH par `sheet_path` / `instance_path`

### Problématique

R152 a prouvé le workflow hiérarchique **partiellement** : le bon contexte actif n’est atteignable qu’via **`qa_open_editor_frame`**, qui appelle **`OpenProjectFiles`** sur un **fichier** `.kicad_sch` — pas une **instance hiérarchique**. Conséquence observée sur **`ipc_footprint_readiness_v1`** : `active_sheet_is_root=true` sur sub-file, `shared_sheet_detected=true` au dry-run assign.

R153 **audite la faisabilité** d’un tool MCP produit pour focaliser une feuille hiérarchique par identifiant stable, puis valider le contexte via **`get_schematic_active_sheet_context_session`**.

### Verdict faisabilité

| Question | Réponse |
|----------|---------|
| Navigation SCH par `sheet_path` faisable ? | **Oui** — API interne mature, non exposée MCP aujourd’hui |
| Équivalent double-clic hierarchy pane ? | **Oui** — `RunAction(SCH_ACTIONS::changeSheet, &SCH_SHEET_PATH*)` |
| Peut-on ouvrir une sous-feuille par `SCH_SHEET_PATH` ? | **Oui** — résolution via `Schematic().Hierarchy()` + `PathAsString()` |
| `CurrentSheet()` mis à jour correctement ? | **Oui** si **`DisplayCurrentSheet()`** est appelé (pattern `SCH_NAVIGATE_TOOL::changeSheet`) |

**Hors scope R153** : implémentation — audit + contrat R154 proposé uniquement.

### APIs KiCad candidates (inspectées)

| API | Fichier | Rôle | Recommandation R154 |
|-----|---------|------|---------------------|
| **`SCH_NAVIGATE_TOOL::changeSheet(const SCH_SHEET_PATH&)`** | `eeschema/tools/sch_navigate_tool.cpp` | Canonique UI : cancel interactive, clear selection, save zoom, `Schematic().SetCurrentSheet`, **`DisplayCurrentSheet()`**, history | **Préférée** — via `RunAction(SCH_ACTIONS::changeSheet, &path)` |
| **`RunAction<SCH_SHEET_PATH*>(SCH_ACTIONS::changeSheet, …)`** | `hierarchy_pane.cpp`, `cross-probing.cpp`, `sch_editor_control.cpp`, `dialog_erc.cpp`, … | Même sémantique que double-clic navigateur hiérarchie | **Point d’entrée MCP** (GUI thread) |
| **`SCH_EDIT_FRAME::SetCurrentSheet`** | `sch_edit_frame.cpp` | Met à jour `Schematic().SetCurrentSheet` + `DisplaySheet` canvas | **Insuffisante seule** — ne remplace pas `DisplayCurrentSheet()` (refs, dangling, title, tools) |
| **`SCHEMATIC::SetCurrentSheet`** | `schematic.h` | Mutation pointeur `m_currentSheet` uniquement | **Basse niveau** — plot/export ; commentaire `sch_plotter.cpp` : ne pas utiliser sans affichage |
| **`qa_open_editor_frame`** | `common/mcp/mcp_executor.cpp` | `Player()->OpenProjectFiles(sub.kicad_sch)` | **Rejetée produit** — ouvre document, pas instance hiérarchique |
| **`API_HANDLER_SCH::validateDocumentInternal`** | `api_handler_sch.cpp` | TODO serdes `SCH_SHEET_PATH` | Preuve gap upstream API formelle |

Pattern canonique (`sch_navigate_tool.cpp`) :

```cpp
m_frame->Schematic().SetCurrentSheet( aPath );
m_frame->DisplayCurrentSheet();
```

### Résolution identifiants

| Identifiant | Format | Aligné readiness / active context | Navigation R154 |
|-------------|--------|-----------------------------------|---------------|
| **`sheet_path` (`PathAsString`)** | `/uuid/` ou `/uuid/uuid/` | **`active_sheet_context.active_sheet_path`** ; readiness `symbols[].instance_path` prefix | **Recommandé primaire** |
| **`instance_path`** | `PathAsString() + symbol_uuid` (sans séparateur) | readiness / assign hierarchy | **Secondaire** — dériver `SCH_SHEET_PATH` via `Hierarchy().GetSymbols()` + `ref.GetSheetPath()` |
| **`PathHumanReadable`** | `/Root/SubSheet/` | readiness `symbols[].sheet_path` (humain) | Alias optionnel — conversion via parcours hiérarchie |
| **Fichier `.kicad_sch`** | chemin absolu | `active_sheet_file` | **Interdit seul** — ne garantit pas `CurrentSheet()` instance |
| **`KIID_PATH` / uuid chain** | interne | `SCH_SHEET_PATH::Path()` | Interne implémentation |

Algorithme résolution proposé R154 :

```text
1. schematic.RefreshHierarchy()
2. sheets = schematic.Hierarchy()
3. SI sheet_path fourni :
     match sheets[i].PathAsString() == sheet_path (normaliser trailing '/')
4. SINON SI instance_path fourni :
     refs = sheets.GetSymbols(...)
     match buildInstancePath(ref) == instance_path → target = ref.GetSheetPath()
5. SINON → missing_instance_selector / invalid_sheet_path
6. SI plusieurs SCH_SHEET_PATH pour même fichier partagé → ambiguous_sheet_path (blocker)
```

Fixture **`ipc_footprint_readiness_v1`** — cible R2 :

- readiness hierarchy `instance_path` : `/a1000003-…/a1000006-…`
- `sheet_path` attendu : `/a1000003-0003-4003-8003-000000000003/`
- sub-sheet uuid : `a1000003-0003-4003-8003-000000000003`

### Sécurité post-navigation

Chaîne agent obligatoire après navigation réelle :

```text
1. navigate_schematic_sheet_session (dry_run=false)
2. get_schematic_active_sheet_context_session
3. Vérifier active_sheet_path_after == sheet_path cible
4. SI verify_instance_path demandé : instance_path ∈ symbols[]
5. SI mismatch → post_navigation_context_mismatch ; ne pas muter assign
6. schematic_assign_footprint_session(scope=current_sheet, dry_run=true)
```

Blockers alignés existants : `sch_context_unavailable`, `schematic_not_loaded`, `sheet_path_not_found`, `instance_path_not_found` (déjà assign hierarchy).

### Contrat tool R154 proposé

**Nom recommandé** : **`navigate_schematic_sheet_session`**

Read/write UI, **non-mutant SCH** (pas de `SCH_COMMIT`) — même famille que active sheet context.

**Entrée** :

```json
{
  "schematic_filename": "...",
  "sheet_path": "/a1000003-0003-4003-8003-000000000003/",
  "instance_path": "",
  "verify_instance_path": "",
  "dry_run": false
}
```

Règles :

- **`sheet_path`** OU **`instance_path`** requis (pas les deux obligatoires ; `instance_path` prime pour résolution si seul).
- **`dry_run=true`** : résoudre cible + retourner `active_sheet_path_before` / chemin résolu **sans** `changeSheet`.
- **`verify_instance_path`** optionnel : si non vide, échec **`post_navigation_context_mismatch`** si absent de contexte post-nav (via lecture interne ou appel agent enchaîné).

**Sortie** :

```json
{
  "ok": true,
  "dry_run": false,
  "active_sheet_path_before": "/",
  "active_sheet_path_after": "/a1000003-0003-4003-8003-000000000003/",
  "active_sheet_name": "SubSheet",
  "active_sheet_file": ".../sub_ipc_fp.kicad_sch",
  "resolved_sheet_path": "/a1000003-0003-4003-8003-000000000003/",
  "context_changed": true,
  "exact_blocker_if_any": "N/A"
}
```

### Blockers R154 proposés

| Blocker | Cas |
|---------|-----|
| `sch_context_unavailable` | pas de frame SCH / wxTheApp |
| `schematic_not_loaded` | projet non chargé |
| `schematic_filename_mismatch` | garde-fou filename (optionnel) |
| `invalid_sheet_path` | syntaxe / vide / normalisation impossible |
| `sheet_path_not_found` | absent de `Hierarchy()` |
| `instance_path_not_found` | selector instance sans match |
| `ambiguous_sheet_path` | feuille partagée, plusieurs instances |
| `missing_sheet_selector` | ni `sheet_path` ni `instance_path` |
| `navigation_api_unavailable` | `ToolManager` / `NavigateTool` indisponible |
| `navigation_failed` | `changeSheet` n’a pas changé le contexte |
| `post_navigation_context_mismatch` | `active_sheet_path_after` ≠ attendu ou instance absente |
| `N/A` | succès |

### QA R154 proposée (fixture `ipc_footprint_readiness_v1`)

Nouvelle probe **`targeted_schematic_navigate_sheet_probe.ps1`** (hors checkpoint v2 initialement, comme R150) :

| Étape | Action | Critère |
|-------|--------|---------|
| 1 | Ouvrir projet root | `navigation_fixture_loaded_ok` |
| 2 | Active context root | sub `instance_path` **absent** |
| 3 | Readiness hierarchy → cible R2 | `target_sheet_path` / `target_instance_path` capturés |
| 4 | Navigate **`dry_run=true`** | `resolved_sheet_path` OK, `context_changed=false` |
| 5 | Navigate **`dry_run=false`** par `sheet_path` | `active_sheet_path_after` match |
| 6 | Active context post-nav | `instance_path` **présent** |
| 7 | Assign dry-run `current_sheet` | comparer vs R152 : **`active_sheet_is_shared`** attendu **false** si navigation native |
| 8 | Navigate retour root (ou restart KiCad) | non-régression |
| 9 | Non-régression | `targeted_schematic_active_sheet_context_probe.ps1` green |

Summary champs proposés : `navigation_dry_run_ok`, `navigation_real_ok`, `post_nav_instance_match_ok`, `post_nav_assign_dry_run_ok`, `navigation_shared_sheet_regression_ok`.

### Implémentation R153

- **Aucun handler produit modifié** — audit documentation uniquement.
- Probes optionnelles R153 : active sheet + readiness **green** ; CI VerifyOnly **green** (après fermeture KiCad).

### Recommandation R154 (implémentation)

1. **Option A — recommandée** : tool **`navigate_schematic_sheet_session`** — callback GUI `RunAction(SCH_ACTIONS::changeSheet)` + résolution `Hierarchy()` ; identifiant primaire **`sheet_path` (`PathAsString`)**.
2. **Option B** : étendre API KiCad formelle (`DocumentSpecifier.sheet_path` — TODO existant) puis MCP wrapper.
3. **Rejetée** : promouvoir **`qa_open_editor_frame`** en tool produit.

Effet attendu vs R152 : navigation native → `CurrentSheet()` instance-correct → **`shared_sheet_detected`** plus fidèle ; workflow assign **`current_sheet`** testable sans contournement fichier.

- **Date** : 2026-06-07

## R154 — `navigate_schematic_sheet_session` (navigation hiérarchique produit)

### Problématique

R153 a audité la faisabilité. R154 **implémente** le tool MCP produit **`navigate_schematic_sheet_session`** pour focaliser une feuille hiérarchique par **`sheet_path`** ou **`instance_path`**, sans mutation SCH.

### Implémentation

| Composant | Fichier |
|-----------|---------|
| Handler | `eeschema/api/api_handler_sch.cpp` — `SchematicNavigateSheetSessionToolInvoke` |
| Callback | `common/mcp/mcp_sch_navigate_sheet_callback.cpp`, `include/mcp/mcp_sch_navigate_sheet_callback.h` |
| Registry | `common/mcp/mcp_executor.cpp` |
| Smoke | `qa/tools/ipc_tools/ipc_smoke.cpp` |
| Probe | `targeted_schematic_navigate_sheet_probe.ps1` |

Navigation réelle : **`RunAction<SCH_SHEET_PATH*>(SCH_ACTIONS::changeSheet, &targetPath)`** (équivalent hierarchy pane / `SCH_NAVIGATE_TOOL::changeSheet`).

### Contrat

Entrée : `schematic_filename` (optionnel), `sheet_path` \| `instance_path` (obligatoire l’un des deux), `verify_instance_path` (optionnel), `dry_run` (défaut false).

Sortie : `resolved_sheet_path`, `resolved_from`, `active_sheet_path_before/after`, `context_changed`, `post_navigation_verified`, blockers.

Blockers : `missing_sheet_selector`, `invalid_sheet_path`, `invalid_instance_path`, `schematic_filename_mismatch`, `sch_context_unavailable`, `schematic_not_loaded`, `sheet_path_not_found`, `instance_path_not_found`, `ambiguous_sheet_path`, `navigation_api_unavailable`, `post_navigation_context_mismatch`, `N/A`.

### QA R154

Probe **`targeted_schematic_navigate_sheet_probe.ps1`** (fixture **`ipc_footprint_readiness_v1`**) :

| Critère | Attendu |
|---------|---------|
| `root_initial_ok` | contexte root, instance sub absente |
| `dry_run_navigation_ok` | pas de changement contexte |
| `real_navigation_ok` | `active_sheet_path_after` = sub-sheet |
| `post_nav_instance_match_ok` | instance cible visible active context |
| `instance_path_navigation_ok` | navigation par `instance_path` |
| `post_nav_assign_dry_run_ok` | assign dry-run OK après nav native |
| `return_root_ok` | retour `/` |
| négatifs | missing selector, sheet/instance not found |

### Workflow agent (post-R154)

```text
1. schematic_footprint_readiness_session(scope=hierarchy) → sheet_path + instance_path
2. navigate_schematic_sheet_session(sheet_path=..., dry_run=true) — preview
3. navigate_schematic_sheet_session(sheet_path=..., verify_instance_path=...) — navigation réelle
4. get_schematic_active_sheet_context_session — confirmer instance_path ∈ symbols[]
5. schematic_assign_footprint_session(scope=current_sheet, dry_run=true)
6. Mutation seulement si dry-run sûr (R146/R152)
```

### Recommandation R155

- Remplacer contournement **`qa_open_editor_frame`** dans probe **`targeted_schematic_active_sheet_context_probe.ps1`** (cas B R152) par **`navigate_schematic_sheet_session`**.
- Documenter workflow consolidé footprint + navigation produit.

- **Date** : 2026-06-07

## R155 — Workflow SCH navigable sans `qa_open_editor_frame`

### Problématique

R152 utilisait **`qa_open_editor_frame`** pour ouvrir **`sub_ipc_fp.kicad_sch`**, provoquant un faux positif **`shared_sheet_global_update_detected`**. R154 a prouvé que **`navigate_schematic_sheet_session`** restaure un contexte hiérarchique natif avec **`global_update=false`**.

R155 **consolide le workflow produit** dans la probe active sheet context et la documentation agent.

### Workflow IA officialisé (end-to-end)

```text
get_project_context_snapshot(include_footprints_summary=true, footprints_scope=hierarchy)
→ schematic_footprint_readiness_session(scope=hierarchy) — retenir instance_path
→ navigate_schematic_sheet_session(instance_path=..., verify_instance_path=...)
→ get_schematic_active_sheet_context_session — comparer instance_path ∈ symbols[]
→ schematic_assign_footprint_session(scope=current_sheet, dry_run=true)
→ mutation seulement si dry-run safe :
     exact_blocker_if_any=N/A
     global_update=false
     shared_sheet_detected=false
```

### Règles agent R155

| Règle | Action |
|-------|--------|
| **`qa_open_editor_frame`** | **Interdit** en workflow produit footprint |
| Navigation échouée | **Ne pas muter** |
| **`instance_path` absent** post-navigation | **Ne pas muter** |
| **`global_update=true`** | **Ne pas muter** sans opt-in explicite |
| **`global_update=false`** + blocker **`N/A`** | Mutation **`current_sheet`** autorisée (si dry-run sûr) |

### QA R155

Extension **`targeted_schematic_active_sheet_context_probe.ps1`** — cas hiérarchique :

| Critère | Attendu |
|---------|---------|
| **`hierarchy_navigation_product_used`** | **`navigate_schematic_sheet_session`** appelé |
| **`workflow_uses_qa_open_editor_frame`** | **`false`** (workflow hiérarchique) |
| **`hierarchy_current_sheet_dry_run_global_update_false_ok`** | assign dry-run post-nav native : **`global_update=false`** |
| Cas A root | inchangé (instance absente, assign bloqué) |

Probe dédiée **`targeted_schematic_navigate_sheet_probe.ps1`** conservée (contrat tool navigation).

**Note harness** : ouverture initiale éditeur SCH root via **`qa_open_editor_frame`** (bootstrap QA, hors workflow navigation) ; **`workflow_uses_qa_open_editor_frame=false`** atteste que la navigation vers sous-feuille n’utilise **pas** ce contournement.

### Diff comportemental confirmé

| Navigation | Assign dry-run post-nav |
|------------|-------------------------|
| **`qa_open_editor_frame`** (R152) | **`shared_sheet_global_update_detected`** |
| **`navigate_schematic_sheet_session`** (R154/R155) | **`global_update=false`** |

### Implémentation R155

- **Aucun handler C++ modifié** — probes + documentation uniquement.

### Recommandation R156

- Mutation footprint **instance-safe** upstream KiCad (hors **`SetFootprintFieldText`** global).
- Intégrer workflow snapshot → navigate → assign dans guide utilisateur exemples complets.

- **Date** : 2026-06-07

## R156 — Exemple end-to-end « Navigable SCH Footprint Workflow »

### Problématique

R155 a consolidé le workflow produit sans navigation **`qa_open_editor_frame`**. R156 fournit un **exemple reproductible** documentant la chaîne complète pour agents et développeurs.

### Script exemple

**`qa/tools/ipc_tools/r156_example_navigable_sch_footprint_workflow.ps1`**

Fixture : **`ipc_footprint_readiness_v1`**

| Étape | Tool MCP | Vérification |
|-------|----------|--------------|
| A Bootstrap | **`qa_open_editor_frame`** (root only, harness) | Frame SCH disponible |
| B Readiness | **`schematic_footprint_readiness_session`** `scope=hierarchy` | Cible sans footprint |
| C Navigation | **`navigate_schematic_sheet_session`** `instance_path=...` | **`post_navigation_verified=true`** |
| D Context | **`get_schematic_active_sheet_context_session`** | **`instance_path`** ∈ **`symbols[]`** |
| E Dry-run | **`schematic_assign_footprint_session`** `scope=current_sheet` `dry_run=true` | **`global_update=false`**, blocker **`N/A`** |
| F Décision | Summary | **`agent_decision=mutation_allowed_after_human_or_policy_confirmation`** ; **`mutation_performed=false`** |

### Workflow IA officialisé (exemple concret)

```text
1. schematic_footprint_readiness_session(scope=hierarchy) — pick missing footprint instance
2. navigate_schematic_sheet_session(instance_path=...)
3. get_schematic_active_sheet_context_session — verify instance_path + symbol_id
4. schematic_assign_footprint_session(scope=current_sheet, dry_run=true)
5. Mutate only if dry-run safe AND policy/user allows (never automatic on global_update=true)
```

### Règles agent R156

| Règle | Action |
|-------|--------|
| Navigation produit | **`navigate_schematic_sheet_session`** uniquement |
| **`qa_open_editor_frame`** | Harness bootstrap root OK ; **interdit** navigation hiérarchique |
| **`global_update=false`** | Requis pour mutation automatique |
| **`allow_shared_symbol_global_update`** | **Jamais** automatique |
| **`scope=hierarchy` mutation** | **Unsupported** — dry-run diagnostic only |

### Summary champs

**`qa_r156_navigable_sch_footprint_workflow_summary.txt`** — **`r156_workflow_green`**, **`workflow_uses_qa_open_editor_frame_for_navigation=false`**, **`mutation_performed=false`**, **`agent_decision`**.

### Implémentation R156

- **Aucun handler C++ modifié** — script exemple + documentation uniquement.

- **Date** : 2026-06-07

## R157 — Audit/spike API footprint instance-safe KiCad

### Objectif

Approfondir l’audit R145 avec **mesure readiness after in-session** (instances A/B), navigation produit, audit disque, reload sans save MCP — **sans** activer mutation **`scope=hierarchy`**.

### Questions tranchées

#### 1. Stockage

| Couche | Emplacement | État |
|--------|-------------|------|
| **Global** | `(property "Footprint" …)` sur **`SCH_SYMBOL`** / écran **`sub_shared.kicad_sch`** | **Actif** — source unique pour symbole partagé |
| **Instance legacy** | **`SCH_SYMBOL_INSTANCE::m_Footprint`** | **« Do not use »** — commentaire explicite dans **`sch_sheet_path.h`** |
| **Variant projet** | **`instance.m_Variants[name].m_Fields["Footprint"]`** | **Lecture** si variant non vide ; **pas** utilisé par readiness MCP (variant vide) |
| **Save moderne `(instances … (path …))`** | ref + unit + variants | **N’écrit pas** `(footprint …)` — writer **`sch_io_kicad_sexpr.cpp`** |
| **Parse legacy `(footprint …)` dans `(instances …)`** | Appelle **`SetFootprintFieldText`** | **Global** — pas instance-safe |

**Conclusion stockage** : le footprint « par instance » hiérarchique **n’existe plus** comme donnée indépendante hors **variants projet nommés** ; le modèle moderne = **une propriété FOOTPRINT par symbole-écran**.

#### 2. API d’écriture (comparaison options)

| Option | Verdict R157 |
|--------|--------------|
| **A** — `SetFootprintFieldText(value, &sheetPath, variant)` | **Absent** — seule surcharge = global ; **`SetValueFieldText`** a le pattern instance/variant, pas FOOTPRINT |
| **B** — `SCH_FIELD::SetText(value, &path, variant)` FOOTPRINT variant vide | **Global** — **`SetFieldText`** → **`field->SetText(aFieldText)`** ; testé via assign **`current_sheet`** + **`allow_shared_symbol_global_update`** |
| **B′** — variant non vide | Théorique via **`m_Variants`** ; **non exposé MCP** ; readiness ignore sans variant actif |
| **C** — `SCH_SYMBOL_INSTANCE::m_Footprint` | **Legacy** — non lu par **`GetFootprintFieldText(true, &path, false)`** |
| **D** — pas d’écriture instance-safe fiable | **Retenu** |

**Commit / undo** : assign MCP utilise **`SCH_COMMIT::Modify(symbol, screen)`** + **`SetFootprintFieldText`** global — undo enregistre mutation **symbole-écran**, pas instance hiérarchique isolée.

#### 3. Isolation (fixture partagée)

Script : **`r157_spike_footprint_instance_safe_write.ps1`**

| Étape | Résultat attendu audit négatif |
|-------|--------------------------------|
| Readiness before A/B | footprint vide |
| Navigate **`instance_path` A** + write **`current_sheet`** | **`SetFootprintFieldText`** global |
| Readiness after A | **changed** → **`Resistor_SMD:R_0201_0603Metric`** |
| Readiness after B | **also changed** → **`instance_b_unchanged_ok=false`** |
| Disque post-write sans save MCP | propriété **`Footprint`** souvent **inchangée** (session-only) |
| Reload processus sans save | **`save_reload_preserved=false`** |

#### 4. Compatibilité readiness

**`schematic_footprint_readiness_session scope=hierarchy`** appelle **`GetFootprintFieldText(true, &sheetPath, false)`** — reflète l’écriture **globale** sur **A et B** après mutation ; **pas** d’isolation instance.

#### 5. Décision MCP

| Item | Valeur |
|------|--------|
| **`instance_safe_write_supported`** | **`false`** |
| Mutation **`scope=hierarchy`** | **Reste bloquée** — **`hierarchy_instance_write_unsupported`** |
| **`allow_shared_symbol_global_update`** | **Non** une solution instance-safe |

### Summary champs

**`qa_r157_spike_footprint_instance_safe_write_summary.txt`**

### Implémentation R157

- Script spike + documentation audit — **aucun handler C++**, **aucun critère green modifié**.

- **Date** : 2026-06-07

## R158 — Officialisation limite instance-safe + note design upstream

### Objectif

Clôturer R157 en **officialisant** la limite produit MCP et en publiant la **note design upstream** pour une future API KiCad instance-safe.

### Limite produit officialisée

| Mode | Mutation | Notes |
|------|----------|-------|
| **`scope=current_sheet`** | Oui (garde-fous R146) | Contexte feuille active ; **`global_update=false`** requis pour mutation auto |
| Symbole partagé | Bloqué par défaut | **`shared_symbol_global_update_blocked`** |
| **`allow_shared_symbol_global_update=true`** | Mutation **globale volontaire** | **Pas** instance-safe — mute toutes les instances du symbole partagé |
| **`scope=hierarchy`** | **Dry-run / diagnostic uniquement** | Mutation → **`hierarchy_instance_write_unsupported`** |
| Statut MCP | **`hierarchy_assign_safe_limited`** | Inchangé jusqu’à API upstream validée |

**Preuve R157** : spike green avec **`instance_safe_write_supported=false`** — écriture ciblée A mute aussi B.

### Note design upstream

**`qa/tools/ipc_tools/MCP_UPSTREAM_FOOTPRINT_INSTANCE_SAFE_WRITE_DESIGN.md`**

| Section | Contenu |
|---------|---------|
| A | Problème symbole partagé + `SetFootprintFieldText` global |
| B | Preuve R157 fixture spike |
| C | API cible `SetFootprintFieldTextForSheet` / surcharge calquée `SetValueFieldText` |
| D | Sémantique isolation + save/load + undo |
| E | Critères validation upstream (gate R159/R160) |
| F | Impact MCP futur — déblocage hierarchy derrière preuve QA |

### Critères gate R159/R160 (documentés)

```text
instance_a_changed_ok=true
instance_b_unchanged_ok=true
readiness_reflects_instance_write=true
save_reload_preserved=true
undo_redo_instance_safe=true
```

Spike renouvelable : **`r157_spike_footprint_instance_safe_write.ps1`**.

### Implémentation R158

- **Documentation uniquement** — aucun handler C++, schéma MCP, critère green modifié.

- **Date** : 2026-06-07

## R159 — Prototype KiCad `SetFootprintFieldTextForSheet` (sans activation MCP)

### Objectif

Prototype upstream **Option A** : `SCH_SYMBOL::SetFootprintFieldTextForSheet` + stockage **`SCH_SYMBOL_INSTANCE::m_Footprint`** / **`m_FootprintOverride`** ; exposition **QA-only** via **`qa_spike_footprint_instance_api`**.

### Implémentation

| Composant | Détail |
|-----------|--------|
| **API** | **`SetFootprintFieldTextForSheet(footprint, sheetPath, variant)`** — retourne `false` si instance absente |
| **Lecture** | **`GetFootprintFieldText(true, &path, false)`** + **`GetFieldText` FOOTPRINT** + **`sch_field` getUnescapedText** |
| **Persistance** | Parser/writer `(instances … (path … (footprint …)))` — instance override, plus global **`SetFootprintFieldText`** au parse legacy default |
| **QA tool** | **`qa_spike_footprint_instance_api`** — actions `set_footprint_for_sheet` \| `save` \| `undo` \| `redo` |
| **MCP public** | **`scope=hierarchy` mutation inchangée** — **`hierarchy_instance_write_unsupported`** |

### Spike QA

Script : **`r159_spike_footprint_instance_safe_api.ps1`**

Summary : **`qa_r159_spike_footprint_instance_safe_api_summary.txt`**

Gate critères R158 : **`instance_a_changed_ok`**, **`instance_b_unchanged_ok`**, **`save_reload_preserved`**, **`undo_redo_instance_safe`**.

### Résultats QA (2026-06-07)

| Critère | Valeur |
|---------|--------|
| **`r159_spike_green`** | **true** |
| **`instance_safe_api_prototyped`** | **true** |
| **`prototype_option`** | **A** (+ stockage instance C) |
| **`instance_safe_write_supported`** | **true** (API prototype QA-only) |
| **`instance_a_changed_ok`** | **true** |
| **`instance_b_unchanged_ok`** | **true** |
| **`readiness_reflects_instance_write`** | **true** |
| **`save_reload_preserved`** | **true** |
| **`undo_redo_instance_safe`** | **true** |
| **`mcp_hierarchy_mutation_activated`** | **false** |

Run : `build/msvc-win64-release/ipc-v1-artifacts/20260607-135345288-r159-footprint-instance-safe-api-spike`

### Non-régression

| Suite | Résultat |
|-------|----------|
| R157 (chemin MCP public global) | green — **`instance_safe_write_supported=false`** inchangé |
| R156 workflow | **`r156_workflow_green=true`** |
| assign probe | **`schematic_assign_footprint_green=true`** |
| readiness probe | **`schematic_footprint_readiness_green=true`** |
| MCP v2 checkpoint | **9 familles green** |
| CI VerifyOnly | **`MCP_V2_CI_BASELINE_OK=1`** |

### Décision

**R159 validée** : l’isolation A/B est prouvée via **`SetFootprintFieldTextForSheet`** ; le chemin MCP public **`schematic_assign_footprint_session`** reste global et bloqué en **`scope=hierarchy`**. Gate R160 : activer mutation hierarchy MCP derrière QA strict.

### Recommandation R160

- Si spike R159 green : activer **`scope=hierarchy` mutation** MCP derrière gate QA strict (assign probe + R159 + R157 non-régression).
- Sinon : itérer upstream save/undo avant MCP.

- **Date** : 2026-06-07

## R160 — Activation MCP `scope=hierarchy` via `SetFootprintFieldTextForSheet`

### Objectif

Brancher **`schematic_assign_footprint_session scope=hierarchy`** sur l’API instance-safe R159 ; **`global_update=false`** enforced ; restore footprint vide autorisé en hierarchy.

### Implémentation handler

| Élément | Détail |
|---------|--------|
| **Mutation** | **`SCH_COMMIT`** + **`SetFootprintFieldTextForSheet`** |
| **Sortie** | **`instance_safe_write_used`**, **`shared_sheet_detected`**, **`global_update=false`**, **`affected_instances_count=1`** |
| **Blockers retirés (cas supporté)** | **`hierarchy_instance_write_unsupported`** |
| **Blockers ajoutés** | **`instance_safe_write_unavailable`**, **`instance_safe_write_failed`**, **`post_mutation_instance_readback_mismatch`** |
| **`current_sheet`** | **Inchangé** — garde-fou R146 conservé |
| **`allow_shared_symbol_global_update`** | **Ignoré** en hierarchy |

### Résultats QA (2026-06-07)

| Suite | Résultat |
|-------|----------|
| **`targeted_schematic_assign_footprint_hierarchy_probe.ps1`** | **green** — isolation A/B + restore MCP |
| assign probe | **`schematic_assign_footprint_green=true`**, **`hierarchy_instance_safe_ok=true`** |
| R159 / R157 / R156 | **green** (R157 : **`instance_safe_write_supported=false`** sur `current_sheet` global) |
| readiness / navigate / active / snapshot | **green** |
| MCP v2 checkpoint | **9 familles green** |
| CI VerifyOnly | **`MCP_V2_CI_BASELINE_OK=1`** |

### Décision

**R160 validée** : mutation hierarchy MCP **activée** et instance-safe via **`instance_path`**.

### Recommandation R161

- Workflow agent : readiness → assign **`scope=hierarchy`** si **`instance_path`** connu.
- Documenter undo/redo + save explicite pour mutations hierarchy.
- R156 reste workflow conservateur recommandé.

- **Date** : 2026-06-07

## R161 — Mini-release « Hierarchy Footprint Assignment enabled »

### Objectif

Officialiser statut **`hierarchy_assign_instance_safe_enabled`** ; documenter workflows IA A/B et limites save/undo ; **documentation uniquement**.

### Livrables

| Document | Mise à jour |
|----------|-------------|
| **`MCP_V1_OFFICIAL_MANIFEST.md`** | Statut **`hierarchy_assign_instance_safe_enabled`** |
| **`MCP_KICAD_USER_DEV_GUIDE.md`** | Workflow A (direct) + Workflow B (R156) + save/undo |
| **`README_IPC_QA_BASELINE.md`** | Gate QA R161 |
| **`MCP_V1_CHECKPOINT_RELEASE_NOTE.md`** | Entrée R161 |
| **`MCP_TOOL_MAP_R1.tsv`** | Tag **`R161_hierarchy_assign_instance_safe_enabled`** |

### Statut produit

- **`scope=hierarchy`** : mutation instance-safe (**R160**) — **`instance_safe_write_used=true`**, **`global_update=false`**
- **`scope=current_sheet`** : inchangé (R146)
- **`allow_shared_symbol_global_update`** : **`current_sheet` only**
- **`hierarchy_instance_write_unsupported`** : fallback legacy uniquement

### Décision

**R161 validée** — capacité hierarchy assign **officialisée** documentation ; handlers **inchangés** ; critères green MCP v2 **inchangés**.

### Recommandation R162

- Manifest runtime / snapshot projet exposant **`hierarchy_assign_instance_safe_enabled`**
- Tool MCP save/undo **uniquement** si contrat produit explicite

- **Date** : 2026-06-07

## R162 — Runtime status `hierarchy_assign_instance_safe_enabled`

### Audit options

| Option | Verdict |
|--------|---------|
| **A** — champ dans snapshot racine | Rejeté — hub existant = **`project_readiness_summary`** |
| **B** — champs dans **`project_readiness_summary`** | **Retenu** — point d'entrée IA readiness |
| **C** — manifest capabilities séparé | Absent dans codebase |
| **D** — documentation-only | Rejeté — objectif = discovery runtime |

### Implémentation

Emplacement : **`buildProjectReadinessSummary`** dans **`common/mcp/mcp_executor.cpp`**.

Champs additifs (toujours présents si snapshot readiness émis) :

- **`hierarchy_assign_instance_safe_enabled`** = `true`
- **`recommended_footprint_assignment_workflow`** = `hierarchy_instance_safe`
- **`fallback_footprint_assignment_workflow`** = `navigate_current_sheet_dry_run`
- **`recommended_assign_tool`** = `schematic_assign_footprint_session`
- **`recommended_assign_scope`** = `hierarchy`

Flatten QA : **`ipc_smoke.cpp`** → `readiness_*` ; probe **`hierarchy_assign_runtime_ok`**.

### Décision

**R162 validée** — statut runtime machine-readable sans nouvelle mutation ; critères green MCP v2 **inchangés**.

### Recommandation R163

- Consommer champs R162 dans agents/scripts (branch Workflow A vs B automatique)
- Évaluer duplication dans réponse **`schematic_footprint_readiness_session`** si agents contournent snapshot

- **Date** : 2026-06-07

## R163 — Consommer statut runtime R162 dans workflow IA

### Objectif

Prouver qu'un agent peut lire **`project_readiness_summary`** (champs R162) et choisir automatiquement le workflow footprint recommandé, sans modifier handlers C++ ni critères green MCP v2.

### Implémentation

Script QA : **`r163_example_runtime_selected_footprint_workflow.ps1`**

| Étape | Action |
|-------|--------|
| A | **`get_project_context_snapshot`** → lire champs R162 |
| B | Si **`hierarchy_assign_instance_safe_enabled=true`** + **`recommended_assign_scope=hierarchy`** → Workflow A |
| C | Workflow A : readiness hierarchy + assign hierarchy **dry-run** ; **`selected_workflow=hierarchy_instance_safe`** |
| D | Workflow B : fallback **`navigate_current_sheet_dry_run`** documenté ; exécutable via **`r156_example_navigable_sch_footprint_workflow.ps1`** (non supprimé) |

Summary : **`qa_r163_runtime_selected_footprint_workflow_summary.txt`**

### Décision

**R163 validée** — exemple runtime-driven ; handlers C++ **inchangés** ; checkpoint MCP v2 **inchangé, 9 familles** ; critères green **inchangés**.

### Recommandation R164

- Intégrer **`r163_example_runtime_selected_footprint_workflow.ps1`** dans CI baseline optionnelle (post-checkpoint advisory)
- Évaluer echo champs R162 dans **`schematic_footprint_readiness_session`** si agents contournent snapshot

- **Date** : 2026-06-07

## R164 — Intégrer R163 en CI observability-only

### Objectif

Surveiller automatiquement que le runtime MCP expose le workflow footprint recommandé et qu'un agent peut sélectionner Workflow A, via **`run_mcp_v2_ci_baseline.ps1`**, sans modifier **`ci_baseline_green`** ni critères green MCP v2.

### Implémentation

| Élément | Détail |
|---------|--------|
| Fonction | **`Invoke-RuntimeFootprintWorkflowReport`** dans **`run_mcp_v2_ci_baseline.ps1`** |
| Position | **G4** — après checkpoint MCP v2 (G) et advisory SCH (G2/G3) |
| Script enfant | **`r163_example_runtime_selected_footprint_workflow.ps1`** via **`Invoke-IpcQaChildScript`** |
| Gate | **Observability-only** — échec R163 → **`runtime_footprint_workflow_ok=false`**, **`ci_baseline_green` inchangé** |
| **`-VerifyOnly`** | R163 **non lancé** ; champs **`runtime_footprint_workflow_*=N/A`** |

Champs CI : **`runtime_footprint_workflow_ok`**, **`runtime_footprint_workflow_green`**, **`runtime_footprint_workflow_selected_workflow`**, etc.

### Décision

**R164 validée** — R163 intégré CI baseline observability-only ; handlers C++ **inchangés** ; checkpoint MCP v2 **inchangé, 9 familles** ; critères green **inchangés**.

### Recommandation R165

- Évaluer echo champs R162 dans **`schematic_footprint_readiness_session`** si agents contournent snapshot
- Dashboard/alerting sur **`runtime_footprint_workflow_ok=false`** en CI (hors gate)

- **Date** : 2026-06-07

## R165 — Echo runtime workflow dans `schematic_footprint_readiness_session`

### Audit options

| Option | Verdict |
|--------|---------|
| **A** — duplication locale `api_handler_sch.cpp` | Rejeté — risque divergence R162 |
| **B** — helper partagé minimal | **Retenu** — **`include/mcp/mcp_footprint_assignment_workflow.h`** |
| **C** — doc-only | Rejeté — objectif = echo runtime direct-readiness |

### Implémentation

Helper **`appendFootprintAssignmentWorkflowDiscoveryFields`** utilisé par :

- **`buildProjectReadinessSummary`** (`mcp_executor.cpp`, R162 refactor)
- **`SchematicFootprintReadinessSessionToolInvoke`** success path (`api_handler_sch.cpp`)

Champs top-level (tous scopes / validate modes, success path) :

- **`hierarchy_assign_instance_safe_enabled`**
- **`recommended_footprint_assignment_workflow`**
- **`fallback_footprint_assignment_workflow`**
- **`recommended_assign_tool`**
- **`recommended_assign_scope`**

Flatten QA : **`ipc_smoke.cpp`** → `footprint_readiness_*` ; probe **`runtime_workflow_echo_ok`**.

### Décision

**R165 validée** — echo runtime readiness sans mutation ; critères green MCP v2 **inchangés**.

### Recommandation R166

- Option agent : lire workflow depuis readiness **ou** snapshot (priorité snapshot si les deux appelés)
- Dashboard alerting CI **`runtime_footprint_workflow_ok=false`**

- **Date** : 2026-06-07

## R166 — Cohérence dual-source workflow footprint runtime

### Objectif

Formaliser la priorité agent entre **`project_readiness_summary`** (source principale) et **`schematic_footprint_readiness_session`** (source locale), et prouver read-only que les 5 champs R162 sont cohérents.

### Politique agent

| Situation | Action |
|-----------|--------|
| Choix workflow | Lire d'abord **`project_readiness_summary`** |
| Agent déjà dans readiness | Utiliser echo local R165 |
| Divergence snapshot vs readiness | **Ne pas muter** ; relire snapshot ; signaler incohérence |

### Implémentation

Script : **`report_footprint_workflow_discovery_consistency.ps1`**

- Appelle snapshot + readiness hierarchy (fixture **`ipc_footprint_readiness_v1`**)
- Compare 5 champs ; summary **`qa_footprint_workflow_discovery_consistency_summary.txt`**
- Standalone : **`footprint_workflow_discovery_consistency_green=false`** → exit 1
- **Hors gate MCP v2** ; intégration CI future = observability-only

### Décision

**R166 validée** — dual-source documenté ; contrôle QA read-only ; handlers C++ **inchangés** ; critères green **inchangés**.

### Recommandation R167

- Intégrer **`report_footprint_workflow_discovery_consistency.ps1`** en CI baseline advisory (post-G4)
- Alerting si **`footprint_workflow_discovery_consistency_green=false`**

- **Date** : 2026-06-07

## R167 — CI advisory dual-source footprint workflow consistency

### Objectif

Surveiller automatiquement la cohérence snapshot vs readiness via **`run_mcp_v2_ci_baseline.ps1`**, sans modifier **`ci_baseline_green`** ni critères green MCP v2.

### Implémentation

| Élément | Détail |
|---------|--------|
| Fonction | **`Invoke-FootprintWorkflowDiscoveryConsistencyReport`** |
| Position | **G5** — après G4 (R163 runtime workflow) |
| Script enfant | **`report_footprint_workflow_discovery_consistency.ps1`** |
| Gate | **Observability-only** — échec → **`footprint_workflow_discovery_consistency_ok=false`**, **`ci_baseline_green` inchangé** |
| **`-VerifyOnly`** | Report **non lancé** ; champs **`N/A`** |
| **`-SkipCheckpoint`** | Champs **`skipped`** (convention R164) |

Champs CI : **`footprint_workflow_discovery_consistency_*`**.

### Décision

**R167 validée** — dual-source consistency intégré CI advisory ; handlers C++ **inchangés** ; checkpoint MCP v2 **inchangé, 9 familles** ; critères green **inchangés**.

### Recommandation R168

- Dashboard/alerting sur **`footprint_workflow_discovery_consistency_ok=false`** en CI
- Évaluer factorisation fixture hiérarchie partagée entre R163/R166/R167

- **Date** : 2026-06-07

## R168 — Politique d'alerte observability footprint workflow (clôture R162–R167)

### Objectif

Documenter la politique d'alerte pour les signaux CI **observability-only** G4 (`runtime_footprint_workflow_*`) et G5 (`footprint_workflow_discovery_consistency_*`), sans modifier handlers, scripts CI (comportement), ni critères green.

### Principe fondamental

**`ci_baseline_green=true` ne suffit pas** pour ignorer un advisory rouge G4/G5 : le gate principal couvre probes PCB + checkpoint MCP v2 ; G4/G5 signalent la santé du workflow footprint runtime **en parallèle**.

### Source de vérité agent

| Rôle | Source |
|------|--------|
| **Principale** | **`project_readiness_summary`** (`get_project_context_snapshot`) |
| **Locale** | **`schematic_footprint_readiness_session`** (echo R165) |
| **Contrôle CI** | G5 (`footprint_workflow_discovery_consistency_*`) |
| **Divergence** | Ne pas muter ; relire snapshot ; consulter G5 ; ouvrir ticket |

Politique agent : **`do_not_mutate_and_recheck_snapshot`**.

### Signaux G4 (`runtime_footprint_workflow_*`)

| Champ | Politique |
|-------|-----------|
| **`runtime_footprint_workflow_ok=false`** | Alerte CI advisory — lire **`runtime_footprint_workflow_target_run_dir`** + summary enfant |
| **`runtime_footprint_workflow_green=false`** | Release hold manuel — ne pas publier release MCP sans analyse |
| **`runtime_footprint_workflow_mutation_performed=true`** | **Incident QA** — R163 doit rester dry-run only |
| **`selected_workflow != hierarchy_instance_safe`** avec runtime enabled | Divergence workflow — investiguer |
| **`runtime_footprint_workflow_blocker != N/A`** | Lire blocker dans summary enfant R163 |

### Signaux G5 (`footprint_workflow_discovery_consistency_*`)

| Champ | Politique |
|-------|-----------|
| **`footprint_workflow_discovery_consistency_ok=false`** | Alerte CI advisory |
| **`footprint_workflow_discovery_consistency_green=false`** | Ne pas muter footprint via agent autonome avant investigation |
| **`footprint_workflow_discovery_consistency_scope_match_ok=false`** | Divergence critique workflows recommandés |
| **`footprint_workflow_discovery_consistency_blocker != N/A`** | Lire report standalone summary |

### Niveaux d'alerte

| Niveau | Condition | Action |
|--------|-----------|--------|
| **Info** | G4/G5 = `N/A` en **`-VerifyOnly`** | Normal |
| **Warning** | G4 ou G5 **`ok=false`** en CI complet | Lire **`target_run_dir`** + summary |
| **Release hold manuel** | G4 ou G5 **`green=false`** sur branche release | Ne pas publier sans analyse |
| **Incident QA** | **`mutation_performed=true`** dans R163 / G4 | Stopper et corriger immédiatement |
| **Divergence agent** | snapshot/readiness mismatch (G5) | Ne pas muter ; relire snapshot ; ouvrir ticket |

### Décision

**R168 validée** — série **R162–R167** clôturée côté documentation alerte ; **aucun handler** ; **aucun critère green** modifié ; G4/G5 restent **non bloquants**.

### Recommandation R169

- Brancher alerting externe (dashboard CI) sur champs G4/G5
- Factoriser fixture hiérarchie partagée R163/R166/R167 (maintenance QA)

- **Date** : 2026-06-07

## R169 — Audit workflow post-footprint vers PCB (SCH→PCB sync, sans implémentation)

### Objectif

Auditer la faisabilité d'une capacité MCP permettant à une IA de continuer le flux **après assignation footprint SCH** : détecter ce qui manque côté PCB, synchroniser/importer les footprints depuis le schéma, ou proposer un workflow sûr vers le PCB. **Aucune implémentation** ; recommandation contrat **R170**.

### Contexte produit

- Série **R162–R168** clôturée : workflow footprint SCH observable, G4/G5 advisory, politique alerte documentée.
- Capacité SCH actuelle solide : `schematic_footprint_readiness_session`, `schematic_assign_footprint_session`, navigation/active sheet, `get_project_context_snapshot` + `sch_to_pcb_ready` (**côté SCH uniquement**).
- Prochaine question : après footprints assignés, comment avancer vers un **PCB cohérent** ?

### Fichiers inspectés (audit code KiCad + MCP)

| Zone | Fichiers |
|------|----------|
| Entrée UI / tool PCB | `pcbnew/tools/board_editor_control.cpp` (`UpdatePCBFromSchematic`), `pcbnew/tools/board_editor_control.h` |
| Dialog + dry-run | `pcbnew/dialogs/dialog_update_pcb.cpp`, `dialog_update_pcb.h` |
| Moteur sync | `pcbnew/netlist_reader/board_netlist_updater.{h,cpp}`, `pcbnew/netlist_reader/netlist.cpp` (`OnNetlistChanged`) |
| Fetch netlist live | `pcbnew/pcb_edit_frame.cpp` (`FetchNetlistFromSchematic`), `eeschema/cross-probing.cpp` (`MAIL_SCH_GET_NETLIST`) |
| Import fichier | `pcbnew/dialogs/dialog_import_netlist.cpp`, `pcbnew/netlist_reader/*` |
| Board / footprint | `pcbnew/board.cpp`, `pcbnew/footprint.h` (`GetPath`, `GetLink`, `GetReference`) |
| Connectivité | `pcbnew/connectivity/*` (post-update via `BuildConnectivity`, `testConnectivity`) |
| API MCP PCB | `pcbnew/api/api_handler_pcb.cpp` — **aucun** tool update-from-schematic |
| Orchestration MCP | `common/mcp/mcp_executor.cpp` (`sch_to_pcb_ready`, `workflow_handoff_readiness_session`) |
| Docs / QA | `MCP_V1_OFFICIAL_MANIFEST.md`, `MCP_TOOL_MAP_R1.tsv`, `MCP_KICAD_USER_DEV_GUIDE.md`, `README_IPC_QA_BASELINE.md` |

### Question 1 — API KiCad « Update PCB from schematic »

| Aspect | Constat |
|--------|---------|
| **Existe ?** | **Oui**, chaîne mature et utilisée en production GUI. |
| **Où ?** | `BOARD_EDITOR_CONTROL::UpdatePCBFromSchematic` → `PCB_EDIT_FRAME::FetchNetlistFromSchematic` → `DIALOG_UPDATE_PCB` → `BOARD_NETLIST_UPDATER::UpdateNetlist`. |
| **Liée UI ?** | **Oui** : dialog modal wx (`ShowModal`) ; dry-run automatique à l'ouverture (`PerformUpdate(true)`). |
| **Appel MCP possible ?** | **Oui techniquement** : extraire le noyau hors dialog (pattern déjà partiel dans `dialog_import_netlist.cpp` et `cross-probing.cpp`). Nécessite **PCB frame ouvert**, **projet KiCad** (pas standalone), **schéma annoté**, **Eeschema joignable** via Kiway (`MAIL_SCH_GET_NETLIST`). |
| **Dry-run ?** | **Oui** : `BOARD_NETLIST_UPDATER::SetIsDryRun(true)` ; messages via `REPORTER` (add/remove/reconnect nets, replace footprint, etc.). |
| **Bornage ?** | Options dialog mappées sur setters updater : `SetLookupByTimestamp`, `SetDeleteUnusedFootprints`, `SetReplaceFootprints`, `SetTransferGroups`, `SetOverrideLocks`, `SetUpdateFields`, `SetRemoveExtraFields`. |

**Prérequis runtime identifiés :**

1. `TestStandalone() > 0` — projet manager, pas pcbnew seul.
2. Schéma **fully annotated** (`ReadyToNetlist` côté eeschema).
3. Netlist live via `NETLIST_EXPORTER_KICAD` (pas de fichier intermédiaire obligatoire).
4. Mutation réelle : `m_commit.Push(_("Update Netlist"), …)` — **undo KiCad natif** via `BOARD_COMMIT`.
5. Post-commit : `OnNetlistChanged` — sync nets/netclasses, reinit DRC engine, **`SpreadFootprints`** nouveaux FP à l'origine, activation outil **move** (hors undo commit).

**Limite dry-run structuré :** compteurs `m_errorCount` / `m_warningCount` / `m_newFootprintsCount` et liste `GetAddedFootprints()` **non exposés** publiquement ; en dry-run les footprints ajoutés ne sont **pas** insérés dans `m_addedFootprints`. Preview MCP devra soit **sous-classe `REPORTER`**, soit **petits getters** R170.

### Question 2 — Read-only (sans netlist updater)

| Capacité | Faisable V1 | Mécanisme |
|----------|-------------|-----------|
| Footprints manquants sur board | **Partiel** | Comparer `schematic_footprint_readiness_session` (symboles + FPID) vs `pcb_get_board_items_session` (footprints par ref/path) — **pas de bool sync natif**. |
| Board synchronisé avec schéma | **Non direct** | KiCad n'expose pas `is_pcb_in_sync` ; heuristique = dry-run updater ou diff ref/KIID_PATH. |
| Compter symboles SCH vs FP PCB | **Oui** | Deux tools existants + agrégation MCP. |
| Lier symbole ↔ footprint | **Oui** | Matching updater : **`KIID_PATH`** (`footprint->GetPath()` vs `component->GetPath()+uuid`) ou **reference** si `lookup_by_timestamp=false`. `GetLink()` disponible mais matching updater utilise surtout `GetPath()`. |

**Signal existant à ne pas confondre :** `sch_to_pcb_ready` (snapshot / readiness) = footprints SCH **présents** + chemin PCB projet — **ne garantit pas** que le PCB contient les composants.

### Question 3 — Preview / dry-run

| Sortie | Faisable | Source |
|--------|----------|--------|
| Footprints à ajouter | **Oui** | Messages reporter « Add %s (footprint '%s'). » + `m_newFootprintsCount` (interne) |
| Nets à mettre à jour | **Oui** | Reporter « Connect/Reconnect … pin … » ; cache dry-run `m_padNets` |
| Footprints orphelins | **Oui** | `SetDeleteUnusedFootprints(true)` → « Remove unused footprint %s. » en dry-run |
| Estimation sans commit | **Oui** | `SetIsDryRun(true)` + `UpdateNetlist` — **aucun** `m_commit.Push` |

**Risque preview :** `FetchNetlistFromSchematic` peut ouvrir dialog d'erreur GUI (`DisplayErrorMessage`) si annotation/netlist échoue — handler MCP devra **supprimer/rediriger** ces modales (pattern déjà utilisé ailleurs MCP avec `RunMainStack`).

### Question 4 — Mutation contrôlée

| Capacité | Faisable | Notes |
|----------|----------|-------|
| Importer footprints manquants | **Oui** | `addNewFootprint` + commit |
| Conserver positions existantes | **Oui** | Footprints matchés : update params/nets, pas repositionnement |
| Placer nouveaux hors board / staging | **Partiel** | Défaut KiCad : `estimateFootprintInsertionPosition()` + `SpreadFootprints` à (0,0) — **pas de zone staging** native ; R170 pourrait ajouter option MCP **staging_bbox** (nouveau code). |
| Undo/redo | **Oui** (commit) | `BOARD_COMMIT::Push("Update Netlist")` ; **non** pour spread/move post-update |
| Restore QA | **Oui** | Pattern session gate existant (`execute` + rollback probe) applicable en R170+ ; **hors gate v2** au début si mutation. |

### Comparaison options A / B / C / D

| Critère | **A** `get_pcb_sync_readiness_session` | **B** `pcb_update_from_schematic_preview_session` | **C** `pcb_update_from_schematic_session` | **D** enrichir snapshot `pcb_sync_summary` |
|---------|----------------------------------------|-----------------------------------------------------|---------------------------------------------|---------------------------------------------|
| Bénéfice IA | Orientation rapide, faible risque | Plan d'action détaillé, dry-run | Exécution one-shot | Contexte agent sans appel dédié |
| Risque mutation | **Nul** | **Nul** (dry-run) | **Élevé** (nets, delete orphans, zone fill) | **Nul** si read-only |
| API KiCad | Compteurs + diff léger | **`BOARD_NETLIST_UPDATER` dry-run** | Même moteur + `OnNetlistChanged` | Délègue A ou B |
| Testabilité QA | **Forte** | **Forte** (messages structurés) | Complexe (undo, spread, DRC) | **Forte** (opt-in snapshot) |
| Checkpoint MCP v2 | Compatible (nouvelle famille read-only) | Compatible preview | **Reporter gate v2** recommandé | Compatible opt-in |
| Dry-run | N/A (read-only) | **Natif** | Via preview d'abord | Dépend A/B |
| Restore fiable | N/A | N/A | Commit oui ; side-effects partiels | N/A |

### Recommandation R170 (phased)

**Phase 1 — Option A (+ D opt-in)** puis **Phase 2 — Option B** ; **Option C repoussée** tant que preview + restore QA non verts.

1. **`get_pcb_sync_readiness_session` (A)** — read-only, `board_filename` absolu, projet ouvert PCB+SCH :
   - Compteurs : `sch_symbols_with_footprint_count`, `pcb_footprint_count`, `missing_on_pcb_count`, `orphan_on_pcb_count`, `reference_mismatch_count` (heuristique ref/path).
   - Flags : `sch_to_pcb_ready` (echo readiness), `pcb_sync_likely` (heuristique conservative), `exact_blocker_if_any`.
   - **Sans** mutation ; **sans** gate v2 initial.

2. **`pcb_update_from_schematic_preview_session` (B)** — dry-run :
   - Entrées : `board_filename`, options miroir dialog (`delete_extra_footprints`, `update_footprints`, `match_by_reference`, …), `max_report_lines`.
   - Sortie structurée : `actions[]` (kind, reference, footprint, net, severity), `new_footprints_count`, `removed_footprints_count`, `net_changes_count`, `error_count`, `warning_count`, `report_text` (optionnel).
   - Implémentation : `FetchNetlistFromSchematic` + `BOARD_NETLIST_UPDATER` + **REPORTER MCP** (pas de dialog).

3. **Snapshot (D)** — opt-in `include_pcb_sync_summary=true` dans `get_project_context_snapshot` ; agrège A (ou B léger) dans `project_readiness_summary.pcb_sync_summary`.

4. **Mutation (C)** — **R171+** seulement si B stable : `execute` + `allow_single_correction` + restore QA ; documenter side-effects `SpreadFootprints` / move tool.

### Contrat R170 proposé (schéma indicatif — implémentation R170)

**Tool A — `get_pcb_sync_readiness_session`**

```json
{
  "board_filename": "string (absolute, required)",
  "match_mode": "timestamp_path | reference",
  "include_symbol_footprint_pairs": false,
  "max_pairs": 50
}
```

**Réponse clés :** `ok`, `sch_symbols_with_footprint_count`, `pcb_footprint_count`, `missing_on_pcb_count`, `orphan_on_pcb_count`, `pcb_sync_likely`, `sch_to_pcb_ready`, `exact_blocker_if_any`, `diagnostics`.

**Tool B — `pcb_update_from_schematic_preview_session`**

```json
{
  "board_filename": "string (absolute, required)",
  "delete_extra_footprints": false,
  "update_footprints": true,
  "match_by_reference": false,
  "transfer_groups": false,
  "override_locks": false,
  "update_fields": false,
  "max_actions": 200
}
```

**Réponse clés :** `preview_ok`, `actions[]`, `new_footprints_count`, `orphan_footprints_count`, `net_change_count`, `error_count`, `warning_count`, `exact_blocker_if_any`, `session_log`.

**Politique agent post-R168 :**

1. `get_project_context_snapshot` + `include_footprints_summary=true` → si `sch_to_pcb_ready=false`, **ne pas** tenter PCB sync.
2. Si `sch_to_pcb_ready=true` → **A** puis **B** ; mutation **interdite** autonome tant que preview ≠ vert ou divergence G5.
3. `workflow_handoff_readiness_session` (`handoff_target=sch_to_pcb`) reste **workflow gate** générique — **ne remplace pas** A/B.

### Fixtures QA recommandées (R170)

| Fixture | But |
|---------|-----|
| **`ipc_pcb_sync_v1_empty`** | Projet dérivé `ipc_footprint_readiness_v1` + PCB vide ou sans FP — missing count > 0 |
| **`ipc_pcb_sync_v1_synced`** | Même projet après update manuelle GUI — missing = 0, preview actions vides |
| **`ipc_pcb_sync_v1_orphan`** | PCB avec FP extra non présent netlist — orphan > 0 en A/B |
| Probes | `targeted_pcb_sync_readiness_probe.ps1`, `targeted_pcb_update_preview_probe.ps1` |
| CI | Checkpoint MCP v2 **non régression** ; **pas** gate v2 mutation au début |

### Décision

**R169 validée (audit-only)** — API KiCad **fiable** pour sync SCH→PCB via `BOARD_NETLIST_UPDATER` ; dry-run **natif** ; mutation **possible** mais **risquée** pour V1 MCP ; **handlers C++ non modifiés** ; **schémas MCP non modifiés** ; **critères green inchangés** ; **checkpoint MCP v2 inchangé**.

**Option recommandée R170 : A puis B** ; D en opt-in snapshot ; C différé.

- **Date** : 2026-06-07

## R170 — `get_pcb_sync_readiness_session` V1 read-only (Option A)

### Objectif

Implémenter **Option A** R169 : tool MCP read-only comparant footprints SCH assignés vs footprints PCB, sans mutation, sans `BOARD_NETLIST_UPDATER`, hors checkpoint MCP v2.

### Choix retenu

- **Tool** : `get_pcb_sync_readiness_session`
- **SCH** : délégation interne à `schematic_footprint_readiness_session` (`scope=hierarchy`, `include_sheet_paths=true`)
- **PCB** : scan `BOARD::Footprints()` via `PCB_EDIT_FRAME` ouvert
- **Matching V1** : `reference_or_path` (défaut) — path si présent, sinon reference (case-insensitive)
- **Sémantique** :
  - `pcb_sync_likely=true` ⇔ expected>0, missing=0, orphan=0
  - `needs_update_from_schematic=true` ⇔ missing>0 ou (PCB vide et `sch_to_pcb_ready`)
  - Orphelins ⇒ `pcb_sync_likely=false`, `recommended_next_action=review_orphans`
- **Handler** : `pcbnew/api/api_handler_pcb_sync_readiness.cpp` ; callback `mcp_pcb_sync_readiness_callback`
- **QA** : fixtures `qa/data/pcbnew/sync/ipc_pcb_sync_v1_{empty,synced,orphan}/` ; probe `targeted_pcb_sync_readiness_probe.ps1`
- **Checkpoint MCP v2** : **non intégré** (famille candidate R171+)

### Décision

**R170 validée** — Option A livrée read-only ; preview (Option B) repoussée **R171**.

### Recommandation R171

- `pcb_update_from_schematic_preview_session` via `BOARD_NETLIST_UPDATER::SetIsDryRun(true)` + reporter structuré
- Opt-in `include_pcb_sync_summary` dans snapshot (Option D)

- **Date** : 2026-06-07

## R171 — `pcb_update_from_schematic_preview_session` V1 dry-run (Option B)

### Objectif

Implémenter **Option B** R169/R170 : preview MCP read-only via **`BOARD_NETLIST_UPDATER::SetIsDryRun(true)`**, reporter structuré, sans mutation PCB, sans dialog, sans `BOARD_COMMIT::Push`, hors checkpoint MCP v2.

### Choix retenu

- **Tool** : `pcb_update_from_schematic_preview_session`
- **Moteur** : `FetchNetlistFromSchematic` + `BOARD_NETLIST_UPDATER` avec **`SetIsDryRun(true)`**
- **Reporter** : `MCP_PCB_UPDATE_PREVIEW_REPORTER` — catégorise messages en `actions[]` + compteurs `would_*`
- **Garde-fou mutation** : comptage footprints avant/après ; champ **`board_mutation_detected`**
- **Options V1** : `delete_orphans`, `replace_footprints`, `match_mode` ; `delete_unused_nets` documenté non évalué en dry-run KiCad
- **`pcb_update_needed` V1** : delta **footprints** uniquement (add / remove orphan / replace) — pas les actions net-only du dry-run
- **Fixtures synced/orphan** : `(path "/<symbol_uuid>")` sur footprints (netlist export root sheet → path vide + kiid symbole)
- **Handler** : `pcbnew/api/api_handler_pcb_update_preview.cpp` ; callback `mcp_pcb_update_preview_callback`
- **QA** : fixtures R170 `ipc_pcb_sync_v1_{empty,synced,orphan}` ; probe **`targeted_pcb_update_preview_probe.ps1`**
- **Checkpoint MCP v2** : **non intégré** (famille candidate ultérieure)

### Cas QA probe

| Fixture | Attendu |
|---------|---------|
| empty | `would_add_footprints_count >= 2`, `pcb_update_needed=true`, `board_mutation_detected=false` |
| synced | `pcb_update_needed=false`, `actions_count=0`, pas de mutation |
| orphan (`delete_orphans=true`) | `would_remove_orphan_footprints_count >= 1`, `recommended_next_action=review_orphans` |

### Décision

**R171 validée** — Option B livrée dry-run ; mutation **`pcb_update_from_schematic_session`** (Option C) repoussée **R172+**.

### Recommandation R172

- **`pcb_update_from_schematic_session`** avec gate explicite, rollback probe, hors checkpoint v2 initial
- Opt-in `include_pcb_sync_summary` dans snapshot (Option D R169)

- **Date** : 2026-06-07

## R172 — `pcb_sync_summary` opt-in dans `get_project_context_snapshot` (Option D)

### Objectif

Enrichir **`get_project_context_snapshot`** avec bloc opt-in **`pcb_sync_summary`**, dérivé de **`get_pcb_sync_readiness_session`** (R170) et optionnellement **`pcb_update_from_schematic_preview_session`** (R171), sans `actions[]` complètes, sans mutation PCB, hors checkpoint MCP v2.

### Choix retenu

- **Entrées** : `include_pcb_sync_summary` (défaut `false`), `pcb_sync_include_preview` (défaut `false`), `pcb_sync_board_filename` / `pcb_sync_schematic_filename` optionnels, `pcb_sync_match_mode`, `pcb_sync_max_items`
- **Résolution chemins** : overrides absolus validés ; sinon `snapshot_summary.open_pcb_board_absolute_path` + premier SCH ouvert — pas de devinette ambiguë
- **Délégation** : `CallTool(get_pcb_sync_readiness_session)` avec `include_items=false` ; preview avec `include_actions=false`
- **Sortie compacte** : compteurs + `recommended_next_tool` / `recommended_next_action` ; preview counts nullable si preview non demandée
- **Échec partiel** : readiness KO → `available=false` ; preview KO avec readiness OK → `preview_blocker` sans faire échouer le snapshot
- **Args invalides** : `invalid_pcb_sync_*` → bad request snapshot entier
- **QA** : **`targeted_project_context_snapshot_pcb_sync_probe.ps1`** ; fixtures `ipc_pcb_sync_v1_*`
- **Checkpoint MCP v2** : **non intégré**

### Décision

**R172 validée** — Option D livrée snapshot opt-in ; mutation **`pcb_update_from_schematic_session`** repoussée **R173+**.

### Recommandation R173

- **`pcb_update_from_schematic_session`** avec gate explicite, rollback probe, hors checkpoint v2 initial

- **Date** : 2026-06-07

## R173 — `pcb_update_from_schematic_session` prototype contrôlé avec gate rollback

### Objectif

Prototyper une mutation PCB contrôlée via **`BOARD_NETLIST_UPDATER::UpdateNetlist`** (commit réel), avec gate **`confirm_apply`**, preview gate R171, preuve restore fichier QA, hors checkpoint MCP v2.

### Audit mutation

- **`UpdateNetlist` non dry-run** : modifications via **`BOARD_COMMIT`** (`m_commit.Add/Remove/Push`) ; `OnNetlistChanged` post-commit (ratsnest, spread footprints, DRC engine refresh) — **move tool MCP ignoré**
- **Dry-run R171** : reporter seulement ; pas de commit ; footprints temporaires détruits
- **Rollback QA** : restore fichier `.kicad_pcb` fixture golden (plus fiable que undo natif cross-session MCP)

### Choix retenu

- **Tool** : `pcb_update_from_schematic_session` ; callback GUI pcbnew
- **Gate** : `confirm_apply=false` (défaut) → `apply_confirmation_required` sans mutation
- **Preview gate** : `require_preview_green=true` (défaut) → dry-run interne avant apply ; échec → `preview_required_failed`
- **Orphelins** : `delete_orphans=false` par défaut
- **Post-apply** : compteurs before/after + `result_items` ; `recommended_next_action=review_orphans` si orphelins probables
- **Pas de save public** ; pas d'intégration checkpoint v2
- **QA** : **`targeted_pcb_update_from_schematic_probe.ps1`** — confirm / empty apply / restore / orphan safe
- **Fixtures** : footprints QA alignés bibliothèque `Resistor_SMD` (`R_0603_1608Metric` / `R_0201_0603Metric`)

### Décision

**R173 validée** — prototype mutation contrôlé livré ; mini-release documentaire repoussée **R174**.

### Recommandation R174

- Mini-release doc **`pcb_update_apply_controlled_v1`** — officialiser statut, workflow IA, limites ; **doc only**

- **Date** : 2026-06-07

## R174 — mini-release documentaire `pcb_update_apply_controlled_v1` (PCB Update Apply V1 controlled)

### Objectif

Clôturer R173 par une **mini-release documentaire** officialisant **`pcb_update_from_schematic_session`** comme capacité MCP de mutation PCB contrôlée — **sans nouveau code produit**, sans modification critères green MCP v2.

### Statut produit officialisé : `pcb_update_apply_controlled_v1`

| Aspect | Sémantique |
|--------|------------|
| Mutation | Uniquement via **`confirm_apply=true`** ; défaut → **`apply_confirmation_required`** |
| Preview | R171 préalable recommandée / utilisée (`require_preview_green=true` par défaut) |
| Orphelins | **`delete_orphans=false`** par défaut |
| Save | **Pas de save MCP public** |
| Restore | QA = restore fichier fixture golden ; **pas undo produit garanti** |
| Checkpoint | **Hors checkpoint MCP v2** (9 familles inchangées) |
| CI VerifyOnly | **Ne couvre pas** apply comme gate |

### Workflow IA recommandé

1. **`get_project_context_snapshot`** (`include_pcb_sync_summary=true`)
2. **`get_pcb_sync_readiness_session`**
3. **`pcb_update_from_schematic_preview_session`**
4. Si preview OK + confirmation explicite : **`pcb_update_from_schematic_session`** (`confirm_apply=true`)
5. **`get_pcb_sync_readiness_session`** post-apply
6. **`get_pcb_drc_violations_context_session`** (`run_drc=true`)

**Règles agent** : jamais apply sans preview récente ; jamais `confirm_apply=true` automatique ; orphelins conservés par défaut ; save/restore = stratégie dédiée future.

### Choix retenu

- **Périmètre** : docs manifest, guide, baseline, release note, tool map, décision R174
- **Hors périmètre** : handlers, schémas, checkpoint v2, critères green, save/undo, DRC gate
- **QA clôture** : apply probe + R171/R170/R172 + DRC v1 + R163 + checkpoint v2 + CI VerifyOnly

### Décision

**R174 validée** — mini-release doc **`pcb_update_apply_controlled_v1`** officialisée ; évolutions produit (save opt-in, DRC gate, checkpoint) repoussées **R175+**.

### Recommandation R175

- Évaluer **`save_board` opt-in** ou gate DRC post-apply produit ; décision séparée intégration checkpoint v2

- **Date** : 2026-06-07

## R175 — Audit save / undo / DRC gate post-apply PCB

### Objectif

Auditer les options de durcissement après **`pcb_update_from_schematic_session`** (save, undo, rollback, DRC gate) pour décider la suite **R176** — **audit doc only**, aucune implémentation.

### 1. Save board

| Question | Constat audit |
|----------|---------------|
| API directe ? | **Oui** — `PCB_EDIT_FRAME::SaveBoard()` → `SavePcbFile()` (`pcbnew/files.cpp`) ; IPC protobuf **`SaveDocument`** déjà dans `API_HANDLER_PCB::handleSaveDocument` → `frame()->SaveBoard()` |
| Sans dialogue GUI ? | **Partiel** — si board a un filename absolu et `SaveBoard(false)` : pas de `AskSaveBoardFileName` ; échecs (`IsWritable`, rules copy) peuvent appeler **`DisplayError`** (dialogue GUI) |
| Blockers propres ? | **Non aujourd'hui en MCP** — pas de tool save MCP ; erreurs GUI non structurées pour agent |
| Tool dédié vs option apply ? | **Recommandé : tool dédié** `save_board_session` (Option A) — séparation update mémoire / persistance disque |

**Blockers R176 proposés (save)** : `save_confirmation_required`, `board_not_loaded`, `board_filename_mismatch`, `board_file_not_writable`, `save_dialog_required`, `save_failed`, `pcb_context_unavailable`

### 2. Undo / redo

| Question | Constat audit |
|----------|---------------|
| Couvert par `BOARD_COMMIT` ? | **Partiel** — `UpdateNetlist` non dry-run pousse **`m_commit.Push("Update Netlist")`** ; footprints add/remove/replace via commit |
| Hors commit | **`OnNetlistChanged`** post-apply : `SynchronizeNetsAndNetClasses`, component classes, **DRC engine InitEngine**, **SpreadFootprints**, **Compile_Ratsnest**, sélection — **non inclus dans l'undo unitaire** |
| Pilotage MCP | **Possible techniquement** via `toolMgr->RunAction(ACTIONS::undo)` (`PCB_CONTROL::Undo` → `RestoreCopyFromUndoList`) ; `GetUndoCommandCount()` disponible |
| Undo post-update fiable ? | **Non garanti** — undo restaure l'état commit mais pas spread/ratsnest/DRC/netclass sync post-commit ; MCP apply **skip move tool** mais spread reste exécuté |
| QA rollback | **Garder restore fichier fixture** (R173) comme preuve QA ; undo produit = **hors scope R176 prioritaire** |

### 3. DRC gate post-apply

| Question | Constat audit |
|----------|---------------|
| Appel fiable après apply ? | **Oui** — `get_pcb_drc_violations_context_session` + **`run_drc=true`** exécute `DRC_ENGINE::RunTests` (`api_handler_pcb_drc_violations_context.cpp`) ; même contraintes GUI (board ouvert, filename match) |
| Gate bloquant `require_drc_clean_after_apply` ? | **Déconseillé R176** — flaky (durée, règles projet, violations préexistantes), hors périmètre checkpoint apply |
| Champ advisory | **Recommandé** — opt-in **`include_post_apply_drc_summary`** sur apply (délégation read-only DRC, compteurs bornés) **ou** workflow documenté step 6 (déjà R174) |
| Risque runtime | **Oui** sur gros boards — mitiger `max_violations`, timeout probe, **jamais bloquant apply** en V1 |

### 4. Comparaison options R176

| Option | Verdict R175 |
|--------|--------------|
| **A** `save_board_session` | **Priorité R176** — tool dédié, `confirm_save=true`, réutilisable, contrat clair ; risque mutation disque **mitigé par gate** |
| **B** `save_after_apply` | **Rejeté primaire** — mélange update + persistance ; garder apply = mémoire only |
| **C** `post_apply_drc_gate` | **Secondaire R176/R177** — **advisory opt-in only** ; pas de gate bloquant V1 |
| **D** `undo_last_board_action_session` | **Rejeté R176** — stack undo non bornée au seul netlist update ; effets post-commit non undo-safe |
| **E** pas de save/undo MCP | **Statut quo undo** ; save reste gap produit → **A combler en R176** |

### Choix retenu R176 (recommandation)

1. **Implémenter Option A** — `save_board_session` avec gate **`confirm_save=true`**, headless quand filename connu + writable, blockers structurés, **hors checkpoint v2**.
2. **Option C advisory** — opt-in **`include_post_apply_drc_summary`** sur apply (délégation DRC existante, pas de blocage) — **même release ou R177** si scope serré.
3. **Ne pas implémenter** undo MCP ni save inline dans apply.
4. **Ne pas intégrer** apply/save/DRC post-apply au checkpoint MCP v2 en R176.

### Contrat R176 proposé (Option A)

```json
{
  "board_filename": "path/to/board.kicad_pcb",
  "confirm_save": false
}
```

Sortie : `ok`, `saved`, `board_filename`, `exact_blocker_if_any`, `diagnostics`

### QA R176 proposée

- Probe **`targeted_pcb_save_board_probe.ps1`** : confirm required / save OK / readonly blocker / restore fixture
- Non-régression : apply R173, preview R171, sync R170, snapshot R172, DRC v1, checkpoint v2 **9 familles**, CI VerifyOnly

### Ce qu'il ne faut pas faire (R176)

- Pas de `confirm_apply=true` automatique couplé au save
- Pas de undo MCP comme rollback QA
- Pas de gate DRC bloquant apply
- Pas d'intégration checkpoint v2 sans décision dédiée
- Pas de modification critères green

### Décision

**R175 validée** — audit livré ; **R176 prioritaire = Option A `save_board_session`** + **Option C advisory** (opt-in, non bloquant).

### Recommandation R176

- Livrer **`save_board_session`** + opt-in **`include_post_apply_drc_summary`** sur apply ; probes dédiées ; hors checkpoint v2

- **Date** : 2026-06-07

## R176 — `save_board_session` + DRC advisory post-apply

### Objectif

Implémenter **`save_board_session`** (gate **`confirm_save=true`**) et opt-in advisory **`include_post_apply_drc_summary`** sur **`pcb_update_from_schematic_session`** — **hors checkpoint MCP v2**, critères green inchangés.

### Livrables

| Composant | Fichier(s) |
|-----------|------------|
| Handler save | `pcbnew/api/api_handler_pcb_save_board.cpp` |
| Callback | `include/mcp/mcp_pcb_save_board_callback.h`, `common/mcp/mcp_pcb_save_board_callback.cpp` |
| Registry MCP | `common/mcp/mcp_executor.cpp` |
| DRC post-apply | `pcbnew/api/api_handler_pcb_update_from_schematic.cpp` |
| Probe save | `targeted_pcb_save_board_probe.ps1` |
| Probe apply DRC | extension `targeted_pcb_update_from_schematic_probe.ps1` |
| Smoke | `ipc_smoke.cpp` lignes `save_board_*` et `pcb_update_post_apply_drc_*` |

### Contrat `save_board_session`

Entrée : `board_filename` (requis), `confirm_save` (default `false`).

Blockers : `save_confirmation_required`, `missing_or_invalid_board_filename`, `board_filename_mismatch`, `pcb_context_unavailable`, `board_not_loaded`, `board_has_no_filename`, `save_failed`, `save_verification_failed`.

Sortie : `ok`, `saved`, `confirm_save`, `file_mtime_before/after`, `file_size_before/after`, `exact_blocker_if_any`, `diagnostics`.

Implémentation : `SavePcbFile` headless si filename connu + `IsWritable` ; vérification post-save (mtime/size).

### Contrat DRC advisory post-apply

Entrée additionnelle apply : `include_post_apply_drc_summary` (default `false`), `post_apply_drc_max_violations` (default `20`).

Sortie : `post_apply_drc_summary_present`, `post_apply_drc_summary{...}`, `post_apply_drc_blocker`.

Sémantique : délégation **`PcbDrcViolationsContextSessionToolInvoke`** avec `run_drc=true` ; **apply reste `ok=true`** si apply réussi même si DRC échoue ; `post_apply_drc_blocker` rempli advisory.

### Hors périmètre respecté

- Pas undo MCP
- Pas `save_after_apply` automatique
- Pas gate DRC bloquant apply
- Pas intégration checkpoint MCP v2
- Pas modification critères green / `run_mcp_v2_checkpoint.ps1`

### QA

```powershell
& targeted_pcb_save_board_probe.ps1
& targeted_pcb_update_from_schematic_probe.ps1  # cas DRC advisory
# + non-régression R173/R171/R170/R172/DRC v1/R163/checkpoint v2/CI VerifyOnly
```

### Décision

**R176 validée** — Option A save + Option C DRC advisory livrées ; checkpoint v2 **9 familles** inchangé.

### Recommandation R177

- Documenter workflow post-apply complet (apply → DRC advisory → save) dans mini-release doc ; évaluer probe chaînée apply+save sans intégrer checkpoint v2.

- **Date** : 2026-06-07

## R177 — Mini-release doc « PCB Apply → DRC Advisory → Save Workflow »

### Objectif

Clôturer R176 en **documentant et validant** le workflow IA complet post-footprint : preview → apply confirmé → DRC advisory → save confirmé → vérification — **doc + probe exemple**, **aucun handler C++**.

### Workflow IA officialisé (`pcb_apply_drc_save_workflow_v1`)

```text
1. get_project_context_snapshot(include_pcb_sync_summary=true, pcb_sync_include_preview=true)
2. pcb_update_from_schematic_preview_session
3. pcb_update_from_schematic_session(confirm_apply=true, include_post_apply_drc_summary=true)
4. get_pcb_sync_readiness_session
5. save_board_session(confirm_save=true)
6. optionnel : get_pcb_drc_violations_context_session(run_drc=true)
7. post-save : snapshot ou sync readiness pour vérifier l'état persisté
```

### Règles agent

| Règle | Action |
|-------|--------|
| Preview | Ne **jamais** apply sans preview récente (R171). |
| Apply | Ne **jamais** `confirm_apply=true` sans policy / confirmation utilisateur. |
| Save | Ne **jamais** `confirm_save=true` sans confirmation explicite ; apply **ne sauvegarde pas**. |
| DRC advisory | `include_post_apply_drc_summary` = **advisory** ; si DRC échoue mais apply OK → **pas de rollback**. |
| Post-save | Relire readiness ou snapshot pour vérifier l'état. |

### Probe exemple

**`r177_example_pcb_apply_drc_save_workflow.ps1`** — chaîne complète + gates confirm apply/save.

### Hors périmètre respecté

- Aucun handler / schéma MCP modifié
- Pas intégration checkpoint MCP v2 ; critères green inchangés
- Pas undo MCP ; DRC non bloquante

### Décision

**R177 validée** — mini-release doc **`pcb_apply_drc_save_workflow_v1`** ; probe chaînée exemple.

### Recommandation R178

- Probe CI advisory R177 (hors `ci_baseline_green`) si répétabilité requise ; pas d'intégration checkpoint v2 sans décision dédiée.

- **Date** : 2026-06-07

## R178 — CI observability G6 « PCB apply DRC save workflow »

### Objectif

Intégrer **`r177_example_pcb_apply_drc_save_workflow.ps1`** dans **`run_mcp_v2_ci_baseline.ps1`** comme step **G6** advisory observability-only — **sans** impacter **`ci_baseline_green`**, **`exact_blocker_if_any`**, checkpoint MCP v2 ni critères green.

### Position pipeline

**G6** — après **G5** (dual-source footprint consistency).

### Champs summary

`pcb_apply_drc_save_workflow_ok`, `pcb_apply_drc_save_workflow_exit_code`, `pcb_apply_drc_save_workflow_green`, `pcb_apply_drc_save_workflow_preview_ok`, `pcb_apply_drc_save_workflow_apply_confirm_required_ok`, `pcb_apply_drc_save_workflow_apply_with_drc_advisory_ok`, `pcb_apply_drc_save_workflow_post_apply_drc_summary_present_ok`, `pcb_apply_drc_save_workflow_post_apply_sync_readiness_ok`, `pcb_apply_drc_save_workflow_save_confirm_required_ok`, `pcb_apply_drc_save_workflow_save_confirmed_ok`, `pcb_apply_drc_save_workflow_post_save_verify_ok`, `pcb_apply_drc_save_workflow_blocker`.

### Comportement

| Mode | G6 |
|------|-----|
| CI complet (post-checkpoint) | Lance R177 ; remplit champs ; échec R177 **ne modifie pas** `ci_baseline_green` |
| **`-VerifyOnly`** | Tous champs G6 = **`N/A`** ; R177 non lancé |
| **`-SkipCheckpoint`** | Tous champs G6 = **`skipped`** ; R177 non lancé |

### Hors périmètre respecté

- Aucun handler C++ / schéma MCP
- `run_mcp_v2_checkpoint.ps1` inchangé
- Critères green MCP v2 inchangés
- Script R177 inchangé

### Décision

**R178 validée** — G6 observability-only livré ; pattern G4/G5 respecté.

### Recommandation R179

- Étendre politique alerte R168 à G6 ; dashboard CI optionnel sur `pcb_apply_drc_save_workflow_*`.

- **Date** : 2026-06-07

## R179 — Politique d'alerte G6 et clôture série PCB apply/save (R170–R179)

### Objectif

Étendre la politique d'alerte **R168** au step CI advisory **G6** (`pcb_apply_drc_save_workflow_*`) et **clôturer** la série PCB apply/save workflow **R170–R179** — **documentation only**, aucun handler, aucun comportement CI modifié.

### Principe fondamental (étendu R168)

**`ci_baseline_green=true` ne suffit pas** pour valider une release si **G6 est rouge**.

Un G6 rouge est un **advisory critique** — **pas** un blocker automatique du pipeline CI baseline. **Décision humaine ou PM requise** avant release.

Le gate principal couvre probes PCB + checkpoint MCP v2 (**9 familles**) ; G6 signale la santé du workflow **preview → apply confirmé → DRC advisory → save confirmé → verify** **en parallèle**.

### Série clôturée R170–R179

| Mission | Livrable |
|---------|----------|
| **R170** | `get_pcb_sync_readiness_session` — read-only SCH↔PCB |
| **R171** | `pcb_update_from_schematic_preview_session` — dry-run |
| **R172** | `pcb_sync_summary` opt-in dans snapshot |
| **R173** | `pcb_update_from_schematic_session` — apply contrôlé |
| **R174** | Mini-release doc `pcb_update_apply_controlled_v1` |
| **R175** | Audit save / undo / DRC gate |
| **R176** | `save_board_session` + DRC advisory post-apply |
| **R177** | Workflow doc `pcb_apply_drc_save_workflow_v1` + probe chaînée |
| **R178** | CI G6 observability-only |
| **R179** | Politique alerte G6 + clôture série |

Chaîne complète et surveillée : sync readiness → preview → apply (confirm + DRC advisory) → save (confirm) → verify post-save.

### Signaux G6 (`pcb_apply_drc_save_workflow_*`)

Champs critiques dans **`qa_mcp_v2_ci_baseline_summary.txt`** :

| Champ | Politique |
|-------|-----------|
| **`pcb_apply_drc_save_workflow_ok`** | Alerte CI advisory si `false` en CI complet |
| **`pcb_apply_drc_save_workflow_green`** | Release hold si `false` reproductible |
| **`pcb_apply_drc_save_workflow_preview_ok`** | Incident si preview gate cassé |
| **`pcb_apply_drc_save_workflow_apply_confirm_required_ok`** | **Incident QA** si `false` (mutation sans confirm) |
| **`pcb_apply_drc_save_workflow_apply_with_drc_advisory_ok`** | Incident produit potentiel si apply KO |
| **`pcb_apply_drc_save_workflow_post_apply_drc_summary_present_ok`** | Incident produit si DRC advisory absent alors que demandé |
| **`pcb_apply_drc_save_workflow_post_apply_sync_readiness_ok`** | Incident produit potentiel si sync KO post-apply |
| **`pcb_apply_drc_save_workflow_save_confirm_required_ok`** | **Incident QA** si `false` (save sans confirm) |
| **`pcb_apply_drc_save_workflow_save_confirmed_ok`** | Incident produit potentiel si apply OK mais save KO |
| **`pcb_apply_drc_save_workflow_post_save_verify_ok`** | **Incident QA** si verify post-save KO |
| **`pcb_apply_drc_save_workflow_blocker`** | Lire summary enfant R177 (`qa_r177_pcb_apply_drc_save_workflow_summary.txt`) |

Note : les champs `mutation_without_confirm_blocked_ok` et `save_without_confirm_blocked_ok` sont dans le summary **R177** (probe) ; G6 CI expose les sous-champs mappés ci-dessus.

### Niveaux d'alerte G6

| Niveau | Condition | Action |
|--------|-----------|--------|
| **Info** | G6 = `N/A` en **`-VerifyOnly`** | Normal |
| **Info** | G6 = `skipped` en **`-SkipCheckpoint`** | Normal (diagnostic court) |
| **Warning** | G6 échoue localement une fois mais standalone R177 repasse green | Investiguer flake ; relancer R177 |
| **Release hold** | **`pcb_apply_drc_save_workflow_ok=false`** en CI complet reproductible | Ne pas publier sans analyse PM |
| **Release hold** | **`pcb_apply_drc_save_workflow_green=false`** | Idem |
| **Incident QA** | **`apply_confirm_required_ok=false`** ou **`save_confirm_required_ok=false`** (gates confirm) | Stopper — régression sécurité workflow |
| **Incident QA** | **`post_save_verify_ok=false`** | Stopper — verify post-save cassé |
| **Incident produit potentiel** | apply confirmé OK mais **`save_confirmed_ok=false`** | Analyse produit ; pas de rollback auto |
| **Incident produit potentiel** | apply OK mais **`post_apply_sync_readiness_ok=false`** | Analyse sync ; DRC advisory à part |
| **Incident produit potentiel** | **`post_apply_drc_summary_present_ok=false`** avec DRC demandé | DRC advisory manquant |

### Règle agent / release

```
ci_baseline_green=true ne suffit pas pour valider une release si G6 est rouge.
Un G6 rouge = advisory critique, non blocker automatique CI.
Décision humaine ou PM requise avant release.
```

Workflow agent documenté : **`MCP_KICAD_USER_DEV_GUIDE.md`** R177 ; probes ciblées R170–R176 restent la preuve unitaire ; G6 = surveillance chaîne complète.

### Hors périmètre respecté

- Aucun handler C++ / schéma MCP
- `run_mcp_v2_checkpoint.ps1` et logique G6 inchangés
- Critères green MCP v2 inchangés

### Décision

**R179 validée** — politique G6 documentée ; série **R170–R179** clôturée.

### Recommandation R180

- Dashboard CI externe sur G4/G5/G6 ; factorisation fixtures QA PCB sync (`ipc_pcb_sync_v1_*`) ; nouvelle capacité MCP uniquement sur décision produit distincte (hors série apply/save).

- **Date** : 2026-06-08

## R180 — Rapport local d'observabilité G4/G5/G6

### Objectif

Créer **`report_mcp_v2_ci_advisories.ps1`** — script **read-only** qui lit **`qa_mcp_v2_ci_baseline_summary.txt`**, interprète G4/G5/G6 selon **R168/R179**, et produit un rapport PM/release **sans** modifier gates CI.

### Contrat

```powershell
& report_mcp_v2_ci_advisories.ps1 -BuildDir $BuildDir
& report_mcp_v2_ci_advisories.ps1 -SummaryPath <path> [-OutDir <path>]
```

Sorties :

- **`qa_mcp_v2_ci_advisory_report.txt`** (greppable)
- **`qa_mcp_v2_ci_advisory_report.md`** (lisible PM)

Exit **0** si rapport généré (même avec `release_hold`) ; exit **1** si summary introuvable/illisible.

### Champs rapport

`ci_advisory_report_green`, `advisory_overall_status`, `advisory_overall_action`, `g4_status`, `g5_status`, `g6_status`, `release_requires_manual_review`, champs `g*_ok`, `source_ci_baseline_summary`.

### Interprétation

| Condition | Statut groupe |
|-----------|---------------|
| `N/A` (VerifyOnly) | `not_applicable` |
| `skipped` (SkipCheckpoint) | `skipped` |
| `ok=true` + `green=true` | `green` |
| G4 `mutation_performed=true` | `incident_qa` |
| G6 gates confirm / post-save verify `false` | `incident_qa` |
| `ok=false` ou `green=false` | `release_hold` |
| Champ absent | `unknown` + `release_requires_manual_review=true` |

### Hors périmètre respecté

- Aucun handler / schéma MCP
- `run_mcp_v2_ci_baseline.ps1` et checkpoint inchangés
- Critères green inchangés

### Décision

**R180 validée** — outil local advisory livré.

### Recommandation R181

- Intégrer appel optionnel post-CI baseline (observability-only, exit 0) ; alerting externe sur champs rapport.

- **Date** : 2026-06-08

## R181 — Wrapper release checklist advisories G4/G5/G6

### Objectif

Créer **`check_mcp_v2_release_advisories.ps1`** — wrapper **read-only** qui consomme le rapport **R180** et applique une politique **release go/no-go** (exit non-0 en pipeline release) **sans** modifier `ci_baseline_green`, handlers, checkpoint ni critères green.

### Contrat

```powershell
& check_mcp_v2_release_advisories.ps1 -BuildDir $BuildDir
& check_mcp_v2_release_advisories.ps1 -BuildDir $BuildDir -AdvisoryReportPath <path>
& check_mcp_v2_release_advisories.ps1 -BuildDir $BuildDir -AllowWarning
& check_mcp_v2_release_advisories.ps1 -BuildDir $BuildDir -RequireGreen
```

Options : **`-SummaryPath`**, **`-AdvisoryReportPath`**, **`-AllowWarning`**, **`-RequireGreen`**.

Si aucun rapport advisory fourni : appelle **`report_mcp_v2_ci_advisories.ps1`**, puis lit **`qa_mcp_v2_ci_advisory_report.txt`**.

Sortie : **`qa_mcp_v2_release_advisory_check_summary.txt`**.

### Politique release (exit code)

| Condition | `release_decision` | Exit |
|-----------|-------------------|------|
| `advisory_overall_status=green` + advisories runtime validées | `go` | **0** |
| `warning` | `manual_review_required` | **1** (défaut) ; **0** avec **`-AllowWarning`** |
| `warning` + **`-RequireGreen`** | `manual_review_required` | **1** |
| `release_hold` | `hold_release` | **1** |
| `incident_qa` | `incident_qa` | **1** |
| `unknown` | `unknown` | **1** |
| `release_requires_manual_review=true` | selon statut | **1** (R181 : pas d'opt-out) |

### VerifyOnly — choix documenté

**R180** peut émettre `advisory_overall_status=green` quand G4/G5/G6 = `not_applicable` (VerifyOnly).

**R181** ne traite **pas** ce cas comme `go` : si `verify_only=true` **ou** tous les groupes sont `not_applicable`/`skipped`, le wrapper force :

- `release_decision=manual_review_required`
- `release_requires_manual_review=true`
- `release_advisory_check_green=false`
- exit **1**

Raison : VerifyOnly ne valide pas les advisories runtime G4/G5/G6 ; une release exige un CI complet green.

### Champs summary release

`release_advisory_check_green`, `release_decision`, `advisory_overall_status`, `release_requires_manual_review`, `g4_status`, `g5_status`, `g6_status`, `exact_blocker_if_any`, `advisories_runtime_validated`, `source_advisory_report`.

### Hors périmètre respecté

- Aucun handler / schéma MCP
- `run_mcp_v2_ci_baseline.ps1`, `run_mcp_v2_checkpoint.ps1`, critères green inchangés
- Logique G4/G5/G6 (R180) inchangée

### Décision

**R181 validée** — wrapper release advisory livré.

### Recommandation R182

- Intégration optionnelle dans pipeline CI release (étape post-baseline) ; export JSON du summary release ; alerting sur `release_decision != go`.

- **Date** : 2026-06-08

## R182 — Export Release Pack Markdown + JSON

### Objectif

Créer **`export_mcp_v2_release_pack.ps1`** — script **read-only** qui agrège CI baseline summary, rapport **R180** et décision **R181** en pack **Markdown + JSON** pour PM/release, **sans** modifier gates dev, handlers ni scripts CI existants.

### Contrat

```powershell
& export_mcp_v2_release_pack.ps1 -BuildDir $BuildDir
& export_mcp_v2_release_pack.ps1 -BuildDir $BuildDir -SummaryPath <path> [-AdvisoryReportPath <path>] [-ReleaseCheckPath <path>] [-OutDir <path>]
```

Résolution par défaut : dernier **`qa_mcp_v2_ci_baseline_summary.txt`**, puis advisory + release check **dans le même dossier de run** (cohérence artefact).

Si rapport R180 absent : exit **1**, **`export_blocker_if_any=missing_advisory_report`** (ne génère **pas** le rapport — exécuter **`report_mcp_v2_ci_advisories.ps1`** avant).

Si summary R181 absent : exit **1**, **`export_blocker_if_any=missing_release_check_summary`**.

Sorties : **`qa_mcp_v2_release_pack.md`**, **`qa_mcp_v2_release_pack.json`**.

Exit **0** si pack généré (même si **`release_decision=hold_release`** ou **`manual_review_required`**).

### Champs JSON

`release_pack_green`, `release_decision`, `release_advisory_check_green`, `release_requires_manual_review`, `ci_baseline_green`, `mcp_v2_checkpoint_green`, `checkpoint_family_count`, `advisory_overall_status`, `g4_status`, `g5_status`, `g6_status`, champs `*_ok`, `exact_blocker_if_any`, `source_files`, `pack_generated_at`.

**`release_decision`** provient de **R181** (pas du seul `advisory_overall_status` R180). VerifyOnly → **`manual_review_required`**, jamais **`go`**.

### Hors périmètre respecté

- Aucun handler / schéma MCP
- `run_mcp_v2_ci_baseline.ps1`, `run_mcp_v2_checkpoint.ps1`, critères green inchangés
- Wrapper R181 inchangé

### Décision

**R182 validée** — export release pack livré.

### Recommandation R183

- Publication artefact pack dans pipeline release (upload) ; variante `-IncludeCheckpointSummary` pour embarquer le summary checkpoint 9 familles.

- **Date** : 2026-06-08

## R183 — Enrichir le Release Pack avec le checkpoint MCP v2 détaillé

### Objectif

Étendre **`export_mcp_v2_release_pack.ps1`** avec **`-IncludeCheckpointSummary`** pour embarquer le détail checkpoint MCP v2 (9 familles) dans le pack MD/JSON, **sans** modifier gates dev, handlers, checkpoint ni critères green.

### Contrat

```powershell
& export_mcp_v2_release_pack.ps1 -BuildDir $BuildDir -IncludeCheckpointSummary
& export_mcp_v2_release_pack.ps1 -BuildDir $BuildDir -IncludeCheckpointSummary -CheckpointSummaryPath <path>
```

Sans **`-IncludeCheckpointSummary`** : comportement **R182** inchangé.

Résolution checkpoint summary :

1. **`-CheckpointSummaryPath`** → `checkpoint_summary_match_confidence=explicit`
2. Sinon **`checkpoint_summary_path`** du CI summary → `same_run`
3. Sinon dernier **`qa_mcp_v2_checkpoint_summary.txt`** → `latest`
4. Sinon pack généré avec **`checkpoint_summary_included=false`**, **`checkpoint_summary_blocker=missing_checkpoint_summary`**, exit **0**

Familles extraites dynamiquement des clés **`*_ok` + `*_exit_code`** présentes (pas d'invention).

### Champs ajoutés

`checkpoint_summary_included`, `checkpoint_summary_match_confidence`, `checkpoint_summary_path`, `checkpoint_summary_blocker`, objet **`checkpoint`** (`mcp_v2_checkpoint_green`, `checkpoint_family_count`, `kiface_smoke_ok`, `families[]` avec `name`, `ok`, `exit_code`, `elapsed_ms`).

Section Markdown **## MCP v2 Checkpoint Detail** avec tableau familles.

**`release_decision`** (R181) inchangé.

### Hors périmètre respecté

- Handlers, schémas MCP, `run_mcp_v2_ci_baseline.ps1`, `run_mcp_v2_checkpoint.ps1`, critères green, R180/R181 inchangés

### Décision

**R183 validée** — release pack enrichi checkpoint.

### Recommandation R184

- Upload automatique du pack release dans pipeline CI ; variante `-IncludeCheckpointSummary` par défaut en contexte release uniquement.

- **Date** : 2026-06-08

## R184 — Wrapper release pipeline local avec release pack complet

### Objectif

Créer **`run_mcp_v2_release_pack.ps1`** — commande unique release locale qui orchestre CI baseline + **R180** + **R181** + export **R182/R183** (`-IncludeCheckpointSummary`), **sans** modifier gates dev, handlers, checkpoint ni critères green.

### Contrat

```powershell
& run_mcp_v2_release_pack.ps1 -BuildDir $BuildDir
& run_mcp_v2_release_pack.ps1 -BuildDir $BuildDir -SkipBuild -KillRunningKiCad
& run_mcp_v2_release_pack.ps1 -BuildDir $BuildDir -AllowWarning
```

Options : **`-SkipBuild`**, **`-KillRunningKiCad`**, **`-AllowWarning`** (transmis à R181), **`-OutDir`**.

Chaîne :

1. `run_mcp_v2_ci_baseline.ps1` (CI complet, pas VerifyOnly)
2. `report_mcp_v2_ci_advisories.ps1`
3. `check_mcp_v2_release_advisories.ps1`
4. `export_mcp_v2_release_pack.ps1 -IncludeCheckpointSummary`

Sortie : **`qa_mcp_v2_release_pack_pipeline_summary.txt`** + artefacts pack MD/JSON.

### Exit code (release, pas dev)

| `release_decision` | Exit |
|--------------------|------|
| `go` | **0** |
| `warning` + **`-AllowWarning`** | **0** |
| `manual_review_required`, `hold_release`, `incident_qa`, `unknown` | **1** |
| Échec technique CI / export | **1** |

**`ci_baseline_green`** et gates dev **inchangés** — ce wrapper est un outil **release** uniquement.

### Hors périmètre respecté

- Handlers, schémas MCP, `run_mcp_v2_ci_baseline.ps1`, `run_mcp_v2_checkpoint.ps1`, critères green, R180–R183 inchangés

### Décision

**R184 validée** — pipeline release local livré.

### Recommandation R185

- Intégration CI/CD release (job dédié) ; notification sur `release_pack_pipeline_green=false`.

- **Date** : 2026-06-08

## R185 — Commande officielle de release MCP v2

### Objectif

Officialiser **`run_mcp_v2_release_pack.ps1`** comme **commande standard de release MCP v2** : usage, artefacts, politique d'exit code, distinction **CI dev** vs **pipeline release** — **doc only**, sans job CI/CD externe ni modification handlers/gates.

### Commande officielle

```powershell
$BuildDir = "c:\dev\kicad-master\build\msvc-win64-release"
$tools = "c:\dev\kicad-master\qa\tools\ipc_tools"

& "$tools\run_mcp_v2_release_pack.ps1" -BuildDir $BuildDir -SkipBuild -KillRunningKiCad
```

Options documentées : **`-SkipBuild`**, **`-KillRunningKiCad`**, **`-AllowWarning`**, **`-OutDir`**.

Chaîne interne (inchangée depuis R184) : CI baseline → R180 → R181 → export R182/R183 (`-IncludeCheckpointSummary`).

### Artefacts officiels (sous `{BuildDir}\ipc-v1-artifacts\{run}-mcp-v2-ci-baseline` sauf `-OutDir`)

| Fichier | Rôle |
|---------|------|
| **`qa_mcp_v2_release_pack_pipeline_summary.txt`** | Verdict pipeline release (`release_pack_pipeline_green`, `release_decision`) |
| **`qa_mcp_v2_release_pack.md`** | Pack lisible PM |
| **`qa_mcp_v2_release_pack.json`** | Pack machine-readable (checkpoint 9 familles si R183) |
| **`qa_mcp_v2_ci_baseline_summary.txt`** | Gate CI dev (`ci_baseline_green`) |
| **`qa_mcp_v2_ci_advisory_report.txt`** | Interprétation G4/G5/G6 (R180) |
| **`qa_mcp_v2_release_advisory_check_summary.txt`** | Décision release R181 |

### Politique d'exit code (release)

| Condition | Exit |
|-----------|------|
| **`release_decision=go`** | **0** |
| **`warning`** + **`-AllowWarning`** | **0** |
| **`manual_review_required`**, **`hold_release`**, **`incident_qa`**, **`unknown`** | **1** |
| Erreur technique (CI/export) | **1** |

**`-AllowWarning`** : accepte uniquement **`warning`** → `go` ; **pas** `release_hold` ni `incident_qa`.

### Distinction CI dev vs release

| | **CI dev** | **Release** |
|---|------------|-------------|
| **Commande** | `run_mcp_v2_ci_baseline.ps1` | **`run_mcp_v2_release_pack.ps1`** |
| **Gate principal** | `ci_baseline_green` | `release_decision` / `release_pack_pipeline_green` |
| **G4/G5/G6** | Advisory **non bloquant** pour `ci_baseline_green` (R168/R179) | **Bloquant release** via R181 (VerifyOnly ≠ `go`) |
| **Usage** | Merge / QA dev | Décision PM avant publication |

Les scripts intermédiaires R180–R183 restent utilisables **à la carte** pour diagnostic ; la **release officielle** passe par le wrapper R184/R185.

### Références doc

- **`README_IPC_QA_BASELINE.md`** — section R185
- **`MCP_KICAD_USER_DEV_GUIDE.md`** — release officielle
- **`MCP_V1_CHECKPOINT_RELEASE_NOTE.md`** — R185
- **`MCP_V1_OFFICIAL_MANIFEST.md`** — runners release R180–R185

**`MCP_TOOL_MAP_R1.tsv`** : registre **handlers MCP** uniquement — les scripts release QA ne sont **pas** des tools MCP.

### Hors périmètre respecté

- Handlers, schémas MCP, `run_mcp_v2_checkpoint.ps1`, critères green, logique R180–R184 inchangés
- Pas de job CI/CD externe (R186+)

### Décision

**R185 validée** — commande release MCP v2 officialisée (documentation).

### Recommandation R186

- Job CI/CD release optionnel consommant `run_mcp_v2_release_pack.ps1` ; archivage artefacts pack ; alerting `release_pack_pipeline_green=false`.

- **Date** : 2026-06-08

## R186 — Audit CI/CD pour intégration release pack MCP v2

### Objectif

Auditer l'environnement CI/CD du repo KiCad pour déterminer comment intégrer **`run_mcp_v2_release_pack.ps1`** (job release, archivage, alerting) — **audit doc only** ; **aucun** job CI/CD implémenté en R186.

### Environnement CI/CD identifié

| Élément | Constat |
|---------|---------|
| **CI principal** | **GitLab CI** — `.gitlab-ci.yml` + includes `.gitlab/*.yml` |
| **GitHub Actions** | **Miroir one-way** uniquement — `.github/workflows/lockdown.yml` (redirection PR vers GitLab) ; **pas** de build/test MCP |
| **Azure Pipelines** | **Absent** |
| **Dossier `ci/`** | **Absent** ; config sous **`.gitlab/`** |
| **MCP v2 / `ipc_tools`** | **Non intégré** dans les pipelines officiels actuels |

### Workflows / jobs existants (liste)

**GitLab (`.gitlab/`)** :

| Fichier | Rôle |
|---------|------|
| `Fedora-Linux-CI.yml` | Build Fedora + ctest `qa_*` (Linux) |
| `Ubuntu-20.04-CI.yml` | Build Ubuntu Release + tests |
| `Windows-CI.yml` | Build Windows **Debug** (`build/windows`, Ninja, vcpkg) ; artefact = `compilation_log.txt` |
| `macOS-CI.yml` | Build macOS |
| `coverity.yml` | Analyse Coverity (schedule) |
| `doxygen.yml` | Doxygen (schedule) |
| `linux-metadata-validate.yml` | Métadonnées |
| `mr_formatting.yml` | Format MR |
| `templates.yml` | Template `.unit_test` (JUnit artifacts Linux) |

**GitHub** : `lockdown.yml` seulement.

**Triggers observés** : branches/MR (`kicad` namespace) ; jobs schedule pour doxygen/coverity ; **pas** de job release MCP.

### Contraintes runner Windows (release pack)

| Contrainte | Détail |
|------------|--------|
| **OS / scripts** | Release pack = **PowerShell** + binaires **Windows x64** (`kicad.exe`, `qa_ipc_smoke.exe`, KIFACE DLLs) |
| **Layout build** | Scripts QA ciblent **`build/msvc-win64-release`** (preset CMake **`msvc-win64-release`**) — **≠** `build/windows` du job GitLab `win64_build` (Debug) |
| **Toolchain** | **VsDevCmd** / MSVC 2022 requis (`run_mcp_v2_ci_baseline.ps1`) |
| **GUI / IPC** | Pipeline lance **`kicad.exe`** (checkpoint 9 familles, probes PCB/SCH) ; **`KICAD_IPC_AUTOMATION=1`** ; pas de mode headless documenté — runner **Windows LTSC2022** (`kicad-windows-ltsc2022`) **probablement capable** (agents Windows natifs) mais **non prouvé** dans CI officielle actuelle |
| **Session interactive** | **Non requise** pour l'opérateur humain ; processus GUI headless possible sur runner Windows serveur (à valider R187) |
| **Durée** | **~5–8 min** (`run_mcp_v2_release_pack.ps1` avec `-SkipBuild`) ; **~8+ min** si build inclus |
| **Process cleanup** | **`-KillRunningKiCad`** recommandé sur runner partagé |
| **Artefacts volumineux** | Dossier `ipc-v1-artifacts/` peut être **lourd** ; upload ciblé MD/JSON/summaries recommandé |

### Artefacts release à archiver (upload CI)

**Obligatoires (gate release)** :

- `qa_mcp_v2_release_pack_pipeline_summary.txt`
- `qa_mcp_v2_release_pack.md`
- `qa_mcp_v2_release_pack.json`

**Recommandés (contexte PM)** :

- `qa_mcp_v2_ci_baseline_summary.txt`
- `qa_mcp_v2_ci_advisory_report.txt`
- `qa_mcp_v2_release_advisory_check_summary.txt`
- `qa_mcp_v2_checkpoint_summary.txt` (référencé dans pack JSON si `same_run`)

**Optionnel (debug)** : répertoire complet `{BuildDir}/ipc-v1-artifacts/{timestamp}-mcp-v2-ci-baseline/` (expire court).

**Parsing gate** : `release_pack_pipeline_green=false` ou `release_decision!=go` → job **failed** ; alerting GitLab natif (email) + webhook Slack/Teams **hors repo** (variable CI).

### Comparaison options A / B / C / D

| | **A — Local PM** | **B — CI manuel Windows** | **C — Nightly advisory** | **D — Tag release** |
|---|------------------|---------------------------|--------------------------|---------------------|
| **Déclenchement** | Humain / Cursor | `workflow_dispatch` / `when: manual` GitLab | Schedule | Tag Git |
| **Bloque dev/MR** | Non | Non (job séparé) | Non | **Oui** (publication) |
| **Exit release** | Local exit 0/1 | Job failed si `!= go` | Alerte seulement | Block release |
| **Build** | Local `-SkipBuild` | `-SkipBuild` ou build preset dédié | `-SkipBuild` | Build complet |
| **Effort** | **R185 fait** | Moyen (nouveau job YAML) | Faible | Élevé (gouvernance) |
| **Risque** | Faible | Runner GUI à valider | Bruit nightly | Trop tôt |

### Réponses aux questions d'audit

| Question | Réponse |
|----------|---------|
| Runner Windows + KiCad GUI/IPC ? | **Théoriquement oui** sur `kicad-windows-ltsc2022` ; **jamais testé** en CI officielle ; **gap R187** |
| Session interactive ? | **Non** pour l'humain ; processus `kicad.exe` automatisés |
| ~5–8 min acceptable ? | **Oui** pour job manuel release ; **non** pour chaque MR |
| Upload artefacts ? | **Oui** — pattern GitLab `artifacts: paths:` existant (`templates.yml`, `fedora_report_build_warn`) |
| Manuel vs auto ? | **Manuel d'abord** (Option B) ; nightly (C) ensuite ; tag (D) après stabilisation |
| Statut bloquant release ? | **`release_decision=go`** et `release_pack_pipeline_green=true` (R184/R185) |
| `-SkipBuild` vs rebuild ? | **CI** : job build `msvc-win64-release` amont **ou** `-SkipBuild` si artefact build réutilisé ; **ne pas** réutiliser `build/windows` Debug |

### Recommandation R187

**Option B — Job GitLab CI manuel Windows** (première intégration) :

1. Nouveau include **`.gitlab/MCP-Windows-Release.yml`** (hors R186) — stage **`release`**, `when: manual`, tag `kicad-windows-ltsc2022`
2. Build **`cmake --preset win64-release`** (ou `needs:` job build dédié produisant `build/msvc-win64-release`)
3. Script : `run_mcp_v2_release_pack.ps1 -BuildDir $BuildDir -KillRunningKiCad` (**sans** `-SkipBuild` si build frais dans le job)
4. `artifacts` : pack MD/JSON/pipeline summary + summaries associés ; `expire_in: 30 days` ; `expose_as: 'MCP v2 Release Pack'`
5. Job **failed** si exit ≠ 0 ; notification GitLab ; webhook optionnel via `$MCP_RELEASE_WEBHOOK_URL`
6. **Ne pas** brancher sur pipelines MR Fedora/Linux — job **parallèle** namespace `kicad` uniquement

Option C en **R188+** si nightly souhaité (non bloquant). Option D réservée post-stabilisation runner.

### Hors périmètre respecté (R186)

- Aucun handler / schéma MCP
- Aucun script CI/QA modifié
- Aucun critère green modifié
- Aucun job YAML créé

### Décision

**R186 validée** — audit CI/CD documenté ; intégration différée à **R187**.

### Recommandation R187

Implémenter **Option B** : job GitLab manuel Windows + upload artefacts + fail sur `release_decision != go`.

- **Date** : 2026-06-08

## R187 — Job GitLab manuel Windows pour release pack MCP v2

### Objectif

Ajouter un job GitLab **manuel** Windows exécutant **`run_mcp_v2_release_pack.ps1`**, upload des artefacts release pack, échec si **`release_decision != go`** — **sans** modifier gates dev, checkpoint MCP v2, ni scripts QA R180–R185.

### Livrables

| Fichier | Rôle |
|---------|------|
| **`.gitlab/MCP-Windows-Release.yml`** | Job **`mcp_v2_release_pack_windows`** |
| **`.gitlab-ci.yml`** | Stage **`release`** + include explicite |

### Job `mcp_v2_release_pack_windows`

| Propriété | Valeur |
|-----------|--------|
| **Stage** | `release` |
| **Tag** | `kicad-windows-ltsc2022` |
| **When** | `manual` (namespace `kicad` uniquement) |
| **allow_failure** | `false` |
| **Image** | `ltsc2022-msvc` (même que `win64_build`) |
| **Bloque dev/MR** | **Non** — job manuel, stage séparé |

### Script (résumé)

1. `C:\builder\build.ps1 -Env -Arch x64` ; `VCPKG_ROOT=C:\builder\vcpkg`
2. Si binaires absents : `cmake --preset msvc-win64-release` + `cmake --build --preset win64-release --target kicommon pcbnew_kiface kicad qa_ipc_smoke`
3. `run_mcp_v2_release_pack.ps1 -BuildDir build/msvc-win64-release -SkipBuild -KillRunningKiCad`
4. Exit job = exit script (**0** = `release_decision=go` uniquement)

### Artefacts uploadés (`when: always`, 30 jours)

- `qa_mcp_v2_release_pack_pipeline_summary.txt`
- `qa_mcp_v2_release_pack.md` / `.json`
- `qa_mcp_v2_ci_baseline_summary.txt`
- `qa_mcp_v2_ci_advisory_report.txt` / `.md`
- `qa_mcp_v2_release_advisory_check_summary.txt`
- `qa_mcp_v2_checkpoint_summary.txt`

Chemins : `build/msvc-win64-release/ipc-v1-artifacts/**/…`

### Politique d'échec

- Job **rouge** si `run_mcp_v2_release_pack.ps1` exit **1** (`manual_review_required`, `hold_release`, `incident_qa`, `unknown`, erreur technique)
- **`ci_baseline_green`** dev inchangé ; job **hors** checkpoint MCP v2
- Artefacts uploadés **même si échec** (`when: always`)

### Hors périmètre respecté

- Handlers, schémas MCP, `run_mcp_v2_checkpoint.ps1`, critères green, logique `run_mcp_v2_ci_baseline.ps1`, scripts R180–R185 inchangés

### Décision

**R187 validée** — job GitLab manuel Windows livré (YAML) ; validation runner officiel **GitLab `kicad`** en attente d'exécution manuelle.

### Recommandation R188

- Option C : job **schedule** nightly advisory (`allow_failure: true`) ; webhook `$MCP_RELEASE_WEBHOOK_URL` sur échec release.

- **Date** : 2026-06-08

---

## R188 — Premier run GitLab réel `mcp_v2_release_pack_windows`

### Objectif

Tester réellement le job GitLab manuel **R187** sur le runner Windows officiel ; documenter statut, runner, artefacts et erreurs éventuelles — **sans** modifier handlers MCP, schémas, critères green ni logique des scripts QA.

### Fichiers inspectés

| Fichier | Rôle |
|---------|------|
| `.gitlab-ci.yml` | stage `release` + include `MCP-Windows-Release.yml` |
| `.gitlab/MCP-Windows-Release.yml` | job `mcp_v2_release_pack_windows` |
| `.gitlab/Windows-CI.yml` | référence runner tag `kicad-windows-ltsc2022` |
| `qa/tools/ipc_tools/run_mcp_v2_release_pack.ps1` | commande release (inchangée) |

### Pré-vol YAML (local, statique)

| Vérification | Résultat |
|--------------|----------|
| Stage `release` dans `.gitlab-ci.yml` | **OK** |
| Include `/.gitlab/MCP-Windows-Release.yml` | **OK** |
| Job `mcp_v2_release_pack_windows` défini | **OK** |
| Tag runner `kicad-windows-ltsc2022` | **OK** |
| Image `ltsc2022-msvc:latest` | **OK** (aligné `Windows-CI.yml`) |
| `when: manual` si namespace `kicad` | **OK** |
| `allow_failure: false` | **OK** |
| Artefacts `when: always` (7 globs + advisory `.md`) | **OK** |
| Script : `build.ps1` → cmake preset `msvc-win64-release` si binaires absents → `run_mcp_v2_release_pack.ps1 -SkipBuild -KillRunningKiCad` | **OK** |
| Bug YAML bloquant constaté | **non** — aucune modification YAML |

### Blocage exécution GitLab

| Blocage | Détail |
|---------|--------|
| **`blocked_by_gitlab_access`** | **true** — pas d'accès API/UI au namespace `gitlab.com/kicad` depuis cet environnement |
| **`blocked_by_no_git_repo`** | **true** — workspace `c:\dev\kicad-master` sans répertoire `.git` (pas de push possible) |
| CLI `glab` / `gh` | **absents** |

**Conséquence** : premier run runner **non exécuté** ; résultats GitLab ci-dessous = **inconnus / non observés**.

### Référence locale R187 (hors périmètre GitLab)

Dernier run local `run_mcp_v2_release_pack.ps1` (R187 QA) — **ne remplace pas** la validation runner :

```
release_pack_pipeline_green=true
release_decision=go
ci_baseline_green=true
mcp_v2_checkpoint_green=true
checkpoint_family_count=9
```

Artefact : `build/msvc-win64-release/ipc-v1-artifacts/20260608-112346562-mcp-v2-ci-baseline/qa_mcp_v2_release_pack_pipeline_summary.txt`

### Résultats GitLab (attendus — non observés)

| Champ | Valeur R188 |
|-------|-------------|
| Job visible dans GitLab | **inconnu** (YAML non poussé depuis cet environnement) |
| Job déclenché | **non** |
| Runner utilisé | **non observé** (attendu : `kicad-windows-ltsc2022`) |
| Durée du job | **non observée** |
| Résultat build `msvc-win64-release` | **non observé** |
| Résultat release pack | **non observé** |
| `release_decision` | **non observé** |
| Checkpoint MCP v2 (9 familles) | **non observé** |
| Artefacts uploadés | **non observés** |

### Artefacts attendus (si job exécuté avec succès)

- `qa_mcp_v2_release_pack_pipeline_summary.txt`
- `qa_mcp_v2_release_pack.md` / `.json`
- `qa_mcp_v2_ci_baseline_summary.txt`
- `qa_mcp_v2_ci_advisory_report.txt` / `.md`
- `qa_mcp_v2_release_advisory_check_summary.txt`
- `qa_mcp_v2_checkpoint_summary.txt`

Chemins job : `build/msvc-win64-release/ipc-v1-artifacts/**/…` — `expose_as: MCP v2 Release Pack`

### Procédure exacte — déclencher le job (mainteneur `kicad`)

1. **Merger ou pousser** la branche contenant R187 (`.gitlab-ci.yml` + `.gitlab/MCP-Windows-Release.yml`) vers `gitlab.com/kicad/code/kicad` (MR ou `master`).
2. Ouvrir **CI/CD → Pipelines** sur le commit cible.
3. Vérifier que le pipeline liste le stage **`release`** et le job **`mcp_v2_release_pack_windows`** (icône ▶ manuel).
4. Cliquer **Play** sur `mcp_v2_release_pack_windows`.
5. Noter : **URL job**, **statut final**, **durée**, **runner** (section job → *Runner*).
6. Dans les logs, copier :
   - extrait **cmake build** (`Configuring and building MCP v2 release targets` ou `Release binaries present`)
   - extrait **release pack** (`release_pack_pipeline_green`, `release_decision`, `mcp_v2_release_pack_windows_ok=true`)
7. Télécharger artefacts **MCP v2 Release Pack** ; vérifier les 7 fichiers listés ci-dessus.
8. Coller le contenu de `qa_mcp_v2_release_pack_pipeline_summary.txt` pour clôture R188 ou R189.

### Informations à renvoyer (checklist)

```
job_url=
job_status=          # success | failed | canceled
runner_name=
runner_tags=
job_duration=
build_result=        # skipped | success | failed (+ extrait log si failed)
release_pack_exit_code=
release_decision=
release_pack_pipeline_green=
ci_baseline_green=
mcp_v2_checkpoint_green=
checkpoint_family_count=
artifacts_uploaded=  # yes | no
artifact_file_list=
runner_gui_ipc_error=  # N/A ou description
```

### Cas d'échec à surveiller (premier run)

| Symptôme | Cause probable |
|----------|----------------|
| Job gris / absent | namespace ≠ `kicad` ou YAML non mergé |
| `stuck` / pas de runner | tag `kicad-windows-ltsc2022` indisponible |
| Échec `build.ps1` / cmake | MSVC env non initialisé sur runner |
| Échec build cibles | preset `msvc-win64-release` ou vcpkg cache |
| KiCad GUI ne démarre pas | session Windows headless / droits GUI runner |
| IPC smoke échoue | binaire `qa_ipc_smoke.exe` ou socket IPC |
| `release_decision != go` | advisory G4/G5/G6 ou checkpoint famille |
| Artefacts vides | chemins `ipc-v1-artifacts` ≠ sortie script |

### Hors périmètre respecté

- Handlers C++ : **non modifiés**
- Schémas MCP : **non modifiés**
- `run_mcp_v2_checkpoint.ps1` / `run_mcp_v2_ci_baseline.ps1` : **non modifiés**
- Critères green : **non modifiés**
- Scripts CI YAML : **non modifiés** (pré-vol OK)
- Nightly / webhook : **non créés**

### Décision

**R188 partielle** — pré-vol YAML validé ; **`blocked_by_gitlab_access=true`** ; premier run runner GitLab **en attente** d'exécution manuelle par mainteneur `kicad`.

### Recommandation R189

1. **Priorité** : exécuter la procédure ci-dessus ; documenter **R188bis** ou **R189** avec logs/artefacts réels.
2. Si run **vert** : envisager **Option C** (nightly advisory `allow_failure: true`) + webhook `$MCP_RELEASE_WEBHOOK_URL` sur `release_decision != go`.
3. Si run **rouge** (runner/GUI/IPC) : corriger **uniquement** le job YAML ou infra runner — pas les critères green.

- **Date** : 2026-06-05

---

## R189 — Procédure mainteneur GitLab release pack MCP v2

### Objectif

Transformer le blocage R188 en **procédure mainteneur opérationnelle** + **checklist standardisée** ; collecter le premier run réel si accès disponible — **sans** modifier handlers, schémas MCP, critères green ni logique CI/QA.

### Pré-vol local (R189)

| Vérification | Résultat |
|--------------|----------|
| `.git` présent | **non** — workspace sans dépôt Git |
| `glab` CLI | **absent** |
| Stage `release` + include `MCP-Windows-Release.yml` | **OK** |
| Job `mcp_v2_release_pack_windows` | **OK** |
| Tag `kicad-windows-ltsc2022` | **OK** |
| Artefacts `when: always` (7 fichiers QA) | **OK** |
| Bug YAML bloquant | **non** |

Commandes de pré-vol :

```powershell
cd c:\dev\kicad-master
git status --short
Select-String -Path .gitlab-ci.yml,.gitlab\*.yml `
  -Pattern "MCP-Windows-Release|mcp_v2_release_pack_windows|release|kicad-windows-ltsc2022|artifacts" `
  -CaseSensitive:$false
```

### Procédure mainteneur — déclencher `mcp_v2_release_pack_windows`

**Prérequis** : accès **Maintainer** ou **Developer** sur `gitlab.com/kicad/code/kicad` ; branche mergée contenant R187/R188 (`.gitlab-ci.yml` + `.gitlab/MCP-Windows-Release.yml`).

| Étape | Action |
|-------|--------|
| **1** | Pousser ou **merger** la branche contenant R187/R188 vers `gitlab.com/kicad/code/kicad` (`master` ou MR mergée). |
| **2** | Ouvrir le projet : **https://gitlab.com/kicad/code/kicad** |
| **3** | Menu **CI/CD → Pipelines** |
| **4** | Ouvrir le **pipeline** du commit concerné (hash visible après merge). |
| **5** | Aller au stage **`release`** (dernier stage du pipeline). |
| **6** | Cliquer **▶ Play** sur le job **`mcp_v2_release_pack_windows`** |
| **7** | Attendre la fin du job (premier run : build CMake possible → **30–90 min** ; runs suivants avec cache : **~10–20 min**). |
| **8** | Job terminé → **Browse** ou **Download** les artefacts **« MCP v2 Release Pack »** (`expire_in: 30 days`, uploadés même si échec). |
| **9** | Remplir la **checklist standardisée** ci-dessous et la renvoyer (issue, MR, ou canal équipe MCP). |

**Vérifications pendant l'exécution** (onglet job → *Log*) :

| Phase log | Marqueur attendu |
|-----------|------------------|
| Env MSVC | `C:\builder\build.ps1 -Env -Arch x64` sans erreur fatale |
| Build (si nécessaire) | `Configuring and building MCP v2 release targets` puis cmake exit 0 |
| Build skip | `Release binaries present; skipping cmake build` |
| Release pack | exécution `run_mcp_v2_release_pack.ps1` |
| Succès job | `mcp_v2_release_pack_windows_ok=true` en fin de log |
| Échec release | `release_pack_pipeline_failed exit=1` (ou autre code) |

**Où trouver le runner** : onglet job → section droite **Runner** (nom + tags `kicad-windows-ltsc2022`).

### Checklist standardisée — à remplir après run GitLab

Copier-coller ce bloc, remplir chaque champ, joindre les fichiers indiqués :

```text
# MCP v2 Release Pack — rapport run GitLab (R189)
# Projet : gitlab.com/kicad/code/kicad
# Job    : mcp_v2_release_pack_windows

job_url=
job_status=                    # success | failed | canceled | skipped
runner_name=
runner_tags=
job_duration=
build_result=                  # skipped | success | failed
release_pack_exit_code=
release_decision=              # go | manual_review_required | hold_release | incident_qa | unknown
release_pack_pipeline_green=   # true | false
ci_baseline_green=             # true | false
mcp_v2_checkpoint_green=       # true | false
checkpoint_family_count=
artifacts_uploaded=            # yes | no
artifact_file_list=
runner_gui_ipc_error=          # N/A ou description
first_error_log_excerpt=       # première ligne d'erreur si failed ; N/A si success
qa_mcp_v2_release_pack_pipeline_summary_attached=yes/no
qa_mcp_v2_release_pack_json_attached=yes/no
```

### Artefacts attendus (téléchargement job)

| Fichier | Rôle |
|---------|------|
| `qa_mcp_v2_release_pack_pipeline_summary.txt` | **Principal** — verdict pipeline |
| `qa_mcp_v2_release_pack.md` | Pack release lisible |
| `qa_mcp_v2_release_pack.json` | Pack release machine |
| `qa_mcp_v2_ci_baseline_summary.txt` | CI baseline |
| `qa_mcp_v2_ci_advisory_report.txt` | Advisory G4/G5/G6 |
| `qa_mcp_v2_release_advisory_check_summary.txt` | Go/no-go R181 |
| `qa_mcp_v2_checkpoint_summary.txt` | Checkpoint MCP v2 (9 familles) |

Chemins dans le zip : `build/msvc-win64-release/ipc-v1-artifacts/<timestamp>-mcp-v2-ci-baseline/…`

### Résultat idéal (succès)

```text
job_status=success
release_pack_exit_code=0
release_decision=go
release_pack_pipeline_green=true
ci_baseline_green=true
mcp_v2_checkpoint_green=true
checkpoint_family_count=9
artifacts_uploaded=yes
runner_gui_ipc_error=N/A
```

### Classification des échecs

| Classe | Symptômes log / UI | Action mainteneur |
|--------|-------------------|-------------------|
| **runner_not_found** | Job `pending` indéfiniment ; aucun runner `kicad-windows-ltsc2022` | Vérifier runners projet/groupe ; contacter infra CI |
| **runner_timeout** | Job `timed out` | Augmenter timeout job ou optimiser cache vcpkg |
| **msvc_cmake_init_failed** | Échec `build.ps1` ou cmake avant build | Vérifier image `ltsc2022-msvc` ; logs `C:\builder\` |
| **build_msvc_win64_release_failed** | Échec `cmake --build --preset win64-release` | Copier extrait log link/compile ; ne pas modifier critères green |
| **kicad_gui_failed** | KiCad ne démarre pas ; timeout GUI | Vérifier session Windows runner ; droits interactifs |
| **ipc_smoke_failed** | `qa_ipc_smoke.exe` exit ≠ 0 | Extrait log smoke + socket IPC |
| **release_pack_non_go** | `release_decision != go` ; exit 1 | Joindre `qa_mcp_v2_release_advisory_check_summary.txt` + checkpoint summary |
| **artifacts_missing** | Job success mais zip vide / fichiers absents | Vérifier chemins `ipc-v1-artifacts/**/` vs sortie script |
| **yaml_include_error** | Pipeline ne parse pas ; job absent | Vérifier merge `.gitlab-ci.yml` + lint CI |

### Bloc prêt à envoyer au mainteneur (copier-coller)

Fichier dédié : **`MAINTAINER_GITLAB_MCP_V2_RELEASE_HANDOFF.md`** (même contenu).

```
Objet : Premier run GitLab — job mcp_v2_release_pack_windows (MCP v2 release pack)

Bonjour,

Merci d'exécuter le premier run du job release pack MCP v2 sur le runner Windows officiel.

Contexte :

* Job livré en R187 ; YAML validé en R188/R189
* Fichiers : `.gitlab-ci.yml` stage `release` + `.gitlab/MCP-Windows-Release.yml`
* Job : `mcp_v2_release_pack_windows`
* Type : manuel, stage `release`
* Runner attendu : `kicad-windows-ltsc2022`
* Le job échoue si `release_decision != go` — c'est volontaire pour le pipeline release

Procédure :

1. Merger la branche contenant R187/R188/R189 si ce n'est pas déjà fait.
2. Aller sur `https://gitlab.com/kicad/code/kicad`.
3. Ouvrir `CI/CD → Pipelines`.
4. Ouvrir le pipeline du commit concerné.
5. Aller au stage `release`.
6. Cliquer sur Play pour `mcp_v2_release_pack_windows`.
7. Attendre la fin du job.
8. Télécharger les artefacts `MCP v2 Release Pack`.
9. Renvoyer la checklist ci-dessous.

Artefacts à joindre en priorité :

* `qa_mcp_v2_release_pack_pipeline_summary.txt`
* `qa_mcp_v2_release_pack.json`
* `qa_mcp_v2_checkpoint_summary.txt`
* premier extrait d'erreur si le job échoue

Checklist à remplir :

job_url=
job_status=
runner_name=
runner_tags=
job_duration=
build_result=
release_pack_exit_code=
release_decision=
release_pack_pipeline_green=
ci_baseline_green=
mcp_v2_checkpoint_green=
checkpoint_family_count=
artifacts_uploaded=
artifact_file_list=
runner_gui_ipc_error=
first_error_log_excerpt=
qa_mcp_v2_release_pack_pipeline_summary_attached=yes/no
qa_mcp_v2_release_pack_json_attached=yes/no

Résultat idéal attendu :

job_status=success
release_pack_exit_code=0
release_decision=go
release_pack_pipeline_green=true
ci_baseline_green=true
mcp_v2_checkpoint_green=true
checkpoint_family_count=9
artifacts_uploaded=yes
runner_gui_ipc_error=N/A

Merci !
```

### Exécution GitLab R189 (cet environnement)

| Champ | Valeur |
|-------|--------|
| **`blocked_by_gitlab_access`** | **true** |
| **`blocked_by_no_git_repo`** | **true** |
| Job GitLab exécuté | **non** |
| Job URL | **N/A** |
| Runner utilisé | **non observé** |
| Résultat job | **non observé** |
| Artefacts uploadés | **non observés** |

Référence locale R187 (hors GitLab) : `release_decision=go`, checkpoint 9 familles green — **ne valide pas** le runner officiel.

### Hors périmètre respecté

- Handlers C++ : **non modifiés**
- Schémas MCP : **non modifiés**
- `run_mcp_v2_checkpoint.ps1` / `run_mcp_v2_ci_baseline.ps1` : **non modifiés**
- Scripts release R180–R185 : **non modifiés**
- Critères green : **non modifiés**
- `.gitlab/MCP-Windows-Release.yml` : **non modifié** (aucun bug bloquant observé)
- Nightly / webhook : **non créés**

### Décision

**R189 clôturée (handoff)** — fichier canonique **`MAINTAINER_GITLAB_MCP_V2_RELEASE_HANDOFF.md`** ; procédure, checklist et classification échecs livrés. Phase opérationnelle : envoi mainteneur + attente retour.

| Action post-R189 | Statut |
|------------------|--------|
| Vérifier merge branche R187/R188/R189 | **à faire** par mainteneur / auteur MR (non vérifiable ici : pas de `.git`, pas d'accès GitLab) |
| Ouvrir/merger MR R187–R189 sur upstream | **à faire** — guide **`MR_GITLAB_MCP_V2_RELEASE_PACK.md`** ; **non mergé** sur `master` (vérif. publique) |
| Envoyer handoff au mainteneur `kicad` | **à faire** après MR ouverte (email / issue / MR) |
| Attendre checklist + 3 artefacts prioritaires | **en cours** |
| Modifier code / handlers / CI / critères green | **interdit** avant retour |

### R190 — porte d'entrée (non démarrée)

**R190 commence uniquement** à réception d'**au moins un** des éléments suivants :

1. URL du job GitLab `mcp_v2_release_pack_windows`
2. Checklist R189 remplie (18 champs)
3. Artefacts : `qa_mcp_v2_release_pack_pipeline_summary.txt`, `qa_mcp_v2_release_pack.json`, `qa_mcp_v2_checkpoint_summary.txt`

Jusqu'à réception : **aucune** modification handlers C++, schémas MCP, scripts CI, scripts QA release, critères green.

### Recommandation R190 (après retour mainteneur)

1. Documenter run réel (statut, runner, durée, logs, artefacts).
2. Si **success + go** : **Option C** nightly advisory (`allow_failure: true`) + webhook `$MCP_RELEASE_WEBHOOK_URL`.
3. Si **échec infra** (runner/GUI/IPC) : ticket infra CI KiCad — correction YAML/runner uniquement, pas critères green.
4. Si **échec non-go** : analyser advisory/checkpoint summaries — décision produit, pas assouplissement automatique des gates.

- **Date clôture handoff R189** : 2026-06-05
