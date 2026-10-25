# Bloc 1 : préchargement des images natives

La première capture de ce bloc a révélé un rejet de toutes les demandes sur l’installation réelle. Ce défaut a été reproduit et corrigé : voir `docs/asset_prefetch_repository_fix_20261018.md` pour l’état installé et les vérifications actuelles. Les mesures ci-dessous décrivent la référence et l’implémentation initiale.

## Résultat de la capture du bloc 0

Le benchmark `416990209346` s’est terminé avec le statut `finished`, dans Tarant. Ses déplacements sont des allers-retours courts de deux cases : il n’a pas chargé de nouveau secteur pendant son exécution. Les captures ordinaires de cette exécution ont les sessions `415820324782`, `416892772442` et `417373998126`.

Les chiffres suivants excluent les sessions anciennes présentes dans les mêmes fichiers. L’analyseur accepte maintenant `--instrumented` ou `--session` pour les isoler.

| Mesure | Capture ordinaire | Benchmark |
|---|---:|---:|
| Secteurs chargés | 38 | 0 |
| Temps cumulé des chargements de secteur | 11 221 ms | — |
| Temps cumulé de préparation ART des secteurs | 9 010 ms | — |
| Temps cumulé de chargement ART | 13 322 ms | 694 ms |
| Dont ouvertures des fichiers ART | 12 930 ms | 681 ms |
| Lectures natives, tous fichiers instrumentés | 296 ms | 5,5 ms |
| Décompression DAT instrumentée | 29 ms | aucune observation |
| Plus long intervalle d’image observé | 1 151 ms | 584 ms |

Les timings sont imbriqués et ne doivent pas être additionnés. Les grands chargements de la capture ordinaire incluent notamment l’entrée dans la partie : ils ne sont pas tous des pauses de scrolling.

Environ **97 %** du temps de chargement ART observé dans la capture ordinaire, et **98 %** dans le benchmark, se trouve dans la phase d’ouverture. Le pixel decoding et le transfert du contenu ne dominent donc pas cette capture. Elle oriente le premier prototype vers l’anticipation des accès aux fichiers et la réutilisation des octets préparés.

L’intervalle moyen du benchmark est de 17,28 ms, mais cela ne décrit pas ses pointes. Son plus mauvais p99 de fenêtre est de 584 ms ; ce n’est pas un p99 global.

Aucune séquence d’attaque exploitable n’a été enregistrée. La trace ordinaire contient aussi 895 événements perdus lors de sa saturation ; celle du benchmark n’en a perdu aucun. Les très grands retards d’événements de jeu issus du chargement ne prouvent pas une cadence d’attaque incorrecte. Le bloc combat reste à mesurer sur une scène adaptée.

Analyse : `out/runtime-instrumented-capture.json`. Les captures originales sont sauvegardées avant installation de ce bloc.

## Implémentation

Réglage ajouté :

```ini
[global]
asset_prefetch=1
```

La valeur par défaut du code reste `0`. Elle est activée à `1` dans l’installation locale pour l’essai. La valeur `0` conserve le chargement habituel.

Version initialement installée et vérifiée par SHA-256 : `8D959C015021B81A9C9F5CC27350942A5288F5C090412367A7760F90A1AB69FF`. L’exécutable précédent, l’INI et les captures de référence sont conservés dans `out/asset-prefetch-install-20261018-175834/`.

Le moteur anticipe les prochaines images de la liste ART d’un secteur, avec une fenêtre de 16 entrées. Un worker lit les fichiers ordinaires et place leurs octets dans une file de préparation. Le fil principal récupère les résultats et les conserve dans un cache LRU.

Le décodeur ART consomme directement ce buffer lorsqu’il est disponible. Sinon, il ouvre le fichier comme auparavant : il n’attend pas le résultat du worker. Le décodage, les palettes et les ressources graphiques restent sur le fil principal.

### Limites de mémoire

