# Benchmark du 17 octobre 2026 — 1093352916283

Les seize phases sont terminées. Les entrées de comparaison, dont les quatre
points du trajet, sont compatibles avec la référence 1072040107236.
Capture et rapport : `out/performance-baselines/measured-v2-20261017-1093352916283/`.

Le maximum `frame_work` atteint 71,1282 ms pendant `zoom_075`, dont la moyenne
est de 17,1450 ms. Dans cette phase, `world_render` atteint 65,0891 ms et
`object_render` 54,1764 ms. Le chargement ART y reste sous 1,9471 ms.
Ces maxima ne prouvent pas qu'ils appartiennent au même appel ou à la même image.
La simulation culmine à 4,0726 ms ; les recherches de chemin dans `walk_1`
culminent à 0,0113 ms. Le ralentissement de rendu observé précédemment n'est
donc pas considéré résolu par les changements de décodage ART.

Des opérations ART lentes sont présentes ailleurs : chargement du schéma de
soins 11,6482 ms (métadonnées 9,4310 ms), ouverture de `i_coca_leaf.ART`
8,9447 ms, et blit de `drtgrs7b.art` 17,0713 ms. Les métriques sont inclusives
et ne doivent pas être additionnées. Les opérations de configuration des phases
peuvent précéder la mesure `frame_work`.

Le correctif d'anticipation et de déplacement latéral des compagnons a été
compilé après ce relevé ; l'exécutable archivé ne le contient pas.
