# Premier shader d'eau

Prototype du 18 octobre 2026.

```ini
[video]
water_shader=1
water_strength=45

[bindings]
toggle_water_shader=F9
```

`water_shader` active l'effet ; il vaut `0` par défaut dans le code et `1` dans l'installation de test. `water_strength` règle son intensité entre 0 et 100. F9 bascule immédiatement l'effet et affiche un message traduit. F6 reste réservé au jour/nuit.

L'effet combine une faible réfraction de la texture existante et une variation animée des reflets. Il conserve la couleur de l'eau originale. Les coordonnées des vagues sont attachées au monde pour éviter qu'elles suivent la caméra. Il fonctionne avant le dessin des objets, des personnages, des toits et de l'interface, puis le shader jour/nuit peut teinter le résultat normalement.

La détection s'appuie sur les métadonnées de `art/tile/tilename.mes` : son de surface `TILE_SOUND_WATER`. Les ressources installées décrivent ainsi l'eau des marais, plusieurs eaux peu profondes, l'eau profonde et les égouts. Une tuile utilisant réellement une transition entre deux matériaux est exclue de ce premier essai. Ce choix évite de déformer une rive contenant de la terre ; certaines eaux de transition ou des décors représentant de l'eau peuvent donc rester statiques.

Pendant le dessin du sol, les tuiles aquatiques produisent un masque isométrique limité aux régions fraîchement redessinées. Le masque passe au shader via le canal alpha d'une texture intermédiaire. La réfraction vérifie également le masque au point échantillonné afin d'éviter de tirer une couleur de terre dans l'eau.

L'animation demande une actualisation du monde toutes les 50 ms lorsqu'une surface d'eau a été observée récemment. Le menu et les pauses suspendent ces demandes. Les tampons GPU de l'eau et de la correction colorimétrique sont conservés séparément pour éviter leur recréation à chaque image, notamment avec le zoom. La passe d'eau recadre également les transferts à la région demandée plutôt qu'au tampon complet réservé au zoom.

Il s'agit encore d'un prototype avec transfert GPU puis récupération CPU, pas d'un rendu entièrement GPU. Il ajoute un coût lorsque l'eau est visible et doit être comparé avec F9 et le compteur FPS. Il ne calcule pas les reflets des bâtiments, de la physique des vagues ou les interactions avec les personnages. Si la passe GPU échoue, l'eau originale reste affichée et les demandes d'animation cessent.

Sur cette machine, le test isolé d'une surface 1280 × 720 mesure environ 15 ms par passe d'eau, transferts compris. Ce n'est pas le temps complet d'une image de jeu ; la correction jour/nuit constitue une autre passe lorsqu'elle est active.

La compilation HLSL et l'embarquement du bytecode utilisent `tools/build_color_grade_shader.ps1`, commun au jour/nuit et à l'eau.

Le test GPU vérifie l'animation dans le masque, la conservation exacte des pixels hors du masque, une intensité nulle et les régions de mise à jour partielles. Le test jour/nuit contrôle aussi le shader partagé. Le rendu artistique sur les quais de Tarant reste à valider en jeu.
