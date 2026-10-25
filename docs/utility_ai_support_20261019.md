# Utility AI : soins de groupe et fatigue engagée

Suite : [recul tactique local](utility_ai_retreat_20261019.md). Le présent document décrit le deuxième bloc avant l'ajout du déplacement.

Ce deuxième bloc remplace la priorité historique des soins pendant un combat déjà engagé, pour les acteurs couverts par le mode Utility AI actif. Le mode installé reste `[game] utility_ai=2`, limité aux compagnons. Les autres modes sont conservés : 0 historique, 1 observation des compagnons, 3 tous les PNJ.

## Choix des actions

Lorsqu'un acteur éligible est en état de combat ciblé, `ai_process_measured` passe par l'actualisation normale du danger et le sélecteur Utility AI. Le soin historique ne peut donc plus consommer un objet ou lancer un sort avant la comparaison des options dans ce cas. Hors combat engagé, en fuite, en reddition, en observation ou avec l'option désactivée, la voie historique est conservée. Les gardes de scripts, conversation, sommeil, inconscience et priorité d'animation restent en amont.

La comparaison inclut désormais antidote et résurrection. Les actions sur une cible morte sont réservées à la résurrection ; les sorts, les objets et les invocations passent toujours les validations et l'exécution du moteur. Les soins urgents à 25 % de santé ou moins reçoivent une priorité supplémentaire. Un consommable a un coût d'opportunité de dix points dans le score pour favoriser une solution réutilisable comparable.

Le groupe candidat comprend l'acteur, son chef et au plus 32 entrées de la liste des compagnons du chef. Seuls les membres proches, à douze cases au plus, sont retenus. Les quatre besoins les plus urgents sont classés par santé, fatigue, poison, décès et distance. L'examen n'ajoute pas d'ennemis inconnus. Au plus deux candidatures de soutien par catégorie et cible, huit offensives, huit défensives et huit invocations sont considérées. Le plafond global de 128 et le budget de huit validations restent en place. Une candidature de récupération pour l'acteur est également proposée indépendamment de son classement parmi les alliés.

Les listes, gardes de suivi et recherches de cible existantes restent utilisées. Aucun nouveau déplacement de recul ni recherche de chemin n'est ajouté à l'évaluation. La perception et la possibilité d'atteindre une cible sont vérifiées dans les voies habituelles sur les meilleurs candidats ; un allié proche derrière un obstacle peut donc être classé puis refusé. Le budget de validation peut limiter l'accès à des solutions moins bien notées.

## Prévision de fatigue

La réserve prend maintenant en compte les sorts maintenus déjà actifs, en plus du nouveau sort. Une lecture bornée des 512 emplacements du moteur retient les entretiens payants dont la source est l'acteur et l'action courante est le maintien. Elle estime les paiements dans les 80 prochaines secondes de l'horloge de jeu, depuis l'échéance enregistrée de chaque sort.

Le délai d'entretien du moteur multiplie par huit la période de référence. Le calcul du nouveau sort et des sorts actifs utilise cette même échelle, avec la période adaptée à la maîtrise et le coût adapté à la race par `spell_maintain_cost`. Les paiements aux limites de la fenêtre sont comptés explicitement. Il s'agit d'une estimation prudente : les réserves de mana des objets portés, les effets limitant la fatigue, la régénération future et la fin anticipée d'un sort ne sont pas simulés. Les règles réelles de consommation ne changent pas.

Un nouveau sort payant est défavorisé ou écarté si sa dépense, son entretien et les engagements existants menacent la réserve. Une récupération ne coûtant aucune fatigue personnelle reste possible lorsque les engagements sont déjà trop élevés. L'évaluation ne lance aucun sort et ne retire aucun objet. Le mode observation reste sans validation susceptible de consommer un jet aléatoire et laisse les décisions historiques s'exécuter.

## Mesures et validation

Le nouvel événement `utility_ai_detail` accompagne `utility_ai`. Son champ `value` indique l'action : 0 combat normal, 1 offensif, 2 défensif, 3 invocation, 4 soin, 5 récupération, 6 antidote, 7 résurrection. Son champ `auxiliary` indique la fatigue projetée après les dépenses estimées. La collecte est bornée ; ce sont des décisions proposées ou sélectionnées, pas une preuve que l'effet s'est effectivement produit.

L'analyseur de captures produit les décisions par mode, les catégories d'actions enregistrées et la fatigue projetée minimale/moyenne. Les comptes restent des échantillons lorsque la limite de traces est atteinte. Les détails d'action mélangent les modes si une capture contient plusieurs modes ; les décisions par mode restent séparées.

Compilation Release, 39 suites CTest et six tests Python de l'analyseur réussis. Les cas ajoutés couvrent l'urgence des alliés, la portée de douze cases, l'antidote, la résurrection, le refus de soigner un mort, la dette d'entretien, la récupération gratuite et les paiements en bord de fenêtre. L'analyseur est vérifié avec filtrage de session, décisions actives et observation, et fatigue projetée négative.

Le comportement et les performances en partie restent à mesurer lorsque l'utilisateur effectuera son essai. Installation dans `C:\Games\Arcanum`, avec exécutable, INI et captures précédents conservés dans `out/utility-ai-support-install-20261019/`. L'INI est inchangé ; aucun prototype ni fichier de sauvegarde n'est modifié.
