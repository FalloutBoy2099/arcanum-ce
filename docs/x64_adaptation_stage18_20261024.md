# Adaptation x64, dix-huitième bloc — 24 octobre 2026

Ce bloc sécurise les lectures des archives DAT et leurs états de flux. Il ne modifie ni leur format ni la limite actuelle de INT_MAX octets par entrée, contrôlée à l’ouverture des tables.

Le compteur interne de données compressées passe à size_t. La multiplication taille d’élément × nombre d’éléments est contrôlée avant lecture. La demande est bornée aux données restantes avant toute réduction vers les types utilisés par zlib. La progression de la position publique reste un entier 32 bits : l’ajout explicite porte uniquement sur un nombre d’octets prouvé inférieur ou égal à la place restante. Les conversions implicites signalées par le compilateur disparaissent.

La lecture interne retourne maintenant le nombre réel d’octets produits. Une lecture physique interrompue conserve une position cohérente avec ces octets et signale l’erreur. Une lecture nulle ou une lecture après la fin annoncée du fichier ne déclenche plus une fausse erreur de lecture. Un flux zlib terminé trop tôt ou sans progression est arrêté ; les octets réellement produits restent comptés.

Les lectures avec un caractère remis par ungetc respectent les données disponibles. Ungetc est refusé au début du flux afin de ne pas produire de position négative. Le repositionnement d’un flux compressé distingue sa position logique et la position déjà décompressée : après ungetc puis fseek(0, SEEK_CUR), le caractère original à cette position est relu correctement.

La lecture de lignes termine proprement la dernière ligne sans saut de ligne. Les séquences CRLF sont converties en LF en mode texte ; un CR isolé est conservé. Les buffers absents et les capacités non positives sont refusés.

Le cache sonore vérifie aussi l’état d’erreur du flux avant de publier une entrée : une erreur de somme de contrôle zlib peut être signalée après production de tous les octets attendus. Le seul compte d’octets ne suffisait donc pas pour accepter le fichier.

## Validation

- 58/58 suites réussies en Release x64 et 57/57 en x86.
- Demandes nulles, multiplication débordante de size_t, fin exacte et dépassement de fin, élément partiel de fread, octets de garde, ungetc et repositionnement : contrôlés pour les entrées simples et compressées.
- Lignes CRLF, CR isolé, fin sans saut de ligne et capacité de lecture égale à un : contrôlées dans les deux modes de stockage.
- Un fichier d’archive tronqué après ouverture produit exactement les 32 octets physiques restants, conserve la position 32 et signale l’erreur.
- Une entrée compressée dont la taille annoncée dépasse les données réelles, et une entrée dont la somme de contrôle est endommagée, sont refusées par le cache sans entrée publiée. Un repositionnement rencontrant l’erreur de somme de contrôle échoue aussi.
- Les tests antérieurs d’archives situées au-delà de 2 Gio restent présents : cette possibilité concerne l’offset d’une entrée dans une grande archive, et non une entrée individuelle dépassant INT_MAX.
- Lecture complète de tig.dat et modules/Arcanum.dat dans chaque architecture : 26 + 75 485 fichiers, soit 75 511 fichiers lus et 81 terrains vérifiés, sans échec.
- Chargement isolé de Tarant, 48 images à quatre cibles de zoom et sortie normale dans chaque architecture. La capture x64 est inspectée visuellement. La sonde est lancée depuis le dossier de build contenant aussi les bibliothèques FFmpeg du bloc précédent.

Les sources de diagnostic sont retirées et main.c est restauré à l’identique. Aucun commentaire de code n’est ajouté. Aucun gain de FPS ou de temps de chargement n’est revendiqué. Les autres conventions historiques des API de flux ne sont pas toutes révisées par ce bloc.

## Installation

La version x64 1.0.0.39 est installée dans C:/Games/Arcanum/arcanum-ce-x64.exe. Les bibliothèques des cinématiques installées correspondent toujours aux versions utilisées par la compilation. Les empreintes du principal x86 et de arcanum.ini sont inchangées.

SHA256 de l’exécutable installé : 5def8571f40880f978e22a44ff65d971f4a6f224e5dcfd4d30a84e5c4a0b5b02.

Sources avant modification, différences, journaux, captures et sauvegarde d’installation : out/x64-adaptation-stage18-20261024. Le portage et son audit restent en cours.
