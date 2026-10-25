# Adaptation x64, quinzième bloc — 24 octobre 2026

Ce bloc corrige la conversion des frames indexées ART vers les buffers et la préparation de leurs lots dans le cache.

## Conversion des frames

`tig_art_frame_blit_software` valide la frame, la palette, le stockage et les dimensions avant utilisation. La conversion commune `tig_art_frame_convert`, dans tig/art_frame.h, calcule les adresses de lignes en size_t et retourne chaque ligne horizontalement lorsque le drapeau ART le demande.

L’ancien code mélangeait des déplacements en pixels et en octets : son pas de fin de ligne divisait le pitch par quatre mais l’appliquait à un pointeur d’octets. La variante retournée combinait un parcours source inversé avec un déplacement destination incorrect. Le nouveau chemin écrit chaque pixel de sa ligne à une adresse calculée depuis la base, respecte les espaces de fin de ligne et ne fait pas avancer de pointeur hors du stockage après la dernière ligne. L’indice transparent zéro produit le pixel zéro attendu pour ce chemin de conversion.

La copie utilise memcpy pour les mots de quatre octets, ce qui permet de vérifier aussi une destination volontairement non alignée sans introduire d’accès C non aligné.

## Préparation et comptabilité du cache

Les nouveaux lots de buffers sont créés dans une table locale initialisée à zéro. En cas d’échec d’allocation ou de préparation, tous leurs buffers sont détruits et la table libérée. Aucun lot incomplet n’est publié dans l’entrée du cache.

Une fois toutes les images prêtes, la table est publiée et ses octets de table et de stockage sont comptés une seule fois. La mise à jour d’un lot déjà présent et marqué sale conserve sa comptabilité ; si cette mise à jour échoue, son état sale est conservé pour permettre une nouvelle tentative. La sortie de art_get_video_buffer est initialisée à NULL avant les chemins d’erreur.

Les variantes de toiture 9–12 utilisent les dimensions de leurs frames sources 0–3, conformément à l’encodage des variantes retournées. Cela corrige aussi les dimensions incohérentes possibles avec des en-têtes de toiture contenant 13 frames. Les rotations impaires mur/porte sont normalisées avant sélection des tables de frame et de cache, au lieu de conserver un ancien index de rotation après modification de l’identifiant.

`tig_art_cache_statistics` fournit des diagnostics en lecture seule : nombre d’entrées, tables et buffers publiés, octets système et octets de stockage des buffers comptabilisés. Le champ video_bytes reprend le budget historique du cache ; il ne constitue pas une mesure de VRAM du pilote SDL.

## Vérification

Les 54 suites passent en Release x64 et x86, y compris les 3 888 configurations de dessin ART des blocs précédents.

- Conversion normale et retournée d’une image non uniforme avec transparence : six configurations de pitch, destination plus grande, adresse non alignée et octets de garde. Tous les octets, y compris les zones non écrites et les espaces de fin de ligne, sont contrôlés.
- Neuf paramètres invalides de conversion sont refusés sans écrire dans la destination : source/palette/destination absentes, source tronquée, dimensions ou pitch insuffisants et stockage hors limites.
- Cache ordinaire : création de deux buffers, répétitions sans double comptage, rafraîchissement de palettes et retour à zéro après vidage.
- Échec partiel de création : une seconde frame vide fait échouer la création du lot après la première allocation. Plusieurs tentatives et un rafraîchissement ne laissent aucun lot publié ni octet supplémentaire comptabilisé ; le dessin de la première frame continue via le chemin logiciel de repli.
- Toitures à 9 et 13 frames avec dimensions différentes : vérification des pixels des 13 morceaux, palettes et retournements virtuels, réutilisation puis rafraîchissement sans double comptage.

Le test moteur sur une copie isolée de la sauvegarde de Tarant réussit dans les deux architectures : 48 rendus à quatre cibles de zoom, captures et sorties normales. Les zones centrales contiennent 1 811 couleurs ; la capture x64 est inspectée visuellement. Ce test ne force pas une panne du pilote ni une pénurie mémoire réelle pendant le remplissage d’un lot.

Les moteurs de dessin, du marqueur 0x505EB0 au marqueur 0x51AA90, sont vérifiés identiques au bloc précédent. Aucun commentaire de code n’est ajouté. Les fins de ligne sont conservées. main.c est restauré à l’identique et les exécutables normaux reconstruits puis contrôlés.

## Installation et suite

L’exécutable séparé C:\Games\Arcanum\arcanum-ce-x64.exe est installé avec sauvegarde du précédent. SHA256 : 50F28CCE6601D83A00ADF4A62C3A323374BD3708450C7E273740C4D990443681. Les empreintes du principal x86 et du fichier INI sont inchangées. Aucun format de données n’est modifié.

Sources avant modification, différences, journaux, captures et manifestes : out/x64-adaptation-stage15-20261024/.

Ce bloc couvre la préparation des buffers du cache ART. Il ne constitue pas une validation exhaustive des autres allocations du moteur ni une session prolongée en jeu. Aucun gain de FPS ni achèvement global du portage n’est revendiqué.
