# Version dans les propriétés Windows

La cible Windows `arcanum-ce` contient une ressource `VERSIONINFO`. Les versions du fichier et du produit sont identiques et suivent le format `1.0.P.R`. Le compteur commence à `1.0.0.1` ; la version x64 installée est `1.0.0.3` après les vérifications d'incrémentation.

Le nom du produit, la description, l'organisation, le copyright, le nom interne et le nom du fichier d'origine sont des chaînes fixes définies dans `windows/arcanum_version.rc.in`. Ils identifient Arcanum Community Edition. Les versions numériques et textuelles sont générées ensemble.

## Compteur

Chaque build de la cible Windows attribue un nouveau numéro, que la commande provienne de `tools/build_windows.ps1`, de CMake ou de la solution Visual Studio. Cela entraîne une compilation de la ressource et une nouvelle édition de liens, même lorsque le code C est inchangé. Une simple configuration CMake ou le lancement du jeu ne consomme aucun numéro.

Le compteur persiste dans `.arcanum-build-number`, à la racine du dépôt. Ce fichier et ses fichiers de verrouillage sont ignorés par Git ; il est indépendant des dossiers de compilation x86 et x64. Il faut le conserver pour maintenir la suite des numéros dans cette copie de travail. Une autre copie du dépôt possède son propre compteur, sauf configuration explicite d'un fichier partagé via `ARCANUM_WINDOWS_VERSION_COUNTER_FILE`.

`tools/stamp_windows_version.cmake` verrouille le compteur pendant l'attribution du numéro. Les builds x86 et x64 en parallèle reçoivent ainsi des numéros distincts. À partir de 65536 builds, le compteur se répartit sur les deux derniers composants (`1.0.1.0`, etc.) pour respecter la limite Windows de 65535 par composant. Un compteur invalide ou épuisé produit une erreur explicite, sans remise à zéro silencieuse. Un build qui échoue après l'attribution peut laisser un trou dans la suite.

## Vérification et installation

- Compilation de la ressource réussie sur x64 et x86.
- Champs relus avec `FileVersionInfo`, l'API Windows utilisée pour ces propriétés.
- Deux builds x64 successifs ont produit `1.0.0.1` puis `1.0.0.3`, le build x86 intermédiaire ayant reçu `1.0.0.2`.
- Six attributions en parallèle sur un compteur isolé ont produit six numéros distincts ; le passage de 65535 à 65536 et le refus des compteurs invalides/épuisés ont également été vérifiés.
- La section de code `.text` du nouvel exécutable x64 est identique à celle de la version précédemment installée. Cette modification ajoute uniquement les métadonnées et leur génération ; les suites du moteur n'ont pas été relancées pour ce changement.

La version x64 est installée dans `C:\Games\Arcanum\arcanum-ce-x64.exe`. L'exécutable principal x86 et l'INI restent inchangés. Les journaux, vérifications et sauvegardes d'installation sont conservés dans `out/windows-version-20261024/`.
