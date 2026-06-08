# MR GitLab — release pack MCP v2 (R187–R189)

> **Objectif** : ouvrir/merger une MR sur `gitlab.com/kicad/code/kicad` pour rendre le job `mcp_v2_release_pack_windows` visible et exécutable.  
> **R190** ne démarre qu'après un run GitLab réel (URL job, checklist, ou artefacts).

---

## État upstream (`master`, lecture publique)

| Contrôle | Upstream `master` | Local |
|----------|-------------------|-------|
| `.gitlab/MCP-Windows-Release.yml` | **absent** | présent |
| Stage `release` dans `.gitlab-ci.yml` | **absent** | présent |
| Include `MCP-Windows-Release.yml` | **absent** | présent |
| Job `mcp_v2_release_pack_windows` | **non visible** | défini |
| `run_mcp_v2_release_pack.ps1` | **absent** | présent |

**Conséquence** : la MR doit être **ouverte et mergée** (ou le job joué sur le pipeline de la branche MR) avant tout handoff mainteneur sur `master`.

---

## Fichiers obligatoires — périmètre R187–R189

### CI GitLab (R187)

| Fichier | Modification |
|---------|--------------|
| `.gitlab/MCP-Windows-Release.yml` | **nouveau** — job `mcp_v2_release_pack_windows` |
| `.gitlab-ci.yml` | stage `release` + include `/.gitlab/MCP-Windows-Release.yml` |

### Documentation (R187 / R188 / R189)

| Fichier | Contenu |
|---------|---------|
| `qa/tools/ipc_tools/MCP_DECISIONS.md` | sections R187, R188, R189 |
| `qa/tools/ipc_tools/README_IPC_QA_BASELINE.md` | sections R186–R189 |
| `qa/tools/ipc_tools/MCP_KICAD_USER_DEV_GUIDE.md` | références R186–R189 |
| `qa/tools/ipc_tools/MCP_V1_CHECKPOINT_RELEASE_NOTE.md` | R186–R189 |
| `qa/tools/ipc_tools/MAINTAINER_GITLAB_MCP_V2_RELEASE_HANDOFF.md` | handoff mainteneur (canonique) |
| `qa/tools/ipc_tools/MR_GITLAB_MCP_V2_RELEASE_PACK.md` | ce fichier |

---

## Dépendance runtime — scripts QA (hors périmètre doc R187–R189)

Le job appelle `qa/tools/ipc_tools/run_mcp_v2_release_pack.ps1`, **absent de `master` upstream**.

Chaîne invoquée à l'exécution :

| Script | Mission |
|--------|---------|
| `run_mcp_v2_release_pack.ps1` | R184/R185 — pipeline release |
| `run_mcp_v2_ci_baseline.ps1` | CI baseline |
| `report_mcp_v2_ci_advisories.ps1` | R180 |
| `check_mcp_v2_release_advisories.ps1` | R181 |
| `export_mcp_v2_release_pack.ps1` | R182/R183 |
| `ipc_qa_common.ps1` | module partagé |

**Recommandation MR** : inclure **dans la même MR** (ou une MR empilée préalable) tous les scripts QA nécessaires au job, sinon le pipeline échouera immédiatement (`script not found`).  
Ne pas modifier la logique de ces scripts dans le cadre R187–R189 — seulement les **ajouter** s'ils ne sont pas déjà mergés.

---

## Checklist avant ouverture MR

```text
[ ] Branche créée depuis master récent
[ ] .gitlab/MCP-Windows-Release.yml ajouté
[ ] .gitlab-ci.yml : stage release + include MCP-Windows-Release.yml
[ ] Docs R187/R188/R189 incluses
[ ] MAINTAINER_GITLAB_MCP_V2_RELEASE_HANDOFF.md inclus
[ ] Scripts run_mcp_v2_release_pack.ps1 + dépendances présents sur la branche
[ ] Pipeline MR : stage release visible
[ ] Job mcp_v2_release_pack_windows visible (manuel)
[ ] Aucune modification handlers C++ / schémas MCP / critères green
```

---

## Description MR — modèle (copier-coller)

```markdown
## Summary

Add manual GitLab Windows job `mcp_v2_release_pack_windows` for MCP v2 release pack validation (R187–R189).

- New stage `release` (manual job only; does not block dev MR pipelines)
- Job runs `run_mcp_v2_release_pack.ps1` on runner `kicad-windows-ltsc2022`
- Job fails if `release_decision != go` (intentional release gate)
- Artifacts uploaded with `when: always` (30 days)

## CI changes

- `.gitlab/MCP-Windows-Release.yml` (new)
- `.gitlab-ci.yml` — stage `release` + include

## Docs

- `qa/tools/ipc_tools/MCP_DECISIONS.md` — R187/R188/R189
- `MAINTAINER_GITLAB_MCP_V2_RELEASE_HANDOFF.md` — maintainer runbook

## Maintainer — first run options

Please choose **one**:

**Option A — MR branch pipeline (recommended before merge)**  
1. Open CI/CD → Pipelines on **this MR branch**  
2. Stage `release` → Play `mcp_v2_release_pack_windows`  
3. Return filled checklist + artifacts (see handoff file)

**Option B — After merge to master**  
1. Merge this MR  
2. Open pipeline on merged commit  
3. Stage `release` → Play `mcp_v2_release_pack_windows`  
4. Return filled checklist + artifacts

## Test plan

- [ ] Pipeline parses (CI lint green)
- [ ] Job `mcp_v2_release_pack_windows` visible on MR pipeline
- [ ] Manual Play completes (or failure documented with logs)
- [ ] Artifacts `MCP v2 Release Pack` downloadable

## Out of scope

- No C++ handler changes
- No MCP schema changes
- No green criteria changes
- No nightly / webhook (future R190+)
```

---

## Commandes locales — pré-vol MR

```powershell
cd <kicad-repo-with-git>

git status --short

# Fichiers CI
git diff -- .gitlab-ci.yml .gitlab/MCP-Windows-Release.yml

# Présence job dans YAML
Select-String -Path .gitlab-ci.yml,.gitlab\*.yml `
  -Pattern "MCP-Windows-Release|mcp_v2_release_pack_windows|release|kicad-windows-ltsc2022" `
  -CaseSensitive:$false

# Script release présent sur la branche
Test-Path qa\tools\ipc_tools\run_mcp_v2_release_pack.ps1
```

---

## Après merge / premier run

1. Envoyer **`MAINTAINER_GITLAB_MCP_V2_RELEASE_HANDOFF.md`** au mainteneur.
2. Attendre checklist remplie ou artefacts.
3. Démarrer **R190** à réception.
