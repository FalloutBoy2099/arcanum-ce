# Utility AI : recul tactique local

Suite : [arrêt prudent d'un entretien offensif](utility_ai_maintenance_20261019.md).

Ce troisième bloc ajoute une action de recul au choix des soins, des objets et des sorts. Il vise le combat en temps réel. Le mode installé reste `[game] utility_ai=2`, limité aux compagnons. Le tour par tour conserve les actions de soutien du bloc précédent, sans cette nouvelle action de déplacement.

## Déclenchement et priorité

Le recul peut être proposé lorsque la cible ennemie est à deux cases ou moins et que la santé est à 30 % ou moins. Au-dessus de ce seuil de santé, il faut une fatigue à 20 % ou moins et soit un profil de combat à distance, soit une action offensive de magie/technologie disponible dans la liste du moteur. La fatigue engagée n'est pas assimilée à une fatigue déjà consommée pour déclencher le recul.

Le score est de 95 plus l'écart sous 30 % de santé, ou de 85 pour le cas de fatigue seule. Les soins urgents et la récupération de fatigue peuvent ainsi rester préférables. Le recul ne consomme aucun objet ni sort. Le mouvement normal reste soumis aux règles du moteur ; le calcul ne suppose pas qu'un pas est gratuit en fatigue.

Les ordres `ONF_AI_WAIT_HERE`, le drapeau `OCF_NO_FLEE`, le recul historique déjà engagé et le combat au tour par tour empêchent la proposition. Les gardes ordinaires de conversation, scripts, inconscience, sommeil et priorité d'animation restent en amont. Le suivi hors combat n'est pas modifié.

## Choix de la case

La recherche évalue uniquement les huit cases adjacentes. Elle considère la cible actuelle et les autres PNJ visibles, conscients et engagés contre l'acteur ou son groupe, dans un rayon de six cases. Elle inspecte au plus 64 entrées et retient au plus huit menaces, cible actuelle comprise. Si ces limites ne suffisent pas, elle abandonne le recul au lieu de calculer sur une liste tronquée.

Une destination doit augmenter la distance à la menace la plus proche sans réduire la distance à aucune menace retenue. Un compagnon doit rester à huit cases au plus de son chef. Cette contrainte peut interdire tout recul dans une mêlée serrée ou lorsque le compagnon est déjà éloigné.

La validation utilise le chemin direct du moteur, limité à un pas, sans recherche A*. Les portes et fenêtres sont bloquantes, les personnages et autres obstacles restent pris en compte ; le feu et les pièges repérés sont évités. La destination est validée à nouveau juste avant de demander l'animation de marche. Si cette validation ou la demande échoue, l'action de combat habituelle reprend.

Il ne s'agit pas d'une carte complète des dangers : les ennemis non perçus, les effets à distance, les projectiles futurs et les zones de sorts ne sont pas prédits. Les monstres voisins non encore engagés ne deviennent pas des cibles par ce calcul. Aucun test d'hostilité exécutant un script n'est ajouté à l'évaluation.

## Temporisation et coût

Un tableau fixe de 64 acteurs limite les tentatives à une par seconde réelle et les reculs acceptés à un toutes les cinq secondes réelles. Si tous les emplacements sont occupés par des délais actifs, aucun nouveau recul n'est proposé. Le tableau est vidé à l'initialisation et au déchargement du module ; les entrées sont associées à la carte. Il ne modifie pas le format des sauvegardes. Une recharge sur la même carte peut conserver au plus le délai court déjà présent en mémoire.

Pendant un recul accepté, la décision suivante ne remplace pas immédiatement la marche par une attaque : elle attend l'arrivée sur la case ou la fin du mouvement, au plus deux secondes. Les autres gardes du moteur restent exécutées avant cette attente.

Le mode observation peut proposer un recul dans les traces sans le lancer. La temporisation d'examen reste appliquée, mais aucun délai de recul réussi n'est installé dans ce mode. La collecte `utility_ai` utilise la valeur -3 ; `utility_ai_detail` utilise l'action 8. L'analyseur expose cette action sous `retreat`. Comme les autres choix, ces traces décrivent une sélection, sans garantir que le mouvement a abouti.

## Validation

Compilation Release et 39 suites CTest réussies. Les tests du module Utility AI couvrent les seuils de santé et fatigue, la distance au danger, la priorité du soin et de la récupération, le gain de distance, le refus d'approcher une autre menace, la distance au chef et les bornes d'entrée. Les six tests Python de l'analyseur passent avec la nouvelle action de recul ; les deux gardes des catalogues de messages passent également.

Les vérifications de collision, l'animation, les ordres et la temporisation intégrés au moteur sont compilés mais restent à observer en partie. Aucun gain de performance ni amélioration visuelle n'est encore mesuré. Le résultat peut être volontairement conservateur : ne pas bouger reste préférable si aucune case ne satisfait les contraintes.

Installation dans `C:\Games\Arcanum`, avec sauvegarde de l'exécutable précédent et de l'INI dans `out/utility-ai-retreat-install-20261019/`. Aucun changement de l'INI, des prototypes ou des sauvegardes.
