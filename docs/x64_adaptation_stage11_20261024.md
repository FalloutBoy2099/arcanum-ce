# Adaptation x64, onzième bloc — 24 octobre 2026

Le découpage général de `tig_video_buffer_blit` utilise désormais des calculs entiers 64 bits : extrémités des rectangles, décalages et produits de mise à l’échelle. Les conversions flottant-vers-entier non bornées de ce chemin sont supprimées. Les rectangles fournis par l’appelant restent intacts.

Le chemin précédent découpait la destination puis ajustait la source en divisant par le rapport source/destination, alors que cette transformation demande le rapport inverse. Au-delà de ce défaut, redimensionner une source redécoupée peut modifier la phase du filtrage. Le nouveau chemin découpe d’abord la source avec des rapports entiers, puis confie à SDL la destination redimensionnée complète ; SDL conserve ainsi le placement des échantillons lors du découpage à l’écran. La copie sans redimensionnement découpe les deux rectangles simultanément sans flottants.

Les bornes physiques des surfaces et la zone de clipping de destination sont prises en compte. Les images entièrement hors de la zone utile ne modifient pas la destination. Les tailles de redimensionnement qui dépassent la limite SDL de 65 535 pixels par axe sont refusées avant l’appel, et le stockage potentiel de l’image intermédiaire est contrôlé avec les mêmes limites de surface que les blocs précédents (INT_MAX octets maximum). Aucune allocation proportionnelle à une dimension INT_MAX n’est tentée.

Le chemin SDL d’un redimensionnement partiellement hors écran peut utiliser une surface intermédiaire afin de préserver le filtrage. Ce bloc corrige la sécurité et le résultat graphique ; il ne démontre pas de gain de performances.

## Vérification

- Les 54 suites passent en Release x64 et x86.
- 32 cas comparent tous les pixels à un appel SDL de référence, sur de petites dimensions sûres : agrandissements, réductions, source et destination découpées, filtrages nearest/linéaire, avec/sans couleur transparente.
- Dix cas de copie à coordonnées extrêmes contrôlent les pixels avec un calcul indépendant en int64_t et vérifient les rectangles d’entrée.
- Deux redimensionnements utilisent un rectangle source initial immense et un décalage négatif proche de INT_MIN ; le résultat est comparé à une référence SDL explicitement normalisée sur un rectangle de petite taille.
- Les dimensions de redimensionnement non prises en charge et une surface intermédiaire qui dépasserait INT_MAX octets sont refusées.
- Les tests de dégradé des deux blocs précédents passent toujours.
- Une copie isolée d’une sauvegarde de Tarant est chargée par les deux moteurs : 48 rendus, cibles de zoom 1, 0,75, 1,25, 1, capture réussie et sortie normale. Chaque capture contient 1 811 couleurs dans la zone centrale contrôlée ; la capture x64 a été inspectée visuellement.

L’instrumentation temporaire de main.c a été retirée à l’identique et les deux exécutables normaux reconstruits. Les binaires ont été contrôlés pour leur architecture PE et l’absence du marqueur de diagnostic. Aucun commentaire de code n’est ajouté ; l’ancienne note sur l’initialisation des rapports supprimés disparaît avec ce code.

## Installation et suite

L’exécutable séparé C:\Games\Arcanum\arcanum-ce-x64.exe est installé avec sauvegarde du précédent, SHA256 : 42DA28B8D1E466C6F6868E0BF560714B901E4C68FA1E50E38E4EA64BCE1764DB. Les empreintes du principal x86 et du fichier INI sont inchangées. Aucun format de données n’est modifié.

Les sources avant modification, journaux, captures et manifestes sont conservés dans out/x64-adaptation-stage11-20261024/. Le chemin logiciel de dégradé ART reste à auditer, ainsi que les autres anciens chemins graphiques hors de ce blitter. La validation prolongée en jeu reste nécessaire ; le portage n’est pas déclaré terminé.