- Cache brut : **32 Mio**, au maximum 128 entrées.
- Données en préparation : **16 Mio**, au maximum 16 travaux.
- Taille maximale anticipée d’un fichier : **4 Mio** ; les autres utilisent la voie habituelle.
- Structures de gestion, buffers de la bibliothèque C et pile du worker : mémoire supplémentaire distincte.
- Le cache ART décodé existant reste une autre couche, avec ses limites historiques.

Un buffer utilisé par un décodage est épinglé et ne peut pas être évincé. Un changement de génération invalide les travaux périmés ; les buffers encore utilisés restent valides jusqu’à leur libération. La destruction attend la fin de la lecture en cours.

### Résolution des ressources

Le fil principal prend un instantané des chemins candidats dans l’ordre des dépôts. Le worker possède uniquement ces chaînes et ses handles de fichiers : il ne manipule pas les listes de dépôts, les objets, les scripts ou les caches graphiques.

Si une archive DAT contient le fichier demandé, le préchargement de cette ressource est écarté pour cette première version. Son chargement reste entièrement confié au système existant, y compris lorsqu’un remplacement libre peut être prioritaire. Ce repli conservateur évite d’interpréter différemment les règles de remplacement des archives.

Les ajouts, suppressions et changements de priorité des dépôts invalident le cache brut. `tig_art_flush` l’invalide aussi. Comme pour le cache ART existant, les fichiers d’assets sont considérés comme stables pendant une partie ; une modification externe à chaud nécessite une invalidation ou un redémarrage.

### Portée du premier bloc

Ce bloc prépare les **images** associées aux secteurs qui passent par leur préparation ART. Il ne précharge pas encore tout le contenu de secteurs futurs, ne change pas leur activation et ne déplace aucun script sur un worker. Il ne garantit donc pas la suppression de toutes les pauses de scrolling.

La grille spatiale existante est conservée. Le cache brut utilise une petite table bornée et une sélection LRU ; aucun gain provenant d’un nouveau spatial hash n’est revendiqué. L’anticipation prédictive des secteurs autour de la caméra reste une extension distincte.

Les compteurs `prefetch_requests`, `prefetch_hits`, `prefetch_misses` et `prefetch_ready_bytes` permettent de vérifier l’utilisation du bloc. Le dernier est un cumul d’octets publiés, pas une mesure de mémoire résidente.

## Validation

Les **34 suites CTest passent** en Release x86. Les quatre tests Python de l’analyseur passent également.

Les nouveaux tests couvrent :

- Lecture complète, demande dupliquée, file pleine, limite de taille, fichier absent et vide.
- Rejet des résultats d’anciennes générations.
- Priorité des répertoires et invalidation après ajout d’un remplacement.
- Repli sur une archive DAT synthétique, même en présence d’un fichier libre concurrent.
- Dépassement des 32 Mio avec neuf buffers de 4 Mio, éviction de l’ancien non épinglé et maintien du buffer épinglé.
- Conservation d’un buffer actif pendant une invalidation.
- Comparaison de tous les pixels du rendu d’un ART chargé normalement puis depuis le cache, avec vérification qu’un accès au cache a effectivement eu lieu.

Sources principales : `first_party/tig/src/async_asset_reader.c`, `asset_prefetch.c`, `file.c`, `art.c` et `src/game/sector.c`.

## Prochaine comparaison en partie

Recharger la même sauvegarde, refaire le benchmark, puis marcher environ une minute dans Tarant pour traverser de nouveaux secteurs. Le benchmark court seul ne suffit pas à mesurer le chargement spatial. Quitter normalement le jeu pour vider les captures.

Comparer les défauts de cache, ouvertures ART sur le fil principal, temps de chargement de secteurs et pointes d’images. Les caches du système se réchauffent entre les exécutions : une baisse du temps de démarrage ne peut pas être attribuée intégralement au préchargement sans comparaison contrôlée.

Aucun facteur d’accélération ni gain de FPS en partie n’est annoncé à ce stade. Le pas fixe, le ralentissement du combat et Utility AI ne sont pas activés.
