# Correction des descriptions de magie

Le survol des écoles de magie pouvait afficher `Error! Missing line 9015 in Rules\spelllist.mes`. Les textes sont pourtant présents dans `mes/spell.mes`, en français comme en anglais.

Une régression du renommage avait attribué le même identifiant C à deux variables statiques distinctes : la référence temporaire du fichier des règles à l'adresse d'origine 0x5E6D20 et la référence persistante du catalogue de textes à 0x5E7594. C accepte ces déclarations répétées et les associe au même stockage. Le chargement des règles écrasait ainsi la référence du catalogue de textes ; le chargement des composants déchargeait ensuite cette référence, alors que l'interface et les noms des sorts devaient continuer à utiliser le catalogue.

La référence des règles s'appelle désormais `magictech_spell_rules_mes_file`. Elle est utilisée pour charger les en-têtes, les composants et décharger les règles. Les noms, descriptions et messages continuent à employer `magictech_spell_mes_file`, chargé depuis `mes/spell.mes`. Aucun texte ni règle de sort n'a été modifié.

Validation : compilation Release, 38 suites CTest réussies, deux tests Python dont une recherche des déclarations statiques de références MES en double dans tout `src`, et vérification de 132 identifiants de texte dans chacun des catalogues anglais et français. L'identifiant 9015 y décrit la maîtrise de l'école temporelle. Le survol en partie reste à vérifier après redémarrage.

Exécutable installé dans `C:\Games\Arcanum` après fermeture du jeu. Version précédente conservée dans `out/spell-catalog-fix-install-20261018/`.
