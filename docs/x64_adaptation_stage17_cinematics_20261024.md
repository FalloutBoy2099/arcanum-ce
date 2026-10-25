# Adaptation x64, dix-septième bloc : cinématiques — 24 octobre 2026

## Remplacement de la passerelle Bink

Le moteur 64 bits tentait encore de charger binkw32.dll et ses exports x86. Une DLL 32 bits ne peut pas fournir le décodage à ce processus. Le nouveau backend x64 décode directement les fichiers Bink 1 avec les bibliothèques FFmpeg, puis utilise SDL pour la vidéo et le son. Les fichiers du jeu ne sont pas convertis ou modifiés.

Le décodeur est isolé dans first_party/movie_decoder. Il conserve les contextes de décodage, réutilise les buffers et traite progressivement les paquets vidéo et audio. Les pixels sont convertis en BGRA, puis copiés au format natif XRGB du moteur par SDL_ConvertPixels. Le son est converti en PCM flottant entrelacé et envoyé à un SDL_AudioStream.

La lecture suit les horodatages du film avec une horloge monotone. La boucle continue de traiter les messages pendant l’attente. Une touche interrompt la lecture, sauf avec TIG_MOVIE_IGNORE_KEYBOARD. Une demande de fermeture reste dans la file pour être traitée par le jeu après la libération des ressources. Le mode sans son est respecté ; un périphérique audio indisponible laisse la vidéo se poursuivre avec un diagnostic.

La piste sonore est sélectionnée par son identifiant Bink, pas par le numéro du flux FFmpeg. Les films plus grands que l’écran sont réduits en conservant les proportions ; les films plus petits sont centrés à leur taille native. Les drapeaux de fondu et de conservation de la dernière image sont conservés dans l’interface TIG.

Les arguments invalides, les tailles non valides, les échecs de lecture et les fins de flux vidéo prématurées sont contrôlés. Le backend FFmpeg s’applique par défaut aux configurations Windows 64 bits de ce projet. Le backend historique reste utilisé pour la compilation x86.

## Dépendance et déploiement

Le SDK retenu est FFmpeg n8.1.3-9-g29e619e767, compilation BtbN LGPL partagée du 30 septembre 2026. L’URL exacte et son SHA256 sont épinglés dans third_party/ffmpeg/CMakeLists.txt. ARCANUM_FFMPEG_ROOT permet de fournir un SDK compatible. Le SDK par défaut est x86-64 ; il ne convient pas à ARM64.

Les cinq bibliothèques partagées avformat-62.dll, avcodec-62.dll, avutil-60.dll, swscale-9.dll et swresample-6.dll accompagnent l’exécutable. Elles occupent environ 124 Mio sur disque ; cette taille ne mesure pas la mémoire résidente du jeu. Le programme ffmpeg.exe n’est pas requis à l’exécution. La licence LGPL et les liens vers les sources et scripts de compilation sont fournis dans licenses/ffmpeg. Une compilation FFmpeg limitée aux codecs utiles pourrait réduire cette distribution dans un prochain bloc.

## Validation

- 58/58 suites passent en Release x64 ; 57/57 en x86. La suite supplémentaire concerne le nouveau décodeur.
- Le logo Sierra original est décodé intégralement : 201 images, 320 640 frames audio stéréo à 44 100 Hz. La première image BGRA et tous les échantillons PCM sont identiques aux références produites séparément par le programme FFmpeg du même SDK.
- Une copie temporaire dont l’identifiant de piste audio est changé en 7 permet de vérifier la sélection par identifiant. Une piste absente produit une vidéo silencieuse. Le refus d’une sortie audio par le callback, les paramètres invalides et une copie tronquée sont également contrôlés.
- Les douze films présents dans les répertoires du module et de Arcanum3.dat sont décodés jusqu’à leur fin, soit 18 291 images. Le test vérifie les comptes annoncés, les horodatages croissants, les dimensions réduites dans 640x480, les échantillons finis et un écart audio/vidéo inférieur à une seconde.
- Dans le jeu isolé, le logo Sierra se lit en environ 7,237 secondes pour 7,230 secondes attendues, avec sortie audio ouverte. Une touche interrompt la lecture à environ 603 ms ; le mode non interrompable conserve la durée complète. La fermeture de fenêtre interrompt la lecture et préserve le message QUIT. Le mode sans son dure aussi environ 7,237 secondes, sans sortie audio.
- Un extrait HD de 00069.bik est affiché avec le son pendant environ 3,504 secondes puis interrompu par la touche injectée. Les captures du logo et du film HD sont inspectées visuellement.

Ces contrôles ne constituent pas une écoute subjective de chaque film ni une mesure de dérive audio sur toutes les longues cinématiques en temps réel. Le décodage complet des douze films est effectué sans attendre leur durée réelle. Bink 2 ne fait pas partie de cette implémentation.

Les sources temporaires de diagnostic sont retirées, main.c est restauré à l’identique et l’exécutable normal est reconstruit. Les artefacts sont conservés dans out/x64-cinematics-20261024.

Sources de référence : [formats et codecs FFmpeg](https://ffmpeg.org/general.html), [SDK et scripts BtbN](https://github.com/BtbN/FFmpeg-Builds), [démultiplexeur Bink](https://github.com/FFmpeg/FFmpeg/blob/n8.0/libavformat/bink.c).

## Installation

Version x64 1.0.0.29 installée dans C:/Games/Arcanum avec les cinq DLL et les fichiers de licence. Chaque empreinte installée correspond au fichier validé. Le chargeur Windows vérifie avec succès les cinq DLL depuis leur dossier installé, avec leurs dépendances. Les empreintes du principal x86 et de arcanum.ini restent identiques. Le précédent exécutable et le manifeste sont conservés dans out/x64-cinematics-20261024/install-20261024-165827.
