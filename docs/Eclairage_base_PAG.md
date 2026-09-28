# Éclairage de la base PAG — guide de pose (carte #54, version 1.0.26)

Pour Jack. Tout ce qu'il faut pour poser les lampes et le générateur dans le Workbench ; le script fait le reste.

## 1. Ce que fait le mod

- Un **générateur** (prefab `SimpleRP/Prefabs/Props/Base/SRP_Generateur.et`, posé au garage) alimente toutes les lampes à moins de **300 m** du marqueur `SRP_SpawnBase` (rayon réglable sur le game mode, composant `SRP_GenerateurComponent`).
- Au contact du générateur : **« Démarrer le générateur » / « Couper le générateur »** (tout le monde, 1 s) et **« Réparer le générateur »** (30 s, logisticiens Logi, officiers, Staff ; visible seulement en panne).
- Générateur **détruit** (tir, grenade, explosion) = **en panne** : toute la base est dans le noir jusqu'à réparation. À la réparation, l'épave est remplacée par un générateur neuf au même endroit.
- L'état (allumé / éteint / en panne) est sauvegardé dans `profile/SimpleRP/generateur.json` et rétabli au redémarrage du serveur.
- Pendant une coupure, l'interrupteur des lampes électriques de la base refuse d'allumer (« Pas de courant »). Les **lanternes à pétrole** (`LanternMilitary_US_01`, `Lamp_Interactive`) ne dépendent pas du courant : c'est le secours.
- La nuit, une lanterne allumée apparaît à côté de chaque livraison en cours (points de livraison) et disparaît le jour ou quand la livraison est terminée.
- Mode nuit du Game Master : composant vanilla `SCR_NightModeGameModeComponent` sur le game mode, avec « Allow global night mode » coché. Dans l'éditeur Game Master (Staff), le bouton « Mode nuit » éclaircit l'image de nuit pour tout le monde ; un raccourci existe aussi pour l'éclaircir seulement chez soi.

Le rythme jour/nuit ne change pas.

## 2. Comment le script reconnaît une lampe

Au démarrage (2 s après le game mode) et à chaque changement d'état, le script parcourt toutes les entités à moins du rayon de la base (`QueryEntitiesBySphere`) et agit selon ce qu'il trouve, sans rien à configurer sur les lampes :

| Ce que porte l'entité | Ce que fait le générateur | Allumable à la main ? |
|---|---|---|
| `SCR_BaseInteractiveLightComponent` (lampe **interactive**) | `ToggleLight(allumé / éteint)` | **oui**, action « Allumer / Éteindre » du prefab (refusée sans courant) |
| `SCR_LampComponent` (lampe **à pétrole** : lanterne, lampe de camp) | rien : elle brûle sans courant | oui, toujours |
| `LightEntity` enfant d'un prop (lampe **statique**) | `LightEntity.SetEnabled(allumé / éteint)` | non, elle suit seulement le générateur |
| `StreetLampComponent` (lampadaire, mât, lampe industrielle extérieure : allumage automatique à la nuit) | `SetBroken(true)` sans courant, `SetBroken(false)` avec : il reprend son allumage automatique | non |

Une lampe posée **hors** du rayon de 300 m n'est pas concernée (les lampadaires du village restent au village).

**Règle des prefabs interactifs posés dans un calque** : leur attribut `m_eInitialLightState` (composant `SCR_BaseInteractiveLightComponent`) doit être **`LIT`**. Vérifié dans `SCR_BaseInteractiveLightComponent.GetInitialState()` : `LIT_ON_SPAWN` retourne *éteint* pour une entité chargée avec le monde (`IsLoaded()` vrai) et *allumé* seulement pour une entité créée en cours de partie. Les prefabs vanilla interactifs (`GeneratorFloodlight_US_01`, `LanternMilitary_US_01`, `Lamp_Interactive`) sont en `LIT_ON_SPAWN` : posés dans un calque, ils démarrent éteints. Soit tu changes l'attribut sur chaque instance posée (Object Properties → le composant → Initial Light State = LIT), soit tu poses les prefabs SimpleRP ci-dessous, déjà réglés sur `LIT`. Le générateur, lui, rallume tout de toute façon quand il démarre allumé : cette règle compte surtout pour l'état de départ après un « Couper » puis « Démarrer ».

## 3. Les prefabs à poser

Tous les chemins sont ceux du jeu (Resource Browser → `ArmaReforger/Prefabs/...`), sauf les `SRP_` qui sont dans `SimpleRP/Prefabs/Props/Base/`. Vérifié dans les prefabs extraits du jeu (1.8).

### Intérieur des bâtiments (lumière chaude, néons blancs)

