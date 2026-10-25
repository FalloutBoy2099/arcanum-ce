# Adaptation x64, cinquième bloc — 23 octobre 2026

## Calculs des buffers graphiques

Les calculs de pas mémoire et de taille passent par `tig/pixel_buffer.h`. La largeur est multipliée par le nombre de bits dans un entier non signé de 64 bits ; l’arrondi des lignes est vérifié avant réduction vers `int`. Le produit pas × hauteur est contrôlé avant allocation, copie ou utilisation par les chemins traités, puis transmis en `size_t`.

Les formats de disposition pris en charge par ce calcul sont 1, 4, 8, 16, 24 et 32 bits par pixel, avec lignes d’au moins la taille arrondie sur quatre octets. Cela n’ajoute pas de nouveaux formats au chargeur BMP : il conserve les formats qu’il acceptait auparavant.

Une surface individuelle reste limitée à `INT_MAX` octets dans ces chemins. Plusieurs opérations graphiques historiques calculent encore leurs décalages dans des `int` ; accepter une surface plus grande sans les reprendre serait dangereux. La compilation x64 ne suffit pas à élargir ces expressions. Le cache peut toujours cumuler plusieurs surfaces dans ses compteurs de 64 bits.

## Modifications

- **BMP** : suppression de la multiplication signée `pitch * height` avant allocation. La disposition retournée par SDL est contrôlée, puis la copie utilise une taille en `size_t`. La destruction remet le pointeur à zéro.
- **Buffers vidéo** : vérification des dimensions avant allocation, puis du pas effectivement retourné par SDL. Une création échouée libère la structure intermédiaire et laisse le pointeur de sortie nul. Le buffer n’est publié qu’une fois créé et initialisé.
- **Fenêtre et surface principale** : contrôle des dimensions logiques, du produit avec l’échelle DPI avant conversion en entier, puis du pas de la surface créée. Les erreurs ajoutées détruisent les ressources déjà acquises.
- **Cache de buffers ART** : comptabilisation à partir de la taille réelle des surfaces créées. Les expressions précédentes multipliaient des `int` avant addition dans le compteur de 64 bits ; deux chemins de toit utilisaient aussi des bits au lieu d’octets. La requête `tig_video_buffer_memory_size` fournit maintenant le nombre d’octets réel, padding compris.
- **Toits ART** : refus des nombres de frames qui ne peuvent pas tenir dans la table de treize buffers ou qui provoqueraient un indice négatif dans sa partie complémentaire. La plage sûre de ce chemin est de neuf à treize frames.
- **Transferts partiels, redimensionnement logiciel et cache de sprites** : vérification du stockage des surfaces avant accès aux pixels. Les rectangles de transfert sont vérifiés avant toute mise à jour de texture ; une requête invalide conserve les zones en attente.

Le compteur historique nommé « vidéo » des ART décrit ici les buffers de rendu créés par ce chemin. Il ne constitue pas une mesure de la VRAM réellement réservée par le pilote. Le cache de textures SDL continue de gérer son propre budget en octets de texels.

## Vérification

- 54 suites CTest réussies en Release x64 et 54 en Release x86.
- Nouvelle suite `graphics_storage` : dispositions des six profondeurs, largeurs non multiples de l’alignement, padding supplémentaire, dimensions nulles/négatives, pas trop courts, limites proches d’`INT_MAX` et produits trop grands. Les grandes limites sont testées arithmétiquement sans réserver plusieurs Go.
- Création d’un buffer vidéo réel, lecture de sa couleur initiale, contrôle de sa taille avec le pas SDL et vérification d’un pointeur de sortie nul sur paramètres invalides.
- BMP indépendants de 4, 8 et 24 bits avec largeurs imposant du padding : comparaison des lignes copiées avec le décodage SDL et contrôle des palettes.
- Refus de surfaces au pas invalide et de rectangles débordants dans les transferts ; refus du même stockage dans le cache de sprites et le redimensionnement logiciel.
- Les tests existants de redimensionnement, transferts partiels, cache de sprites, composition et shaders passent également. Les comparaisons de pixels contrôlées restent identiques à leur référence.
- Sonde du moteur complet, dans une copie du jeu : chargement de `Slot0001` à Tarant, présentation de 48 images avec cibles de zoom 1, 0,75, 1,25 puis 1, et capture BMP, dans les deux architectures.

La sonde reprend le positionnement du personnage et l’affichage du HUD. Elle masque aussi la fenêtre d’initialisation qui recouvrait encore le monde lors des premiers essais hors menu normal. Les captures finales montrent le décor de Tarant et le HUD ; leur région centrale contient 1 811 couleurs dans chacune des architectures, ce qui évite de compter une vue noire comme une validation visuelle. La capture x64 est inspectée visuellement. Les essais ne comparent pas les captures entre architectures au pixel près.

Les résultats sont dans `out/x64-adaptation-stage5-20261023/`, notamment `render-results.json`, les journaux et `render-x64.bmp` / `render-x86.bmp`. Les sources précédentes sont conservées dans `before/`. La sonde temporaire n’est pas livrée : `main.c` est restauré octet par octet, puis les deux exécutables normaux sont recompilés.

## Livraison et limites

L’exécutable de test `C:\Games\Arcanum\arcanum-ce-x64.exe` est actualisé avec sauvegarde du précédent et manifeste SHA-256. La configuration installée et l’exécutable principal x86 sont conservés.

Aucun format ART, BMP, PRO ou de sauvegarde n’est converti. Ce bloc ne mesure aucun gain de FPS ni réduction de temps de chargement. L’essai automatique est court et ne remplace pas une session de jeu prolongée.

Les allocations internes aux bibliothèques et au pilote GPU ne sont pas réécrites. Les autres buffers auxiliaires, les conversions de rectangles et les chemins de rendu historiques non exercés restent à examiner. La libération de la structure sur échec de création SDL est vérifiée dans le code ; les essais ne provoquent pas volontairement un épuisement réel de la mémoire de la machine.
