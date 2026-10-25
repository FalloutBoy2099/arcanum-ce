# Adaptation x64, neuvième bloc — 23 octobre 2026

Petit bloc consacré au chemin logiciel `TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP` de `tig_video_buffer_blit`.

La boucle du dégradé avançait la source d’un pixel par pixel de destination : elle ne réalisait pas de redimensionnement. Avec des tailles différentes, elle pouvait lire hors de la source et décaler les lignes suivantes. Cette combinaison est désormais refusée avant les calculs de découpage, avec `TIG_ERR_INVALID_PARAM`, sans modifier la destination. Le rendu de la carte de ville fournit des rectangles source et destination de même taille.

Avant la boucle, les deux surfaces sont vérifiées : stockage 32 bits valide, pixels présents, dimensions des rectangles identiques, coordonnées et étendues comprises dans les surfaces. Les adresses des lignes sont recalculées à chaque itération avec des offsets en `size_t`, plutôt que de faire avancer un pointeur après la dernière ligne.

Le nouveau test refuse une différence de largeur puis de hauteur et vérifie que les seize pixels de destination sont intacts. Il exécute ensuite un dégradé horizontal avec destination partiellement hors écran et contrôle les pixels copiés ainsi que les zones restées intactes.

Les 54 suites CTest passent en Release x64 et x86. Aucun commentaire n’est ajouté au code. Les sauvegardes des sources, journaux, contrôle des modifications et manifeste sont dans `out/x64-adaptation-stage9-20261023/`.

L’exécutable séparé `C:\Games\Arcanum\arcanum-ce-x64.exe` est actualisé avec sauvegarde du précédent. Les empreintes du principal x86 et du fichier INI sont conservées. Aucun format de données n’est modifié.

Ce bloc sécurise le parcours des pixels de ce mode. Il n’implémente pas son redimensionnement et ne clôt pas l’audit des calculs historiques de clipping ni des interpolations avec coordonnées de dégradé extrêmes. Aucun nouvel essai prolongé en jeu ni gain de performances n’est revendiqué.
