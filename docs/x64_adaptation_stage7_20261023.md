# Adaptation x64, septième bloc — 23 octobre 2026

Petit bloc limité à la validation des paramètres de `tig_video_buffer_blit`, avant les divisions, intersections et accès aux buffers.

- Refus des pointeurs absents : requête, rectangles, buffers ou surfaces.
- Refus des dimensions négatives.
- Conservation des rectangles vides comme opérations sans effet, avec retour `TIG_OK`. Le cas d’une destination vide ne peut plus atteindre le calcul du rapport de redimensionnement.
- Vérification du rectangle du dégradé lorsque ce mode est demandé : pointeur présent, largeur et hauteur strictement positives avant division.

Les nouveaux tests exercent les quatre paramètres manquants, chacune des quatre dimensions négative puis nulle, ainsi que le rectangle de dégradé absent ou invalide. Ils vérifient tous les pixels de la destination après ces appels : aucune modification. Une copie normale est ensuite exécutée et chaque pixel est contrôlé.

Les 54 suites CTest passent en Release x64 et x86. Les sources précédentes, les journaux et le manifeste d’installation sont dans `out/x64-adaptation-stage7-20261023/`. Aucun commentaire n’est ajouté au code.

L’exécutable séparé `C:\Games\Arcanum\arcanum-ce-x64.exe` est actualisé avec sauvegarde du précédent. Le principal x86 et le fichier INI conservent leurs empreintes. Aucun format de données n’est changé.

Ce petit bloc ne réécrit pas l’algorithme historique de clipping/redimensionnement et ne clôt pas son audit, notamment les conversions flottantes de coordonnées extrêmes et le chemin de dégradé avec redimensionnement. Aucun nouvel essai prolongé en jeu ni gain de performances n’est revendiqué.
