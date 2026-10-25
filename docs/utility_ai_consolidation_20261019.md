# Utility AI : priorités de soutien et validation des doublons

Ce cinquième bloc consolide la sélection des actions sans ajouter de catégorie de comportement. Le mode installé reste `[game] utility_ai=2`, limité aux compagnons.

## Priorité des alliés

Le classement des quatre alliés à aider tient maintenant compte des catégories de soutien présentes dans les listes de l'acteur. Un mort ne prend plus une place si l'acteur n'a aucune action de résurrection ; un allié uniquement empoisonné ne prend plus une place s'il n'existe aucune action d'antidote. La fatigue et la santé sont traitées de la même façon.

Les seuils de soin restent cohérents avec le générateur de candidats : soin léger sous 85 % de santé, moyen à 65 % ou moins, lourd à 40 % ou moins. Si seul un soin lourd est connu, un allié à 70 % ne passe donc plus devant un autre besoin que l'acteur peut traiter. Les priorités d'urgence et la portée de douze cases restent en place.

La présence d'une catégorie ne garantit pas qu'une action sera utilisable : coût, état de l'objet, influence déjà active, obstacle et règles du moteur restent vérifiés sur les meilleurs candidats. Ce filtrage retire les besoins sans catégorie correspondante ; il ne prévalide pas tous les sorts de tous les alliés.

## Ressources réutilisées dans une décision

Une structure locale conserve les listes consultées et les catégories déjà demandées. Chaque catégorie appelle au plus une fois `mt_ai_create` pendant cette décision, même si sa liste est vide. Les catégories de soutien sont consultées selon les besoins observés. Le générateur de sorts, le classement des alliés et la détection d'un profil de lanceur pour le recul utilisent la même structure.

Les listes restent détenues par le cache du moteur ; la structure locale ne les libère pas et ne survit pas à la décision. Aucun cache négatif permanent n'est ajouté. À la décision suivante, les mécanismes habituels de collecte et d'invalidation restent utilisés. Aucun effet, objet ou déplacement n'est exécuté pendant cette collecte.

## Une validation par exécution refusée

Un même sort ou objet peut remplir plusieurs rôles. Les candidats conservent maintenant ces rôles distincts pour comparer leur utilité : un objet qui soigne et restaure la fatigue peut être évalué selon les deux besoins.

Si la validation refuse une exécution, tous ses doublons sont écartés pour cette décision. L'identité d'une exécution ordinaire repose sur le sort, l'objet et la cible. Un refus sur une cible ne bloque pas le même sort sur une autre ; un objet refusé ne bloque pas un autre objet. Cela évite aussi de répéter un jet d'échec pour le même sort sur la même cible sous un autre rôle.

Le recul est distingué par sa destination, l'arrêt d'entretien par son instance, son sort et sa cible. Le budget reste de huit validations, désormais consacré à des exécutions distinctes après chaque refus. La sélection reste stable à score égal. Aucun refus n'est conservé entre deux décisions : une cible ou une ressource qui redevient utilisable peut être réessayée.

## Validation et installation

Compilation Release et 39 suites CTest réussies. Les tests ajoutés couvrent les besoins sans catégorie de soutien, les limites des soins moyens/lourds, dix doublons refusés suivis d'une action valide, les cibles et objets distincts, les destinations de recul distinctes et les instances d'entretien distinctes. Les huit tests Python des captures et des catalogues passent également.

La réduction des demandes répétées de listes et de validations est vérifiable dans le code ; aucun gain de temps CPU n'est encore mesuré en partie. Les coûts de la perception et de l'activation des secteurs restent à distinguer dans les prochaines captures.

Installation dans `C:\Games\Arcanum`. Exécutable précédent et INI conservés dans `out/utility-ai-consolidation-install-20261019/`. L'INI, les prototypes, les sauvegardes et les règles de déplacement hors combat ne sont pas modifiés par ce bloc.
