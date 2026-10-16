# Revue des anomalies du moteur — 15 octobre 2026

Première revue transversale du code local : ordres et IA des compagnons,
sauvegardes, interface/HUD, raccourcis, scripts, ressources de terrain et zones.
Les constats ci-dessous viennent de la lecture du code et des tests indiqués.
Il ne s’agit pas d’une garantie d’absence de bugs dans l’ensemble du moteur.

## Anomalies corrigées

| Sujet | Défaut et correction | Code |
| --- | --- | --- |
| Ordres collectifs par raccourci | Les raccourcis interprétaient les traductions de broadcast.mes. La commande française 505 « Suivez-moi » ne correspondait à aucun ordre reconnu. Ils utilisent maintenant des identifiants ; la sixième conserve le comportement Move du moteur anglais. | [broadcast.c:565](../src/game/broadcast.c#L565) |
| Copie des textes des raccourcis | Un texte mis en cache sur 1 000 octets était copié sans limite dans un Broadcast de 128 octets. Les raccourcis ne font plus cette copie : les textes servent uniquement au retour visuel. | [broadcast.c:40](../src/game/broadcast.c#L40) |
| Suppression de la sauvegarde rapide | La protection dépendait du titre anglais Auto-Save. Elle identifie maintenant le slot interne SlotAuto, quel que soit le titre ou la langue. | [gamelib.c:1702](../src/game/gamelib.c#L1702) |
| Tri des sauvegardes automatiques | Le comparateur pouvait renvoyer -1 dans les deux sens, et même pour une entrée comparée à elle-même. Il compare maintenant la priorité du slot puis la date, conformément aux règles de qsort. | [gamelib.c:1899](../src/game/gamelib.c#L1899) |
| Attente des compagnons | Le minuteur NPC Wait Here ne renseignait pas son objet et pouvait traiter une référence indéterminée. L’événement est initialisé et reçoit le PNJ. Les cas sans chef et les indices négatifs sont aussi protégés. | [ai.c:1457](../src/game/ai.c#L1457) |
| Historique des actions : référence d’objet | La nouvelle référence était écrite dans la deuxième action et écrasait celle qui venait d’y être transférée. Elle est maintenant écrite dans la première. | [hotkey_ui.c:1097](../src/ui/hotkey_ui.c#L1097) |
| Historique des actions : icônes | Les objets recevaient un ART invalide. À la réouverture du HUD, objets et sorts d’objets pouvaient être traités comme des compétences. Le choix de l’icône utilise maintenant le type réel. | [hotkey_ui.c:144](../src/ui/hotkey_ui.c#L144) |
| Pile de polices du HUD compact | Un texte vide quittait la fonction après tig_font_push sans tig_font_pop. Le retour anticipé équilibre maintenant la pile. | [intgame.c:4521](../src/ui/intgame.c#L4521) |
| Zones de la carte | Les fonctions de décalage et de découverte indexaient leurs tableaux sans vérifier la zone. Elles refusent maintenant les indices négatifs ou hors limites. | [area.c:385](../src/game/area.c#L385) |
| Chargement des noms de portes | Une ligne sans espace dans portal.mes provoquait le déréférencement d’un pointeur NULL. Une ligne sans suffixe est maintenant conservée sans cette écriture. | [a_name.c:1187](../src/game/a_name.c#L1187) |
| Lecture des transitions de terrain | Sur un nom de terrain inconnu, la fonction laissait la chaîne tronquée. Elle restaure maintenant le séparateur avant de signaler l’erreur. | [terrain.c:37](../src/game/terrain.c#L37) |
| Callback d’interface | sub_460BC0 vérifiait field_11C puis appelait field_120. Cela pouvait appeler NULL ou ignorer une fonction valide. La vérification porte maintenant sur la fonction appelée. | [ui.c:606](../src/game/ui.c#L606) |
| Scripts appliqués au groupe | Après 100 compagnons, le chef était écrit dans la 101e case du tampon. Une case est maintenant réservée au chef : au maximum 99 compagnons puis le chef. | [script.c:3074](../src/game/script.c#L3074) |
| Ponctuation des titres de journaux flottants | La vérification lisait le caractère nul de fin au lieu du dernier caractère du titre, ajoutant systématiquement un point. Elle vérifie maintenant str[len - 1]. | [script.c:2284](../src/game/script.c#L2284) |

## Seconde revue des sept points — 15 octobre 2026

Les sept points ont été examinés : six sujets ont reçu une correction ; le
septième (drapeau des scripts de secteur) est nécessaire au format de sauvegarde
et a été conservé avec un test de non-régression.

| Sujet | Résultat | Vérification |
| --- | --- | --- |
| Création de terrain | Fonction manquante reconstruite depuis Arcanum.exe. Le champ du deuxième terrain est aussi corrigé : décalage de 6 bits au lieu de 4. Masques pleins/vides et variantes respectent le moteur original. | Désassemblage local en lecture seule aux adresses 0x4E8CE0–0x4E8DF7 ; test des 65 536 combinaisons de champs et banc du générateur avec RNG contrôlé. |
| Titre de journal flottant : cible | Utilisation du premier et unique opérande comme personnage cible. | semes/action.mes, entrée 86 : « newspaper: float current headline over (obj) ». |
| Demandes répétées de course | Suppression des écritures inconditionnelles et des conditions inaccessibles. Test du bit 0x40 et de la charge du personnage, comme dans anim_goal_run_near_tile. | Lecture des chemins locaux et hôte réseau, comparaison avec turn_on_running ; compilation. La course avec différents niveaux de charge reste à vérifier en jeu. |
| Confirmation de sortie | Remplacement des valeurs 3 et 1 par le véritable handle du bouton, aussi bien pour masquer le bouton que pour le transmettre au gestionnaire de retour. | Vérification des deux variantes du menu et du gestionnaire sub_549310 ; compilation. À valider visuellement en jeu. |
| Son d’équipement | Dans un emplacement vide, le son est calculé à partir de l’objet équipé, au lieu d’utiliser l’indice d’emplacement comme identifiant sonore. | Même API sfx_item_sound / ITEM_SOUND_DROP que les transferts d’inventaire. À écouter en jeu. |
| Chargement des secteurs | Fermeture centralisée des fichiers sur toutes les erreurs, y compris un .dif à drapeaux nuls. Un fichier absent à l’ouverture ou un en-tête de différences illisible provoque désormais un échec propre. Suppression également de fclose(NULL) après échec de création d’une sauvegarde de secteur. | Injection d’échec à chaque opération de lecture, avec/sans différences, tous drapeaux/nuls, échec d’ouverture de chacun des deux fichiers. Aucun fichier ouvert oublié ou fermé deux fois. |
| Drapeau des scripts de secteur | Comportement confirmé correct : conserver MODIFIED après une sauvegarde .dif. Les différences sont réécrites en entier avec fopen(..., "wb") ; effacer ce drapeau pourrait faire disparaître un script modifié à la sauvegarde suivante. Le commentaire trompeur est remplacé. | Test CTest de deux sauvegardes successives puis suppression et troisième sauvegarde ; relecture du script à chaque étape. |

Preuves locales : `out/original-anomalies-verification.txt` conserve les
instructions originales consultées et l’empreinte de l’exécutable examiné.
Bancs ciblés : `out/terrain-builder-check.c` et `out/sector-cleanup-check.c`.
Ils compilent les fonctions de production avec un RNG ou des services de
fichiers simulés ; ils ne simulent pas une partie entière.

## Dépendances au texte qui restent intentionnelles

- Les commandes saisies manuellement doivent encore analyser un texte. Leur
  syntaxe vient de broadcast.mes/BroadCastRules.mes ; les noms servent à cibler
  un personnage. Les menus et raccourcis corrigés ne passent plus par ce parseur.
- Le nom provisoire « Choose Name » est comparé à l’entrée localisée du même
  MainMenu.mes. La traduction reste cohérente ; seul un personnage portant
  littéralement ce nom serait refusé. Ce n’est pas le défaut des ordres PNJ.
- Les mots-clés des fichiers rules, les codes de dialogue, les marqueurs de
  terrain et les clés INI sont une syntaxe technique, et non des traductions.
  Ils doivent rester inchangés.

## Validation

- Compilation Windows x86 Release.
- CTest community_features : tests des six identifiants d’ordres, des indices
  invalides, des noms de slots traduits et de la symétrie du tri ajoutés aux
  régressions existantes.
- Banc ciblé compilant les fonctions de production avec des services moteur
  simulés : suppression/tri des sauvegardes, objet du minuteur d’attente,
  restauration des chaînes de terrain, callback absent/présent, limites de
  zones et historique de deux objets distincts. Résultat PASS.
- Banc des ordres directs : actions, affichage localisé, cible explicite,
  contrôle mental, distance et appartenance au groupe. Résultat PASS.
- Les bancs ciblés sont dans out/global-bug-audit-check.c et
  out/follower-orders-dispatch-check.c ; ils ne remplacent pas un test en jeu.
- Les exécutables de test sont compilés avec un manifeste asInvoker.

À valider visuellement en jeu : F1–F5, « Écartez-vous », « Attendez-moi ici »,
réouverture du HUD et activation successive de deux objets récents.

Validation de cette seconde revue : compilation Windows x86 Release réussie,
CTest réussi, banc du générateur de terrain et banc d’échecs de lecture des
secteurs réussis. Les interactions et sons listés ci-dessus restent à confirmer
visuellement ou à l’écoute dans une partie.
