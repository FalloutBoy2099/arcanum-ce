# Correction de la capacité des sources du préchargement

## Capture analysée

La capture ordinaire contient deux nouvelles sessions : `457730176511` et `458375650270`. Le benchmark `457996091393` s’est terminé normalement.

Les compteurs montrent **zéro demande acceptée, zéro hit et zéro octet publié** pour le préchargement. Le cache était bien activé, puisque ses compteurs de défauts étaient présents : 1 625 dans la capture ordinaire et 39 dans le benchmark.

Il est donc incorrect d’attribuer au préchargement les petites différences de temps observées. Le benchmark passe de 17,28 à 17,08 ms d’intervalle moyen, avec une pointe passant de 584 à 568 ms, mais ces variations se produisent sans utilisation effective du cache. Les parcours ordinaires ne chargent pas non plus le même nombre de secteurs : 38 avant contre 20 après.

Comparaison : `out/asset-prefetch-comparison.json`. Les captures de cette version sont conservées avec l’exécutable et l’INI dans `out/asset-prefetch-source-fix-install-20261018-190057/`.

## Cause reproduite

L’adaptateur de résolution ne pouvait stocker que **16 dossiers candidats**. En rencontrant un dossier supplémentaire, il renvoyait volontairement zéro source pour conserver le chargement habituel. Cette protection empêchait un chargement erroné, mais désactivait toutes les demandes sur l’installation réelle.

L’inventaire des dépôts de base, de langue, de haute résolution et du module Arcanum donne déjà **18 répertoires**, plus les archives. Les archives du module et de son patch 9 ne contiennent pas d’ART ; leur présence n’explique pas le rejet des images du monde.

Le test de régression utilise 19 répertoires. Avant correction, il renvoie zéro candidat et échoue, reproduisant la panne : `out/prefetch-repository-before-fix.log`.

## Correctif

Une capacité commune de **64 sources** remplace les limites de 16 dispersées dans le résolveur, le cache et le worker. Elle est déclarée dans `first_party/tig/include/tig/asset_source.h`.

Les limites de contenu restent identiques : cache brut de 32 Mio, staging de 16 Mio et fichier anticipé de 4 Mio maximum. Les structures de sources sont plus grandes ; leur mémoire s’ajoute à ces budgets, comme la pile et les buffers de la bibliothèque C. La capacité de travaux du worker reste 16 : nombre de travaux et nombre de sources par travail sont deux limites différentes.

Deux compteurs rendent les rejets visibles :

- `prefetch_source_rejections` : demandes écartées faute de sources natives prises en charge.
- `prefetch_source_limit_rejections` : dépassement spécifique de la capacité des dossiers candidats.

Un dépassement conserve le repli normal du moteur. La version ne prétend pas prendre en charge un nombre illimité de dépôts.

## Vérifications

- Le test qui échouait obtient maintenant **19 candidats pour 19 répertoires** et charge le bon remplacement.
- Les **34 suites CTest passent**, notamment le cache, les générations, les archives et la comparaison des pixels entre chargement natif et mémoire.
- Les quatre tests Python de l’analyseur passent.
- Un programme de vérification monte les **21 dépôts réels sélectionnés**, dont 18 répertoires, sans charger de carte, d’IA ou de scripts.
- Pour `art/tile/Sodbse0a.ART`, il obtient 18 candidats, accepte la demande, obtient un hit et publie **2 961 octets identiques** à ceux lus par le VFS habituel, sans rejet de source.

Le programme réutilise les routines normales de montage TIG, y compris leur entretien du répertoire de cache technique. Il ne modifie pas les assets ni les sauvegardes.

Programme : `experiments/asset_prefetch_probe.c`. Manifeste : `out/prefetch-game-repositories.txt`. Résultat : `out/prefetch-game-probe.log`. Tests : `out/prefetch-repository-fix-tests.log` et `out/prefetch-repository-fix-suite.log`.

## Installation et suite

Exécutable installé, hash SHA-256 vérifié : `A0A2D1D1DAE765009FF5B76955455C74F861129F4056F5977A2231FAA1AEEFDE`.

`[global] asset_prefetch=1` et `performance_logging=1` sont conservés. Aucun autre réglage du jeu n’est modifié par cette correction.

Le défaut fonctionnel est reproduit puis corrigé et vérifié avec les fichiers réels. Le gain en jeu demande encore une nouvelle capture : même sauvegarde, benchmark, puis déplacement traversant de nouveaux secteurs. Il faudra vérifier des demandes acceptées et des hits avant de comparer les pointes. Les différences de cache système doivent aussi être prises en compte.

La capture ne contient toujours pas de séquence d’attaque exploitable. Le changement de cadence et Utility AI ne sont pas activés pendant cette validation du chargement.
