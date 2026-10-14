# Améliorations communautaires intégrées

Cette intégration reprend les améliorations de caméra, zoom, dialogues et interface
du fork [sevdestruct/arcanum-ce](https://github.com/sevdestruct/arcanum-ce), branche
`feature/ui-improvements`, et les sauvegardes automatiques inspirées du fork
[hogandhound/arcanum-ce](https://github.com/hogandhound/arcanum-ce).
Les adaptations conservent les polices SDL_ttf, les marges de dialogue, les dix
réponses possibles et le déplacement par glissement du bouton droit de ce dépôt.
Les fonctionnalités expérimentales de réattribution des points, brouillard de
perception et multijoueur sont exclues.

## Utilisation

- **Zoom du jeu** : Ctrl + molette, `=` / `-`, ou `+` / `-` du pavé numérique.
  `0` du pavé numérique rétablit le zoom à 100 %. Les objets et les textes flottants
  restent sélectionnables et lisibles aux différents niveaux de zoom.
- **Interface** : Tab parcourt les quatre modes complet, moyen, mini et masqué.
  Les panneaux ouverts temporairement restent accessibles.
- **Dialogues** : les réponses portent un numéro. Les touches 1 à 9 sélectionnent
  les neuf premières réponses, 0 la dixième. Les passages entre crochets peuvent
  être assombris, sans assombrir la réponse survolée.
- **Sauvegardes** : `Autosave - Travel` après les transitions entre cartes et
  `Autosave - Sleep` au réveil. Chaque type remplace son propre emplacement
  (`slotAUTO` et `slotNAPS`) et apparaît dans la liste de chargement.
- **Caméra** : suivi progressif facultatif, cadrage des interlocuteurs et retour
  progressif facultatif après le dialogue. Le déplacement manuel suspend
  temporairement le suivi pour permettre de regarder ailleurs.
- **Vidéo** : VSync désactivée, activée ou adaptative ; filtrage du zoom lisse ou
  par pixels. La VSync adaptative revient au mode activé si le pilote la refuse.

## Configuration

Dans `[video]`, `scroll_using_borders=1` active le défilement aux bords de la
fenêtre (valeur par défaut). `0` le désactive ; le clavier et le glissement au
bouton droit restent disponibles.

Le fichier [community-options.ini](community-options.ini) décrit toutes les nouvelles
clés et leurs valeurs par défaut. Ajouter les clés aux sections correspondantes du
`arcanum.ini` situé dans le répertoire de jeu, puis relancer le jeu. Le moteur
conserve ces clés lors de la sauvegarde des options sans ajouter de commentaires.
La référence complète en anglais est dans [arcanum_ini.md](../arcanum_ini.md).
Les réglages précédents restent utilisables.

Le suivi automatique reste désactivé par défaut : définir
`camera_follows_player=1` dans `[video]` pour l'activer. Le zoom, les réponses numérotées,
la transparence et les sauvegardes automatiques sont activés par défaut.

## Vérifications

La cible CMake facultative `ARCANUM_BUILD_TESTS=ON` ajoute le test
`community_features`, exécutable via CTest. Il contrôle la lecture et la réécriture
de l'INI, les raccourcis par défaut et personnalisés, les limites et la désactivation
du zoom, ainsi que l'assombrissement des crochets avec une police TrueType sur une
ligne et avec retour à la ligne. `ARCANUM_TEST_FONT` doit pointer vers une police
présente sur la machine pour les vérifications du rendu.

Ces tests ne remplacent pas la vérification des transitions, sauvegardes et
interactions dans une partie réelle, ni les essais des pilotes vidéo Linux/macOS.
