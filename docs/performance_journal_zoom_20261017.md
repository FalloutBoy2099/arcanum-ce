# Journal et copie zoomée — 17 octobre 2026

Lot guidé par la capture complète `792206787157`, conservée sous
`out/performance-baselines/measured-v2-20261017-792206787157`.

## Index du journal

`logbook_ui_scan_index_tab` distingue les textes nécessaires à l'index de leur
mise en page d'affichage. Pour Background, il lit directement la description
du personnage ; il ne déclenche plus les mesures successives de préfixes de
mots pour découper cette description en pages. Les autres rubriques conservent
leur collecte et leur formatage, avec leurs restrictions de visibilité.

Le même chemin est utilisé pour vérifier l'empreinte du cache et rechercher les
mentions. Un changement du texte complet invalide donc toujours les résultats.
Le fond de l'index n'est pas mis en cache globalement sans contrôle de validité.
La pagination de la rubrique lorsqu'elle est réellement affichée reste inchangée.

Lire le corps complet permet aussi de reconnaître un nom coupé entre deux pages
par l'affichage. Aucun catalogue de textes inconnus n'est ajouté à l'analyse.
Les tests couvrent les accents, une mention à une ancienne limite de page,
une mention au-delà de la taille usuelle d'un buffer d'entrée, ainsi qu'un nom
absent qui doit rester inconnu.

## Zoom

Le monde conserve son rendu partiel. Sa copie vers la fenêtre conserve désormais
une liste bornée des zones modifiées, qui comprend les rectangles écran des
textes superposés et les rectangles du monde transformés vers l'écran.
Le filtrage linéaire dispose d'une marge conservatrice autour des zones sources.

`tig_scale_region` garde la transformation complète et ne calcule que les pixels
des sous-régions. Il reproduit la convention de centres de pixels et les poids
fixes de SDL. Une petite calibration identifie l'arrondi de la bibliothèque SDL
installée ; une implémentation SSE2 est utilisée sur les processeurs compatibles,
avec un chemin scalaire et un repli sur SDL pour les cas non pris en charge.
Les surfaces à modulation, transparence ou color key passent par le repli.

La copie complète d'origine est conservée lorsque la caméra, le zoom, les
dimensions, les buffers ou le filtre changent, lorsque les buffers sont recréés,
ou à partir de 25 % de surface modifiée. Une erreur du chemin partiel provoque
également un repli complet. Aucune surface de taille écran n'est allouée par
copie partielle. Les rectangles voisins sont fusionnés et leur nombre est borné.

Les bulles, flottants et dialogues sont redessinés dans les mêmes régions
disjointes, après restauration du monde. Seules ces régions sont invalidées
pour le compositeur, avec prise en compte du décalage de la fenêtre isométrique.

Nouveaux compteurs : `zoom_scaled_pixels` et `zoom_full_copies`.
Le temps `zoom_blit` existant permet de comparer les captures avant/après.

## Vérification et suite

Compilation Release réussie ; **18 suites CTest passent**. Les deux suites
touchées ont été exécutées à nouveau après les dernières modifications :

- 320 transformations comparées pixel par pixel au rendu SDL complet, avec
  recadrage, agrandissement/réduction, filtrage nearest/linéaire et découpages
  sans alignement sur les pixels sources ;
- 240 modifications locales successives, avec comparaison complète de l'image,
  y compris aux bords du recadrage ;
- absence de travail sans changement, invalidation des transformations,
  seuil de repli complet et refus des effets non pris en charge ;
- régressions des mentions et empreintes du journal.

Le bénéfice en partie et la validité visuelle des superpositions ne sont pas
encore confirmés par une nouvelle capture. Le prochain passage doit utiliser
la même sauvegarde et le scénario v2 pour comparer notamment `journal_build`,
`text_measure`, `zoom_blit`, `zoom_scaled_pixels` et `texture_upload_bytes`.
Il faudra ensuite une scène en ville avec compagnons et une scène de combat
pour élargir la validation : le scénario automatique actuel reste hors combat.
