# Lot 3 : fabrication, recherche et journal

## Fabrication

Un aperçu local partage le relevé des inventaires entre l'affichage des
composants, des stocks, de l'expertise et de la disponibilité de la paire.
Il est recalculé à chaque affichage. Le clic de fabrication revalide les
inventaires avant consommation : aucun cache persistant d'objets n'est ajouté.

## Recherche des schémas

`ui/search_cache` conserve les indices correspondants et permet de naviguer
dans les résultats en boucle. La requête, le nombre et l'ordre des schémas,
le personnage, la langue, la vue et les degrés technologiques déterminent la
validité. Les données du module sont immuables pendant la vie de cet écran.
Le cache est libéré à la fermeture et à la réinitialisation. Sa capacité suit
le nombre de schémas de l'écran, sans conserver plusieurs jeux de résultats.

Les rafraîchissements refont une empreinte des IDs et des degrés, mais évitent
les recherches textuelles dans les noms et composants si rien n'a changé.

## Conversations

L'historique expose une révision en mémoire, incrémentée lors d'un ajout valide,
d'une remise à zéro ou d'un chargement. Un ajout dans l'historique plein change
également cette révision. Le format des sauvegardes reste identique.

Les fragments et les débuts de pages sont conservés entre consultations. La
révision, la langue, la police et la géométrie des pages invalident le résultat.
Les limites existantes restent de 1 000 fragments de moins de MAX_STRING octets
et 512 doubles pages. Le cache est libéré à la réinitialisation et à la sortie.

## Index des lieux et personnages

Chaque consultation relit les candidats et les connaissances actuels, ainsi
que les textes effectivement visibles des sept rubriques. Une empreinte de
ces données détermine si le résultat peut être réutilisé. Les noms retenus
sont copiés pour ne pas dépendre de la durée de vie du pool d'objets.

Une consultation inchangée évite la recherche de chaque nom dans chaque texte,
le tri et les copies. La première ouverture construit l'index et son empreinte
en un seul parcours. Si un cache existant est périmé, deux parcours sont faits :
le premier valide le cache, le second recherche les mentions. Ce compromis évite
de stocker tous les textes temporairement. La collecte des candidats reste
limitée à 8 192 et l'index à 3 000 entrées. Les copies sont libérées avant une
reconstruction, à la réinitialisation et à la sortie.

Les empreintes FNV-1a sur 64 bits servent uniquement en mémoire, sans impact
sur les sauvegardes. Elles ne constituent pas une garantie contre les
collisions. Aucun fil d'exécution supplémentaire n'est créé.

## Validation et mesures

Les sept suites CTest passent en Release x86. `ui_cache` vérifie notamment la
réutilisation sans rappel du prédicat, les changements de requête/révision/
taille, les résultats vides et la navigation circulaire. `community_features`
vérifie les révisions de l'historique, y compris le remplacement à capacité
pleine, et les scénarios de fabrication avec revalidation des inventaires.

Ces tests ne valident pas visuellement toutes les transitions du journal.
La réouverture, le changement de police et le rechargement d'une partie restent
à vérifier en jeu. Aucun gain chiffré n'est déduit des tests unitaires.

Le scénario automatique version 1 et ses actions restent identiques à la
référence archivée. Il ouvre chaque vue une fois ; les gains des caches chauds
nécessitent des ouvertures répétées, mesurables avec les nouveaux compteurs
documentés dans `performance_baseline.md`.

## Relevé du 16 octobre 2026

Le run `72531406147`, archivé dans
`out/performance-baselines/block-3-20261016-080612`, termine les 13 étapes et
revient à la position initiale. La sauvegarde nommée, la résolution, la police,
la langue et le trajet correspondent à la référence. Les empreintes des
fichiers de sauvegarde sont cependant absentes dans les deux métadonnées :
l'identité exacte des données chargées n'est pas démontrée par ce relevé.

L'historique est vide et aucune recherche n'est saisie. Tous les compteurs de
réutilisation sont donc nuls ; ce scénario ne mesure pas les bénéfices chauds.
La construction des fragments passe de 5,368 à 3,209 ms ; la première ouverture
de l'index passe de 70,768 à 115,728 ms. Un relevé par version ne suffit pas à
attribuer toute la variation au code.

Le double parcours à froid a été supprimé après ce relevé : l'empreinte est
maintenant calculée pendant la construction lorsqu'aucun cache n'existe.
Les sept suites CTest passent après ce correctif.

Le run suivant `78410356424`, archivé dans
`out/performance-baselines/block-3-cold-fix-20261016-081559`, termine également
les 13 étapes avec les mêmes étapes de marche et la position finale attendue.
L'index est construit en 58,070 ms, contre 115,728 ms avant le correctif et
70,768 ms dans la référence. Les fragments de l'historique vide prennent
0,705 ms, et leur pagination 0,033 ms. Ces observations ne constituent pas
un pourcentage de gain généralisable : une seule capture est disponible par
version et les caches chauds restent hors de ce scénario. L'autorun est
désactivé dans l'INI installé après archivage.
