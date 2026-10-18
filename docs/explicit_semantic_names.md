# Lisibilité des noms : passe transversale

Date : 18 octobre 2026.

## Modifications validées

Cette passe modifie **113 fichiers de code**. Elle comprend 938 règles de substitution, corrections de noms intermédiaires comprises, et 6051 substitutions. Les paramètres sont comptabilisés séparément : 25 paramètres typés, 95 paramètres de définitions supplémentaires et leurs déclarations correspondantes.

Les noms concernent les fonctions de composants magiques, types de références persistantes, callbacks, constantes de combat et de pathfinding, champs de dialogue, caches graphiques, compteurs et états des interfaces. Aucun commentaire source ajouté.

Les formats de sauvegarde et de paquets, tailles et ordre des structures, valeurs numériques, calculs et branchements sont conservés. Les instructions déjà présentes avant cette passe ne sont pas annulées.

## Exemples de correspondances finales

| Ancien nom | Nom final |
| --- | --- |
| `Ryan` | `ObjectReference` |
| `FollowerInfo` | `ObjectHandleReference` |
| `S4ABF10` | `AiMagicTechCandidates` |
| `S4F2680` | `TargetDescriptorEvaluation` |
| `S5754C0` | `InventoryDeferredDrag` |
| `S64B870` | `MainmenuArtAnimation` |
| `S5C8150` | `CharacterEditorTextEntry` |
| `S5C87D0` | `CharacterEditorArtButton` |
| `S5C8CA8` | `CharacterEditorHitArea` |
| `S5CB3A8` | `MaintainedSpellButton` |
| `S5FC668` | `WaypointDrawEntry` |
| `S5A3FF0` | `WallVisibilityProfile` |
| `MainMenuWindowInfoF3C` | `MainMenuArtLayer` |
| `MTComponentTestNBranch_ProcFunc` | `magictech_component_test_and_branch_process` |
| `MTComponentTraitIdx_ProcFunc` | `magictech_component_indexed_trait_process` |
| `MM_WINDOW_26` | `MM_WINDOW_MAINMENU_WITHOUT_LOAD` |
| `WMAP_UI_STATE_1` | `WMAP_UI_STATE_WAYPOINT_DRAG` |
| `MT_80` | `MAGICTECH_PLAYER_SPELL_COUNT` |
| `MT_140` | `MAGICTECH_FIRST_TECH_ENTRY` |
| `PATH_FLAG_0x0008` | `PATH_FLAG_IGNORE_TILE_BLOCKING` |
| `SKILL_INVOCATION_0x02` | `SKILL_INVOCATION_PLANT_ITEM` |
| `SKILL_INVOCATION_0x04` | `SKILL_INVOCATION_APPLY_DISARM_TRAP` |
| `MAGICTECH_RUN_0x04` | `MAGICTECH_RUN_PROCESSING_STARTED` |
| `MAGICTECH_RUN_0x40` | `MAGICTECH_RUN_END_IMMEDIATE` |
| `dword_5E3500` | `anim_allocated_slot_count` |
| `dword_5DE6CC` | `anim_custom_pause_milliseconds` |
| `dword_5DE5FC` | `path_budget_search_count` |
| `dword_5DE600` | `path_budget_elapsed_milliseconds` |
| `dword_5FC250` | `combat_current_turn_index` |
| `dword_5FC26C` | `combat_taking_damage_script_active` |
| `dword_5E8618` | `timeevent_pause_depth` |
| `dword_6038CC` | `terrain_row_cache_next_slot` |
| `dword_64C6D8` | `intgame_mouse_movement_active` |
| `dword_681510` | `inven_ui_target_max_scroll_offset` |
| `dword_66D880` | `wmap_ui_world_travel_available` |
| `dword_5E2F98` | `object_active_list_count` |
| `dword_6876DC` | `magictech_allocated_run_count` |

Les champs sont qualifiés par leur structure. Par exemple, `MainMenuButtonInfo` expose `destination_window`, `shortcut_key` et `description_message_id`. `WmapInfo` expose `interactive_rect`, `scroll_x`, `scroll_y`, `hovered_note_id` et `screen_to_map_coordinates`. Les bitmaps des objets distinguent `stored_field_bitmap` et `modified_field_bitmap`.

## Vérifications

- Compilation Release x86 réussie.
- Les 24 suites CTest passent.
- `git diff --check` passe sur les sources, tests et outils.
- Inverser toutes les substitutions et les renommages de paramètres restitue exactement les fichiers précédant cette passe, octet par octet, encodages et fins de ligne compris.
- Le nombre de commentaires est inchangé dans chacun des fichiers modifiés.
- L’exécutable installé n’a pas été remplacé.

