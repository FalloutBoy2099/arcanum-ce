# Étude : shaders et modernisation du rendu

Date : 18 octobre 2026. Étude du code présent ; aucune fonctionnalité graphique implémentée dans cette passe.

## Architecture constatée

`first_party/tig/src/video.c`, dans `tig_video_flip`, transfère les zones modifiées de la surface CPU vers une texture SDL, puis présente cette texture. Le fondu et le compteur de FPS sont dessinés ensuite. Il n'existe pas de chaîne de shaders personnalisés dans ce chemin.

`src/game/gamelib.c`, dans `gamelib_draw_game`, organise les passes de lumière, de terrain, d'objets et de toits, puis les textes flottants hors de la passe de zoom. Cette séparation logique est un point d'appui, mais elle ne fournit pas encore des textures GPU distinctes pour les matériaux, les normales, les émissions et la profondeur.

La dépendance SDL du build local est 3.4.8. Ses en-têtes exposent `SDL_CreateGPURenderer` et `SDL_SetGPURenderState`. Un backend permettant des shaders personnalisés peut donc être étudié sans remplacer SDL. Il faut cependant modifier la création du renderer et vérifier les capacités effectives du GPU ; le renderer actuellement créé automatiquement n'offre pas ce contrat.

Sources SDL :

- https://wiki.libsdl.org/SDL3/SDL_CreateGPURenderer
- https://wiki.libsdl.org/SDL3/SDL_SetGPURenderState
- https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader

## Animations et déplacements

Le rythme des animations vient des métadonnées ART : plusieurs fonctions de `anim.c` calculent le délai avec `1000 / art_anim_data.fps`. Les fréquences réelles ne sont donc pas nécessairement identiques pour tous les sprites.

Mélanger les images N et N+1 produit une transition de transparence. Cela ne reconstruit pas la pose intermédiaire d'un membre : deux silhouettes peuvent apparaître simultanément, particulièrement sur les armes, les jambes et les contours. Réserver cette option à des essais sur les effets, la fumée et certains sprites ; ne pas l'activer globalement par défaut.

Les images successives doivent être alignées par leur point d'ancrage ART, avec un traitement cohérent de la transparence. Réinitialiser le mélange lors d'un changement de direction, de type d'animation, d'une téléportation ou d'une interruption. Éviter de montrer par anticipation une pose liée à un événement de combat.

Le lissage des positions est plus prioritaire. `location.c` conserve des origines entières et `object.c` applique des offsets entiers aux sprites. Conserver séparément les états de simulation et une position de présentation interpolée permettrait de lisser caméra et mouvements, sans changer les collisions, les points d'action ni les sauvegardes. Un simple filtre sur l'image finale ne suffit pas à déplacer chaque entité indépendamment.

L'interpolation classique entre deux états connus peut introduire jusqu'à un pas de simulation de retard. Évaluer ce compromis avant de choisir une extrapolation, qui peut dépasser visuellement les obstacles. L'occlusion, les textes attachés et le survol doivent rester cohérents avec la position présentée.

## Lumières, normales et ombres

Arcanum possède déjà des lumières locales, des couleurs ambiantes et un système d'ombres dans `light.c`. L'objectif serait d'enrichir ce système en évitant de multiplier deux fois la même contribution lumineuse.

Une normale déduite de la luminosité d'un sprite ne constitue pas une reconstruction fiable de sa géométrie : la texture contient déjà couleur des matériaux, éclairage et ombres précalculés. Elle peut donner un métal bosselé ou transformer une ombre peinte en relief.

Commencer par quelques surfaces avec normales préparées et mises en cache, plutôt que générer toutes les normales à chaque image. Les sprites animés demanderaient des normales cohérentes entre images et orientations, ce qui représente un travail d'assets conséquent.

Les normales seules ne produisent pas d'ombres portées entre objets. Il faudrait aussi une représentation des obstacles et de leur hauteur : silhouettes, masques d'occlusion ou géométrie simplifiée issue des murs et objets. Le relief peint des bâtiments ne fournit pas automatiquement cette information.

Pour une première amélioration : lumières existantes, éclairs brefs liés aux tirs et sorts, puis halos et bloom sélectif. Un masque d'émission doit désigner les sources lumineuses ; un seuil de luminosité appliqué à toute l'image ferait aussi briller les pages de journal et les surfaces blanches.

## Distorsion, particules et ambiance

Les ondes de choc et la réfraction sont de bons effets localisés après composition du monde. Fournir au shader la position de l'événement, son âge, son rayon et son intensité, avec une référence de texture évitant de lire et écrire la même cible simultanément.

