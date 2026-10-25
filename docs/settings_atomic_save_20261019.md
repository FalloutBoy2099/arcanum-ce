# Sauvegarde atomique des paramètres

Correction du point 7 du résumé de l'audit, identifié F08 dans `project_audit_20261019.md`.

`settings_save` écrit désormais dans un fichier temporaire créé exclusivement dans le même répertoire que la destination. Un nom déjà occupé provoque une nouvelle tentative, sans écraser ce fichier. Le contenu conserve les règles de classement et les sections existantes de l'INI et du CFG.

Avant publication, le code vérifie l'état du flux, `fflush`, la synchronisation du descripteur (`_commit` sous Windows, `fsync` ailleurs) et `fclose`. Le remplacement utilise ensuite `SDL_RenamePath`, dont l'implémentation Windows appelle `MoveFileExW` avec remplacement de la destination. L'ancien fichier n'est jamais supprimé préalablement.

En cas d'échec, le temporaire appartenant à cette tentative est supprimé, l'ancien fichier reste en place et `SETTINGS_CHANGED` reste actif. Les valeurs en mémoire sont conservées et une nouvelle sauvegarde peut être tentée. Le drapeau n'est effacé qu'après un remplacement réussi. Les échecs de création ou de sauvegarde sont journalisés.

Cette protection couvre les échecs d'E/S contrôlés et évite de publier un contenu partiel. Elle ne promet pas une résistance absolue à toute panne matérielle ou coupure électrique : la persistance de l'opération de renommage dépend aussi du système de fichiers. Une interruption du processus avant publication peut laisser un temporaire orphelin ; il ne remplace pas l'INI et n'est pas réutilisé comme configuration.

## Validation

Le nouveau test `settings_atomic` compile le code réel de `settings.c` avec substitution des opérations d'E/S dans le seul exécutable de test. Il couvre :

- remplacement normal, création d'un nouveau fichier et sauvegarde sans modification ;
- échec de création du temporaire ;
- vraie erreur d'écriture sur un flux ouvert en lecture ;
- erreurs simulées de vidage, synchronisation, fermeture et remplacement ;
- conservation exacte de l'ancien fichier et des valeurs en mémoire ;
- conservation du drapeau de modification et réussite d'une nouvelle tentative ;
- nettoyage des temporaires de la tentative et préservation d'un temporaire concurrent ;
- présence de l'ancien contenu jusqu'à l'appel de remplacement et classement des sections.

Compilation Release Windows x86 et 40/40 suites CTest réussies. Les tests existants de configuration et d'options passent également. Les branches POSIX sont présentes mais n'ont pas été compilées sur cette machine Windows.

Installation dans `C:\Games\Arcanum`. L'exécutable précédent et l'INI sont conservés dans `out/settings-atomic-install-20261019/`. L'installation ne modifie pas le contenu de l'INI, les prototypes ni les sauvegardes.
