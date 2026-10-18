# Optimisations du 17 octobre 2026

Premier lot issu de `performance_audit_20261017.md`, après autorisation.

## Changements appliqués

- **Panneaux de combat** : stockage du dernier texte et du style dans
  `src/ui/status_panel.c`. Remplissage, cadre et texte ne sont redessinés
  que lorsque nécessaire. Déplacement et visibilité sont traités séparément.
  Masquer un panneau déjà masqué ne réinvalide plus l'écran. Les coûts et
  chances restent recalculés : aucune règle de combat n'est mise en cache.
- **Index du journal** : espace de travail réutilisable, déduplication des
  descriptions par table de hachage, et recherche des mentions en un parcours
  du texte avec candidats regroupés par première lettre. Les connaissances,
  rubriques visibles et contrôles de validité existants sont conservés.
  La collecte du pool d'objets et la vérification des textes restent nécessaires.
  L'indexation des mentions conserve les règles de mots et d'accents précédentes.
- **Texte moderne** : cache LRU des surfaces finales dans
  `first_party/tig/src/text_cache.c`, limité à 128 entrées et environ 8 Mio
  (pixels, texte et structures comptabilisés). La clé contient la police SDL,
  l'encodage, les styles de glyphes, la largeur, la couleur et l'atténuation
  des crochets. L'ombre possède sa propre entrée avec sa couleur et son alpha ;
  sa position est appliquée à chaque dessin. Les surfaces en cours de dessin
  sont protégées de l'éviction. Une entrée trop grande est dessinée puis libérée.
  La destruction de chaque police purge ses entrées avant de libérer SDL_ttf.
  La mesure des textes reste séparée et ne rasterise toujours pas.
- **Texture de présentation** : suivi des trois chemins d'écriture de la
  surface finale (copie, transparence du HUD, remplissage). Les rectangles
  voisins sont fusionnés. Aucun transfert si rien n'a changé ; transfert complet
  au-delà de 32 rectangles ou de 60 % de la surface, à la création de texture,
  ou après notification de réinitialisation du rendu. La présentation reste
  exécutée, ainsi que fondus, compteur FPS et VSync. Une erreur de transfert
  conserve les zones à renvoyer.

## Mesures ajoutées

Le profilage existant compte désormais `text_cache_hits`, `text_cache_misses`
et `texture_upload_bytes`. Les temps `texture_upload` et
`render_present_with_vsync` séparent transfert et attente de présentation.
Les temps `lighting`, `object_render`, `collision` (appel public de coût de
franchissement) et `event_dispatch` préparent l'analyse du prochain lot.
Les mesures sont inclusives et ne doivent pas être additionnées aveuglément.
Le profilage désactivé ne lit pas l'horloge. Aucun INI ni scénario de benchmark
automatique n'est activé par cette installation.

## Validation et limites

- Comparaison différentielle du moteur de mentions avec l'ancien, sur 500
  corpus déterministes et des cas d'accents, préfixes et limites de mots.
- Déduplication : descriptions distinctes ayant toutes le même compartiment
  de hachage, doublons et réinitialisation.
- Panneaux : répétition, masquage, réapparition, déplacement, couleur,
  redimensionnement et recréation, dans les véritables fenêtres TIG.
- Cache : budget mémoire, éviction, surfaces protégées, saturation, gros textes
  hors cache, purge par police et changement de clés.
- Texte réel avec SDL_ttf : comparaison pixel par pixel à froid/à chaud et
  absence de nouvelle rasterisation au deuxième dessin, pour 12 styles.
  Les tests existants de langues, crochets et sous-lignage restent exécutés.
- Texture : comparaison avec la surface source après 1 000 modifications,
  découpage aux bords, absence de transfert à vide, seuils de repli et reprise
  après échec, avec le renderer logiciel SDL.

Ces validations automatiques ne remplacent pas une comparaison en partie
sur la même sauvegarde, notamment HUD transparent, zoom et curseur logiciel.
Aucun gain de FPS ni réduction du pic historique de 58 ms n'est annoncé.

Compilation Release réussie ; les **13 suites CTest** passent. Après le dernier
ajout de la clé du chemin de police des panneaux, recompilation et suite
`window_visibility` exécutées à nouveau.

Microbenchmark déterministe exécuté sur cette machine : **2 000 noms / 400
entrées**, ancien moteur **225 ms**, nouveau **28 ms**, avec résultats identiques.
Ce test mesure uniquement la recherche des mentions ; il exclut collecte des
objets, empreintes, pagination et affichage. Ce n'est pas un benchmark de FPS.

## Suite de l'audit

Mise à jour : les requêtes directes de pièges/feu et le pool des événements
sont maintenant appliqués et documentés dans
`docs/performance_queries_events_20261017.md` (deuxième lot).

Les autres listes de collision, les caches locaux de recherche et un changement
de la file du planificateur restent à examiner avec les nouvelles mesures.
L'éclairage, les évictions ART et le tri des objets restent également des pistes
à mesurer.
