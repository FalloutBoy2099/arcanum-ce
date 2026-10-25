# Aperçu du déplacement au survol

Le survol du sol affiche une ligne semi-transparente entre le personnage et la destination, des repères aux changements de direction et un losange à l'arrivée. Le tracé est dessiné après la composition du monde et avant les textes flottants. Il suit le zoom et la caméra sans ajouter de fenêtre qui intercepterait les clics.

## Configuration

Dans `[ui]` de `arcanum.ini` :

```ini
movement_preview=1
movement_preview_color=#90D8A0
movement_preview_blocked_color=#F08080
movement_preview_beyond_ap_color=#D8B060
movement_preview_opacity=70
```

`movement_preview=0` désactive l'aperçu. L'opacité est exprimée en pourcentage, de 0 à 100. La couleur normale est verte, l'arrivée sans trajet trouvé est rouge, et les portions dépassant les PA disponibles en combat au tour par tour sont ambrées.

L'aperçu disparaît pendant les dialogues, les menus, les modes d'action autres que le déplacement, le glissement d'objet, le déplacement manuel de caméra et le survol de l'interface ou d'un objet. En combat au tour par tour, il apparaît pendant le tour du personnage local actif.

## Calcul et rendu

`anim_path_preview` applique les capacités de déplacement du personnage : portes, fenêtres, métamorphose, pièges et feu. La recherche utilise le même solveur et les mêmes coûts que les déplacements. Elle ne crée aucun objectif d'animation et ne modifie ni la position, ni les PA, ni les ordres des compagnons.

`path_make` conserve son contrat de tronçons de huit cases. La nouvelle requête `path_make_preview` restitue le trajet complet, dans la limite de 200 pas et du domaine local du solveur. Elle n'utilise pas le quota des recherches des PNJ. Pendant A*, elle vérifie un délai coopératif de cinq millisecondes entre les expansions. Ce délai ne borne pas la durée d'une opération individuelle, telle qu'un accès aux données.

Les recherches sont espacées d'au moins 200 ms lorsque la case cible ou le personnage change ; une cible inchangée est réévaluée chaque seconde pour prendre en compte les obstacles mobiles. Les tronçons déjà parcourus sont retirés du tracé en cache sans nouvelle recherche. Les modifications du zoom, de la caméra et des décalages du personnage actualisent la projection sans relancer le pathfinding.

Le tracé reste indicatif : les obstacles peuvent bouger avant le clic, et un échec peut correspondre au budget ou à la portée locale de recherche. Les chemins depuis le centre de la case ne reproduisent pas chaque détail de recentrage du personnage pendant une animation. Le déplacement réel conserve son propre calcul au clic.

Le rendu emploie un buffer réutilisé avec une couleur de transparence, un découpage explicite des segments à l'écran et un mélange des pixels limité aux régions à restaurer. Les zones de l'ancien et du nouveau tracé sont invalidées pour éviter les traces persistantes. Le buffer est libéré lors des changements de résolution, des réinitialisations et de la fermeture.

## Validation

- 55 suites réussies en x64 et 55 en x86, dont une nouvelle suite de rendu de l'aperçu.
- Vérification du trajet complet au-delà de huit pas, du maintien du contrat des déplacements et de l'indépendance du quota des PNJ.
- Vérification des couleurs, de la transparence à 70 % et de l'opacité complète, de l'effacement, des régions partielles, du zoom, de la réutilisation du trajet et du découpage d'un trajet de 200 pas sortant de l'écran.
- Vérification que l'aperçu n'ajoute pas de fenêtre au-dessus de la carte.
- Essai isolé à Tarant sur les deux architectures : trajet de douze cases détecté par le survol, 48 images rendues à quatre niveaux de zoom, captures produites et capture x64 inspectée visuellement.

La version x64 est installée dans `C:\Games\Arcanum\arcanum-ce-x64.exe`. Les cinq paramètres ont été ajoutés à la section `[ui]` du fichier installé. L'exécutable x86 principal est inchangé. Les copies de l'exécutable précédent et de l'INI, ainsi que les empreintes avant/après, figurent dans `out/movement-preview-20261024/install-20261024-135240/`.

Les sources avant modification, les journaux et les captures de l'essai à Tarant sont conservés dans `out/movement-preview-20261024/`. Aucun format de sauvegarde ou fichier de données du jeu n'est modifié par cette fonctionnalité.
