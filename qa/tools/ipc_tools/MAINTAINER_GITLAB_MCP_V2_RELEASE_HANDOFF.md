# Handoff mainteneur — premier run GitLab `mcp_v2_release_pack_windows`

> Copier-coller le bloc ci-dessous (email, issue GitLab, MR).  
> Référence : **`MCP_DECISIONS.md`** R189 · job livré R187 · YAML validé R188/R189.

---

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

---

## Statut

| Phase | Statut |
|-------|--------|
| R189 handoff | **clôturé** — ce fichier est la version canonique |
| Merge R187/R188/R189 sur `master` upstream | **non mergé** (vérif. publique 2026-06-05) — voir ci-dessous |
| Envoi mainteneur | action manuelle (email / issue / MR) |
| R190 | **non démarrée** — attend checklist ou artefacts |

### Vérification GitLab upstream (`master`, lecture publique)

| Contrôle | Résultat |
|----------|----------|
| `.gitlab-ci.yml` contient stage `release` | **non** — stages actuels : `build`, `test`, `report` uniquement |
| Include `MCP-Windows-Release.yml` | **non** |
| Fichier `.gitlab/MCP-Windows-Release.yml` | **absent** sur `master` |
| Job `mcp_v2_release_pack_windows` visible | **non** — YAML non présent sur `master` |

**Conséquence** : ouvrir/merger la MR R187–R189 **avant** le premier run — guide : **`MR_GITLAB_MCP_V2_RELEASE_PACK.md`**.

Le mainteneur choisit :

| Option | Quand | Action |
|--------|-------|--------|
| **A — Branche MR** | avant merge | Play sur le pipeline de la **branche MR** |
| **B — `master`** | après merge | Merger la MR, puis Play sur le pipeline **`master`** |

Sources : [`.gitlab-ci.yml` sur master](https://gitlab.com/kicad/code/kicad/-/blob/master/.gitlab-ci.yml) · [`MCP-Windows-Release.yml` absent](https://gitlab.com/kicad/code/kicad/-/blob/master/.gitlab/MCP-Windows-Release.yml)

**Attention** : `run_mcp_v2_release_pack.ps1` est aussi **absent de `master`** — la MR doit inclure les scripts QA requis (voir **`MR_GITLAB_MCP_V2_RELEASE_PACK.md`**).

## Après retour mainteneur

Coller la checklist remplie + pièces jointes pour démarrer **R190** (`MCP_DECISIONS.md`).
