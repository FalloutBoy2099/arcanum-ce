# Budgets de cache adaptés à la mémoire disponible

Le 24 octobre 2026, la version x64 de test a été installée dans `C:\Games\Arcanum\arcanum-ce-x64.exe`. L'exécutable principal x86 et le fichier `arcanum.ini` sont inchangés. La sauvegarde de l'exécutable précédent et les empreintes figurent dans `out/adaptive-cache-budget-20261024/install-20261024-132530/manifest.json`.

## Fonctionnement

Un budget commun répartit la mémoire entre les images ART décodées, leurs buffers, le cache des fichiers préchargés et les lectures asynchrones. Les données sont allouées à la demande, sans réserver immédiatement toute l'enveloppe.

- x64 : minimum entre un quart de la RAM totale, la moitié de la RAM disponible et 8 Gio.
- x86 : minimum entre un quart de la RAM totale, un quart de la RAM disponible et 768 Mio.
- L'enveloppe est calculée à la première initialisation ; elle ne diminue pas automatiquement si d'autres applications consomment ensuite davantage de mémoire.
- Les métadonnées et les instantanés des sources du préchargement sont comptabilisés. Les entrées épinglées restent protégées contre l'éviction.
- Les sources de fichiers sont désormais allouées seulement pendant leur préparation, au lieu de réserver 64 descriptions de sources dans chaque entrée.
- Le cache de fichiers dispose au maximum de 8192 entrées en x64 et 2048 en x86. Les compteurs exposent les octets résidents, le maximum atteint et les évictions.

Cette enveloppe ne constitue pas une limite globale de mémoire du processus : les autres sous-systèmes, certaines allocations temporaires et la mémoire GPU ne sont pas tous inclus. Les seuils ART restent des objectifs de cache. Le cache des secteurs reste à 16 : augmenter ce nombre nécessite d'auditer les parcours des secteurs qui participent à la simulation.

Aucun nouveau réglage INI et aucun changement de format de données ne sont nécessaires.

## Validation

- 54 suites réussies en x64 et 54 en x86, respectivement 17,63 et 17,45 secondes.
- Tests de la politique mémoire, de ses limites et de la comptabilité ; changement de configuration interdit pendant l'activité ; conservation des entrées épinglées.
- Un scénario déterministe de huit entrées de 512 Kio conserve 3/8 entrées avec un cache de 2 Mio et 8/8 avec 8 Mio. Ce scénario utilise des alias de chemin d'un même fichier et ne mesure pas les performances en jeu.
- Chargement isolé d'une sauvegarde à Tarant et 48 images rendues à plusieurs niveaux de zoom sur les deux architectures. Les captures ont 1811 couleurs distinctes dans la zone centrale ; la capture x64 a également été inspectée visuellement.
- Les exécutables normaux ont été reconstruits après la sonde ; `src/main.c` est identique à sa copie avant instrumentation et les exécutables ne contiennent plus le marqueur de la sonde.

Sur la capture finale x64, la machine disposait d'environ 3,12 Go de RAM libre sur 16,48 Go. L'enveloppe calculée était de 1 561 470 976 octets : ART système 863 688 633, buffers 575 792 423, préchargement 97 591 936 et lecteur 24 397 984. Le préchauffage a préparé 62 images ART en 48 ms ; le cache de fichiers a atteint 12 525 410 octets sans éviction.

Une première itération du nouveau code limitait trop fortement le nombre d'entrées et produisait 963 évictions pendant la sonde, malgré une occupation bien inférieure au budget en octets. La capacité a été corrigée. Les conditions de mémoire disponible différaient entre ces captures ; elles ne constituent pas une comparaison de vitesse avec l'ancienne version installée.

Les bénéfices attendus concernent surtout la réutilisation des ressources et la réduction des relectures. Aucun gain de FPS ou de temps de chargement réel n'est encore établi : il reste à comparer des parcours et chargements identiques en jeu.

Les journaux, captures, résultats et empreintes sont conservés dans `out/adaptive-cache-budget-20261024/`.