## Ce qui reste à comprendre

**Le code entier ne peut pas être déclaré parfaitement compréhensible après ce renommage.** Les noms ne reconstruisent pas les corps manquants ni les intentions absentes des appels. Le précédent inventaire de fonctions signale notamment 57 stubs ou fonctions vides et 10 rôles indéterminés.

Il reste 212 graphies distinctes d’identifiants hérités dans 53 fichiers, hors commentaires et chaînes. Ce nombre ne correspond pas à 212 champs : plusieurs structures emploient le même nom `field_4`, par exemple. Il reste également 367 paramètres de définitions nommés `aN` ; cet inventaire n’inclut pas les variables locales `vN`.

Tous ces restes ne sont pas déclarés indéchiffrables. Certains demandent encore une analyse des appels. D’autres ne sont que des champs jamais consultés, des données sérialisées sans interprétation, ou des callbacks réseau remplacés par des macros constantes. Leur rôle original nécessite une reconstruction distincte ; un nom inventé masquerait cette limite.

Exemples de limites établies :

- `MultiplayerDeferredEntry` conserve plus de cent champs copiés en bloc sans traitement reconstruit permettant de leur attribuer une intention.
- Plusieurs valeurs d’animation sont uniquement sauvegardées et rechargées.
- Des champs de `GameSaveInfo` sont présents dans le format binaire mais ne sont pas exploités individuellement.
- Les callbacks d’interface non affectés conservent un nom de rôle réservé ; leur signature est identifiable, leur fonction d’origine ne l’est pas.
- Six macros réseau `sub_xxxx` n’ont plus de corps réel ; trois autres ont pu être nommées à partir de leurs appels.

## Inventaire des fichiers conservant des noms hérités

| Fichier | Graphies distinctes dans le fichier |
| --- | --- |
| `src/main.c` | 1 |
| `src/net_compat.h` | 6 |
| `src/game/anim.c` | 21 |
| `src/game/anim_private.c` | 3 |
| `src/game/anim_private.h` | 12 |
| `src/game/combat.c` | 2 |
| `src/game/gameinit.c` | 5 |
| `src/game/gamelib.c` | 1 |
| `src/game/gamelib.h` | 8 |
| `src/game/invensource.c` | 1 |
| `src/game/item.c` | 1 |
| `src/game/magictech.c` | 15 |
| `src/game/magictech.h` | 17 |
| `src/game/map.c` | 1 |
| `src/game/mp_utils.c` | 8 |
| `src/game/mp_utils.h` | 34 |
| `src/game/mt_ai.c` | 1 |
| `src/game/mt_ai.h` | 2 |
| `src/game/mt_obj_node.c` | 1 |
| `src/game/mt_obj_node.h` | 1 |
| `src/game/multiplayer.c` | 120 |
| `src/game/name.c` | 1 |
| `src/game/obj.c` | 2 |
| `src/game/object.c` | 1 |
| `src/game/obj_private.h` | 1 |
| `src/game/script.c` | 2 |
| `src/game/sector.c` | 1 |
| `src/game/target.h` | 10 |
| `src/game/tb.c` | 1 |
| `src/game/terrain.c` | 2 |
| `src/game/tf.c` | 1 |
| `src/game/tile.c` | 2 |
| `src/game/timeevent_node.h` | 1 |
| `src/game/townmap.h` | 1 |
| `src/game/wallcheck.c` | 1 |
| `src/game/wp.c` | 1 |
| `src/ui/charedit_ui.c` | 5 |
| `src/ui/compact_ui.c` | 1 |
| `src/ui/dialog_ui.c` | 4 |
| `src/ui/intgame.c` | 5 |
| `src/ui/inven_ui.c` | 16 |
| `src/ui/iso.c` | 1 |
| `src/ui/mainmenu_ui.c` | 17 |
| `src/ui/mainmenu_ui.h` | 3 |
| `src/ui/types.h` | 1 |
| `src/ui/wmap_ui.c` | 14 |
| `first_party/tig/src/art.c` | 5 |
| `first_party/tig/src/video.c` | 2 |
| `first_party/tig/include/tig/art.h` | 2 |
| `first_party/tig/include/tig/database.h` | 1 |
| `first_party/tig/include/tig/font.h` | 2 |
| `first_party/tig/include/tig/video.h` | 1 |
| `first_party/tig/include/tig/window.h` | 1 |