| Prefab | Chemin | Type | Usage | Réglage |
|---|---|---|---|---|
| **SRP_Plafonnier** | `SimpleRP/Prefabs/Props/Base/SRP_Plafonnier.et` | interactive (dérivé de `LightCeiling_01`) | plafonnier chaud des chambrées, bureaux, PC, infirmerie | rien : déjà `LIT`, action « Allumer / Éteindre la lumière », origine au plafond, la lampe pend de 80 cm |
| **SRP_Neon** | `SimpleRP/Prefabs/Props/Base/SRP_Neon.et` | interactive (dérivé de `LightIndustrial_02`) | néon blanc des garages, ateliers, couloirs, soute | rien : déjà `LIT`, action « Allumer / Éteindre le néon » |
| LightCeiling_01_on | `Prefabs/Props/Furniture/LightCeiling_01_on.et` | statique (deux `LightEntity` enfants : ambiance radius 10 + spot 150° radius 8, couleur chaude 1 / 0.87 / 0.70) | plafonnier chaud, version du jeu | rien ; suit le générateur, pas d'interrupteur |
| LightIndustrial_02_on_interior | `Prefabs/Structures/BuildingParts/Lights/LightIndustrial_02_on_interior.et` | statique (point radius 5 + spot 140° radius 6, verre blanc) | néon blanc, version du jeu | rien ; suit le générateur |
| LightIndustrial_02_red_on_interior | `Prefabs/Structures/BuildingParts/Lights/LightIndustrial_02_red_on_interior.et` | statique | néon rouge (PC de nuit, soute, salle radio) | rien |
| LampSurgical_01_on | `Prefabs/Props/Medical/LampSurgical_01_on.et` | statique (`LightEntity` LV 2.9 radius 27) | scialytique de l'infirmerie | rien ; suit le générateur |
| LampFluorescent_01 | `Prefabs/Props/Furniture/LampFluorescent/LampFluorescent_01.et` | **décor seul** : aucune lumière dans le prefab (juste le tube et sa destruction) | tube fluorescent de décor | ne compte pas sur lui pour éclairer ; poser un `SRP_Neon` à côté |
| LampInterior_01 | `Prefabs/Props/Furniture/LampInterior_01.et` | **décor seul** : aucune lumière | suspension de décor | idem |
| LightWall_01 | `Prefabs/Props/Furniture/LightWall_01.et` | **décor seul** | applique de décor | idem |
| LampMedical_01 | `Prefabs/Props/Services/Healthcare/LampMedical_01.et` | **décor seul** | lampe d'examen de décor | idem |
| Lamp_Interactive | `Prefabs/Props/Military/Camps/Lamp_Interactive.et` | interactive **à pétrole** (`SCR_LampComponent`, flamme, LV 6) | lampe de camp posée sur une table, une caisse | `Initial Light State = LIT` si tu la veux allumée au chargement ; indépendante du générateur |
| LanternMilitary_US_01 | `Prefabs/Props/Military/Camps/LanternMilitary_US_01.et` | interactive **à pétrole** (`SCR_LampComponent`, LV 2, radius 4) | lanterne portable ; c'est aussi la lanterne posée par le script la nuit | idem |

### Entrées, cour, parking (mâts, projecteurs, lampadaires)

| Prefab | Chemin | Type | Usage | Réglage |
|---|---|---|---|---|
| GeneratorFloodlight_US_01 | `Prefabs/Props/Military/Generators/GeneratorFloodlight_US_01.et` | interactive (`SCR_BaseInteractiveLightComponent`, spot 120° radius 20, LV 7, couleur chaude) | projecteur sur groupe électrogène : entrées, poste de garde, parking | **`Initial Light State = LIT`** sur chaque instance ; action « Allumer / Éteindre » du jeu ; suit le générateur |
| LightTower_01 | `Prefabs/Structures/Industrial/Towers/LightTower_01/LightTower_01.et` | automatique (4 `LightEntity` enfants avec `StreetLampComponent`) | grand mât à quatre projecteurs : cour, hélisurface | rien ; s'allume tout seul à la nuit, coupé par le générateur (« cassé ») |
| LampStreet_E_01_4m | `Prefabs/Structures/Infrastructure/Lamps/LampStreet_E_01/LampStreet_E_01_4m.et` | automatique (`StreetLampComponent`, spot 160° radius 20, lumière verdâtre 0.78 / 1 / 0.66) | lampadaire de 4 m le long des allées et des entrées | rien ; automatique à la nuit, coupé par le générateur |
| LampIndustrial_01_on_exterior | `Prefabs/Structures/BuildingParts/Lights/LampIndustrial_01_on_exterior.et` | automatique (`StreetLampComponent`, lumière orange) | applique extérieure au-dessus des portes des hangars | rien ; automatique à la nuit, coupé par le générateur |
| LampIndustrial_01_on_mine | `Prefabs/Structures/BuildingParts/Lights/LampIndustrial_01_on_mine.et` | automatique | variante | rien |

