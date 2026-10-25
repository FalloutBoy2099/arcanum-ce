# Migration progressive du rendu vers le GPU

## Bloc 1 : cache de textures et premier consommateur

Le cache `first_party/tig/src/sprite_cache.c` conserve les images préparées dans des textures du renderer SDL principal. Il ne crée pas de deuxième périphérique GPU et ne récupère aucun pixel vers le CPU pendant l'affichage normal.

La clé représente l'image, sa révision représente son contenu. La taille, le format et la couleur de transparence font également partie des conditions de réutilisation. Une révision différente remplace la texture. La conversion vers RGBA et le transfert ne sont effectués que lors d'un chargement ou remplacement.

Le cache est limité par un nombre d'entrées et un budget en octets de pixels. Une éviction retire l'entrée la moins récemment utilisée. Ce budget ne comprend pas les surcoûts d'allocation propres au pilote graphique. Le pointeur de texture renvoyé reste valide jusqu'à une éviction, un remplacement, un effacement ou la destruction du cache ; les consommateurs doivent soumettre leur dessin avant une opération susceptible de le libérer.

Le premier consommateur réel est le curseur, y compris ses images d'animation et les objets ajoutés lors d'un glisser-déposer. Le moteur prépare encore ses pixels depuis les ART sur CPU. Une empreinte de leur contenu, recalculée uniquement après modification, distingue les variantes composées et les changements de palette. Elle permet de retrouver une texture déjà transférée lorsqu'une image d'animation revient.

Le curseur est composé sur GPU après l'image des fenêtres et avant le fondu final et les FPS. Le tampon CPU de présentation garde ainsi son fond sans curseur dans ce mode. Les captures BMP reconstituent le curseur dans une copie CPU à la demande, sans récupération GPU. Les mouvements et images animées continuent de bénéficier des invalidations existantes ; le GPU réaffiche également le curseur conservé pendant les présentations sans nouveau rectangle sale.

```ini
[video]
gpu_cursor=1
```

`0` conserve le chemin CPU. Le réglage vaut `0` par défaut dans le code ; l'installation d'essai utilise `1`. Une erreur de création ou de dessin retourne au curseur CPU. Le cache du curseur possède 128 entrées et un budget de 16 Mio de pixels. Il est libéré avant le renderer et vidé lors d'une réinitialisation de celui-ci. Des compteurs de réutilisations, transferts, octets transférés et évictions sont inscrits dans le journal de débogage à sa fermeture normale.

## Validation du bloc

- Test GPU du cache : transparence par couleur, alpha partiel, rectangles source, retournement horizontal, découpage à l'écran, ordre de superposition, révisions, plafond de mémoire et éviction LRU.
- Une animation de 16 images répétée dix fois produit 16 transferts et 144 réutilisations dans le test, soit 1 024 octets transférés pour ses images synthétiques de 4 × 4 pixels.
- Test d'intégration avec une fenêtre SDL cachée : le vrai curseur ART est identique pixel par pixel au chemin CPU, puis testé avec changement d'image, masquage, perte de focus, réinitialisation du cache, ajout d'un objet au curseur et retour CPU après rejet d'une image dépassant le budget.
- La persistance du réglage est testée.

Ces tests établissent la réutilisation et le respect du rendu sur leur périmètre. Ils ne constituent pas un benchmark global en partie. Le gain FPS attendu de ce seul consommateur est faible ; cette étape valide le mécanisme avant son extension aux éléments nombreux du monde. La vérification interactive des menus et de l'inventaire reste à effectuer.

## Blocs suivants

1. Transmettre progressivement les commandes de dessin des tuiles et sprites au GPU, avec leur ordre exact, leurs palettes, transparences, masques de murs et toits. Conserver les données CPU nécessaires aux clics et aux collisions, sans lire les résultats du GPU pour la simulation.
2. Composer le monde et son zoom dans une cible GPU persistante. Les déplacements de caméra ne doivent plus nécessiter la mise à l'échelle CPU de tous les pixels.
3. Composer les textes et fenêtres sur GPU, en reproduisant notamment les panneaux translucides et les régions visibles du compositeur actuel. Les décisions de disposition restent sur CPU.
4. Brancher les passes jour/nuit et eau directement entre ces cibles GPU, puis migrer l'application de l'éclairage. Supprimer les récupérations CPU intermédiaires des prototypes de shaders.

L'ordre est guidé par les dépendances : déplacer un sprite isolé puis récupérer ses pixels sur CPU à chaque dessin ajouterait une synchronisation coûteuse. L'extension au monde demande donc une chaîne de composition cohérente plutôt qu'un remplacement aveugle de tous les blits.

À chaque bloc, comparer les images et les temps sur les mêmes sauvegardes, parcours, zooms et réglages. Les chargements de ressources, le pathfinding, l'IA et les scripts restent des chantiers CPU distincts.
