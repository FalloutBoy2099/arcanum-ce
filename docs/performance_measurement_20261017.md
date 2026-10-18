# Préparation des nouvelles mesures — 17 octobre 2026

Après les trois lots d'optimisation, la suite dépend d'un relevé en partie.
Les dernières captures disponibles sont celles du 16 octobre.

## Corrections du benchmark

- Les empreintes de fichiers de sauvegarde utilisaient `fopen` au lieu du
  système de fichiers virtuel TIG. Les fichiers résident dans le module monté,
  pas nécessairement dans le dossier de l'exécutable : les anciens relevés
  signalaient donc `missing` pour tous les fichiers contrôlés.
- La lecture utilise maintenant TIG, via `benchmark_fingerprint`, avec CRC32,
  longueur et distinction entre fichier absent et erreur de lecture.
- Le scénario est versionné **2**. Ses treize phases initiales sont conservées,
  suivies de trois secondes consultations du journal et des schématiques.
  Ce sont des consultations répétées, sans promesse que tous les caches
  restent chauds après fermeture. Les compteurs indiquent les hits réels.

## Rapport

`tools/benchmark_report.py` lit les trois fichiers de capture. Il sélectionne
un scénario terminé, rattache les mesures par session et intervalles et refuse
les phases manquantes, répétées, en échec ou qui se chevauchent. Il contrôle
aussi les mesures absentes, dupliquées ou numériques invalides.

Le rapport produit Markdown et JSON ; les moyennes sont pondérées par le nombre
d'appels. Aucun percentile global n'est inventé à partir des percentiles
d'intervalles. Les comparaisons automatiques exigent des métadonnées compatibles,
y compris une empreinte `data.sav` lisible. Les anciens relevés restent utiles
à titre indicatif, mais leur identité de sauvegarde ne peut pas être certifiée
rétrospectivement. La machine, l'exécutable et les ressources externes doivent
également être conservés avec les captures.

## Vérification

Compilation Release et **17 suites CTest réussies**. Les nouveaux tests couvrent
la lecture d'une sauvegarde dans un dépôt TIG situé hors du répertoire courant,
les deux séparateurs de chemin, les fichiers vides/absents et les lectures en
plusieurs blocs. Huit tests Python couvrent l'analyse des sessions, les moyennes
pondérées, les scénarios interrompus/incomplets et les comparaisons incompatibles.

Un rapport des anciennes captures a été généré sous
`out/performance-baselines/legacy-reference-report.md`, avec son JSON.
Les mesures v2 en partie ne sont pas encore disponibles à cette étape.

Pour la prochaine capture, lancer le jeu et charger la même sauvegarde, avec
un personnage conscient hors combat dans une zone dégagée. Le scénario dure
environ 40 secondes à 60 Hz. Garder le jeu au premier plan ; Échap l'interrompt.
Il faut recharger la sauvegarde d'origine avant de reprendre une partie normale,
car le scénario déplace le personnage et laisse tourner la simulation.

Voir `automatic_benchmark.md` pour les fichiers, les limites et les commandes.

## Premier essai en partie

Le premier lancement v2 a signalé `route_unavailable` avant de commencer la
capture. Aucun nouveau scénario terminé n'a donc été enregistré. La sélection
de parcours a été étendue : carré de quatre cases, puis deux cases, puis
aller-retour en corridor. Les quatre destinations exactes restent enregistrées
et doivent correspondre pour comparer des captures.

Un échec de sélection ne condamne plus tout le processus : le conducteur laisse
les commandes normales actives et réessaie au bout de trois secondes. Un scénario
commencé, terminé ou interrompu conserve la règle d'une seule exécution.
Les tests ajoutés couvrent une petite clairière, un corridor, l'absence de
parcours et la disparition d'un obstacle. Les 17 suites passent.

## Capture v2 terminée

Le scénario `792206787157` a terminé les 16 phases avec `finished`.
Les fichiers `data.sav` et `data2.sav` possèdent désormais leurs CRC32/longueurs.
La capture, l'INI utilisé, l'exécutable et les rapports sont conservés dans
`out/performance-baselines/measured-v2-20261017-792206787157`.
SHA256 de l'exécutable :
`8F1CCD4BAADAB7AA49FA7119EE3D5EB519944B2A76255BEB40C69740372DA2A0`.
Le jeu étant fermé après la capture, `benchmark_autorun` a été remis à zéro.

Principales observations de cette scène (un seul passage, pas une preuve
statistique de gains avant/après) :

- Index du journal : premier appel **67,09 ms** ; deux appels dans la seconde
  consultation, moyenne **56,86 ms**, malgré deux accès valides au cache.
  Les mesures de texte cumulent **61,24 ms** dans la première phase et
  **112,91 ms** dans la seconde, contre **113,72 ms** cumulées de construction
  d'index dans cette seconde phase. Ces métriques sont inclusives et mesurent
  toute la phase : elles ne démontrent pas seules l'imbrication de chaque appel.
- La lecture du code fournit une piste précise : pour examiner les textes
  visibles de chaque rubrique, l'index appelle `logbook_ui_load_data` ; la
  rubrique Background découpe alors sa description en pages avec des mesures
  répétées à chaque limite de mot. Cette pagination sert à l'affichage, alors
  que l'index recherche seulement les noms dans le texte. Éviter cette mise
  en page pendant l'analyse de l'index est la prochaine correction à valider.
- Recherches de chemin pendant les quatre déplacements : moyennes d'environ
  **0,005 à 0,008 ms** ; le plus long appel est **0,020 ms**. Les tris graphiques
  sont autour de **0,001 ms**. Ce ne sont pas les principaux coûts de cette scène.
- Zoom 0,75 : copie mise à l'échelle **1,92 ms** en moyenne ; rendu du monde
  **1,48 ms**, avec un pic à **41,91 ms**. Cette phase reste une piste de rendu.
- Repos avec HUD opaque : **105 600 octets** transférés vers la texture sur
  180 présentations ; transfert moyen **0,0024 ms**. Les 16,47 ms de présentation
  incluent la VSync et ne doivent pas être interprétées comme du calcul CPU.

Les anciennes captures ne permettent pas une comparaison certifiée de sauvegarde
et la version du scénario a changé. Aucun pourcentage de gain global n'est déduit.
