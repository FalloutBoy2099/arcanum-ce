# Préparation des interfaces et attribution des événements lents

## Changement installé

Le réglage existant `[global] asset_prefetch=1` active maintenant la préparation des images fixes du journal, de l'inventaire et du troc. L'INI installé est inchangé.

Les interfaces enregistrent leurs fonds, onglets, boutons de navigation et silhouettes d'équipement dans un catalogue borné à 128 identifiants uniques. Le journal inclut ses variantes normales et XL. La préparation ne crée aucune fenêtre et ne charge aucun texte de dialogue.

Pendant la boucle de jeu principale, hors menus, pause, chargement et sauvegarde :

- Au maximum quatre images encore à préparer sont proposées au lecteur asynchrone par image affichée.
- Au maximum une image prête est décodée sur le fil principal par image affichée.
- Une image indisponible ne bloque pas la progression vers les suivantes.
- Une image déjà décodée est considérée comme prête.
- Une fois le catalogue préparé, le travail s'arrête. Une réinitialisation de l'interface réarme la préparation.

La lecture utilise le worker et le cache brut existants. Le décodage conserve le cache ART du moteur, ses palettes et ses limites mémoire. Cette limite d'une image par passage n'est pas une garantie absolue de durée : une image volumineuse ou l'entretien du cache peut rester coûteux, ce que mesure `ui_asset_warmup`.

`tig_art_touch_prefetched` ne décode que si le buffer brut est prêt. Il garde ce buffer épinglé pendant le décodage et n'utilise pas de chargement de secours synchrone d'une image manquante. La vérification de disponibilité n'augmente pas les compteurs de hits ou de défauts ; seule la lecture réelle du décodeur conserve cette comptabilisation.

Une ouverture immédiate reste possible : si l'image n'a pas encore été préparée, le chemin normal du moteur est conservé. Les images propres aux objets, équipements et portraits d'un marchand ne sont pas couvertes par le catalogue fixe ; le premier troc peut donc encore charger ces ressources.

## Priorité des lectures

Le lecteur asynchrone choisissait déjà le travail le plus prioritaire parmi les travaux qu'il avait reçus. En revanche, le cache lui transmettait ses demandes dans l'ordre des cases de son tableau. Cette admission est désormais triée par priorité, puis par ancienneté de demande à priorité égale.

Les images d'interface utilisent les priorités 8 à 5. Elles restent sous les ressources immédiatement anticipées par le chargeur de secteur (jusqu'à 16) et au-dessus des propositions éloignées des voisins (1). Une lecture système déjà en cours n'est pas préemptée.

Ce correctif traite l'ordre d'admission ; il ne démontre pas que celui-ci expliquait à lui seul le faible nombre de hits de la capture précédente.

## Attribution de TimeEvent

La capture précédente identifiait des pointes dans `TimeEvent`, sans distinguer la résolution des objets et l'exécution des callbacks.

Deux mesures sont ajoutées :

| Mesure | Travail couvert |
|---|---|
| `event_recovery` | Récupération et validation des objets référencés par l'événement |
| `event_callback` | Exécution de son callback : animation, IA, script, lumière, etc. |

Les opérations de 5 ms ou plus écrivent le nom du type d'événement dans le fichier de détails. Un callback lent ajoute aussi une trace `slow_callback`, dont `subject` est le type d'événement, `correlation` le domaine temporel et `value` sa durée en microsecondes.

L'analyseur regroupe ces traces par type et domaine, convertit leurs durées en millisecondes et regroupe les détails lents par nom. Ces détails ne constituent pas la durée totale de tous les callbacks d'un type : seuls ceux enregistrés comme lents y figurent. Les mesures imbriquées restent inclusives et ne doivent pas être additionnées.

L'ordre d'exécution, les échéances, la récupération des objets et le résultat des callbacks sont inchangés. Le pas fixe, la vitesse de combat et Utility AI ne sont pas activés par cette modification. L'origine exacte de l'ancienne pause de 3,36 secondes reste à confirmer par une nouvelle capture.

## Vérifications

- **36 suites CTest passent** : `out/ui-warmup-events-tests.log` et `out/ui-warmup-events-suite.log`.
- **5 tests Python passent**, dont la sélection des sessions et la conversion des durées de callbacks et de leurs domaines.
- Le catalogue est testé pour les doublons, sa capacité, les états désactivés, le budget de quatre demandes et d'une tentative de décodage, l'absence de blocage sur une image non prête, l'arrêt une fois prêt et le réarmement.
- Le test graphique existant vérifie qu'une image présente sur disque mais non préparée n'est pas chargée par le nouveau helper.
- Après préparation, le décodage consomme le buffer sans lecture native, ne double pas le compteur de hits et ne redécode pas une image déjà en cache.
- Le résultat est comparé pixel par pixel sur une surface de 800 × 600 avec le décodage normal.
- La sérialisation de `slow_callback` est vérifiée dans les traces.

Les tests valident les limites et l'intégration. Ils ne mesurent pas encore le gain à la première ouverture du journal ou du troc dans l'installation réelle.

## Installation et capture suivante

Exécutable installé, hash SHA-256 vérifié : `BD181EC36E25E4A6D9B94A599EEFA0989F90D165EABFAE8EE8BF681F90B5023C`.

Ancien exécutable, INI et captures conservés dans `out/ui-warmup-events-install-20261018-215836/`.

Pour la prochaine capture : charger la même sauvegarde, laisser le jeu actif quelques secondes, lancer le benchmark jusqu'au message de fin, puis ouvrir le journal et le troc et marcher environ deux minutes avant de quitter normalement. Vérifier `prefetch_decoded_art`, `ui_asset_warmup`, les détails de `event_recovery` et de `event_callback`, ainsi que les fenêtres adjacentes aux plus longues images.
