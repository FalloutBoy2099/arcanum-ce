# Bilan du renommage — 24 octobre 2026

## Résultat

111 fichiers C/H modifiés, pour 4 396 substitutions d’identifiants. Le périmètre comprend le moteur, l’interface, TIG et les références concernées dans les tests. Aucun commentaire n’est ajouté ou modifié.

Le relevé reproductible ne trouve plus les familles de noms hérités suivantes hors commentaires et littéraux :

| Famille | Occurrences avant | Après |
|---|---:|---:|
| Noms issus des adresses, dont les macros `sub_xxxx` | 166 | 0 |
| Champs nommés par offset `field_XX` | 568 | 0 |
| Paramètres `a1`, `a2`, etc. | 1 432 | 0 |
| Variables `v1`, `v2`, etc. | 2 142 | 0 |
| Constantes numériques candidates du relevé | 57 | 0 |

Ce sont des occurrences lexicales, pas des comptes de variables indépendantes. Le relevé ne prétend pas détecter tout nom stylistiquement améliorable.

## Exemples

- Les paramètres des recherches de chemin indiquent maintenant le départ, la destination, la largeur de grille et les coûts ; les coordonnées et parcours de secteurs ont des noms de lignes et de colonnes.
- Les traitements des inventaires distinguent l’objet, le propriétaire source, le destinataire, le marchand, la quantité et l’emplacement. Le booléen du curseur qui commande la boîte de quantité s’appelle `prompt_for_quantity`.
- Le traitement des dialogues distingue types de réponses, valeurs, options, conditions, actions et réponses de formation/recrutement.
- Les coûts magiques distinguent consommation d’énergie, échec des prérequis, difficulté de résistance et résultat du jet.
- `TigArtBlitInfo` expose `blend_colors` et `blend_rect` ; les paramètres des interfaces et callbacks sont nommés selon leurs usages.
- Les indicateurs de présence d’une surcharge externe aux DAT et l’attribut des fichiers provenant d’une archive possèdent des noms explicites. Les valeurs des indicateurs restent identiques.

## Données réservées et fonctions incomplètes

Des emplacements hérités n’ont pas de sens métier reconstructible dans le code disponible. Plusieurs ne sont que déclarés, initialisés ou recopiés ; d’autres sont conservés dans un format sérialisé. Ils sont explicitement nommés `reserved_*`, `*_unused_*` ou `*_serialized_reserved_*`, selon les usages observables. Le tableau de correspondance compte 233 membres marqués réservés, principalement dans les structures réseau anciennes, les métadonnées et les états non exploités.

Ces noms indiquent le statut de l’emplacement. Ils ne signifient pas que son rôle dans le moteur original a été retrouvé. Les champs restent distincts et conservent leur type, leur position et leur stockage ; aucune structure n’est compactée et aucun octet de format n’est supprimé.

Le suffixe `stub` est conservé pour les fonctions ou macros dont l’implémentation est absente ou partielle. Le relevé final en compte 61 graphies, dont les six anciennes macros réseau adressées. Leurs noms décrivent leur emplacement dans les appels et signalent explicitement l’absence d’implémentation. Le renommage ne rétablit pas le multijoueur.

## Validation

- Compilation Release x86 et x64 réussie.
- 54 suites CTest réussies dans chaque architecture après les derniers ajustements de noms.
- Annulation des seules substitutions d’identifiants : les 111 fichiers modifiés redeviennent exactement identiques, octet par octet, aux copies prises au début de cette passe.
- Les commentaires et littéraux sont aussi comparés séparément ; leurs séquences restent identiques.
- Les constantes, types, signatures binaires, dispositions des structures et instructions ne sont pas modifiés par cette passe.
- Aucun nouvel essai prolongé en jeu ni gain de performances n’est revendiqué.

L’exécutable séparé `C:\Games\Arcanum\arcanum-ce-x64.exe` est actualisé avec sauvegarde du précédent et manifeste SHA-256. L’exécutable principal x86 et le fichier INI installé conservent leurs empreintes.

## Suivi

- [Relevé final détaillé](identifier_naming_audit_20261024.json).
- [Correspondances des noms et vérification](identifier_renames_20261024.json).
- Sources précédentes, plan d’application, preuve d’inversion, journaux de compilation/tests et manifeste d’installation : `out/naming-final-20261024/`.

Les journaux finaux sont `x64-final-build-tests.log` et `x86-final-build-tests.log`. Les fichiers d’audit du 19 octobre sont conservés comme historique.
