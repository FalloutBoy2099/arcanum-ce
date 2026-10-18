# Lisibilité des callbacks d’interface

Suivi : l’anomalie du callback schématique décrite dans ce rapport a été [corrigée dans le bloc suivant](explicit_ui_messages.md).

Date : 18 octobre 2026. **30 champs, 30 types de callbacks et 2 variables globales renommés**, dans trois fichiers de code.

## Rôles établis

Les noms proviennent des affectations dans `tb_ui_init`, des wrappers de `ui.c` et, pour l’ouverture des schématiques, de l’action de script correspondante. Aucun callback n’a été remplacé et aucune signature n’a été modifiée.

| Ancien champ | Nouveau champ | Type nommé | Fonction affectée |
| --- | --- | --- | --- |
| field_0 | character_panels_refresh | UiCharacterPanelsRefresh | tb_ui_character_panels_refresh |
| field_4 | object_destroyed_notify | UiObjectDestroyedNotify | tb_ui_object_destroyed_notify |
| field_18 | selected_object_set | UiSelectedObjectSet | intgame_selected_object_set |
| field_3C | dialog_script_and_greeting | UiDialogScriptAndGreeting | tb_ui_dialog_script_and_greeting |
| field_58 | object_examine_and_select | UiObjectExamineAndSelect | tb_ui_examine_and_select_object |
| field_64 | hotkey_slot_clear | UiHotkeySlotClear | tb_ui_hotkey_slot_clear |
| field_68 | item_location_assign | UiItemLocationAssign | hotkey_ui_item_location_assign |
| field_6C | item_hotkey_bind | UiItemHotkeyBind | hotkey_ui_item_slot_bind |
| field_70 | targeting_reset | UiTargetingReset | tb_ui_targeting_reset |
| field_74 | counters_refresh | UiCountersRefresh | tb_ui_counters_refresh |
| field_78 | spell_apply | UiSpellApply | tb_ui_spell_apply |
| field_84 | numbered_feedback_display | UiNumberedFeedbackDisplay | intgame_numbered_feedback_display |
| field_88 | mainmenu_request | UiMainmenuRequest | gameuilib_wants_mainmenu_set |
| field_D0 | character_sheet_open | UiCharacterSheetOpen | tb_ui_character_sheet_open |
| field_D4 | item_transaction_process | UiItemTransactionProcess | inven_ui_item_transaction_process |
| field_D8 | inventory_drag_begin | UiInventoryDragBegin | inven_ui_item_drag_begin |
| field_DC | inventory_drag_restore_and_refresh_cursor | UiInventoryDragRestoreAndRefreshCursor | tb_ui_inventory_drag_cancel |
| field_E0 | inventory_drag_object_invalidate | UiInventoryDragObjectInvalidate | inven_ui_drag_object_invalidate |
| field_FC | mainmenu_return | UiMainmenuReturn | mainmenu_ui_return_to_main |
| field_104 | target_skill_resolve | UiTargetSkillResolve | skill_ui_target_skill_resolve |
| field_108 | inventory_drag_cancel | UiInventoryDragCancel | inven_ui_drag_cancel_and_restore |
| field_110 | schematic_open_request | UiSchematicOpenRequest | Aucune affectation observée |
| field_11C | multiplayer_confirmation_1902 | UiMultiplayerConfirmation1902 | tb_ui_multiplayer_confirmation_1902 |
| field_120 | multiplayer_confirmation_1903 | UiMultiplayerConfirmation1903 | tb_ui_multiplayer_confirmation_1903 |
| field_124 | multiplayer_named_confirmation | UiMultiplayerNamedConfirmation | tb_ui_multiplayer_named_confirmation |
| field_12C | inventory_target_object_get | UiInventoryTargetObjectGet | inven_ui_target_object_get |
| field_130 | hovered_object_refresh | UiHoveredObjectRefresh | intgame_hovered_object_refresh |
| field_134 | inventory_open_preconditions_check | UiInventoryOpenPreconditionsCheck | inven_ui_open_preconditions_check |
| field_138 | inventory_game_mode_select | UiInventoryGameModeSelect | inven_ui_game_mode_select |
| field_13C | inventory_target_prepare | UiInventoryTargetPrepare | inven_ui_target_inventory_prepare |

## Variables globales

| Ancien nom | Nouveau nom | Rôle |
| --- | --- | --- |
| stru_5E87C8 | ui_pending_messages | Table des messages différés |
| dword_5E87D8 | ui_next_message_id | Identifiant incrémenté pour programmer un message |

## Limites et anomalie constatée

Les neuf champs non affectés à un traitement identifiable (`field_C`, `field_F8` et les sept callbacks de `field_144` à `field_15C`) conservent leur nom historique. Les champs anonymes de `UiMessage` ne font pas partie de ce bloc. Les confirmations multijoueurs restent désignées par leurs numéros de messages, sans supposer leur formulation dans les ressources.

Le callback `schematic_open_request`, anciennement `field_110`, possède un rôle établi par `SAT_START_SCHEMATIC_UI`, mais aucune affectation dans `tb_ui_init`. La variable locale `UiCallbacks callbacks` n’y est pas initialisée à zéro avant sa copie dans `ui_init`. Ce membre peut donc contenir une valeur indéterminée, ensuite testée et appelée par `ui_schematic_open_request`. Le renommage conserve cet état ; cette anomalie doit être corrigée séparément et ne constitue pas une fonctionnalité implémentée par ce lot.

Les deux callbacks liés à l’annulation d’un déplacement d’objet restent distincts : l’un restaure ou dépose l’objet puis actualise le curseur ; l’autre appelle directement la routine de restauration de l’inventaire. Le renommage ne fusionne pas ces chemins.

## Vérification

Les signatures, types de retour, affectations, conditions de présence et valeurs de retour par défaut sont inchangés. Les champs conservent leur ordre et leur type dans `UiCallbacks`. Les adresses historiques et commentaires de disposition restent présents.

L’inversion des substitutions restitue les trois fichiers précédents octet par octet, y compris leurs encodages et fins de ligne. Aucune modification de comportement ni installation du nouvel exécutable.

Validation : compilation Release x86 réussie et 24 suites CTest réussies, aucun échec.
