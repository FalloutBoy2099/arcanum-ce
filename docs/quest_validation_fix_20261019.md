# Validation des quêtes — 19 octobre 2026

Correction du point F06 de l’audit.

## Chargement des définitions

Le parseur de `rules/gamequest.mes` exige exactement 23 entiers : niveau de récompense, ajustement d’alignement et trois tableaux de sept dialogues. Il accepte les espaces, tabulations, retours à la ligne et signes numériques valides. Il refuse les champs manquants, supplémentaires, non numériques ou hors plage entière, les niveaux de récompense négatifs et les indices de dialogue inférieurs à -1. Le niveau zéro et la sentinelle -1 restent acceptés.

Les identifiants doivent être compris entre 1000 et 1999. Les définitions sont préparées dans une table temporaire ; une erreur empêche la publication de l’ensemble et fait échouer le chargement du module, avec un diagnostic donnant fichier et identifiant. L’absence de ce fichier facultatif reste autorisée. Les valeurs par défaut sont réinitialisées, y compris l’alignement.

## Accès et sauvegardes

Les accès aux quêtes vérifient la disponibilité des tableaux et la plage d’identifiants avant tout calcul d’indice. Les états demandés sont bornés à l’énumération ; les états individuels corrompus sont exposés comme inconnus. Les récompenses ne sont pas attribuées pour un identifiant ou un état invalide. Les descriptions sont vidées en l’absence de donnée exploitable, et leurs références sont retirées lors du déchargement des catalogues.

Le journal et la copie des quêtes parcourent uniquement 1000–1999. Le marqueur de quête ratée conserve sa sémantique et peut être retiré normalement ; un état corrompu ne permet pas de modifier l’état global par cette voie.

Le chargement des états globaux utilise un tampon temporaire et refuse les états hors de l’énumération ou une lecture incomplète. Les états 0–6 sont conservés pour compatibilité. Les formats `.mes`, prototypes et sauvegardes ne changent pas. Les valeurs de récompense XP invalides donnent zéro ; la somme d’alignement utilise un intermédiaire 64 bits.

## Validation

`quest_validation_test` inclut le véritable `quest.c` et simule les catalogues, objets et entrées/sorties : définitions incomplètes ou invalides, publication atomique, bornes 1000/1999, identifiants extrêmes, champs signés, espaces variés, états corrompus, descriptions normales et faible intelligence, récompense unique, journal, copie entre personnages, annulation d’échec et chargement tronqué ou corrompu.

Compilation Release Win32 réussie et 42 suites CTest réussies. La validation interactive dans une partie reste à faire. Aucun changement de sauvegarde, de prototypes ou de configuration utilisateur.
