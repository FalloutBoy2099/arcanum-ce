# Adaptation x64, huitième bloc — 23 octobre 2026

Petit bloc consacré à `tig_video_buffer_blit_sdl_surface`, utilisé par le rendu des polices modernes, pour le texte et son ombre.

Le découpage source/destination utilise des intervalles en 64 bits. Les soustractions et la négation de coordonnées sont effectuées après élargissement : une coordonnée `INT_MIN` ne provoque plus de débordement pendant le découpage. Les coordonnées sont réduites vers `int` seulement une fois comprises dans les surfaces. Les décalages des lignes de destination utilisent `size_t`.

La fonction ne modifie plus les rectangles de l’appelant. Le rendu de police partageait son rectangle source entre ombre et texte : auparavant, une ombre partiellement coupée sur un bord pouvait modifier le rectangle utilisé ensuite pour le texte. Les deux copies calculent maintenant leur découpage indépendamment.

Les dimensions négatives de la source sont refusées ; une source vide ou totalement hors surface reste une opération sans effet. Le stockage de la source et la disposition 32 bits du buffer de destination sont contrôlés avant copie. Le calcul des couleurs et le mélange alpha ne sont pas modifiés. La taille demandée reste celle du rectangle source, sans redimensionnement, comme auparavant.

Les tests couvrent sept couples de rectangles : débordements aux différents bords, coordonnées extrêmes, dimensions très grandes, surface vide. Chacun des 56 pixels de destination est comparé à une référence calculée indépendamment, et les rectangles fournis doivent rester identiques. Les paramètres absents et une largeur négative sont également vérifiés.

Les 54 suites CTest passent en Release x64 et x86. Aucun commentaire n’est ajouté au code. Sources précédentes, journaux et manifeste dans `out/x64-adaptation-stage8-20261023/`.

L’exécutable séparé `C:\Games\Arcanum\arcanum-ce-x64.exe` est actualisé avec sauvegarde du précédent. L’exécutable principal x86 et le fichier INI conservent leurs empreintes. Aucun nouveau test prolongé en jeu ni gain de performances n’est revendiqué. Le blit générique avec redimensionnement et dégradé reste hors de ce petit bloc.
