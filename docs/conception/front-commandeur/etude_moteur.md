# Étude de faisabilité moteur : grille de contrôle de territoire (Reforger 1.8, Enforce)

Je n'ai rien modifié.

Chemins utilisés :
- **Scripts vanilla** (notés `V\`) : `%USERPROFILE%\AppData\Local\Temp\claude\C--Users-goule-Documents\e1f1bd8a-58e8-4a5d-8f58-bc003292c2d8\scratchpad\vanilla\scripts\`
- **Mod** (noté `M\`) : `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\`

## 0. Ce qui existe déjà dans le mod

**Système actuel (`M\SRP_Territory.c`)** :
- Environ 100 secteurs circulaires, de rayon 150, 250 ou 350 m.
- Tout se passe sur le serveur, avec un tick de 5 s (`TICK_MS`).
- Capture : aucun ennemi dans le rayon et au moins un joueur pendant 10 min.
- Affichage : un marqueur statique par secteur (`PlaceMarker` → `SCR_MapMarkerManagerComponent.InsertStaticMarker`).
- Sauvegarde : `JsonSaveContext` dans `$profile:SimpleRP/territoire.json`, avec des clés du type `s<i>_owner`.

**Le mod ne dessine rien sur la carte.** Il n'y a aucune occurrence de `CanvasWidget`, `PolygonDrawCommand`, `LineDrawCommand`, `SCR_MapModuleBase` ou `SCR_MapUIBaseComponent`. Tout l'affichage est à écrire.

**Ce qui est déjà en place et réutilisable :**
- **Noms des localités, côté serveur** : `SRP_Civilians.CreateAutoZonesOfType` appelle `SCR_MapEntity.GetMapInstance().GetByType(items, EMapDescriptorType.MDT_NAME_CITY/TOWN/VILLAGE/SETTLEMENT)` puis `WidgetManager.Translate(item.GetDisplayName())`.
- **Modèle réseau** : RPC `Broadcast` pour les changements, plus `RplSave`/`RplLoad` pour les joueurs qui arrivent (`SRP_Sirene.c`, `SRP_Breche.c`, `SRP_Generateur.c`).
- **Carrés de 100 m** : `SRP_Fleet.c` `GridOf` calcule `floor(x/100)`, `floor(z/100)`.

## 1. Dessin sur la carte du jeu

### API vérifiée

- **`V\Core\generated\UI\CanvasWidget.c`**
  - `SetDrawCommands(array<ref CanvasWidgetCommand>)` ne garde qu'un pointeur (« The caller needs to keep the array alive »).
  - Autres méthodes : `LoadTexture(ResourceName)`, `TessellateCircle`, `TessellateEllipse`, `TessellateRoundedRectangle`.
  - Regroupement en lots : les commandes consécutives du même type et de même texture sont groupées. Changer de type ouvre un nouveau lot. `ImageDrawCommand` n'est jamais groupée.
- **`V\Core\proto\EnWidgets.c`**
  - `PolygonDrawCommand` : `m_iColor` (ARGB), `m_Vertices` au format `[x0,y0,x1,y1…]`, `m_pTexture`, `m_fUVScale`.
  - `TriMeshDrawCommand` : `m_Vertices` et `m_Indices`.
  - `LineDrawCommand` : `m_fWidth`, `m_fOutlineWidth`, `m_iOutlineColor`, `m_bShouldEnclose`, `m_pTexture` et `m_UVScale` (utiles pour des pointillés).
  - Aussi : `TextDrawCommand`, `ImageDrawCommand`, `CompositeDrawCommand`.
- **`V\Core\generated\UI\CanvasWidgetBase.c`** : `PixelPerUnit`, `GetZoom`/`SetZoom`, `PosToPixels`, `SetOffsetPx`, `SetSizeInUnits`.
- **`V\Game\Map\SCR_MapConstants.c`**
  - `CANVAS_COMMAND_VERTICES_LIMIT = 400`, commenté « hardcoded in ENF ».
  - `DRAWING_WIDGET_NAME = "DrawingWidget"` : le canvas vanilla déjà présent dans la carte.
- **`V\Game\Map\SCR_MapEntity.c`**
  - Conversions : `WorldToScreen(wx, wz, out int sx, out int sy, bool withPan)` (inverse l'axe Y, sort des coordonnées écran à l'échelle DPI), `WorldToScreenCustom(..., targetPPU, withPan)`, `ScreenToWorld`.
  - Vue : `GetCurrentZoom()` (en pixels par mètre), `GetMapVisibleFrame(out min, out max)` (rectangle du monde visible), `GetMapCursorWorldPosition`, `GetMapSizeX`/`GetMapSizeY`.
  - Événements statiques : `GetOnMapOpen`, `GetOnMapClose`, `GetOnMapPan`, `GetOnMapZoom`, `GetOnMapZoomEnd`.
  - `UpdateMap(timeSlice)` appelle `Update()` de chaque module ou composant actif à chaque image.
- **Modules et composants de carte** : `SCR_MapModuleBase` et `SCR_MapUIBaseComponent` (`Init`, `Update`, `OnMapOpen`/`OnMapClose`). Ils sont déclarés dans `SCR_MapConfig.m_aModules` / `m_aUIComponents`, dans des fichiers comme `MapFullscreen.conf` ou `MapSpawnMenu.conf`, choisis par `SCR_MapConfigComponent` sur le game mode.

### Exemples vanilla de dessin

- **`V\Game\Camera\Components\SCR_MapDescriptorManualCameraComponent.c`** : un `PolygonDrawCommand` semi-transparent (couleur `0 0 0 0.2`). Ses sommets viennent de `WorldToScreenCustom(..., GetCurrentZoom(), true)` et `SetDrawCommands` est appelé à chaque image. C'est le modèle direct pour une case.
- **`V\Game\Map\Modules\SCR_MapSelectionModule.c`** : récupère le canvas `DrawingWidget`, puis appelle `SetZoom(GetCurrentZoom()/PixelPerUnit())` et `SetOffsetPx(-pan)`.
- **`SCR_WaypointLinesEditorUIComponent.c`** : utilise `LineDrawCommand`.

### Comment Conflict dessine

Conflict ne dessine ni polygone ni zone, et il n'existe aucune « ligne de front » dans le vanilla (recherches front, frontline, influence, territor : rien d'utile).
- **Liens radio entre bases** : natifs, par `MapItem.LinkTo(MapItem)` → `MapLink.GetMapLinkProps()` → `SetLineWidth`, `SetLineColor`, `SetLineType(EMapLineType.LT_DASHED…)`. Voir `SCR_RadioCoverageMapDescriptorComponent.CreateLinks` et `ColorMapLink` : largeur 3, opacité 0,3, qui passe à 1 au survol (`SCR_GraphLinesData`).
- **Zone d'une base** : un cercle natif, `MapItem.SetRange()` avec `MapDescriptorProps.SetBackgroundColor(couleur de faction, alpha 0,1)` (`SCR_CampaignMilitaryBaseMapDescriptorComponent.MapSetup`).
- **Icônes** : des widgets repositionnés au pan et au zoom (`SCR_MapUIElementContainer.UpdateIcons` : `WorldToScreen` puis `FrameSlot.SetPos`).

### Architecture proposée côté client

- **Accrochage sans toucher aux .conf**, deux façons :
  - un `modded class SCR_MapEntity` qui surcharge `OnMapOpen` et `UpdateMap` (méthodes protégées) ;
  - ou un objet client abonné à `SCR_MapEntity.GetOnMapOpen()`, qui crée son canvas :
    - soit par `GetGame().GetWorkspace().CreateWidget(WidgetType.CanvasWidgetTypeID, …)` (existence vérifiée dans `WorkspaceWidget.c` et `EnWidgets.c`),
    - soit par un petit `.layout` du mod contenant un `CanvasWidget` plein écran.
- **Variante plus propre** : un `SCR_MapUIBaseComponent` ajouté en surchargeant `MapFullscreen.conf` et `MapSpawnMenu.conf` dans le mod.
- **Le même crochet sert partout** : carte plein écran, carte de réapparition et carte Game Master (`config.MapEntityMode` = `FULLSCREEN`, `SPAWNSCREEN`, `EDITOR`).
- **Trois couches, dans cet ordre** (pour ne pas casser les lots) :
  1. Remplissage : `PolygonDrawCommand` par rectangle, ou `TriMeshDrawCommand` par couleur. Couleurs ARGB semi-transparentes, par exemple `0x404A7BFF` pour le bleu et `0x40D03030` pour le rouge.
  2. Contours de zones : `LineDrawCommand` fin, avec `m_bShouldEnclose`.
  3. Ligne de front : `LineDrawCommand` épais avec contour. Pointillés possibles avec une texture `.edds`.
- **Recalcul des sommets** : tous les exemples vanilla travaillent en pixels écran. Il faut donc recalculer à l'ouverture de la carte, puis sur `GetOnMapPan`/`GetOnMapZoom` (ou dans `Update` seulement si le pan ou le zoom a changé), pas à chaque image.

### Coût d'affichage

- **Jamais un widget par case.** Contre-exemple vanilla : `SCR_MapDotCircleHandler` crée un `ImageWidget` par point, ce qui est lourd.
- **Fusion des cases** : un rectangle par suite de cases de même couleur sur chaque ligne. Mesure sur le relief d'Everon, avec un front diagonal à deux couleurs :
  - environ 136 rectangles à 200 m et environ 300 à 100 m ;
  - dans le pire cas (damier), autant de rectangles que de cases de terre.
- **Front** : les arêtes entre case bleue et case rouge, chaînées en polylignes par parcours des coins (marching squares). Pour un front qui traverse l'île : environ 53 arêtes à 200 m, 100 à 100 m.
- **Culling** : ne dessiner que ce qui est dans `GetMapVisibleFrame`.
- **Niveau de détail selon `GetCurrentZoom`** : île entière à environ 0,08 px/m en 1080p (une case de 200 m = 17 px, 100 m = 8 px, 50 m = 4 px).
  - Vue d'ensemble : zones fusionnées et front seulement.
  - Zoom moyen : les cases.
  - Zoom fort : le quadrillage et les noms de zones.
- **Ordre de grandeur (estimation, non mesuré)** : moins de 2 000 sommets recalculés par pan ou zoom, c'est négligeable. 20 000 (cases de 100 m sans fusion) deviendrait sensible en script pendant un zoom. La fusion est donc obligatoire.

### Manette et console

C'est faisable pour l'affichage. Un `CanvasWidget` ne fait que du rendu, en script seul, sans DLL. Une info au survol d'une case reste possible avec le curseur manette (`SCR_MapCursorModule` et `GetMapCursorWorldPosition`).

### Plan B pour les lignes uniquement

Une chaîne de `MapItem` créés par `CreateCustomMapItem` et reliés par `LinkTo`. C'est natif et sans recalcul au zoom, mais je ne l'ai pas testé pour cet usage.

### Non vérifié

Les layouts vanilla n'ont pas été extraits, donc deux points restent à tester au Workbench :
- l'ordre d'affichage exact : le canvas doit être au-dessus du fond de carte et sous les icônes, sans doute près de `DrawingWidget` ;
- si `SetZoom`/`SetOffsetPx` du canvas transforment les sommets automatiquement, ce qui éviterait tout recalcul. C'est à tester en premier.

## 2. Données et réseau

**Everon** : terrain de 12 800 × 12 800 m. La valeur est codée en dur dans `SRP_MapCapture.c` ; mieux vaut la lire par `SCR_MapEntity.GetMapSizeX/Y()` ou `MapEntity.Size()`.

**Part de mer** : mesurée sur `E:\Projets\PAG-Bot\carte\relief_everon.png` (4 096 px, masque mer « bleu − rouge > 40 », la même règle que `fabriquer_tuiles.py`). Résultat : 68,5 % de mer, environ 51,6 km² de terre.

| Case | Grille | Cases au total | Cases avec ≥ 10 % de terre | ≥ 50 % de terre | Instantané à 2 bits par case de terre |
|---|---|---|---|---|---|
| 50 m | 256 × 256 | 65 536 | 21 316 | 20 635 | environ 5,3 Ko |
| 100 m | 128 × 128 | 16 384 | 5 460 | 5 149 | environ 1,4 Ko |
| 200 m | 64 × 64 | 4 096 | 1 443 | 1 280 | environ 360 o |
| 250 m | 51 × 51 (ne tombe pas juste : 51,2) | 2 601 | 933 | 812 | environ 235 o |
| 500 m | 25 × 25 (ne tombe pas juste : 25,6) | 625 | 249 | 195 | environ 62 o |

Environ 64 à 67 % des cases sont en mer, quelle que soit la taille.

**Masque terre/mer** : calculé une fois au démarrage, de la même façon sur le serveur et les clients, en comparant `BaseWorld.GetSurfaceY(x,z)` à `GetOceanBaseHeight()` (`V\Core\generated\World\BaseWorld.c`). À 200 m, 4 096 cases × 9 points = environ 37 000 appels, ce qui est court. Autre option : un fichier statique livré dans le mod, généré depuis le relief.

### Réplication

Le porteur serait un composant du game mode, comme `SRP_TerritoryComponent` (le game mode a un `RplComponent`).

**Option A : `RplProp` sur un tableau (suffisant à 200 m)**
- Modèle vanilla : `SCR_FactionCommanderHandlerComponent`, avec `[RplProp(onRplName:…)] ref array<int>` et `Replication.BumpMe()`.
- 16 cases par entier (2 bits chacune) : 256 entiers (1 Ko) pour 4 096 cases, ou 91 entiers pour la terre seule.
- Avantage : le rattrapage des joueurs qui arrivent est automatique.
- Inconvénients : tout le tableau repart à chaque changement, et le client ne sait pas quelle case a changé.

**Option B : instantané + changements (recommandée)**
- Modèle : `SCR_DestructibleTreesSynchManager` en vanilla, `SRP_Sirene.c` dans le mod.
- `override bool RplSave(ScriptBitWriter)` / `RplLoad(ScriptBitReader)` pour l'arrivant, avec `WriteIntRange(v, 0, 3)` (2 bits par case) ou `Write(val, bits)`.
- Un `[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)] RpcDo_Cells(array<int> idx, array<int> owners)` pour les changements, groupés par tick. Les paramètres `array<int>` sont acceptés en RPC (par exemple `SCR_VoiceoverSystem.RpcDo_PlaySequenceFor`).
- Quelques octets seulement par capture.

**Points communs aux deux options :**
- Le `Broadcast` ne s'exécute pas sur l'hôte lui-même : il faut appliquer le changement aussi en local (Workbench, serveur hébergé).
- La progression de capture ne concerne que les quelques cases actives : un petit `RplProp` ou un RPC séparé suffit.
- Limite de taille : je n'en ai trouvé aucune documentée dans `EnNetwork.c` ni `Replication.c`. Mieux vaut rester sous quelques Ko par message, ce que l'option B garantit.

### Persistance

Oui, et le mod le fait déjà : `SRP_Territory.c`, fonctions `Save()`/`Load()`, avec `JsonSaveContext`/`JsonLoadContext` vers `territoire.json`.
- `WriteValue(string, void)` accepte n'importe quel type (`V\Game\generated\Plugins\Serialization\SaveContext.c`).
- Le plus sûr reste une chaîne d'un caractère par case (R, B ou N, soit 4 096 caractères à 200 m). On y ajoute la taille de case et l'origine, pour détecter un changement de grille.
- Sauvegarde sur changement avec un délai (par exemple 60 s), plus à la fermeture (`OnDelete`), comme aujourd'hui.
- La même chaîne peut être envoyée au bot par `SRP_Bridge` pour la carte de situation web.

## 3. Calcul côté serveur

### Présence : par entité, pas par case

- Index de case : `idx = floor(z/S) * n + floor(x/S)`.
- **Joueurs** : `PlayerManager.GetPlayers`, puis l'entité contrôlée et sa position.
- **IA ennemie**, deux possibilités :
  - les enregistrements que le mod tient déjà (`SRP_EnemyComponent` : `GroupPosition`, `ActiveAgentsNear`) ;
  - ou `TagSystem.GetTagsInRange(out entities, pos, rayon, ETagCategory.Character)` autour de chaque joueur (`V\Game\generated\System\TagSystem.c`). C'est ce qu'utilise `SCR_SeizingComponent`, et c'est bien plus léger que `QueryEntitiesBySphere`, qui passe par un callback sur toutes les entités.
- Coût : proportionnel au nombre de joueurs plus d'IA, quelques centaines d'opérations par tick.
- **À proscrire** : une requête spatiale par case (4 096 requêtes toutes les 5 s).
- **Filtres à reprendre de `SCR_SeizingComponent.EvaluateEntityFaction`** : personnages vivants uniquement ; ignorer un véhicule à plus de 7 m du sol (`m_iMaximumAltitude`) ; ignorer l'IA au niveau de détail maximal (`AIAgent.GetMaxLOD()`).

**Point important pour le mod** : l'IA ennemie n'existe que près des joueurs (garnisons posées à 2 000 m). Le rouge loin du front est donc un état sauvegardé, pas une présence réelle. Seules les cases autour des joueurs changent, et la reprise par le rouge passe par les contre-attaques qui existent déjà.

### Algorithmes (tous linéaires en nombre de cases ; n = 4 096 à 200 m)

- **Adjacence** : 4 voisins pour le front et la connexité, 8 pour tester les encerclements.
- **Règle de progression de proche en proche** : on ne peut capturer qu'une case voisine d'une case bleue. C'est l'équivalent en grille de `IsHQRadioTrafficPossible` dans Conflict.
- **Front** : une case est au front si l'une de ses 4 voisines est de l'autre camp. La ligne se construit à partir des arêtes bleu/rouge chaînées. Mise à jour locale : la case changée et ses 4 voisines.
- **Zones**, deux choix :
  - **statiques**, calculées une fois et identiques sur les clients : chaque case de terre est rattachée à la localité la plus proche parmi `MDT_NAME_CITY/TOWN/VILLAGE/SETTLEMENT`, puis `MDT_NAME_LOCAL/HILL/RIDGE/VALLEY` pour la campagne. Le nom vient automatiquement ;
  - **dynamiques** : composantes connexes par parcours en largeur.
- **Encerclement** : parcours en largeur depuis la case de la base (Levie) à travers les cases bleues. Une case bleue non atteinte est coupée. Une poche rouge est une composante rouge sans « source » rouge (à définir : QG ennemis, dépôts, bord de carte).
- **Influence** (optionnel) : diffusion vers les voisines, une passe linéaire.

### Fréquences

- **Présence** : toutes les 2 à 5 s. Le tick actuel du territoire est de 5 s ; `SCR_SeizingComponent` utilise 1 s quand il y a du monde et 3 s au repos, avec ±20 % d'aléa pour étaler la charge.
- **Topologie** : seulement quand une case change.
- **Réseau** : envois groupés par tick.
- **Sauvegarde** : différée.

Ordre de grandeur (estimation, non mesuré) : un parcours de 4 096 cases fait environ 16 000 tests de voisins, de l'ordre de la milliseconde en script. À 65 536 cases (50 m), il faudrait l'étaler sur plusieurs images. Dans tous les cas, réutiliser les tableaux plutôt que d'en allouer à chaque tick.

## 4. Systèmes vanilla réutilisables

- **`SCR_SeizingComponent`** (`V\Game\GameMode\Components\SCR_SeizingComponent.c`)
  - Réglages par défaut : `m_iRadius` 100 ; `m_fMinimumSeizingTime` 6 et `m_fMaximumSeizingTime` 10, la durée descend au minimum à partir de 5 attaquants nets (`m_iMaximumSeizingCharacters`).
  - Règles : attaquants moins défenseurs, `m_bGradualTimerReset`, `m_bCapturingRequiresPlayer`, horodatages répliqués en `RplProp WorldTimestamp`.
  - C'est un composant par entité : inutilisable tel quel pour des milliers de cases, mais sa règle de décompte se recopie dans le calcul par case.
- **`SCR_CampaignMilitaryBaseComponent`**
  - Rayon : `m_iRadius`, un `RplProp` défini dans `SCR_MilitaryBaseComponent`.
  - Portée radio : `m_fRadioRange`, en `RplProp`.
  - `IsHQRadioTrafficPossible` : une chaîne de couverture jusqu'au QG, qui sert de modèle pour une règle de ravitaillement ou d'adjacence.
  - `SCR_CoverageRadioComponent` : un `RplProp` de type `array<string>`.
- **Capture & Hold** : `SCR_CaptureArea` (`ScriptedGameTriggerEntity`, `OnActivate`/`OnDeactivate`, occupants par faction). Valable pour quelques zones, pas pour une grille.
- **Combat Ops** : rien de spécifique. Le dossier `CombatOps` ne contient que `SCR_FastTravelAction` ; les objectifs passent par le Scenario Framework (`SCR_ScenarioFrameworkArea`), qui est de la logique de mission, pas du territoire.
- **Noms** : `SCR_MapEntity.GetByType` puis `MapItem.GetDisplayName()`, déjà utilisé côté serveur par le mod. `SCR_MapDescriptorComponent` est vide : il hérite de `MapDescriptorComponent` (`GetBaseType`, `Item()`). `SCR_EditableEntityComponent` n'est pas nécessaire.
- **Affichage natif** : `MapItem.LinkTo` avec `MapLinkProps` pour les lignes, `MapItem.SetRange` pour les cercles, `MapDescriptorProps.SetBackgroundColor`.
- **Références de carré** : `SCR_MapEntity.GetGridLabel`/`GetGridPos`, en 100 m par défaut (`resMin = 2`).

## 5. Conclusion

### Taille de case recommandée : 200 m

**Côté technique :**
- 200 m divise 12 800 exactement (64 × 64) et donne 1 443 cases de terre.
- Instantané d'environ 360 o, changements de quelques octets.
- 150 à 400 rectangles à dessiner.
- Parcours complets de l'ordre de la milliseconde (estimation).
- Sauvegarde de 4 096 caractères.

**Côté jeu :**
- Aligné sur le quadrillage de la carte (2 × 2 carrés de 100 m, 5 cases par km) et sur les « carrés » que le mod annonce déjà.
- Une section tient une case ; un village fait 4 à 9 cases, une ville 15 à 25.
- 17 px par case sur l'île entière en 1080p : le front reste lisible.

**Les autres tailles :**
- **100 m** reste faisable techniquement (5 460 cases de terre), mais avec fusion et envoi par changements indispensables. Avec peu de joueurs, le rythme serait lent : il faudrait faire basculer les cases par zones ou par encerclement.
- **250 m et 500 m** ne tombent pas juste et ne s'alignent pas sur les carrés de 100 m. 500 m est trop grossier (un village = une case).
- **50 m** : 21 000 cases de terre, beaucoup de bruit et un coût inutile.

### Difficulté par élément

- **Facile** : la grille, le masque terre/mer, la présence par entité, la capture par case, le front par arêtes, la sauvegarde JSON, la réplication par instantané + RPC, les noms automatiques des zones.
- **Moyen** :
  - la couche `CanvasWidget` calée sur le pan et le zoom (échelle DPI, axe Y inversé, recalcul sur événements, ordre d'affichage) ;
  - la fusion des rectangles et le chaînage du front ;
  - le niveau de détail selon le zoom et le découpage à 400 sommets ;
  - le lien avec les garnisons et les contre-attaques, et l'équilibrage.
- **Difficile** :
  - un front « joli » (lissage, hachures avec une texture `.edds`) ;
  - une IA qui tient vraiment un front continu sur toute l'île, alors qu'elle n'existe que près des joueurs (il faudrait une IA « virtuelle ») ;
  - des règles d'encerclement et de ravitaillement bien réglées.
- **Impossible ou à éviter** :
  - un widget ou un marqueur `SCR_MapMarkerBase` par case ;
  - une entité déclencheur ou un `SCR_SeizingComponent` par case ;
  - recalculer 20 000 sommets à chaque image ;
  - un `RplProp` géant renvoyé souvent ;
  - colorer directement le fond de carte natif (`MapWidget`) : je n'ai trouvé aucune API pour ça.

### Pièges moteur propres à ce sujet

- `SetDrawCommands` ne copie rien : garder le tableau et les commandes en membres `ref`.
- 400 sommets au plus par commande (`SCR_MapConstants`).
- Alterner les types de commandes ou les textures casse les lots.
- `WorldToScreen` inverse l'axe Y, renvoie des entiers, en coordonnées DPI.
- La carte s'initialise avec une image de retard (`FRAME_DELAY`) : faire le premier calcul dans `OnMapOpen`, pas dans `OnMapInit`.
- Le canvas est à recréer à chaque ouverture de carte ; se désabonner à la fermeture.
- Le RPC `Broadcast` ne s'exécute pas sur l'hôte.
- Deux points restent à tester au Workbench :
  - si le zoom et le décalage propres au `CanvasWidget` transforment les sommets tout seuls ;
  - où placer le canvas pour qu'il soit sous les icônes.