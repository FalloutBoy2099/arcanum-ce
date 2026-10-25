# Adaptation x64, quatorzième bloc — 24 octobre 2026

Le rendu ART logiciel avec redimensionnement utilise désormais `tig_art_blit_scaled`, au lieu de boucles distinctes qui accumulaient des erreurs flottantes et faisaient avancer des pointeurs.

## Échantillonnage et accès mémoire

`TigArtScaleAxis` décrit une progression rationnelle par quotient et reste en entiers 64 bits. Pour une position de destination d relative au rectangle demandé, la règle est l’entier le plus proche de d × taille_source / taille_destination, avec égalité à mi-chemin arrondie vers l’indice inférieur. Cette règle correspond à l’accumulateur historique initialisé à 0,5 avec comparaison stricte > 1, sans sa dérive flottante. L’indice est borné au dernier pixel du rectangle source : l’ancien accumulateur pouvait le dépasser lors d’agrandissements supérieurs à deux.

Le premier échantillon est calculé depuis la position dans le rectangle destination original. Les échantillons suivants avancent par addition, sans division dans la boucle de pixels. Le découpage à l’écran ne réinitialise donc plus la phase de redimensionnement. Le rectangle source n’est pas réétiré lorsque sa partie hors image est éliminée : les échantillons qui tombent en dehors de l’image laissent la destination intacte.

Les retournements X/Y sont appliqués seulement après validation des coordonnées logiques dans l’image. Les pointeurs de ligne sont recalculés en size_t et seuls les pixels effectivement valides sont lus ou écrits. Les couleurs par colonne et le tramé source suivent les coordonnées source logiques ; le tramé destination suit les coordonnées réelles de destination. Le pixel d’indice zéro reste transparent.

Les dimensions et coordonnées INT_MIN/INT_MAX n’entraînent aucune allocation proportionnelle au rectangle demandé. Le parcours est limité à la destination visible. Les produits de calcul d’échantillon sont en uint64_t ; l’ajout aux positions éventuellement négatives est explicitement en int64_t.

## Mélanges et simplification

Le sélecteur commun conserve la priorité des mélanges addition, soustraction, multiplication, alpha selon luminosité, alpha constant, alpha source, alpha interpolé, puis tramé. Les modes de teinte constante et par colonne conservent leurs règles de couleur et d’alpha source. Les alphas X/Y/XY sont évalués aux coordonnées source échantillonnées dans l’image complète, avec conversion bornée.

Les anciennes branches de copie sans redimensionnement sont vérifiées identiques à celles du bloc 13. Le dégradé de couleur sans redimensionnement conserve son chemin du bloc 12. Dégradé de couleur et redimensionnement restent explicitement non pris en charge par l’API, comme depuis ce bloc.

art.c passe de 6 252 à 5 053 lignes pour ce changement. Les anciennes variables de rapports, accumulateurs et décalages de lignes du moteur de redimensionnement sont supprimées. Aucun commentaire de code n’est ajouté.

## Validation

Les 54 suites CTest passent en Release x64 et x86.

2 304 configurations de redimensionnement sont ajoutées à l’oracle : trois modes de couleur, douze mélanges, quatre retournements et seize paires de rectangles. Une recherche binaire indépendante choisit le pixel source selon les intervalles d’arrondi, contrairement au parcours incrémental du moteur. Tous les pixels sont comparés, avec transparence, rectangles d’entrée conservés, agrandissements/réductions, découpage source/destination, annulation de grands décalages négatifs et coordonnées extrêmes.

Les coins alpha du test valent 30, 210, 180, 60 : les trois interpolations sont donc vérifiées avec un alpha variable et des valeurs attendues calculées séparément. Les 1 008 copies sans redimensionnement et 576 dégradés du bloc précédent passent toujours, soit 3 888 configurations de rendu ART dans cet oracle. Ces tests n’affirment pas une identité bit à bit avec toutes les anciennes accumulations flottantes : leur dérive et les dépassements sont précisément supprimés.

Le test moteur charge la sauvegarde isolée de Tarant et effectue 48 rendus à quatre cibles de zoom dans chaque architecture. Captures et fermetures normales ; 1 811 couleurs dans chaque zone centrale contrôlée. La capture x64 est inspectée visuellement.

Les fins de ligne sont conservées. main.c est restauré à l’identique après instrumentation, les exécutables normaux reconstruits, leurs architectures PE et l’absence du marqueur de diagnostic vérifiées. Aucun format de données n’est modifié.

## Installation et reste de l’audit

L’exécutable séparé C:\Games\Arcanum\arcanum-ce-x64.exe est installé avec sauvegarde du précédent. SHA256 : 1E349CE5869D6A56309AAA33F8D1DD88F1B3A0CC59412C5E7F26CA76B20A1BCE. Les empreintes du principal x86 et du fichier INI sont inchangées.

Les sources avant modification, différences, journaux, captures et manifestes se trouvent dans out/x64-adaptation-stage14-20261024/.

La conversion des frames indexées vers les buffers vidéo (`tig_art_frame_blit_software`) et les chemins d’erreur/comptabilité du cache ART restent à auditer. Les tableaux de couleurs sont toujours des pointeurs sans longueur explicite et supposent un tableau fourni par l’appelant suffisamment long pour l’image. Une validation prolongée en jeu reste nécessaire. Aucun gain de FPS ni achèvement global du portage n’est revendiqué.
