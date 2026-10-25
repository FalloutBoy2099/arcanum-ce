# Extraction de l’adaptateur Utility AI — 19 octobre 2026

## Organisation

`ai.c` passe de 4 904 à 4 380 lignes. Le nouveau `ai_utility_adapter.c` rassemble l’accès au mode configuré, la construction des candidats, la sélection, la validation, les délais de repli et l’exécution des actions spécifiques de repli ou d’arrêt de maintien. Le moteur de scores pur reste dans `utility_ai.c` et la collecte des ressources reste dans `mt_ai.c`.

L’orchestration générale, l’IA historique, les animations de lancement, l’utilisation des objets et le retour au combat normal restent pilotés par `ai.c`. `ai_private.h` contient les structures de contexte partagées entre ces deux unités et les trois interfaces internes nécessaires : lecture des paramètres, consultation de la menace et validation d’une action magique. L’interface publique `ai.h` n’est pas étendue avec ces détails.

Les réinitialisations gardent leurs emplacements dans l’initialisation et le déchargement du module. La garde de déplacement en cours délègue au nouvel adaptateur. Une action spécifique devenue impossible retourne toujours vers le combat normal. Le traitement historique de `utility_ai=0` et la sélection en observation de `utility_ai=1` gardent leurs appels existants.

Les seuils, tailles de listes, délais, coûts, classements, traces, règles des objets, soins, résurrection, repli et maintien ne sont pas rééquilibrés par ce bloc. Aucune structure sérialisée ni option d’INI ne change. Les commentaires existants des structures ont été déplacés avec elles ; aucun nouveau commentaire de code n’est ajouté.

## Validation

Les corps de 14 fonctions de décision extraites ont été comparés automatiquement au fichier de départ conservé dans `out/ai-before-utility-extraction.c` : ils sont identiques. Les changements autour de ces fonctions sont les interfaces, l’emplacement du stockage des délais, la délégation des gardes et l’exécution des deux actions spécifiques.

`utility_ai_adapter_test` utilise le véritable adaptateur et le moteur de scores, avec des objets, des ressources et une horloge simulés. Il couvre :

- modes désactivé, observation, compagnons et tous les PNJ ;
- observation sans modification du contexte d’action ;
- potion utilisable lorsque le sort est trop coûteux, puis préférence au sort peu coûteux ;
- rejet d’une potion qui appartient à un autre personnage et sélection de l’alternative ;
- soin d’un allié blessé et absence d’action si aucune ressource n’est disponible ;
- au plus une collecte de chaque catégorie par décision ;
- repli en cours, arrivée, animation arrêtée, expiration, changement de carte et remise à zéro ;
- repli refusé avec ordre d’attente, combat au tour par tour ou chemin bloqué ;
- interruption d’un maintien éligible une seule fois, refus d’un maintien gratuit, d’une autre cible ou d’un acteur encore suffisamment reposé.

Compilation Release Win32 et 48 suites CTest réussies. Journal : `out/utility-adapter-tests.log`. La validation interactive de l’IA et les comparaisons de performances restent à faire ; cette extraction ne revendique aucun gain de FPS.

Le bloc précédent de [préchargement des ART archivés](archive_art_prefetch_20261019.md) est conservé dans cette version.
