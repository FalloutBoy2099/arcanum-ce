# FFmpeg limité aux cinématiques — 24 octobre 2026

Les cinq bibliothèques utilisées par le jeu passent de 130 364 416 à 3 422 720 octets : environ 124,3 Mio à 3,3 Mio, soit 97,4 % de réduction. Les anciens fichiers sont conservés dans la sauvegarde d’installation du dossier de travail ; ce gain concerne les bibliothèques actives dans C:/Games/Arcanum.

| Bibliothèque | Avant, octets | Après, octets |
| --- | ---: | ---: |
| avformat-62.dll | 22722560 | 386048 |
| avcodec-62.dll | 90992128 | 487424 |
| avutil-60.dll | 3014144 | 1136128 |
| swscale-9.dll | 12900864 | 1177088 |
| swresample-6.dll | 734720 | 236032 |

## Compilation

La révision FFmpeg reste 29e619e767cde9045a75c29bc9a8278ae7b3a98b. Les sources ne sont pas modifiées. La nouvelle compilation Visual C++ garde Bink vidéo, Bink audio RDFT/DCT, l’accès aux fichiers locaux, swscale et swresample. Les encodeurs, autres formats et protocoles réseau, avfilter, avdevice et les décodeurs matériels sont désactivés. Les optimisations SIMD et la détection du CPU à l’exécution restent actives ; l’option d’optimisation pour la taille au détriment de la vitesse n’est pas utilisée.

Le diagnostic des DLL confirme exactement trois codecs enregistrés (binkvideo, binkaudio_dct, binkaudio_rdft), un démultiplexeur (bink) et le protocole d’entrée file. Les cinq DLL sont bien AMD64.

`tools/build_ffmpeg_windows.py` fournit la recette de compilation. Les archives FFmpeg, GNU Make et NASM ont des URL et SHA256 épinglés. Le constructeur utilise les outils Visual C++ et Git for Windows existants, sans installer les outils téléchargés globalement. Il ne conserve pas l’environnement complet de compilation dans ses artefacts.

La configuration CMake génère ce SDK par défaut dans son dossier ffmpeg-minimal. Python 3.8 ou ultérieur est nécessaire pour cette génération Windows x64. Une empreinte de la recette, du compilateur et du constructeur, puis une vérification des fichiers produits, permettent de réutiliser le SDK lors des configurations suivantes. Un verrou protège les configurations concurrentes. Les jobs de compilation sont limités à huit par défaut. Cette recette permet de reconstruire le SDK ; une identité binaire entre machines différentes n’est pas revendiquée.

ARCANUM_FFMPEG_ROOT permet toujours de fournir un autre SDK compatible. Le backend x86 ne change pas. La licence de la compilation minimale est LGPL 2.1 ou ultérieure. LICENSE.txt, NOTICE.txt et BUILD-CONFIG.txt accompagnent les DLL. Les outils, les bibliothèques d’import et les sources restent dans le dossier de travail, pas dans le dossier du jeu.

## Vérification

- 58/58 suites réussies en Release x64 et 57/57 en x86.
- Pour le logo Sierra original, première image BGRA et totalité du PCM flottant identiques octet par octet aux références obtenues avec le précédent SDK : 201 images et 320 640 frames audio stéréo.
- Douze vidéos installées décodées jusqu’à leur fin, soit 18 291 images, avec contrôle du nombre d’images, des horodatages et de la cohérence des durées audio/vidéo.
- Lecture réelle du logo Sierra en 7,237 s pour 7,230 s attendues, avec sortie audio ouverte. Interruption clavier à environ 601 ms, mode non interrompable complet, message de fermeture préservé, mode sans son et extrait HD de 3,518 s : succès.
- Capture HD inspectée visuellement. Les longues vidéos sont décodées intégralement sans attendre leur durée réelle ; une écoute subjective et une mesure de dérive à long terme ne sont pas revendiquées.
- Une seconde invocation du constructeur confirme la réutilisation du SDK sans recompilation.

## Installation

Version x64 1.0.0.44 installée avec les cinq DLL minimales et les fichiers de licence. Les fichiers installés correspondent aux empreintes des fichiers testés. Le chargeur Windows a chargé les cinq DLL depuis leur dossier installé et résolu leurs dépendances.

L’exécutable principal x86 et arcanum.ini ont gardé leurs empreintes. main.c est restauré à l’identique ; l’exécutable normal ne contient aucun marqueur de diagnostic. Aucun commentaire de code n’a été ajouté.

Sauvegarde du précédent exécutable, des DLL et des licences : out/ffmpeg-minimal-20261024/install-20261024-190924. Journaux, SDK d’essai, mesures et captures : out/ffmpeg-minimal-20261024.

Le gain mesuré porte sur la taille des DLL sur disque. Aucun gain de FPS, de mémoire résidente ou de temps de démarrage n’est revendiqué.

Références : [options officielles de compilation FFmpeg](https://github.com/FFmpeg/FFmpeg/blob/n8.1.3/configure), [sources exactes](https://github.com/FFmpeg/FFmpeg/tree/29e619e767cde9045a75c29bc9a8278ae7b3a98b), [GNU Make fourni par MSYS2](https://packages.msys2.org/packages/make), [NASM](https://www.nasm.us/).