Le « générateur » vanilla `GeneratorPortable_US_01.et` est un décor muet (pas de lumière, un contexte d'action vide) : c'est lui que `SRP_Generateur.et` reprend en y ajoutant nos actions et notre composant. Ne pas poser le vanilla à la place du nôtre.

### Combien ?

Autant qu'il en faut : le script ne fixe pas de limite. Repère : un plafonnier ou un néon par pièce (deux pour un hangar), un projecteur ou un lampadaire à chaque entrée, un mât pour la cour. Une trentaine de sources lumineuses sur la base ne pose pas de problème ; au-delà d'une centaine, surveille les performances de nuit (chaque lumière dynamique coûte).

## 4. Marche à suivre dans le Workbench

1. Ouvrir le monde `Worlds/EveronSimpleRP/EveronSimpleRP.ent` (World Editor).
2. Dans la fenêtre **Layers** (Hierarchy), clic droit sur le dossier **BaseFR** → **Create Layer** → nom **`Eclairage`**. Le fichier `EveronSimpleRP_Layers/BaseFR/Eclairage.layer` est créé. Cocher ce calque pour qu'il soit **actif** (les objets posés vont dedans).
3. **Resource Browser** → chercher le prefab (par exemple `SRP_Plafonnier`, `LightCeiling_01_on`, `GeneratorFloodlight_US_01`) → glisser dans la scène. Dans les bâtiments : le poser au plafond (les plafonniers ont leur origine au point de fixation ; utiliser la touche de snap au sol puis remonter en Y, ou taper la hauteur dans Object Properties). Vérifier avec **Play** de nuit (`/heure 23` en jeu, ou le composant jour/nuit).
4. Pour un prefab interactif du jeu (`GeneratorFloodlight_US_01`, `Lamp_Interactive`, `LanternMilitary_US_01`) : sélectionner l'instance → Object Properties → composant `SCR_BaseInteractiveLightComponent` (ou `SCR_LampComponent`) → **Initial Light State = LIT**. Les `SRP_Plafonnier` / `SRP_Neon` sont déjà réglés.
5. **Le générateur** : glisser `SimpleRP/Prefabs/Props/Base/SRP_Generateur.et` **au garage**, dans le même calque, au sol, à un endroit accessible (les actions apparaissent quand on regarde le boîtier, côté panneau de commande, à 1,5 m). Un seul générateur. Il doit être à moins de 300 m du marqueur `SRP_SpawnBase` (comme tout le reste).
6. Décor indestructible : les props vanilla ont une destruction (verre qui casse). Pour les rendre indestructibles, sur chaque instance : composant `SCR_DestructionMultiPhaseComponent` → décocher **Enabled**. Le générateur, lui, reste destructible (c'est voulu : détruit = panne).
7. Enregistrer (Ctrl+S) : le calque `Eclairage.layer` est un fichier à part, à ajouter au dépôt avec le reste.
8. Sur `Prefabs/MP/Modes/Plain/SRP_GameMode.et` (déjà fait dans cette version) : composants `SRP_GenerateurComponent` (rayon 300 m, marqueur `SRP_SpawnBase`, prefab de la lanterne, prefab du générateur) et `SCR_NightModeGameModeComponent` (Allow global night mode coché).

## 5. Vérifier en jeu

- Au démarrage, le journal (`profile/SimpleRP/journal/`) écrit `[BASE] Générateur : allumé au démarrage, N lampe(s) dans un rayon de 300 m`. Si N est 0 : les lampes sont hors rayon, ou le marqueur `SRP_SpawnBase` n'est pas là où tu crois.
- Regarder le générateur : « Couper le générateur » → toute la base s'éteint, message aux joueurs à moins de 300 m. « Démarrer le générateur » → tout se rallume. Une lampe interactive éteinte à la main reste éteinte jusqu'au prochain « Démarrer » ou à un « Allumer ».
- Tirer une roquette sur le générateur : panne, l'action « Réparer le générateur » (30 s) apparaît pour un Logi / officier / Staff ; les autres ont un message de refus.
- Redémarrer le serveur avec le générateur coupé : la base doit rester dans le noir (`generateur.json`).
- Menu Staff (quand le bouton sera ajouté) : état, forcer démarrer / couper / panne / réparer, mode nuit du Game Master.

## 6. Limites connues

- Le verre « allumé » des plafonniers et néons (`_on` et `SRP_`) reste lumineux quand la lampe est éteinte : c'est un matériau, pas une lumière. Détail visible de près seulement.
- Une lampe statique ne se rallume pas à la main : seuls les prefabs interactifs ont l'action.
- Un joueur qui se connecte pendant une coupure reçoit l'état 2 s après le chargement (à vérifier en multijoueur : c'est le point le moins sûr du script).
- Les `SRP_Plafonnier` / `SRP_Neon` sont écrits sans Workbench ouvert : si le Workbench râle à l'ouverture du prefab (composant en rouge), ouvrir le prefab, corriger la ligne signalée et enregistrer ; les valeurs de lumière viennent des prefabs `_on` du jeu.
