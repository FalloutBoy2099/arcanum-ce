# Préchargement des images communes au démarrage — 19 octobre 2026

## Comportement

Les images ART communes de l’interface sont désormais préparées avant l’introduction et le premier menu, après l’initialisation des modules et le montage des données. Le moteur utilise son cache ART existant ; les images décodées restent disponibles lors des chargements ultérieurs, sous réserve du budget et des évictions du cache.

La sélection couvre les images enregistrées pour l’inventaire, les silhouettes d’équipement, le journal, les schématiques et le cadre de carte. Les variantes du journal et des schématiques tiennent compte des barres d’outils activées. La carte prépare son cadre classique et sa variante XL lorsqu’elle est activée, afin de conserver le repli existant si la variante manque.

Les définitions du jeu et les prototypes déjà conservés ne sont pas rechargés par ce traitement. Les personnages, quêtes, objets, événements, références et animations en cours restent restaurés depuis chaque sauvegarde.

## Implémentation

- `ui_asset_warmup_startup` utilise le registre de ressources existant, dédupliqué et limité à 128 identifiants.
- Le décodage utilise `tig_art_touch` et la résolution des ressources normale du jeu : les remplacements et priorités des archives sont conservés.
- Le traitement suit le réglage existant `[global] asset_prefetch`. Avec `asset_prefetch=0`, le préchauffage est désactivé, le chargement à la demande reste disponible. Aucun nouveau réglage ni changement de l’INI.
- Les images préparées sont signalées au préchargeur progressif pour éviter une nouvelle préparation immédiate.
- Une ressource absente ne fait pas échouer le démarrage. Le préchargement progressif continue aussi depuis le menu, en complément du travail déjà effectué en partie.
- Aucun verrouillage permanent des images, aucune augmentation des budgets mémoire et aucun chargement de toute la collection ART.
- Le cache préparé contient les données ART décodées ; les textures de rendu peuvent encore être créées à leur première utilisation.

## Vérification

Sur le jeu installé, 62 images communes sont préparées en 234 ms. Deux chargements complets successifs de Slot0001 réussissent. Après chacun, une nouvelle préparation des 62 ressources produit zéro cache miss ART : elles ont survécu au changement d’état de partie sans nouvelle lecture de ressource.

Les chargements de cette sonde prennent respectivement 2 076 ms et 1 019 ms, avec le son désactivé. Ces valeurs ne démontrent pas une accélération du chargement de l’état de la sauvegarde ; le gain directement vérifié est la disponibilité des ressources communes avant le menu et leur réutilisation. Le coût est avancé au démarrage.

Le test du préchargeur couvre la sélection dédupliquée, le réglage désactivé, une ressource manquante suivie d’une reprise progressive, la réinitialisation des marqueurs et la capacité maximale. Compilation Release Win32 et 48 suites CTest réussies.

La sonde temporaire a été retirée avant la compilation finale. Les empreintes des 23 fichiers originaux de sauvegarde restent identiques. Capture : `out/startup-art-probe.log`. Tests : `out/startup-art-tests.log`.
