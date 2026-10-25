# Adaptation x64, treizième bloc — 24 octobre 2026

Ce bloc sécurise les copies ART logicielles sans redimensionnement, hors dégradé de couleur déjà traité dans le bloc 12.

Le nouveau découpage `tig_art_clip_copy` calcule les offsets de source et de destination simultanément en int64_t. Les conversions vers int interviennent après intersection avec les dimensions utiles. Il remplace, pour ces copies, les calculs d’intersection et les soustractions 32 bits de l’ancien chemin.

Les 30 boucles de copie recalculent leurs adresses source et destination au début de chaque ligne via `tig_art_copy_row`. Les offsets utilisent size_t. Le retournement vertical est calculé depuis la hauteur complète de l’image, comme pour le dégradé du bloc précédent, et non depuis la hauteur du rectangle découpé. L’avance source est omise après le dernier pixel ; les avances de ligne ne créent plus de pointeur hors allocation après la dernière ligne. Les formules de couleur et les progressions d’alpha/tramé restent celles des branches existantes.

Des contrôles vérifient également les tables de frame, la taille du décodage, le stockage de destination, la palette sélectionnée et la présence du tableau de couleurs lorsqu’il est requis. Les images vides restent sans effet.

## Validation

Les 54 suites CTest passent en Release x64 et x86.

L’oracle de pixels ajoute 1 008 configurations de copie : trois modes de couleur (palette, couleur constante, couleur par colonne), douze modes de mélange, quatre orientations et sept paires de rectangles. Il contrôle la géométrie avec des calculs indépendants int64_t et les couleurs avec les opérations de mélange publiques. Les cas incluent les rectangles partiellement hors écran, INT_MIN/INT_MAX, de grands décalages négatifs qui s’annulent, ainsi que la conservation des pixels transparents et des rectangles d’entrée.

Les mélanges contrôlés comprennent addition, soustraction, multiplication, alpha depuis la luminosité, alpha constant, alpha de la source, alpha interpolé X/Y/XY et tramé source/destination. Les quatre valeurs d’alpha du test sont identiques à 96 : ce test vérifie les accès et la copie des trois branches interpolées, sans prétendre réévaluer leur formule lorsque les coins diffèrent. Les 576 cas de dégradé du bloc précédent passent toujours.

Le test moteur isolé charge la sauvegarde de Tarant dans les deux architectures : 48 rendus et quatre cibles de zoom, capture réussie et sortie normale. Les captures ont chacune 1 811 couleurs dans la zone centrale contrôlée ; la capture x64 a été inspectée visuellement.

Les boucles de pixels redimensionnées sont vérifiées identiques à celles du début de ce bloc. Aucun commentaire de code n’est ajouté. Les fins de ligne sont conservées. main.c est restauré à l’identique après instrumentation, les exécutables normaux sont reconstruits et leurs architectures PE vérifiées.

## Installation et suite

L’exécutable séparé C:\Games\Arcanum\arcanum-ce-x64.exe est installé, avec sauvegarde de son prédécesseur. SHA256 : 5B69900ACB6D1D514622C7C4425E619D28AC952129E19AC11F655D73B3B37641. Les empreintes du principal x86 et du fichier INI sont inchangées. Aucun format de données n’est modifié.

Sources avant modification, différences, journaux, captures et manifestes : out/x64-adaptation-stage13-20261024/.

Les modes ART logiciels avec redimensionnement conservent encore leurs anciens calculs de clipping, d’échantillonnage et de progression des pointeurs. Ils constituent le prochain bloc. Le tableau de couleurs reste un pointeur fourni par l’appelant sans longueur explicite ; le contrôle de présence ne peut pas vérifier sa longueur réelle. Ce travail ne clôt pas le portage et ne démontre pas de gain de performances ni de stabilité sur une session prolongée.
