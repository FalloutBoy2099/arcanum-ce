# Séparation du monde et de l'interface

Implémentation du 18 octobre 2026, préalable aux shaders.

## Ordre de composition

1. `gamelib_draw_game` dessine les lumières, le terrain, les objets et les toits dans `gamelib_world_video_buffer`.
2. `world_composition_present` copie les régions nécessaires vers le tampon de présentation, avec la transformation de zoom éventuelle.
3. `gamelib_draw_overlays` dessine les bulles, textes flottants et conversations dans le tampon de présentation.
4. Le compositeur de fenêtres ajoute les fenêtres d'interface et menus, puis le curseur ; la présentation vidéo applique le fondu final.

Le tampon du monde est réutilisé à l'échelle normale, où il n'était auparavant utilisé que pour le zoom. Il conserve le monde sans les textes d'interface. Le tampon de présentation contient le résultat composé : il ne constitue pas une couche UI transparente indépendante.

`world_composition_present` forme désormais une frontière explicite avant les textes. Aucun shader n'est chargé ou exécuté dans cette étape, et aucune migration vers le renderer GPU SDL n'est effectuée. Cette dernière restera nécessaire pour traiter le monde directement sur le GPU et y recomposer l'interface.

## Rafraîchissement et défilement

Les rectangles sales restent utilisés. Le retour du zoom vers l'échelle normale invalide le cache du monde et force sa reconstruction. Le redimensionnement et la réinitialisation invalident également ce cache.

`gamelib_scroll_world_and_interface` synchronise le tampon du monde et celui de présentation pendant le défilement par copie de pixels à l'échelle normale. Les cas où le défilement exigeait déjà un redessin complet conservent leur comportement.

La séparation réutilise le tampon de zoom existant et n'alloue pas un nouveau tampon plein écran. Elle ajoute néanmoins une copie des régions modifiées à l'échelle normale ; l'impact réel sur les temps de frame en jeu reste à mesurer.

Si le tampon du monde n'a pas pu être créé, le chemin de secours conserve un rendu direct, suivi des textes. La séparation physique n'est alors pas disponible.

## Validation

- Compilation Release x86 réussie.
- Les 25 suites CTest passent.
- `world_composition_test` vérifie l'indépendance du monde et des textes, la restauration du fond, les copies partielles et leur repli complet, ainsi que l'absence de travail sans dommage.
- Les tests comparent les copies avec réduction et agrandissement à une référence SDL, avec les deux filtres.
- Les tests de défilement vérifient les recouvrements dans les deux axes et la conservation des pixels hors du viewport dans le tampon plus grand.

Les tests du module utilisent des surfaces SDL et des adaptateurs aux appels TIG ; ils ne constituent pas un essai visuel interactif de l'ensemble du jeu. L'exécutable installé n'a pas été remplacé.
