# Adaptation x64, sixième bloc — 23 octobre 2026

## Rectangles de rendu et captures

Les conversions des zones modifiées du monde vers l’écran utilisent maintenant des entiers de 64 bits dès les additions et soustractions de coordonnées. Auparavant, certaines expressions débordaient dans un `int` avant leur conversion en 64 bits. L’extension nécessaire au filtrage linéaire et les arrondis vers l’extérieur sont conservés. Les résultats sont limités au viewport avant réduction vers les coordonnées SDL.

Le chemin des zones écran effectue également ce découpage avant de transmettre les rectangles au regroupement SDL. Un nombre de rectangles hors de la capacité du tableau déclenche un rafraîchissement complet. La finalisation du zoom vérifie les rectangles avant leur lecture et compare les surfaces cumulées à un seuil sans multiplier une somme potentiellement trop grande.

La composition du monde refuse les listes dont le nombre ou les rectangles sont invalides avant de commencer les copies. Le défilement vérifie que les coordonnées et dimensions de sa fenêtre peuvent être additionnées sans débordement.

## Export BMP

L’export de surface est regroupé dans `tig_bmp_save_surface`, dans le module BMP. Le code vidéo l’utilise pour les captures et l’export de buffers.

La découpe précédente utilisait le même rectangle, avec son décalage x/y, pour lire la source et écrire dans une image de la seule taille de la découpe. Une découpe commençant à (3, 2) était donc écrite à (3, 2) dans sa nouvelle image, provoquant une image déplacée et tronquée. Les lignes sont désormais copiées à partir du début de la destination, avec des offsets en `size_t`.

Le calcul utilise les octets physiques par pixel : le format SDL XRGB8888 contient 24 bits de couleur mais occupe quatre octets. Les copies conservent les pixels et la palette sans appliquer les paramètres de modulation ou de mélange du buffer source, et sans modifier ces paramètres.

Les rectangles invalides sont refusés avant l’ouverture du fichier de sortie. Les surfaces indexées de 1 et 4 bits, que cette version de SDL ne sait pas exporter en BMP, sont également refusées avant ouverture ; leur lecture reste inchangée. Les formats d’export testés sont indexé 8 bits, RGB565, RGB24, XRGB8888 et ARGB8888. Le flux de sortie est fermé explicitement après la tentative d’écriture, y compris si elle échoue.

## Vérification

- 54 suites CTest réussies en Release x64 et 54 en Release x86.
- Trois découpes par l’API vidéo : rectangle décalé, dernier pixel et image entière. Chaque pixel exporté est comparé à sa couleur attendue.
- Vérification des cinq formats exportables ci-dessus, des palettes et de la conservation des paramètres de couleur, transparence et mélange de la source.
- Refus des formats non exportables et des coordonnées/dimensions invalides ; un fichier témoin existant reste intact après les tentatives avec rectangles invalides.
- Cas limites du zoom proches de `INT_MIN` et `INT_MAX`, sans allocation de grandes images ; listes de rectangles de taille négative ou excessive ; défilements impossibles.
- Les comparaisons existantes de redimensionnement et de mise à jour partielle des pixels passent également.
- Moteur complet dans une copie isolée du jeu : chargement de Slot0001 à Tarant, 48 images présentées avec changements de zoom, capture BMP et fermeture normale, dans chaque architecture. Les deux captures contiennent 1 811 couleurs dans leur région centrale. La capture x64 est inspectée visuellement et montre le décor et le HUD.

Les sources précédentes et résultats se trouvent dans `out/x64-adaptation-stage6-20261023/`. Les journaux finaux sont `x64-build-tests.log` et `x86-build-tests.log` ; le contrôle du rendu est consigné dans `render-results.json`. La sonde temporaire ne fait pas partie de la livraison : `main.c` est restauré octet par octet, sa date de modification est actualisée pour imposer sa recompilation, puis les deux exécutables normaux sont reconstruits. Le marqueur de diagnostic est absent de ces exécutables.

## Livraison et limites

`C:\Games\Arcanum\arcanum-ce-x64.exe` est actualisé. Le précédent est sauvegardé avec manifeste SHA-256. L’exécutable principal x86 et le fichier INI installé conservent leurs empreintes. Aucun commentaire n’est ajouté au code et aucun format de données du jeu n’est modifié.

Ce bloc ne mesure pas de gain de FPS. La limite individuelle de surface à `INT_MAX` octets, établie dans le bloc précédent, reste en place. Les anciennes routines génériques de blit, les allocations internes des bibliothèques et du pilote, ainsi que les chemins de rendu non exercés ne sont pas déclarés entièrement audités. Une session de jeu prolongée reste utile en complément de ces essais.
