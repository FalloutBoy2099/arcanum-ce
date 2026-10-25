# Fermeture pendant le démarrage — 19 octobre 2026

## Cause reproduite

Le démarrage de l’exécutable installé se ferme avec le code de sortie 0. La capture de sa sortie standard et d’erreur révèle un refus explicite de chargement du module, et non une exception Windows : `Quest: invalid definition in rules\gamequest.mes, entry 1`, suivi de l’échec du module Quest et du module Arcanum.

Le catalogue officiel contenu dans `C:\Games\Arcanum\modules\Arcanum.dat` contient 163 entrées : une entrée auxiliaire `{0001}` et 162 quêtes dans la plage 1000–1999. Les 162 définitions de quêtes satisfont le format attendu. Le parseur historique parcourait uniquement les identifiants 1000–1999 et ignorait les autres entrées du catalogue.

Le correctif F06 avait étendu la validation à toutes les entrées et rejetait l’entrée auxiliaire officielle. C’était une régression du correctif des quêtes, indépendante du dernier refactoring des transferts d’inventaire. Les tests initiaux validaient à tort ce rejet de tout le catalogue.

## Correction

Le chargement ignore désormais les entrées hors de la plage des quêtes, avec un diagnostic. Leur contenu n’est pas interprété comme une définition de quête. Les définitions 1000–1999 restent strictement validées et publiées seulement après validation complète du catalogue utile.

Les API de consultation, modification, description et journal refusent toujours les identifiants hors plage avant tout accès aux tableaux. Les contrôles des états, récompenses, entiers et sauvegardes tronquées restent actifs.

Un message de diagnostic confirme désormais la fin de l’initialisation du jeu et du module d’interface avant l’introduction et la boucle du menu. Le comportement historique de retour 0 sur certaines erreurs d’initialisation n’est pas modifié dans ce correctif.

## Vérification

Le test enrichi échoue avant la correction avec 14 assertions, puis passe après. Il vérifie une entrée auxiliaire d’identifiant 1 et de contenu non interprétable comme une quête, entourée de quêtes valides aux deux bornes. Les cas d’identifiants invalides des API et de définitions de quêtes réellement incomplètes restent couverts.

Compilation Release Win32 et 48 suites CTest réussies. Captures : `out/startup-failure-capture.log`, `out/quest-startup-before-fix.log`, `out/quest-startup-fix-tests.log` et `out/startup-after-fix-capture.log`. Le manifeste d’installation enregistre le résultat du démarrage réel et le processus lancé.

L’INI, les catalogues, les prototypes et les sauvegardes ne sont pas modifiés. Les avertissements historiques sur d’autres données rencontrés dans la capture sont conservés ; ils ne sont pas la cause du refus de chargement identifié ici.
