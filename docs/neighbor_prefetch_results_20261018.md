# Résultats de la capture avec anticipation des voisins

## Captures sélectionnées

Les fichiers sont conservés dans `out/neighbor-prefetch-capture-20261018/`, avec leur analyse dans `out/neighbor-prefetch-analysis.json`. Les sessions historiques présentes dans les CSV cumulatifs sont exclues.

- Jeu : `561150927099`, `561984490008`, `562105921083`.
- Benchmark complet : `561603971314`, terminé après les 15 phases.
- Deuxième tentative : `562025628627`, interrompue pendant `walk_3` par `interrupted_focus_lost`.

Le dernier statut global correspond à cette deuxième tentative ; il n'annule pas la capture complète précédente. Le parcours complet part de 62065,65610 et diffère de la référence antérieure, qui partait de 62064,65616.

## Anticipation des ressources

Sur les nouvelles sessions ordinaires :

| Compteur | Résultat |
|---|---:|
| Lectures de voisins demandées | 97 |
| Prévisualisations valides | 84 |
| Lectures/prévisualisations en échec | 13 |
| Identifiants graphiques proposés | 20 801 |
| Demandes acceptées par le cache brut | 1 185 |
| Octets publiés, cumulés | 20 937 933 |
| Acquisitions servies depuis le cache brut | 1 |
| Acquisitions manquées | 1 918 |
| Échecs de chargement des secteurs du jeu | 0 |

Les 13 échecs de prélecture ne sont pas 13 échecs de chargement du jeu : une absence de secteur cartographique peut être suivie d'un terrain de remplacement. Les octets publiés représentent environ 20 Mio cumulés, pas la mémoire résidente. Les propositions d'identifiants ne sont pas toutes des fichiers distincts : variantes et rotations peuvent partager une image, et les images déjà décodées sont ignorées.

Le fonctionnement est confirmé, mais **aucun gain utile du cache brut n'est démontré** : une seule acquisition est servie depuis les données préparées. Le réchauffement éventuel du cache Windows n'est pas mesuré séparément. Le coût sur le fil principal reste faible : 0,015 ms en moyenne et 0,682 ms au maximum pour le tick d'anticipation, hors coûts des autres fonctions du cache et hors travail du worker.

Pendant la dernière session après benchmark, seuls deux secteurs sont chargés, en environ 1 ms chacun. Ce déplacement sollicite donc peu le chargement de nouveaux secteurs. Les chiffres ne permettent pas de valider l'efficacité de l'anticipation à la traversée de nombreuses frontières.

## Benchmark complet

Le run complet mesure 2 219 intervalles entre images, avec une moyenne de **17,13 ms** et une pointe de **572,88 ms**. La référence précédente était à 16,74 ms, mais les positions, le parcours et l'état des caches diffèrent : cela ne constitue pas une comparaison contrôlée ni la preuve d'une régression causée par l'anticipation.

La pointe principale coïncide avec l'entrée dans les rumeurs : `benchmark_control/begin_frame` mesure **553,72 ms** dans la fenêtre 9. Les ouvertures de `Tab_Keys.ART`, `Tab_BackGrnd.ART` et `Tab_Rep.ART` prennent chacune environ 143 à 145 ms. Cette attente est donc associée à l'ouverture d'une interface et à ses ressources, et non à un nouveau secteur du monde. Aucun secteur n'est chargé pendant le benchmark.

## Attribution des pauses

| Observation | Attribution mesurée |
|---|---|
| Intervalle de 6 901 ms dans la première session | Gestion des messages à 6 881 ms, incluant le menu modal à 6 864 ms. Le temps passé dans le menu est inclus ; ce chiffre n'est pas une preuve de gel de la marche. |
| Image de 1 097 ms dans la première session | Module `TimeEvent` à 1 058 ms, avec des chargements de secteurs dans la même fenêtre. |
| Intervalle de 858 ms dans la dernière session | Gestion des messages à 842 ms, avec chargement des ressources du troc et de l'inventaire dans cette fenêtre. Les appels d'entrées déjà instrumentés ne couvrent pas à eux seuls cette durée. |
| Plus longue simulation de la dernière session | 119,57 ms, dont `TimeEvent` à 119,53 ms. |
| Publication des CSV | 7,39 ms au maximum ; aucun flush de plusieurs secondes dans cette capture. |
| Tick d'anticipation des voisins | 0,682 ms au maximum ; aucun tick de plusieurs secondes. |

Aucun fondu, appel mesuré de téléportation ou de sauvegarde n'est enregistré dans ces sessions. Les traces détaillées n'ont perdu aucun événement par saturation ; 69 062 traces d'IA et de retard sont volontairement omises par les quotas, avec maintien des mesures agrégées.

**La pause antérieure de 3,36 secondes n'est pas reproduite à l'identique.** Sa localisation dans la simulation reste établie par l'ancienne capture, mais cette nouvelle capture ne permet pas d'en attribuer rétroactivement le coût exact à un module. Elle identifie en revanche `TimeEvent` comme responsable des plus longues pointes de simulation observées ici. Ce module exécute plusieurs catégories de callbacks : ce résultat ne signifie pas que la simple gestion de l'horloge est lente, ni que tout le coût vient du calcul des chemins.

Le pathfinding atteint 2,58 ms au maximum dans les nouvelles sessions ordinaires. Les décisions d'IA atteignent 203,96 ms et incluent les opérations qu'elles déclenchent. Les temps imbriqués ne doivent pas être additionnés.

## Ce qu'il faut traiter ensuite

1. Attribuer les callbacks lents de `TimeEvent` à leur type et aux chargements qu'ils déclenchent, pour agir sur la cause des pauses en simulation.
2. Préparer les ressources des interfaces avant leur ouverture, notamment journal et troc : leur première ouverture reste coûteuse.
3. Améliorer la sélection et le moment des lectures anticipées du monde avant d'étendre leur volume. Le seul taux de prévisualisations réussies ne justifie pas une augmentation de la mémoire ou du nombre de workers. Les objets et leurs images ne sont pas anticipés par l'extraction actuelle des sols et toits.

Cette analyse n'ajoute pas de modification à l'exécutable installé et ne change ni l'INI ni les règles de jeu. Le premier benchmark complet est exploitable ; la tentative interrompue est conservée séparément et ne doit pas être agrégée avec lui pour comparer les performances.