Le brouillard, la pluie et la vapeur peuvent être ajoutés par couches animées. Leur position doit être ancrée dans le monde pour rester stable pendant le scroll. Prévoir des masques intérieurs/extérieurs, une profondeur approximative et des émetteurs pour les cheminées ; sans eux, un effet uniforme traverse les murs et les personnages.

Un bruit GPU anime une densité de brume, mais ne simule pas un volume ou des interactions physiques à lui seul. Commencer par quelques couches 2D avec profondeur et éclairage simplifiés.

## Eau

Le moteur doit identifier explicitement les tuiles et variantes d'eau. Déduire l'eau d'une couleur bleue à l'écran serait fragile et inclurait des vêtements, des sorts ou l'interface.

Première version : masque de matériau, ondulations et reflets légers. Une réfraction du fond visible nécessite une texture du fond ou une approximation contrôlée ; le shader ne peut pas retrouver ce que la texture opaque d'eau a déjà masqué. Les remous liés aux personnages nécessitent des événements de contact et une échelle spatiale cohérente.

## Couleurs et cycle jour/nuit

`light_scheme.c` charge des couleurs RGB intérieures et extérieures par heure. `ambient_lighting_process_callback`, dans `anim_ui.c`, actualise le schéma toutes les heures de jeu. Le modèle actuel ne consiste pas simplement à poser une couche noire sur l'écran.

Interpoler les couleurs de deux heures successives est une amélioration indépendante des shaders. Une LUT peut ensuite modifier la teinte, les contrastes et la saturation pour l'aube, le jour, le crépuscule et la nuit. Évaluer les transitions dans un espace colorimétrique cohérent et préserver le réglage d'éclairage existant.

Une LUT appliquée à une image déjà noire ne restitue pas les détails perdus. Une correction par zone ne suffit pas non plus pour distinguer l'intérieur d'un bâtiment de son extérieur dans une même vue : prévoir cette information avant composition, ou dans un masque.

Les ambiances magie/technologie devraient être des profils artistiques par carte ou région, éventuellement ajustés selon des événements. La maîtrise magique du joueur ne doit pas teinter toute la ville par défaut. Prévoir des profils discrets et réglables pour préserver les couleurs d'origine.

## Ordre d'implémentation proposé

1. Backend de présentation GPU optionnel, mode identité, repli vers le rendu actuel et chargement de shaders à partir d'un contrat de ressources et de paramètres défini. Précompiler les formats nécessaires au backend plutôt que supposer qu'un fichier GLSL fonctionne partout.
2. Séparation explicite du monde, des textes lisibles et de l'interface. Chaîne initiale : couleur du monde, effets localisés, bloom sélectif, composition des textes et menus, fondu final. Les passes locales devront préciser leur ordre vis-à-vis des objets transparents et des toits.
3. Cycle jour/nuit progressif et profils colorimétriques optionnels.
4. Interpolation visuelle des positions et de la caméra, avec une migration ciblée du rendu des sprites si le backend de présentation seul ne suffit pas.
5. Effets de sorts, tirs, vapeur et brouillard, puis eau avec identification des matériaux.
6. Essais de normales préparées et d'ombres directionnelles sur un périmètre réduit.

Le support de post-traitement n'impose pas de migrer immédiatement tous les sprites vers le GPU. En revanche, les déplacements sub-pixel indépendants, les normales par sprite et les ombres enrichies demandent une évolution du rendu de scène au-delà du filtre final.

## Validation et performances

Le GPU peut rendre le post-traitement peu coûteux, sans éliminer la composition CPU ni son transfert vers la texture. Aucun gain global de FPS n'est établi avant mesure.

Comparer le mode désactivé et la passe identité, puis chaque effet séparément : temps CPU, temps GPU, transferts, mémoire, temps de frame médian et percentiles élevés. Utiliser les mêmes scènes, trajets et résolutions ; tester aussi redimensionnement, plein écran, zoom, Alt-Tab et reprise après perte de ressources.

Les effets animés doivent avancer même lorsque le monde est immobile. Le suivi actuel des rectangles sales reste utile pour transférer les pixels source, mais ne peut pas être le seul déclencheur de présentation des effets temporels. Conserver une texture source intacte et recomposer les effets évite leur accumulation.

Vérifier les contours des sprites, le tri derrière les murs, les décès et changements de direction, les textes flottants, les intérieurs, les sauvegardes et les captures d'écran. Les clics et la simulation doivent conserver leur comportement lorsque tous les effets sont désactivés.
