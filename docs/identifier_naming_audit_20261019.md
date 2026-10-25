# Audit des noms de fonctions et de variables — 19 octobre 2026

## Résultat de cette passe

45 fichiers modifiés, 719 substitutions de tokens, 19 identifiants globaux renommés et 166 règles de renommage dans les fonctions. Les déclarations et définitions représentent 263 applications de règles locales. Aucun commentaire ajouté au code.

Le rôle des identifiants renommés a été déterminé à partir de leurs lectures, écritures, appels et types. Cette passe couvre notamment les dialogues, les données graphiques, les projectiles, les inventaires, les coûts magiques, les réactions et les coordonnées de la carte.

## Validation

- Compilation Release Win32 réussie.
- 48 suites CTest réussies, aucun échec.
- Inversion des 719 substitutions : les 45 fichiers redeviennent exactement identiques, octet par octet, aux copies précédant cette passe. Cela vérifie que les chaînes, commentaires, constantes, dispositions des structures et instructions restent identiques, hormis les identifiants.
- Exécutable compilé ; aucune installation dans le jeu effectuée pour cette passe de noms.

## Inventaire restant

Le relevé porte sur les fichiers C et H de `src` et `first_party`, hors commentaires et littéraux. Les occurrences comptent les références lexicales, pas les entités : deux paramètres `a1` appartenant à deux fonctions différentes ne sont pas le même paramètre. Les prototypes sont inclus. Les constantes candidates et suffixes `stub` nécessitent une lecture humaine avant toute décision.

| Famille | Occurrences avant cette passe | Après | Graphies distinctes après | Fichiers après |
|---|---:|---:|---:|---:|
| Noms issus des adresses | 262 | 166 | 66 | 22 |
| Champs nommés par offset | 568 | 568 | 129 | 39 |
| Graphies a1, a2… | 1804 | 1432 | 8 | 75 |
| Graphies v1, v2… | 2384 | 2142 | 43 | 37 |
| Fonctions suffixées stub | 159 | 159 | 55 | 30 |
| Constantes numériques candidates | 57 | 57 | 14 | 10 |

Il ne reste pas de définition de fonction de production nommée `sub_xxxx`. Six macros réseau conservent ces noms dans `net_compat.h` : `sub_5280F0`, `sub_52A940`, `sub_52A950`, `sub_52B210`, `sub_5286E0` et `sub_52A9E0`. Leurs corps d’origine sont absents ; les renommer comme des actions établies inventerait une sémantique.

### Limites et classement

- Les champs `field_XX` ne sont pas tous interchangeables : un même offset apparaît dans plusieurs structures. Les renommer exige une analyse par structure et par voie de lecture/écriture.
- Plusieurs données d’animation sont seulement lues et écrites dans les sauvegardes. Leur place dans le format est connue, leur sens métier reste à établir.
- La structure réseau `MultiplayerDeferredEntry` contient de nombreux champs recopiés sans traitement reconstruit. Leur rôle ne peut pas être déduit du seul transfert de mémoire.
- Les noms `aN` et `vN` encore présents comprennent des variables de fonctions complexes dont le rôle ne se déduit pas de leur type. Leur nombre ne constitue pas un nombre de bugs.
- Les fonctions suffixées `stub` signalent une implémentation absente ou partielle. Un renommage ne rétablit pas le comportement manquant.
- Les callbacks réservés sans affectation, la valeur réservée multijoueur et certaines propriétés artistiques restent explicitement indéterminés.

Cette analyse consolide les renommages certains et recense le travail restant. Elle ne conclut pas que tout le code possède désormais une sémantique entièrement reconstruite. Les motifs lexicaux ne détectent pas tous les noms médiocres qui semblent déjà descriptifs.

## Fichiers de suivi

- [Inventaire complet avec fichiers, lignes et fonctions](identifier_naming_audit_20261019.json).
- [Correspondance complète des renommages de cette passe](identifier_renames_20261019.json).
- [Outil de relevé reproductible](../tools/audit_identifier_names.py).
- [Historique des renommages de fonctions](explicit_function_names_final.md).
- [Précédent bilan sémantique](explicit_semantic_names.md).

Pour régénérer le relevé depuis la racine du projet :

```powershell
python tools/audit_identifier_names.py --output docs/identifier_naming_audit_20261019.json
```

## Renommages globaux

| Ancien | Nouveau |
|---|---|
| `anim_create_path_max_length` | `anim_path_length_error_log` |
| `byte_5D0B58` | `gamelib_optional_repository_path` |
| `dword_5B3FD8` | `multiplayer_reserved_setting_value` |
| `dword_5B57FC` | `combat_projectile_offset_by_arrow_type` |
| `dword_5BEA18` | `tig_art_wall_primary_piece_by_direction` |
| `dword_5BEA28` | `tig_art_wall_secondary_piece_by_direction` |
| `dword_5C4004` | `mainmenu_ui_map_entry_hook_enabled` |
| `dword_5C6D58` | `intgame_saved_rotating_window_type` |
| `dword_5F0DEC` | `multiplayer_object_id_list` |
| `dword_5F0E00` | `multiplayer_session_active` |
| `dword_603A20` | `terrain_type_blocked_flags` |
| `dword_64C6AC` | `intgame_requested_rotating_window_type` |
| `dword_739E50` | `name_art_picker_extra_parameters` |
| `dword_739F58` | `inven_ui_close_requested` |
| `redraw_inven_fix_bad_inven` | `inven_ui_invalid_parent_link_remove` |
| `stru_5C36B0` | `mainmenu_ui_mode_behavior_flags` |
| `stru_5C83F0` | `charedit_basic_info_text_rows` |
| `stru_5C8970` | `charedit_gauge_top_cap_rect` |
| `stru_5C8980` | `charedit_gauge_bottom_cap_rect` |


## Suite du 24 octobre 2026

Le [bilan de la passe finale](identifier_naming_completion_20261024.md) remplace l’inventaire restant ci-dessus pour le code actuel : les familles de noms hérités relevées sont à zéro. Les données non reconstruites sont explicitement réservées et les fonctions incomplètes conservent leur suffixe `stub`. Les 54 suites passent dans les deux architectures.
