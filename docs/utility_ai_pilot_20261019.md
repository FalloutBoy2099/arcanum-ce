# Premier essai de Utility AI

Mise à jour : le deuxième bloc est installé et décrit dans [utility_ai_support_20261019.md](utility_ai_support_20261019.md). Il remplace notamment la priorité historique des soins pendant les combats concernés et ajoute l'estimation des entretiens déjà actifs. Le texte ci-dessous conserve le périmètre du premier prototype.

Un premier sélecteur tactique est installé dans `C:\Games\Arcanum`. L'activation de test est `[game] utility_ai=2`. La valeur compilée par défaut reste 0. La configuration est conservée par le chargement et l'enregistrement de l'INI.

| Valeur | Comportement |
|---|---|
| 0 | Sélecteur historique |
| 1 | Observation des compagnons : proposition journalisée, puis sélection historique |
| 2 | Sélection Utility AI pour les PNJ ayant un chef joueur |
| 3 | Sélection Utility AI pour tous les PNJ |

Changer la valeur dans l'INI puis redémarrer le jeu. Les ennemis restent sur la sélection historique dans le mode installé. Le joueur conserve son contrôle manuel.

## Périmètre

Le point de remplacement est `ai_combat_spell_select`, après les contrôles généraux de traitement de l'IA et d'autorisation de combattre. Le sélecteur compare des sorts et objets existants dans les catégories offensif, défensif, invocation, trois intensités de soin et récupération de fatigue. Les cibles sont l'adversaire déjà connu, le lanceur et, pour les soins, son chef joueur à douze cases au plus. Aucune recherche de nouvel ennemi ni nouveau trajet n'est effectuée par la fonction de score.

Les routines historiques de soins, résurrection, antidote et recherche d'objets placées avant ce point gardent leur priorité. Il ne s'agit donc pas encore d'un remplacement complet de toutes les décisions de combat. Les déplacements, la fuite, la cadence, les scripts et l'exécution des actions restent ceux du moteur. Ce premier bloc ne met pas en place un recul tactique ou GOAP.

Les candidatures viennent des listes `mt_ai` existantes. Leur construction examine au maximum 32 entrées par catégorie et retient huit nouvelles candidatures par catégorie et cible, avec déduplication par sort, objet et cible. La limite globale est 128. Les scores utilisent santé du lanceur, santé de la cible, fatigue, distance à la menace, distance à la cible, score de catégorie existant et coût prévisionnel.

Pour un sort lancé directement, le coût comprend le coût initial réel et dix secondes de jeu d'entretien estimé, arrondi au prochain paiement. La réserve visée est 20 % de la fatigue maximale, avec un minimum de 5. Un soin d'urgence sur une cible à 25 % de santé ou moins peut descendre à 2. Ce sont des critères de sélection du prototype ; ils n'altèrent pas les règles globales de lancement. L'entretien des autres sorts déjà actifs n'est pas encore explicitement projeté. Les objets n'utilisent pas ce coût de fatigue personnelle : leur disponibilité et leurs règles de lancement restent vérifiées par les voies existantes.

Un score de 35 représente le repli vers le combat normal. Si aucun candidat ne le dépasse et ne passe la validation, l'action de combat historique prend le relais. Au plus huit candidats sont validés dans l'ordre de leurs scores. Les contrôles de visibilité, coût, influence déjà présente et aptitude du moteur sont conservés. Les objets sont aussi vérifiés chez leur propriétaire. L'évaluation pure ne lance aucun sort, n'utilise aucun objet et ne consomme pas de nombre aléatoire. Le candidat retenu est transmis aux animations et invocations habituelles, qui exécutent leurs contrôles. Les actions déjà engagées conservent les gardes de priorité d'animation existantes ; aucune mémoire d'hystérésis supplémentaire n'est introduite.

Le mode observation ne fait pas appel à la validation de lancement, car celle-ci peut effectuer un jet aléatoire de résistance dans le moteur actuel. Ses propositions sont donc des scores préliminaires et non des actions garanties réalisables. Il ne crée pas de trajet ni d'effet ; la construction peut alimenter les caches de listes `mt_ai` existants.

## Traces et tests

Avec `performance_logging=1`, les décisions sont enregistrées dans `performance.csv.events.csv` sous l'événement `utility_ai`. `subject` identifie l'acteur, `correlation` sa cible, `value` le sort (-1 pour un objet sans sort et -2 pour le combat normal), `auxiliary` le score positif en mode actif et négatif en observation. Le nombre d'événements est borné à 512 par intervalle et la saturation est comptée. Le coût de décision reste couvert par la mesure `ai_decision` existante.

Compilation Release et 39 suites CTest réussies. Le nouveau test vérifie la priorité d'un soin d'urgence, la récupération à faible fatigue, les réserves de lancement et d'entretien, le repli après validation refusée, le budget de huit validations, la stabilité des égalités, les limites de candidatures et l'absence de mutation des données évaluées. Le test de paramètres contrôle la conservation du nouveau réglage après sauvegarde et relecture.

La qualité du comportement et le coût réel en combat ne sont pas encore validés. Essai prévu : charger une sauvegarde avec un compagnon lanceur de sorts, faire quelques combats avec santé et fatigue variables, vérifier les soins, les sorts utilisés, les évanouissements et les consignes, puis quitter normalement. Comparer ensuite la même situation avec `utility_ai=0`. Les parcours peuvent diverger : le mode actif change les décisions et les appels aux validations, donc aussi la suite des tirages aléatoires.

L'exécutable et l'INI précédents ainsi que les captures disponibles ont été sauvegardés dans `out/utility-ai-install-20261019/`. Version installée SHA-256 `309ECB5F0EFC3E7EE7F0C4B097CA01B4FF62E53D07F3D03B0813581B290678AE`. Aucun prototype ni fichier de sauvegarde n'a été modifié.
