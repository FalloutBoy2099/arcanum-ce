# Benchmark du 17 octobre 2026 — 1105553876777

Les seize phases sont terminées et la sauvegarde de départ est vérifiée.
Capture, exécutable et rapport détaillé archivés dans
`out/performance-baselines/measured-v2-20261017-1105553876777/`.
Le comportement des compagnons de cette version a été validé en jeu par
l'utilisateur avant ce relevé.

Le maximum `frame_work` de toutes les phases est de 17,7991 ms, pendant
`zoom_075`. Dans cette phase, la moyenne est de 16,6178 ms et le maximum de
`object_render` de 11,4699 ms. Aucun chargement ART n'est observé pendant le
zoom ; 37 chargements sont mesurés sur l'ensemble du scénario. Aucun événement
ART détaillé n'atteint le seuil de diagnostic de 5 ms. Le maximum des recherches
de chemin dans les quatre phases de marche est de 0,0111 ms.

Le pic de 71,1282 ms du relevé précédent ne se reproduit pas dans cette capture.
Cela ne démontre pas sa correction : les chargements/cache et l'état de la
session peuvent influencer la mesure. Les temps d'image incluent la VSync.

La comparaison automatique avec 1093352916283 est refusée : le nombre de
conversations enregistrées passe de 0 à 8. Le trajet et la sauvegarde vérifiée
correspondent, mais la comparaison complète des phases du journal ne satisfait
pas les critères du rapport. Aucun pourcentage de gain n'est donc annoncé.
