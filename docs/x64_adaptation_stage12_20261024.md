# Adaptation x64, douzième bloc — 24 octobre 2026

Le rendu logiciel `TIG_ART_BLT_BLEND_COLOR_LERP` est isolé dans `tig_art_blit_gradient`. Les autres branches de mélange restent dans leur implémentation existante.

Les coordonnées source/destination sont découpées simultanément en entiers 64 bits avant conversion vers les coordonnées finales bornées. Les différences avec le rectangle de dégradé sont également calculées en int64_t. Les composantes interpolées sont saturées à 0–255 avant conversion vers uint8_t. La progression flottante par ligne et par pixel du dégradé reste celle du chemin précédent pour les valeurs dans la plage.

Les accès à l’image utilisent des indices de pixels et des offsets de lignes en size_t. Aucune avance de pointeur après la dernière ligne n’est nécessaire. Le retournement vertical utilise désormais la hauteur complète de l’image : le calcul précédent utilisait la hauteur du rectangle découpé et pouvait produire une ligne négative. Le retournement horizontal et leur combinaison respectent également les bornes de l’image après découpage. L’indice transparent zéro ne modifie pas la destination.

Le sélecteur de palette conserve les modes ajusté, original (avec repli sur la palette zéro) et personnalisé. Une palette personnalisée absente est refusée. Les paramètres nuls, tailles négatives et rectangles de dégradé nuls ou de dimensions non positives sont vérifiés avant utilisation ; un rectangle de copie vide reste une opération sans effet.

La combinaison dégradé et redimensionnement n’était pas implémentée par le chemin logiciel historique : sa branche était vide. Elle retourne maintenant explicitement TIG_ERR_INVALID_PARAM avant chargement/rendu. Ce bloc n’ajoute donc pas ce mode de redimensionnement. La priorité existante des mélanges constant et par tableau dans le moteur logiciel est conservée.

## Validation

- Les 54 suites passent en Release x64 et x86, y compris le curseur GPU/CPU et les polices anciennes.
- Une image ART synthétique 6×5 avec palette et transparence est dessinée dans 576 configurations : trois modes de palette, quatre retournements, six paires de rectangles et huit dégradés.
- Tous les pixels de la destination 8×7 sont comparés à un oracle indépendant en coordonnées int64_t et interpolation double précision. Les cas incluent les couleurs RGB distinctes aux quatre coins, le découpage partiel, les coordonnées INT_MIN/INT_MAX, les dégradés dépassant leur zone et les pixels transparents.
- Les rectangles d’entrée restent inchangés. Les erreurs de paramètres, le redimensionnement non pris en charge et la palette personnalisée absente ne modifient pas la destination.
- Le test de sauvegarde isolée de Tarant réussit dans les deux architectures : 48 rendus, quatre cibles de zoom (1, 0,75, 1,25, 1), capture et fermeture normales. Les zones centrales contrôlées contiennent 1 811 couleurs ; la capture x64 a été inspectée visuellement.

La première extraction avait supprimé trop de branches voisines ; les tests du curseur et des polices l’ont détecté. Ces branches ont été rétablies avant la validation finale. Le diff final de art.c est limité à trois zones : validation d’entrée, ajout du nouveau chemin et retrait de l’ancienne boucle de dégradé. Les journaux initiaux et finaux sont conservés.

Aucun commentaire de code n’est ajouté. Les fins de ligne sont conservées. L’instrumentation temporaire est retirée, main.c est restauré à l’identique, les exécutables normaux reconstruits et leurs architectures PE vérifiées. Aucun format de données n’est modifié.

## Installation et limites restantes

L’exécutable séparé C:\Games\Arcanum\arcanum-ce-x64.exe est installé avec sauvegarde du précédent. SHA256 : 700C9BA54DCBBF029DBEA6DE75F5A2E0D31C34749C2AC2D033ECC2715768230C. Les empreintes du principal x86 et du fichier INI sont inchangées.

Sources avant modification, différences, vérifications, journaux, captures et manifeste : out/x64-adaptation-stage12-20261024/.

Les autres chemins logiciels ART (couleur constante, tableau de couleurs, alpha et variantes redimensionnées) utilisent encore les anciens calculs de clipping et de progression des pixels ; leur audit reste à effectuer. Les tests de ce bloc ne constituent pas une session prolongée en jeu. Aucun gain de performances ni achèvement global du portage n’est revendiqué.
