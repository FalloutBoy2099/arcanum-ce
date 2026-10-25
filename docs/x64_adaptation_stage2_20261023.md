# Adaptation x64, deuxième bloc — 23 octobre 2026

## Changements

Le lecteur DAT conserve ses positions absolues en `int64_t`, indépendamment de la taille des pointeurs et du type Windows `long`. Les accès synchrones et le préchargement asynchrone utilisent les fonctions de positionnement fichier 64 bits. Les déplacements relatifs dans une entrée sont calculés en 64 bits avant vérification et conversion.

Le format DAT sur disque reste inchangé : les positions stockées dans les entrées sont lues comme des entiers non signés de 32 bits, puis combinées en mémoire avec la position de base de l’archive. La lecture n’utilise plus la taille de la structure en mémoire pour déterminer la taille d’un champ sérialisé. Le mécanisme de compatibilité des archives PATCH communautaires est conservé.

Les index sont vérifiés avant les allocations et la lecture des noms : nombre d’entrées, espace disponible, longueur positive des noms, terminaison nulle, capacité de la table des noms et limites des données référencées. Une ouverture interrompue libère aussi la copie du chemin. Un échec de positionnement à l’ouverture d’une entrée ferme son fichier.

Les dimensions des terrains sont contrôlées avant multiplication et allocation, à la création comme à l’ouverture. Un terrain brut tronqué est refusé. Les longueurs des lignes compressées sont contrôlées avant utilisation ; leur lecture ne déréférence plus un pointeur `uint32_t` potentiellement mal aligné. Les échecs d’ouverture nettoient l’état et les caches de lignes sont remis à zéro après libération. Les éventuels octets de remplissage en fin de terrain restent acceptés.

## Vérification

- 51 suites CTest réussies en Release x86 et 51 en Release x64.
- Nouvelles épreuves DAT : données après la limite de 2 Go, positions absolues et relatives, données compressées et brutes, lecture synchrone et asynchrone, déplacements et index invalides. Les fichiers de test sont creux pour éviter de consommer plusieurs Go de disque.
- Nouvelles épreuves terrain : dimensions nulles, négatives et débordantes, limite représentable, lignes tronquées, tailles de lignes invalides et préfixes non alignés.
- Lecture complète, dans chacune des architectures, des trois archives `tig.dat`, `Arcanum.dat` et `Arcanum.PATCH9` : 136 882 entrées de fichiers lues intégralement sans erreur ; contrôle des dimensions et blocs de lignes des 81 terrains de l’archive principale.
- Le test de préchargement existant est adapté : une entrée dont la taille compressée dépasse la fin de l’archive est désormais refusée au montage. Le cas dépassant le budget de préchargement tout en restant structurellement valide continue de vérifier le refus du préchargement.
- Démarrage du moteur, chargement de la copie de `Slot0001` et arrêt normal réussis dans les deux architectures, dans le dossier isolé `out/x64-data-audit/game`. Les sondes temporaires sont conservées dans `out/x64-adaptation-stage2-20261023`, puis retirées du code livré. Ces passages de compatibilité, réalisés pendant les compilations, ne constituent pas un benchmark de vitesse.

## Limites conservées

Une entrée DAT individuelle reste limitée à `INT_MAX` octets : les API publiques de longueur et de position relative du moteur utilisent encore des `int`. Cela n’empêche plus la lecture d’une petite entrée placée au-delà de 2 Go dans une archive. La table DAT et les champs sérialisés gardent les limites de leur format historique.

Le stockage d’un terrain décompressé reste limité à `INT_MAX` octets, avec refus explicite des dimensions incompatibles. Cette étape sécurise les calculs ; elle n’introduit pas de stockage de terrains arbitrairement grands. Le contrôle des blocs compressés porte sur leurs limites ; le décodeur zlib historique garde son comportement en cas de contenu compressé invalide.

Aucune conversion des fichiers de données n’est nécessaire. Ce bloc ne revendique pas de gain de FPS. Les essais ne remplacent pas une session de jeu avec déplacements, combat et changement de carte.

## Livraison

L’exécutable de test `C:\Games\Arcanum\arcanum-ce-x64.exe` est actualisé après validation. L’exécutable principal 32 bits et la configuration installée sont conservés. Une sauvegarde du précédent exécutable x64 et les empreintes de contrôle accompagnent la livraison dans le dossier de résultats.

Prochaine étape proposée : poursuivre l’examen des allocations et du stockage générique d’objets, des conversions de handles et des autres structures susceptibles de mélanger données sérialisées et pointeurs natifs.
