# Comparaison des chargements x86 et x64 — 23 octobre 2026

## Résultat

Sur ce scénario, les médianes donnent un léger avantage à la version x64 : environ 9 % de durée en moins au démarrage et 9–10 % pour le chargement des deux sauvegardes. Les mesures ne montrent pas de ralentissement systématique du x64.

| Mesure | Médiane x86 | Médiane x64 | Réduction de durée x64 |
|---|---:|---:|---:|
| Démarrage avant introduction/menu | 3.347 s | 3.047 s | 9.0 % |
| Chargement Slot0001 | 1.021 s | 0.918 s | 10.1 % |
| Chargement slotNAPS | 1.099 s | 1.004 s | 8.7 % |

## Protocole

- Même code moteur, compilations Release x86 et x64, avec la même sonde temporaire dans les deux versions.
- Même dossier de travail de test, mêmes fichiers de données, même INI et copies de Slot0001 et slotNAPS.
- Son désactivé dans les deux versions pour comparer les étapes du moteur sans initialisation audio.
- Deux passages de chauffe, un par architecture, exclus selon le protocole fixé avant les mesures.
- Quatre chargements mesurés par architecture et par sauvegarde, soit seize passages.
- Ordre alterné et équilibré pour limiter les effets d’ordre et du cache système. Aucun vidage forcé des caches Windows.
- Démarrage mesuré depuis l’entrée dans main jusqu’à la fin du préchargement ART commun. Le chargement est mesuré autour de gamelib_load, après la réinitialisation chronométrée séparément.
- Les temps de chargement incluent les données du jeu, l’interface et les recalculs finaux. L’introduction, l’attente dans les menus et la première image de reprise ne sont pas chronométrées.
- Les dix-huit chargements, chauffe comprise, réussissent.

## Dispersion

| Mesure | Étendue x86 | Étendue x64 |
|---|---:|---:|
| startup | 3.030–3.436 s | 2.999–3.160 s |
| Slot0001 | 1.014–1.031 s | 0.895–0.928 s |
| slotNAPS | 1.090–1.119 s | 0.988–1.789 s |

Un chargement x64 de slotNAPS prend 1,789 s, contre environ 0,99–1,02 s pour les trois autres. Il est conservé dans les résultats. Les médianes évitent de faire dépendre le bilan de ce seul passage.

Le premier passage de chauffe x86 est beaucoup plus lent : 14,58 s au démarrage et 15,11 s au chargement. Le passage de chauffe x64 suivant prend 2,20 s et 0,93 s. Leur ordre et les caches ne permettent pas d’attribuer cette différence à l’architecture. Ils ne sont pas inclus dans le tableau comparatif.

## Limites

La machine n’était pas au repos : l’utilisation CPU globale moyenne par passage varie de 33,6 % à 42,9 %. Une estimation de la part des autres processus est enregistrée dans les mesures, à partir du temps CPU du processus testé normalisé par les seize processeurs logiques. Cette estimation est approximative et ne mesure pas la concurrence disque.

Ce résultat est indicatif. Il ne constitue pas un gain garanti de 9–10 %, ni une comparaison des FPS ou de la fluidité en jeu. Un passage en 64 bits ne prouve pas à lui seul la cause des écarts observés.

## Fichiers et état final

Les relevés bruts, journaux de chaque passage, sonde exécutable et script de répétition se trouvent dans `out/architecture-benchmark-20261023/`. Les médianes et étendues sont calculées à partir de `measurements.json`, également exporté en CSV.

La source main.c est restaurée octet par octet et les exécutables normaux des deux cibles sont reconstruits. Les exécutables installés x86 et x64 ainsi que l’INI sont inchangés, vérifiés par SHA-256. Les sauvegardes d’origine ne sont pas utilisées comme destination de travail.
