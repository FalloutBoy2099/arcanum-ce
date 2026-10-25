# Adaptation x64, troisième bloc — 23 octobre 2026

## Stockage des propriétés et des objets

Les tableaux de propriétés `SizeableArray` conservaient leurs éléments juste après un en-tête de 12 octets. Les valeurs de 64 bits, pointeurs et identifiants d’objets pouvaient donc être mal alignés. Leur stockage commence désormais à un décalage de 16 octets, sur les deux architectures.

L’en-tête sérialisé reste composé des trois entiers historiques, soit 12 octets. Les écritures dans un fichier ou un tampon séparent explicitement cet en-tête et les éléments ; les quatre octets d’alignement internes ne sont jamais enregistrés. Une assertion de taille et un test avec des octets de référence protègent ce contrat. Les copies, insertions et suppressions utilisent le nouveau décalage en mémoire.

Le pool d’objets aligne également le début des objets et le pas entre deux emplacements sur huit octets. Aligner uniquement sur la taille d’un pointeur ne suffisait pas en x86 ; arrondir seulement l’en-tête ne suffisait pas non plus pour un emplacement dont la taille n’était pas un multiple de huit.

## Validation des handles

Les deux entrées de validation et de réservation vérifient maintenant l’index complet en 64 bits avant toute conversion vers `int`. Un handle avec des bits supplémentaires dans la partie haute ne peut plus se confondre avec un handle d’index inférieur après troncature. Les valeurs négatives sont également refusées par cette vérification. Les handles d’objets gardent leur représentation historique de 64 bits ; les handles TIG qui sont de simples indices restent en 32 bits.

Un test supplémentaire a reproduit un défaut dans la réservation d’un emplacement déjà libéré : le slot demandé restait dans la pile des indices libres tandis qu’un autre en était retiré. Des allocations ultérieures pouvaient alors réattribuer le même emplacement et invalider le premier objet. L’indice réservé est maintenant placé en tête de la pile avant sa consommation. Cette correction concerne `obj_handle_request`, dont aucun appel actif n’a été trouvé dans le code du jeu lors de cette inspection ; elle sécurise cette API sans revendiquer un changement observable immédiat en jeu.

Les pointeurs de propriétés transitoires étaient déjà stockés dans des `intptr_t`. Leur conservation est vérifiée avec une vraie allocation, y compris une adresse supérieure à 4 Go lors du test x64. Les conversions examinées pour les polices et la recherche de fichiers utilisent également des types adaptés à la taille d’un pointeur ; elles n’ont pas été remplacées par des entiers de 32 bits.

## Lectures interrompues et tailles

Les dimensions des tableaux lus depuis un fichier sont vérifiées avant multiplication et allocation : taille d’élément positive, nombre d’éléments non négatif, taille totale représentable et données disponibles. Les longueurs de bitsets négatives, démesurées ou tronquées sont refusées. Les allocations temporaires sont libérées sur échec.

Une lecture de remplacement échouée conserve maintenant le tableau précédent au lieu de le détruire avant de connaître le résultat. Réallouer un tableau existant libère également son ancien bitset.

## Vérification

- 53 suites CTest réussies en Release x64 et 53 en Release x86.
- Les tests des tableaux et du pool reproduisent les défauts d’alignement avec les sources précédentes. Le test du pool reproduit également l’acceptation d’un handle tronqué. Ces exécutables de référence restent uniquement sous `out/` et ne font pas partie de la compilation normale.
- Un test distinct reproduit la collision d’emplacements avant la correction de réservation ; il passe après correction.
- Vérification des tableaux de tailles d’éléments 4, 8, 12, 16 et 24 octets : insertion, copie indépendante, suppression, lecture, énumération et alignement.
- Comparaison exacte avec un tampon sérialisé de référence de 44 octets, en lecture et écriture fichier et mémoire, avec valeurs de 64 bits et clés éparses. Le même tampon est utilisé sur les deux architectures.
- Test de pointeurs natifs, handles dépassant 32 bits, handles périmés, bits hauts invalides, réservations et 8 193 allocations traversant une limite de bucket.
- Dans une copie isolée du jeu, chargement de `Slot0001` et `slotNAPS`, enregistrement d’un slot de test puis rechargement : succès dans chaque architecture. Les écritures ne visent que le slot `Storage64Probe` du dossier isolé.
- Vérification croisée : chargement x64 d’une sauvegarde écrite en x86 puis chargement x86 d’une sauvegarde écrite en x64, avec réenregistrement et rechargement réussis.
- Sources temporaires de sondage retirées ; source `main.c` restaurée octet par octet, puis recompilation forcée des exécutables normaux.

Les résultats, sources avant changement et journaux se trouvent dans `out/x64-adaptation-stage3-20261023/`. Ces essais vérifient le stockage et la compatibilité ; ils ne constituent pas une mesure de gain de FPS.

## Livraison et suite

La version de test `C:\Games\Arcanum\arcanum-ce-x64.exe` est actualisée après validation. Le précédent exécutable x64 est sauvegardé avec un manifeste SHA-256. L’exécutable principal 32 bits et l’INI installé restent inchangés.

Ce bloc ne transforme pas tous les lecteurs mémoire historiques en lecteurs bornés : plusieurs interfaces ne reçoivent encore aucune longueur de tampon. Les capacités et tailles sérialisées de ce stockage restent des entiers de 32 bits, avec contrôles explicites sur les chemins de lecture traités. La prochaine étape peut porter sur les tampons de sérialisation, leurs additions de tailles et ces lectures mémoire, puis sur les allocations graphiques restantes.
