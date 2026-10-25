# Utility AI : visibilité conjointe des sorts et des potions

Correction de F03 de l'audit du projet. Le mode installé reste `[game] utility_ai=2`.

## Changement

Utility AI utilise désormais `mt_ai_utility_actions_collect`, une collecte dédiée qui examine les sorts connus et les objets de l'inventaire, sans utiliser la liste historique déjà mise en cache. La fatigue n'exclut plus une famille de ressources avant la comparaison. La collecte est renouvelée à chaque décision et réutilisée localement pour chaque catégorie consultée.

Le collecteur conserve les règles existantes d'identification, de puissance effective et de restriction des objets lorsque l'arme est maudite. Les charges nulles ne fournissent pas de sort d'objet. Les actions d'objet marquées `OIF_VALID_AI_ACTION` restent reconnues même sans numéro de sort. Les numéros de sort d'objet hors domaine sont écartés avant l'accès aux métadonnées.

Les deux familles sont collectées séparément, puis alternées dans le résultat pour éviter qu'une longue liste de sorts remplisse tous les emplacements avant le premier objet. Les doublons portant le même sort et le même objet sont fusionnés ; deux exemplaires distincts d'une potion restent des ressources distinctes.

Les limites restent explicites : les tampons historiques de collecte contiennent au plus 200 entrées par famille et le résultat exposé à Utility AI contient au plus 32 ressources par catégorie. Il ne s'agit donc pas d'une recherche exhaustive sans borne dans un inventaire arbitrairement grand.

L'adaptateur évalue les 32 ressources proposées et conserve les meilleurs candidats de la catégorie selon leur score, au lieu d'accepter seulement les premières entrées. Les sorts trop coûteux ou sans utilité courante ne prennent plus les places réservées aux candidats. Les quotas existants de deux candidats par soutien et huit par catégorie principale sont conservés, ainsi que le plafond global de 128 et les huit validations finales.

Les listes locales appartiennent maintenant à la décision ; elles ne sont plus des pointeurs vers des listes susceptibles d'être remplacées dans le cache historique. La collecte ne consomme aucun objet et ne lance aucun sort. La validation et l'exécution habituelles du moteur restent responsables de l'utilisation effective.

`mt_ai_create` conserve sa sélection historique pour les autres voies d'IA, notamment le mode `utility_ai=0`. L'observation de Utility AI utilise la collecte fusionnée pour ses propositions, puis laisse la voie historique s'exécuter. La nouvelle collecte ne modifie pas les listes du cache historique.

## Validation

Le test `utility_ai_resources` compile `mt_ai.c` et `utility_ai.c` réels avec un acteur et un inventaire simulés. Il vérifie :

- une liste historique contenant uniquement les sorts malgré la présence d'une potion ;
- une collecte fusionnée voyant la potion et les sorts, y compris après baisse de fatigue ;
- le choix d'une potion lorsque les sorts sont trop coûteux et celui d'un sort réutilisable dans un autre cas où son coût est favorable ;
- la disparition d'un objet sans charges et la prise en compte de changements de sorts connus ;
- la présence d'une action de récupération sans numéro de sort et sa disparition après retrait de l'objet ;
- la suppression des doublons, la conservation de deux objets distincts, les numéros de sort invalides et les bornes du tampon ;
- l'absence de modification de la liste historique par la nouvelle collecte.

Compilation Release Windows x86 et 41/41 suites CTest réussies. L'adaptateur de `ai.c` est compilé ; l'animation et la consommation effective dans une partie restent à vérifier visuellement. Aucun gain de temps CPU n'est annoncé : la collecte fraîche fait davantage de lectures qu'une ancienne liste mise en cache, mais chaque catégorie n'est collectée qu'une fois par décision et les budgets restent bornés.

Installation dans `C:\Games\Arcanum`, sans changement de l'INI, des prototypes ou des sauvegardes. Exécutable précédent et INI conservés dans `out/utility-ai-resources-install-20261019/`.
