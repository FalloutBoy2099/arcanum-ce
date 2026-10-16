# Lot 4 : états et transitions des interfaces

## Première étape : navigation

La pile des modes de jeu est extraite dans `ui/intgame_mode.c`. Le module ne
dépend ni de TIG, ni du moteur, ni des ressources graphiques. Il prépare une
transition, publie son mode après les callbacks existants, puis libère la garde
de réentrance après le rafraîchissement du curseur.

Le mode MAIN garde son sens historique : revenir d'un niveau. Les fermetures
récursives déclenchées par les callbacks sont acquittées sans modifier la pile.
Les valeurs numériques des modes et l'ordre des effets dans `intgame_mode_set`
restent identiques. Les anciens booléens locaux ont des noms décrivant leur
rôle : annulation du glisser-déposer, retour au parent, dépilement du ciblage et
entrée dans un écran enfant.

La capacité historique de 11 entrées inclut MAIN. Une demande hors domaine ou
un empilement au-delà de la capacité renvoie false avec un diagnostic, avant
les callbacks de fermeture/ouverture. Le contrôle du défilement refuse aussi
les valeurs hors domaine au lieu de lire hors du tableau.

L'historique des menus est extrait dans `ui/mainmenu_history.c`, également sans
dépendance graphique. Le retour retire l'écran courant et le parent ; la
réouverture du parent le réinscrit, comme auparavant. Les redirections Intro
et Credits utilisent une opération explicite de retrait du courant, au lieu
d'incrémenter artificiellement le compteur puis de dépiler.

La limite historique de 50 écrans est contrôlée. Un enregistrement impossible
ou une route préchargée invalide est refusé avec un diagnostic ; l'historique
existant reste intact. L'écran déjà créé peut rester affiché si son
enregistrement échoue : ce cas extrême ne devient pas une nouvelle boîte de
dialogue et ne provoque plus d'écriture hors limites.

Les trois séquences identiques de fermeture des panneaux avant Options, Load
Game et reset passent par `mainmenu_ui_close_game_panels`. L'ordre des appels
reste identique, y compris la fermeture de la liste des points de destin.
Les noms historiques des points d'entrée sont conservés.

## Validation

Compilation Release x86 et huit suites CTest réussies. La nouvelle suite
`ui_mode` couvre :

- Les 9 261 combinaisons de trois demandes parmi les 21 modes, suivies de
  retours, comparées aux règles de pile historiques.
- Dialogue → commerce → quantité et les retours vers chaque parent.
- Inventaire → objet, sort ou compétence, puis les retours.
- Les appels récursifs pendant les callbacks et après publication du mode.
- La pile pleine, les modes invalides, le retour à vide et le reset.
- Les 324 couples d'écrans des menus, le raccourci direct des options, les
  redirections, les routes préchargées invalides et les limites de capacité.

Ces tests portent sur les décisions de navigation. Ils ne simulent pas les
callbacks graphiques complets. Vérification en jeu encore nécessaire : ouvrir
et fermer inventaire, fiche, journal et schémas ; ouvrir Options et revenir ;
ouvrir la liste des points de destin puis le menu ; vérifier les retours d'un
dialogue avec commerce et quantité, puis charger une sauvegarde.

Aucune donnée de navigation nouvelle n'est écrite dans les sauvegardes. Le
scénario automatique version 1 et les réglages du joueur restent inchangés.
L'autorun reste désactivé. Aucun gain de FPS n'est attendu de cette extraction.

## Suite du lot

Les callbacks d'entrée/sortie et le dessin restent dans les modules d'origine.
Cette étape prépare leur séparation ; elle ne termine pas la migration de tous
les écrans. Les états du journal et la collecte de ses rubriques sont les
prochains candidats, avant d'aborder le catalogue des réglages du lot 5.
