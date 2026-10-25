# Utility AI : arrêt prudent d'un entretien offensif

Suite : [priorités de soutien et validation des doublons](utility_ai_consolidation_20261019.md).

Ce quatrième bloc ajoute une action d'arrêt d'un sort maintenu à la comparaison des sorts, objets, soins et déplacements. Le réglage installé reste `[game] utility_ai=2`, limité aux compagnons. L'action est disponible en combat engagé, en temps réel et au tour par tour. Le mode 1 observe les propositions sans interrompre les sorts ; le mode 0 conserve la sélection historique.

## Conditions

L'acteur doit être à 20 % de fatigue ou moins. L'entretien prévu doit faire passer sa fatigue sous la réserve de décision : 20 % du maximum, avec un minimum de cinq points. La prévision couvre les 80 prochaines secondes de l'horloge du jeu ; elle ne constitue pas une prédiction exacte de l'instant d'épuisement.

Seul un sort natif lancé par le sélecteur Utility AI actif peut être arrêté. Sa source et son parent doivent être l'acteur ; sa cible doit encore être la cible ennemie actuelle. Le sort doit être en phase de maintien, payant, non technologique, sans invocation attachée ni surcharge de résistance. Ses métadonnées doivent le classer offensif sans le classer dans une autre catégorie d'action. Les effets de soutien, les invocations, les consommables, les sorts d'objets, les sorts gratuits et les effets provenant d'un autre lanceur sont exclus.

Une réserve de mana non nulle sur un objet porté empêche cette nouvelle interruption automatique : la prévision conservatrice ne doit pas supprimer un effet encore alimenté par l'équipement. Les sorts lancés sur ordre et ceux déjà actifs dans une sauvegarde antérieure, sans marque Utility AI, ne deviennent pas éligibles rétroactivement.

Le score d'arrêt vaut 110 plus le coût d'entretien évité dans la fenêtre, plafonné à vingt points supplémentaires. Il ne restitue aucune fatigue déjà dépensée. Un soin urgent ou une récupération peuvent rester mieux classés. Le choix examine au plus les huit premiers entretiens éligibles dans les 512 emplacements existants du moteur, puis utilise le même budget de huit validations que les autres actions.

## Exécution et relance

Les gardes de propriété et d'éligibilité sont revérifiées lors de la sélection puis juste avant l'exécution. L'arrêt utilise `magictech_interrupt_delayed`, la voie existante du moteur qui exécute la fin du sort. La marque de gestion automatique est retirée avant cette demande afin de ne pas la répéter à chaque décision. Si l'ajout de l'événement échoue, le moteur journalise l'erreur et l'effet reste actif sans nouvelle demande automatique pour cette instance.

Un nouveau sort offensif avec entretien estimé positif est refusé à 35 % de fatigue ou moins. Les seuils distincts de 20 % et 35 % réduisent le risque de boucle arrêt/relance. Les sorts instantanés, les soins et les autres catégories ne sont pas soumis à cette nouvelle condition de relance. La réserve de fatigue et les contrôles ordinaires du moteur restent applicables.

La réduction de coût `OCF_FATIGUE_LIMITING` est maintenant prise en compte par paiement dans les estimations du lancement, du nouvel entretien et des entretiens déjà actifs. Elle suit l'arrondi du moteur, y compris le minimum d'un point lorsque cet effet est actif. Les réserves de mana, la régénération future, les variations ultérieures de coût et les surcharges de résistance restent hors de la simulation générale. Les règles réelles de consommation ne changent pas.

## Sauvegardes et mesures

Deux bits disponibles dans les mots de drapeaux existants identifient l'origine Utility AI de l'invocation puis de son instance. Aucun champ sérialisé ni taille de structure d'invocation n'est ajouté. Les fonctions de lecture et d'écriture des sauvegardes restent identiques ; elles conservent le mot de drapeaux de l'instance.

La trace `utility_ai` utilise la valeur -4 pour l'arrêt d'entretien et `utility_ai_detail` l'action 9. L'analyseur affiche `release_maintenance`. La fatigue projetée inclut l'entretien évité, sans augmenter la fatigue réelle. Ces traces indiquent une proposition ou une sélection ; elles ne prouvent pas l'exécution de l'événement de fin. Les compteurs restent échantillonnés sous forte activité.

## Validation et installation

Compilation Release, 39 suites CTest et huit tests Python réussis. Les tests ajoutés couvrent les seuils de 20 % et 35 %, l'absence de dépense évitable, la réserve suffisante, la priorité du soin et de la récupération, les paiements réduits et leurs arrondis. L'analyseur vérifie l'action d'arrêt et son estimation de fatigue.

Les gardes d'origine, les réserves des objets et la fin effective des sorts sont vérifiées dans le code et compilées ; elles restent à tester en partie. Aucun résultat de performance ou de comportement visuel n'est encore mesuré.

Installation dans `C:\Games\Arcanum`, avec sauvegarde de l'exécutable précédent et de l'INI dans `out/utility-ai-maintenance-install-20261019/`. L'INI, les prototypes et les fichiers de sauvegarde ne sont pas modifiés par l'installation.
