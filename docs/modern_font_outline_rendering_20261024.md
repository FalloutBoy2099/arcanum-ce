# Rendu des lettres des polices modernes — 24 octobre 2026

Les dialogues en Calibri 16 pixels utilisaient les glyphes bitmap monochromes intégrés à la police. Les lettres M, O et y avaient ainsi des formes anguleuses malgré le rendu demandé avec lissage. Les caractères accentués étaient présents ; ce défaut concernait leur dessin.

Une comparaison avec la même police privée temporairement de ses tables bitmap a confirmé la différence. Cette copie reste uniquement dans les artefacts de diagnostic : aucune police système ni police distribuée avec le jeu n’est modifiée.

La configuration CMake applique désormais à SDL_ttf 3.2.2 la sélection des contours pour les polices redimensionnables (`FT_LOAD_NO_BITMAP`). Les mêmes options sont utilisées pour le chargement des glyphes et pour HarfBuzz, à l’ouverture et lors des changements de hinting. Les polices uniquement bitmap et les anciennes polices ART conservent leur fonctionnement.

Le correctif est idempotent et protégé par un verrou, car les configurations x86 et x64 peuvent partager les sources téléchargées. Il vérifie les trois expressions attendues dans la version épinglée de SDL_ttf et signale une erreur si elles changent.

La régression rend « Merci Où moyennant » aux tailles 12, 13, 15, 16, 17 et 19 avec la police de test. Elle vérifie la présence de pixels de lissage pour une police redimensionnable. Elle échouait avec Calibri avant le correctif. Les tailles et les chemins configurés dans arcanum.ini restent inchangés.

Les captures et journaux de validation sont conservés dans `out/font-letter-render-20261024`. Le scénario graphique utilise une copie isolée du jeu et d’une sauvegarde à Tarant ; il ne remplace pas une validation visuelle dans toutes les interfaces.

Validation : 56/56 suites réussies en x64 et en x86. Le scénario graphique x64 charge Tarant, affiche les phrases signalées avec le fichier système Calibri, présente 48 images et quatre niveaux de zoom, puis quitte normalement. La capture montre des lettres lissées. La version x64 1.0.0.13 est installée ; les empreintes de l’exécutable principal x86 et de arcanum.ini restent identiques.
