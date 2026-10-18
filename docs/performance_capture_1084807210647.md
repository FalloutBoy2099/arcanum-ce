# Benchmark du 17 octobre 2026 — 1084807210647

Les seize phases sont terminées. Capture et rapport détaillé :
`out/performance-baselines/measured-v2-20261017-1084807210647/`.

Le maximum de `frame_work` est de 26,3857 ms, pendant `zoom_075`.
Les maxima des autres phases sont inférieurs à 18 ms. Aucun chargement
ART de ce scénario n'atteint le seuil de 5 ms du fichier de diagnostic ;
le maximum mesuré de `art_load` est de 3,0801 ms.

La comparaison automatique avec le scénario 1072040107236 est refusée :
les quatre points du trajet diffèrent. Ces mesures décrivent donc cette
capture uniquement ; elles ne prouvent pas un gain relatif ni la disparition
définitive des ralentissements. Le cache du système de fichiers peut aussi
influencer les chargements. Les temps de présentation incluent la VSync.

Le correctif de chargement par double-clic a été compilé après cette capture
et n'est pas présent dans l'exécutable archivé avec elle.
