# Sons répétitifs — 25 octobre 2026

La version 64 bits 1.0.0.115 ajoute le réglage suivant :

```ini
[audio]
mute_repetitive_sounds=1
```

Dans Options → Audio → page 2, « Couper les sons répétitifs » pilote le même
réglage. Les libellés et descriptions sont fournis en français et en anglais
dans les catalogues `.mes` (identifiants 210 et 1210).

La valeur 1 met en sourdine les six effets suivants ; 0 rétablit leur lecture :

- `Krag_fidgeting.wav`
- `Spell_Elemental_Air_Vitality.wav`
- `Spell_Elemental_Earth_Strength.wav`
- `Spell_Elemental_Fire_Agility.wav`
- `Spell_Elemental_Water_Purity.wav`
- `spell_temporal_tempus_fugit.wav`

Le changement s'applique immédiatement aux sons déjà en cours et aux lectures
suivantes. Ces six effets sont coupés entièrement, y compris lorsque le moteur
les déclenche plusieurs fois comme sons courts, et pas seulement lorsqu'ils
utilisent explicitement une boucle infinie. Les autres effets, les voix et la
musique conservent leur fonctionnement.

## Implémentation

`repetitive_sound.c` identifie les noms de fichiers complets, indépendamment de
la casse, des sous-dossiers et du sens des séparateurs de chemin. Un nom proche,
par exemple une variante de lancement de sort, n'est pas filtré.

Le moteur audio TIG applique un filtre aux effets au moment de leur lecture et
lors du changement du réglage. Chaque son conserve son volume et sa position
stéréo ; seul son volume envoyé au mélangeur passe à zéro. Les recalculs de
distance, changements de volume, de balance et fondus respectent cette sourdine.
La désactivation restitue le volume courant sans détruire les handles ni relancer
les sorts. Le résultat du filtrage est conservé sur le son : il n'y a pas de
recherche de fichier à chaque frame audio.

Le réglage est enregistré dans la section `[audio]` avec les autres réglages
sonores. Sa valeur par défaut dans le moteur est 0 ; il est activé dans
l'installation de Michael.

## Fichiers du jeu installé

Les six fichiers prioritaires d'`ArcanumX.dat/sound` étaient déjà des WAV
silencieux de 6 044 octets. Ils ont été sauvegardés avant remplacement par les
sons d'origine : Krag provient d'`Arcanum4.dat`, les cinq sorts d'`Arcanum2.dat`.
Cela permet au réglage désactivé de rétablir effectivement le son.

Aucun WAV d'origine n'est ajouté au dépôt. Les sauvegardes des fichiers remplacés,
empreintes et manifestes d'installation sont dans `out/repetitive-sounds-20261025/`.
L'exécutable 64 bits et les catalogues français, anglais et de repli sont installés.

## Vérifications

- 62 suites réussies en 64 bits et 61 en 32 bits.
- Tests d'identification des six noms, casse et chemins, et exclusion des noms
  voisins ou des sons sans rapport.
- 63 vérifications natives du moteur audio par architecture, avec un périphérique
  SDL fictif et les fichiers audio d'origine. Le gain réel du mélangeur est vérifié
  lors de la coupure et de la réactivation, pour une boucle existante et une
  nouvelle lecture, ainsi qu'après modification du volume et de la balance.
- Vérification d'un effet non concerné et d'un son classé musique.
- Navigation vers la seconde page Audio, clics du bouton Oui/Non et enregistrement
  du paramètre dans l'INI d'une installation de test.
- Vérification visuelle du libellé français et de sa description.

Les sondes temporaires ont été retirées des sources et des exécutables installés.
