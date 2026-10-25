# Adaptation x64, dixième bloc — 24 octobre 2026

Le chemin logiciel de dégradé de `tig_video_buffer_blit`, utilisé par les tuiles de la carte de ville, calcule désormais les différences de coordonnées en `int64_t`. Les positions extrêmes du rectangle de dégradé ne peuvent plus faire déborder ces soustractions avant leur conversion en flottant.

Les composantes interpolées sont bornées à 0–255 avant leur conversion en `uint8_t`. Hors de cette plage, la conversion flottant-vers-entier pouvait avoir un comportement indéfini. Les valeurs situées dans la plage gardent leur conversion précédente, ainsi que l’arithmétique et la progression du dégradé existantes.

Le test `gradient_extreme_coordinates` contrôle les seize pixels de six cas : dégradés horizontaux et verticaux placés à `INT_MIN` ou `INT_MAX`, puis deux dégradés de largeur un qui dépassent leur zone. Il vérifie aussi que le rectangle fourni reste intact. Le test de dégradé normal partiellement découpé du bloc précédent passe toujours.

Les 54 suites passent en Release x64 et x86. Aucun commentaire de code n’a été ajouté ou modifié ; les fins de ligne sont conservées. Les sauvegardes des deux sources avant modification, la vérification et les journaux se trouvent dans `out/x64-adaptation-stage10-20261024/`.

L’exécutable séparé `C:\Games\Arcanum\arcanum-ce-x64.exe` est installé, avec sauvegarde du précédent et contrôle des empreintes. Son SHA256 est `67C6A8D0F7085FF19F76E86C1BDD37B9A37783D32659E221E4F4CC129ACB6900`. Le principal x86 et le fichier INI sont préservés. Aucun format de données ne change.

Ce bloc ne clôt pas le portage : les calculs généraux de clipping/redimensionnement de ce blitter et le chemin de dégradé logiciel ART restent à examiner. Aucune session prolongée en jeu ni amélioration de performances n’est revendiquée pour ce correctif.
