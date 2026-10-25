# Fond des textes flottants

Les bulles au-dessus des personnages et les notifications flottantes disposent d'un fond configurable dans la section `[ui]` de `arcanum.ini` :

```ini
floating_background_color=#000000
floating_background_opacity=60
floating_margin_horizontal=16
floating_margin_vertical=8
```

La couleur utilise le format `#RRGGBB`. L'opacité est un pourcentage de 0 à 100 : 0 désactive le fond, 100 le rend opaque. La valeur initiale de 60 assombrit le décor derrière le texte sans le masquer complètement.

Les marges intérieures sont exprimées en pixels : gauche/droite pour `floating_margin_horizontal`, haut/bas pour `floating_margin_vertical`. Chacune accepte de 0 à 80 pixels. Les valeurs initiales 16 et 8 correspondent à celles des dialogues. Elles sont indépendantes des marges des réponses de dialogue et nécessitent de relancer le jeu après modification.

Le fond est appliqué à la zone de texte avec ses marges, avant les glyphes. Les buffers réservent un espace supplémentaire autour du texte sans réduire sa largeur de composition. Les dimensions complètes participent au positionnement, à la résolution des chevauchements et aux invalidations des bulles. Pour les notifications qui s'estompent, l'opacité du fond est multipliée par celle du texte ; leur espacement inclut les marges verticales. Les suppressions et déplacements restaurent toute la zone, marges comprises.

Le rendu des réponses de dialogue en bas de l'écran conserve son paramètre distinct `dialog_background_opacity`, dont l'échelle historique est de 0 à 255.

Les paramètres sont enregistrés par le moteur et documentés dans `docs/community-options.ini`. Ils s'appliquent aux polices bitmap comme aux polices modernes. Il n'y a aucun changement de format de sauvegarde ou de données.

Les preuves de compilation, tests, captures et installation sont regroupées dans `out/floating-background-20261024/`.

Validation : 56 suites réussies en x64 et 56 en x86. Le nouveau test vérifie les pixels pour onze opacités et six niveaux d'estompage, la couleur personnalisée, l'opacité complète, la désactivation et le découpage. Un essai isolé à Tarant a affiché une bulle de deux lignes et rendu 48 images à quatre niveaux de zoom sur chaque architecture ; la capture x64 a été inspectée visuellement.

La version x64 est installée dans `C:\Games\Arcanum\arcanum-ce-x64.exe` et les deux paramètres ont été ajoutés dans l'INI installé. La copie de l'exécutable précédent et de l'INI, ainsi que les empreintes avant/après, figurent dans `out/floating-background-20261024/install-20261024-140535/`. L'exécutable x86 principal est inchangé. La source de démarrage a été restaurée et les exécutables normaux reconstruits après la sonde graphique.

Les marges ont ensuite été validées par les 56 suites sur chaque architecture et par un rendu de 48 images à quatre niveaux de zoom à Tarant. Les preuves et captures se trouvent dans `out/floating-margins-20261024/`. La version x64 et les deux paramètres ont été installés ; les copies précédentes et les empreintes figurent dans `out/floating-margins-20261024/install-20261024-141417/`.
