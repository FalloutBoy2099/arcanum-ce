# Affichage FPS et sortie automatique du combat

```ini
[global]
show_fps=1

[game]
auto_exit_combat=1
```

Les deux réglages sont désactivés par défaut dans le code (`0`). L'installation de test les active (`1`).

`show_fps` affiche les images présentées par seconde en haut à droite, en blanc sur fond noir, après le shader et l'interface. La mesure se renouvelle toutes les secondes. Le réglage est lu au démarrage ; il faut relancer le jeu après une modification. Il prend priorité sur le réglage historique `ShowFPS` de `HighRes/config.ini` lorsqu'il est présent. L'argument `-fps` reste disponible.

`auto_exit_combat` rengaine automatiquement le personnage et termine le combat au tour par tour via les fonctions habituelles du moteur. Le choix temps réel/tour par tour est conservé. La vérification s'effectue toutes les 250 ms, avec une confirmation de sécurité de 1,5 seconde.

Le contrôle porte sur le personnage et ses compagnons. Il utilise les règles existantes autorisant la sortie de combat et recherche des menaces dans un carré de 24 cases autour de chaque membre : PNJ encore en combat, ciblant le groupe, en fuite ou hostiles selon leurs drapeaux d'attaque à vue et leur faction. Il n'exécute pas de scripts d'hostilité pour cette vérification. Les participants au combat au tour par tour encore actifs empêchent également la sortie, même au-delà de ce rayon.

Les personnages morts ou inactifs, les objets désactivés et les PNJ ayant capitulé ne maintiennent pas le combat. Une attaque ou une autre animation de priorité élevée, le tour d'un autre personnage, le menu, un dialogue, une autre interface, la pause et les chargements suspendent la sortie automatique. Une nouvelle menace ou une suspension annule le délai de sécurité. Une réinitialisation du combat annule également ce délai.

La détection est volontairement prudente : un PNJ non allié combattant quelqu'un d'autre à proximité ou en fuite peut empêcher la sortie automatique. Le joueur peut toujours quitter manuellement le combat selon les règles habituelles.

Tests : lecture de `[global] show_fps`, priorité sur les anciennes sections, valeurs invalides, persistance des deux réglages, absence de menace, option désactivée, attaques en cours, menace visant un compagnon éloigné, tour d'un PNJ, participants encore engagés, réapparition d'une menace, interruption par le menu, morts et capitulation.
