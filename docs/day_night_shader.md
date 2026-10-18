# Premier shader jour/nuit

Prototype du 18 octobre 2026.

## Réglages

```ini
[video]
day_night_shader=1
day_night_strength=60
```

`day_night_shader=0` désactive le shader et l'interpolation supplémentaire de l'éclairage. `day_night_strength` règle l'intensité de la correction colorimétrique entre 0 et 100 ; il ne remplace pas le réglage `lighting` existant.

En jeu, **F6** bascule immédiatement entre le rendu original et le shader, sans modifier l'heure ni l'intensité. Un message traduit indique le nouvel état. L'état choisi est conservé dans `day_night_shader` lors de l'enregistrement des réglages. Le raccourci agit au relâchement de la touche et peut être changé ou désactivé avec `NONE` :

```ini
[bindings]
toggle_day_night_shader=F6
```

Le shader est neutre en journée, bleuté la nuit, chaud à l'aube et ambré au crépuscule. Les coefficients sont interpolés continûment selon l'heure du jeu, avec une actualisation toutes les dix secondes de jeu. Les couleurs ambiantes du moteur sont interpolées entre deux heures successives pour éviter les changements horaires brusques. La suspension de l'éclairage utilisée par le sommeil et les voyages reste respectée.

La correction concerne l'image du monde, avant les bulles, conversations et autres fenêtres d'interface. Il n'existe pas encore de masque de matériaux permettant d'exclure individuellement les lampes ou les intérieurs visibles dans une même vue : leur teinte est également corrigée.

## Implémentation

`src/game/day_night.c` calcule le profil. `light_scheme.c` le transmet au rendu et gère la progression de l'ambiance. `world_composition_present` appelle la correction après avoir restauré les régions du monde et avant la passe des textes. Le tampon propre du monde reste inchangé, évitant l'accumulation de corrections.

`first_party/tig/shaders/color_grade.hlsl` contient le fragment shader. Son bytecode DXIL est embarqué dans l'exécutable ; aucun compilateur ni fichier de shader externe n'est nécessaire sur la machine du joueur. Le prototype utilise Direct3D 12 sous Windows, via un renderer GPU SDL hors écran. Les autres plateformes et les GPU incompatibles conservent le rendu original pour cette correction.

Un autotest GPU sur quelques pixels précède toute modification de l'image du joueur. Si l'initialisation, l'autotest ou un transfert échoue, le rendu du monde demeure disponible sans shader. Les détails sont inscrits dans les journaux SDL.

Pour reconstruire le bytecode après une modification du HLSL :

```powershell
./tools/build_color_grade_shader.ps1
```

Le script utilise le DirectX Shader Compiler du SDK Windows. Les sémantiques d'entrée DXIL suivent le vertex shader réellement embarqué par SDL : couleur sur `TEXCOORD0`, coordonnées de texture sur `TEXCOORD1`.

## Limite de performances

L'interface est encore composée sur le CPU. Ce prototype transfère donc l'image vers le GPU puis récupère les régions traitées avant de dessiner les textes. Le transfert de retour impose une synchronisation.

Sur cette machine, le test complet 1280 × 720 mesure environ **9 à 10 ms par image**, transferts et récupération compris. Cette mesure isolée n'est pas le temps total d'une frame de jeu. Le coût dépend des régions modifiées, de la résolution et du GPU ; les passages neutres de journée n'exécutent pas le shader.

Ce prototype sert à valider l'aspect visuel. Pour généraliser les shaders à faible coût, il faudra composer également l'interface sur le GPU et supprimer la récupération intermédiaire vers le CPU.

## Validation

- Compilation Release x86 réussie.
- 27 suites CTest réussies, y compris le test GPU réellement exécuté sous Direct3D 12.
- La courbe est testée sur une journée entière, aux frontières de transition, au passage de minuit et avec une intensité nulle.
- Le test GPU contrôle la couleur obtenue et la conservation des pixels hors des régions du monde à traiter.
- L'autotest protège le rendu du jeu contre une incompatibilité de pipeline détectable par ces pixels de référence.

La validation visuelle en situation de jeu reste à effectuer.
