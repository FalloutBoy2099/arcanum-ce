# Adaptation x64 — premier bloc, 23 octobre 2026

## Changements

### Lecture des images ART

L’en-tête disque est maintenant représenté par `TigArtFileHeader`, qui contient uniquement des champs de taille fixe de 32 bits. Une assertion vérifie sa taille de 132 octets, identique au format ART existant. Le lecteur consomme cet en-tête, puis remplit séparément les données d’exécution.

Les marqueurs de présence de palettes sont des booléens indépendants des véritables pointeurs de palettes. Le cast entier-vers-pointeur servant de marqueur a disparu. Les anciennes valeurs d’offsets de frames et de pixels restent ignorées comme auparavant. Aucun fichier ART n’est converti ou réécrit.

### Mémoire et caches ART

Les compteurs signés du cache ART passent de `int` à `int64_t`. Leurs valeurs négatives continuent à déclencher l’éviction existante, et les diagnostics utilisent un format d’affichage 64 bits.

Sous Windows, la quantité de mémoire physique installée et disponible provient désormais de `GlobalMemoryStatusEx`. Les valeurs sont bornées avant conversion vers `size_t` en x86. Le repli SDL effectue la multiplication en 64 bits avant cette conversion. Cela évite le débordement modulo qui pouvait affecter le calcul 32 bits et remplace l’ancienne approximation « disponible = installée » lorsque Windows fournit la mesure réelle.

Les budgets sont déterminés au démarrage, sans réservation immédiate de mémoire :

- x64 : chaque budget est limité au minimum de RAM totale / 8, RAM disponible / 4 et 2 Gio ;
- x86 : chaque budget est limité au minimum de RAM totale / 4, RAM disponible / 4 et UINT32_MAX / 4, soit environ 1 Gio.

Il s’agit des deux compteurs existants pour les données système et les surfaces de rendu, pas d’une mesure physique de la VRAM. Le moteur conserve son mécanisme d’éviction. Les caches peuvent donc dépasser l’ancienne limite sur une machine suffisamment disponible, sans augmenter systématiquement en situation de pression mémoire. Le budget n’est pas réévalué en continu.

### Build Windows

`tools/build_windows.ps1` choisit x64 et Release par défaut, détecte Visual Studio 2022 ou 2026 ainsi que son CMake, configure le projet, compile et lance tous les tests. L’option `-Architecture x86` conserve la voie de comparaison. Les presets et les matrices CI existants restent disponibles.

Exemples depuis la racine :

```powershell
./tools/build_windows.ps1
./tools/build_windows.ps1 -Architecture x86
```

Le paramètre `-BuildDirectory` permet de réutiliser un dossier déjà configuré pour la même architecture. Le script n’installe pas l’exécutable et n’accède pas aux sauvegardes.

## Vérification

- 49 suites CTest réussies en Release x64 et 49 en Release x86, via le nouveau script de build.
- Nouveau test du budget : limite supérieure au maximum signé 32 bits, très grandes valeurs d’entrée, faible RAM disponible, disponibilité nulle et borne x86.
- Tests ART enrichis : huit combinaisons de palettes, marqueurs non nuls proches de UINT32_MAX, anciens offsets de pointeurs arbitraires, et troncatures à chacun des 132 octets de l’en-tête.
- Pixels des images réelles d’interface et de deux sprites de personnages à plusieurs rotations identiques à l’oracle indépendant, dans les deux architectures.
- Initialisation réelle du jeu et de son interface réussie en x64 et x86 dans le dossier de test isolé ; 62 images ART communes préparées, sans échec de chargement ART.

Pendant ces essais, la mémoire disponible était faible. Le budget choisi est d’environ 527 Mio par compteur en x64 et 556 Mio en x86, mesurés à des instants différents. Ces chiffres ne sont pas une comparaison de performance entre architectures. Le processus de chaque sonde de démarrage est arrêté une fois l’initialisation confirmée ; il ne s’agit pas d’une session complète de jeu.

Aucun nouveau commentaire dans le code. Les catalogues, prototypes, scripts et autres fichiers de données ne sont pas modifiés.

## Déploiement et suite

La version de test x64 est mise à jour séparément sous `C:\Games\Arcanum\arcanum-ce-x64.exe`, utilisable par `Jouer en 64 bits.cmd`. L’exécutable principal 32 bits reste le retour possible. Aucun réglage de l’INI ne change.

Ce bloc ne termine pas tous les travaux de portabilité. Les contrôles des dimensions et des tailles des terrains, les limites historiques des gros fichiers DAT et la validation d’une session complète de jeu restent les prochaines étapes. La compatibilité des sauvegardes ne fait pas partie de la garantie demandée.

Les journaux de compilation, tests, décodage de sprites et démarrage sont dans `out/x64-adaptation-stage1-20261023/`.
