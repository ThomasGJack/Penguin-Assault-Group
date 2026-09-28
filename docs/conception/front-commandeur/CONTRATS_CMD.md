# CONTRATS DU COMMANDEUR ENNEMI (carte #76) — figés

Ce document accompagne les 10 squelettes du Commandeur écrits dans `code/SimpleRP/` (contrats figés, corps vides qui
compilent et rendent les valeurs votées du front). Il fait foi pour tous les modules qui codent le Commandeur ou
l'appellent. Ordre de préséance : arbitrages de Jack (`plan/arbitrages.txt`, C1 à C8, Q1 à Q10) > `INTERFACES_CORE.md`
(partie COMMANDEUR) > `CONTRATS_FRONT.md` (pour le front) > ce document > consignes `par_fichier/*.md` (les consignes
sont souvent périmées par les trous tranchés : noms, frein, stocks, réglages ; ce document dit laquelle vaut).

Fichiers : `SRP_CmdSettings.c` (COMPLET), `SRP_Commander.c`, `SRP_CmdRegions.c`, `SRP_CmdResources.c`, `SRP_CmdDepot.c`,
`SRP_CmdCapacity.c`, `SRP_CmdManeuvers.c`, `SRP_CmdSupport.c`, `SRP_CmdArmor.c`, `SRP_CmdScreens.c` (le nom
`SRP_EnemyCommandScreens.c` des consignes est abandonné).

Contrôle statique (`python enforce_check.py SimpleRP`, tout le dossier) : **0 remarque** après la relecture du 26/09
(le doublon `SRP_DepotInstallAction` est retiré de la copie de SRP_Territory.c). Contrôles maison en plus
(`_gen/relecteur_returns.py`, `_gen/relecteur_membres.py`) : aucun retour manquant, aucune variable déclarée deux fois
dans une fonction, aucun membre en double par classe, accolades équilibrées, aucun nom interdit, aucun signe pour
cent, aucun `override`, aucun `?:`.

**Relecture du 26/09 (deux relecteurs), corrections portées dans le code ET ici** : `CheckLiberation(zone, staff,
nowUnix)` public, SEULE porte OF11, appelée par `OnZoneCaptured` seul (rien de visible si staff) ;
`OnZoneOwnerChanged(zone, owner, staff)` aux ressources et aux dépôts (zone forcée : dépôt déplacé en silence) ;
`SRP_CmdManeuvers.IsOn()` supprimé au profit de `SRP_FrontEnemyComponent.IsCommanderActive()` ; débit des envoyés au
DÉPART (camion hors colonne : `OnTruckLaunched` ; colonne : `NewColumn`) et UNE porte de retour
(`SRP_EnemyComponent.RetireTick`) ; camions en route sauvés et rendus (RE13, `cmd_ma_t*`) ; `ca_vagues` et
`res_heures_bombe` supprimées (une clé par chiffre) ; `ForceNextHeli` et `RequestTransfer` supprimés (ForceOrder seule
porte des décisions) ; actions Staff des corrections de moyens branchées dans `SRP_CmdScreens.RunStaff` ; ancien
système de place (SoldierCap, GetSoldierReserve, marge d'IA des camions) retiré au §8 ; menace locale pour jeeps,
camions de vague et patrouilles (`ThreatFactor(position)`, §8).

## 1. Décisions prises ici (en plus des trous tranchés)

1. **Forme.** Aucun composant neuf, aucun GUID. `SRP_Commander : Managed` est créé par `SRP_FrontEnemyComponent.OnPostInit`
   (serveur, hors éditeur) dans `m_Commander`. Il tient ses sous-modules : `m_Book` (`SRP_CmdRegionBook`), `m_Resources`
   (`SRP_CmdResources`, qui tient `m_Depots` = `SRP_CmdDepots`), `m_Maneuvers` (`SRP_CmdManeuvers`), `m_Support`
   (`SRP_CmdSupport`, qui tient `m_Armor` = `SRP_CmdArmorManager`). Accès : `SRP_Commander.Get()`,
   `SRP_Commander.Get().GetBook()`, `SRP_CmdResources.Get()`, `SRP_CmdManeuvers.Get()`, `SRP_CmdSupport.Get()` —
   tous rendent **null sans Commandeur** (toujours tester). `SRP_Commander` est `Managed` : son pointeur statique
   retombe à null quand l'hôte le libère ; `Stop()` le remet aussi à null.
2. **Capacité à part.** `SRP_CmdCapacity.Get()` est un singleton **créé à la demande** avec les valeurs de Jack : la
   règle Q7 (120 soldats, 160 IA) vaut même sans Commandeur ou Commandeur gelé. Le Commandeur ne fait que l'appeler
   (DeclareSettings, LoadSettings, Start, Tick, Stop).
3. **Statiques.** `SRP_CmdLog`, `SRP_CmdIntel` et `SRP_CmdScreens` sont statiques ; comme les statiques survivent aux
   parties du Workbench, le constructeur du Commandeur appelle `SRP_CmdLog.Clear()` et `BuildRegions` appelle
   `SRP_CmdIntel.Reset(régions)`.
4. **Réglages : `SRP_CmdSettings.c` est COMPLET** (seul fichier codé en entier, étape 1 du plan). Lecteur unique des deux
   fichiers du profil (`front_*` -> `front_reglages.txt`, le reste -> `commandeur_reglages.txt`), format « clé = valeur
   # aide », contrôle du type, clés inconnues ignorées, clés absentes ajoutées en fin de fichier (fichier absent : écrit
   en entier, groupé par section, avec un mode d'emploi). **Les `DeclareSettings` et `LoadSettings` des 9 classes sont
   déjà écrits** (386 clés, générés par `code/_gen/cmd_settings.py`, source du §7) : le défaut d'une clé est la valeur
   initiale de son champ (une seule source), relue par `GetIntClamped` / `GetFloatClamped` / `GetBool` / `GetString`.
   Pour ajouter une clé : le champ, sa ligne `Declare*` et sa ligne `Get*` dans la même classe, puis la ligne dans
   `cmd_settings.py`. Les champs de réglage sont PUBLICS : un sous-module peut lire ceux d'un autre (ex.
   `m_Commander.m_iAimS`), jamais les écrire hors de `LoadSettings`.
5. **Une clé par chiffre (trou 8), fusions faites :**
   - frein C2 : `renfort_frein_min` (frein_minutes, renfort.frein_minutes, renfort_frein_zone_min supprimés) ;
   - part gardée par la source : `renfort_source_garde_centiemes` = 50 et `renfort_source_garde_prudent_centiemes` = 67
     (frein_garde_centiemes, trait_prudent_garde_source, renfort.source_garde* supprimés) ;
   - hélico AP7 : `cmd_helico_sans_troupe_m` = 800 (appui.helico.personne_m = 600 supprimé) ; pour viser :
     `cmd_contact_vise_s` = 120 (appui.contact_vu_s = 90, mortier_contact_s, appui.helico.contact_s supprimés) ;
     appui lourd justifié : `cmd_contact_confirme_s` = 300 et `cmd_pertes_appui_s` = 600 ;
   - bombe AP3 : `appui_bombe_seuil_joueurs` = 10 joueurs VALIDES à 3 km et `appui_bombe_intervalle_h` = 24 h
     GLISSANTES, qui est AUSSI le temps pour refaire la bombe du stock (res_bombe_joueurs_min, res_bombes_par_jour,
     res_heures_bombe supprimés) ; cible : `cmd_bombe_joueurs` = 4 (cerveau) ;
   - vagues de contre-attaque prévues : `front_attaque_vagues` du front (3), Commandeur gelé OU actif (ca_vagues
     supprimée) ; `ca_vagues_min` / `ca_vagues_max` ne bornent que la révision CA3 ;
   - gratuité d'une décision forcée : `cmd_staff_gratuit` = 1 (res_staff_gratuit, vu_forcer_puise supprimés) ;
   - journal : `vu_journal_memoire` = 200 (journal_staff_lignes = 40 supprimé) ; refus répétés : `vu_refus_repos_min`
     (appui.refus_journal_min) ;
   - région désorganisée : `renfort_facteur_desorg` = 2, appliqué par les MANŒUVRES seules (desorg_renfort_facteur,
     renfort.facteur_desorganisee supprimés) ; officier lent : `cmd_trait_lent_retard_s` = 75, appliqué par le CERVEAU
     seul (renfort.facteur_lent supprimé) — trou 12 ;
   - gravité MA9 : barème du cerveau `cmd_grav_*`, plus `cmd_grav_contre_attaque` = 2 (PROPOSÉ : terme exigé par le
     trou 10, aucun chiffre de Jack) et les zones servies `cmd_grav_zones_servies*` ; gravite.* des manœuvres supprimés ;
   - CA7 : `res_offensive_*` seuls (nuit.* supprimés) ; rupture à 60 min (C8) : `front_attaque_duree_max_minutes` du
     front (ca.abandon_minutes supprimé) ; plafond de 60 soldats par localité : `cap_garnison_max`
     (officier_plafond_garnison, repli.plafond supprimés) ; regarnissage MA7 : `front_regarnissage_heures`
     (repli.fonte_heures supprimé) ;
   - feinte MO8 : `ca_feinte` = 25 (appui.feinte.chance supprimé) ; Su-57 en contre-attaque : `ca_avion_chance` = 50 ;
   - C5 : `appui_mortier_fumee_compte` = 0 (fumigènes gratuits, hors plafonds) ; C4 : aucune clé appui.informateurs
     (règle dure : un signalement de civil ne fait JAMAIS tirer) ;
   - supprimés car contraires à C1 (infanterie gratuite) ou CO5 (jamais la vraie position) : sections et camions en
     stock, renfort.joueurs_min, renfort.bonus_feu, renfort.feu_secondes, renfort.contact_secondes, frein_section_suit
     (RetireSectionFor est supprimé : la source n'est jamais une garnison posée) ; frein_chargement_min devient
     `renfort_chargement_min` ;
   - interrupteurs : `appui_actif`, `renfort_actif`, `repli_actif`, `harcelement_actif`, `info_actif`, plus le gel VU6
     (état sauvé `cmd_gel`, départ `cmd_gel_au_depart`) ; manoeuvre.actif et appui.surveillance_s supprimés.
   - retard radio OF3 : `cmd_radio_retard_min` = 5, lu par `SRP_Commander.RadioDelayMinutes(fallback)` dans
     `SRP_TerritoryComponent.TryRadioCall` (l'attribut `m_iRadioDelayMinutes` n'est plus que le repli sans Commandeur).
6. **Énumérations** (toutes dans `SRP_Commander.c`, trou 3) : SRP_ECmdTrait, SRP_ECmdMood, SRP_ECmdOfficerState,
   SRP_ECmdRegionState, SRP_ECmdReport, SRP_ECmdOrder (fusion tranchée, 13 valeurs), SRP_ECmdMissionStatus,
   SRP_ECmdStock, SRP_ECmdCapClass, SRP_ECmdShell, SRP_ECmdArmorReason, SRP_ECmdArmorState, SRP_ECmdPieceState,
   SRP_ECmdAssaultMode, SRP_ECmdRole, SRP_ECmdColumnState, SRP_ECmdColumnKind, SRP_ECmdLogKind, SRP_ECmdIntelSource.
   Supprimées : SRP_ECmdSupport, SRP_ECmdTemper, SRP_ECmdPriority, SRP_ECmdEnd (la fin d'attaque est
   `SRP_EAttackEnd` du front). Renommée : SRP_EIntelSource -> **SRP_ECmdIntelSource**. `SRP_ECmdRole` est élargi
   (OFFICIER, GARDE_OFFICIER, SERVANT, EQUIPAGE, GARDE_DEPOT) pour que `IsFree` n'ait qu'UN test
   (`m_iCmdRole != AUCUN`).
7. **Noms retenus contre les consignes** : `SRP_CmdLog` (pas SRP_EnemyCommandLog), `SRP_CmdIntel` (pas SRP_EnemyIntel),
   `SRP_CmdScreens` (pas SRP_EnemyCommandScreens), `SRP_CmdRegionBook` / `SRP_CmdRegion` / `SRP_CmdOfficer`,
   `SRP_CmdRegionSupply` (part d'une région, tenue par les RESSOURCES), `SRP_CmdDepots` / `SRP_CmdDepot`,
   `SRP_CmdArmorManager` / `SRP_CmdArmor` / `SRP_CmdArmorType`. Sauvegarde : **WriteTo / ReadFrom partout** (jamais
   WriteState / ReadState). Horloge : **Tick(int nowUnix) partout**, sans CallLater propre (trou 23).
8. **Pas de répartiteur** (trou 2) : `SRP_Commander.IssueOrder` appelle directement le moyen.

   | Ordre (SRP_ECmdOrder) | Appel direct |
   |---|---|
   | RENFORT | `SRP_CmdManeuvers.RequestReinforcement(zone, aim, size, reason, author, staff)` |
   | ENQUETE | `SRP_CmdManeuvers.RequestSearch(aim, zone, reason, staff)` |
   | HARCELEMENT | `SRP_CmdManeuvers.RequestHarass(aim, reason, author, staff)` |
   | COLONNE | `SRP_CmdManeuvers.RequestColumn(aim, reason, author, staff)` |
   | DECROCHAGE | `SRP_CmdManeuvers.RequestRetreat(aim, reason, author, staff)` |
   | MORTIER | `SRP_CmdSupport.RequestMortar(region, aim, SRP_ECmdShell.EXPLOSIF, variant (0 = défaut), reason, staff)` |
   | FUMEE | `SRP_CmdSupport.RequestMortar(region, aim, SRP_ECmdShell.FUMEE, variant, reason, staff)` |
   | FEINTE | `SRP_CmdSupport.RequestFeint(region, aim, reason, staff)` |
   | ARTILLERIE | `SRP_CmdSupport.RequestArtillery(region, aim, -1, reason, staff)` (contre-attaque : zone attachée) |
   | BOMBE | `SRP_CmdSupport.RequestBomb(aim, reason, staff)` |
   | AVION | `SRP_CmdSupport.RequestFlyby(aim, reason, staff)` |
   | BLINDE | `SRP_CmdSupport.RequestArmor(region, aim, raison (STAFF si staff), zone, variant, staff)` |
   | HELICO | `SRP_CmdSupport.RequestHeli(aim, reason, staff)` |

   Chaque `Request*` rend `""` si le moyen part, sinon la raison du refus ; le cerveau écrit `SRP_CmdLog.Decision` ou
   `SRP_CmdLog.Refusal`. Rideau de décrochage et d'assaut : `SRP_CmdSupport.RequestSmoke(region, from, threat, reason,
   staff)` (appelé par les manœuvres). Le soutien d'une contre-attaque (CA4) et la feinte (MO8, par le 2e axe RÉEL)
   sont DÉCIDÉS par `SRP_CmdManeuvers.Support` et exécutés par ces mêmes `Request*` (trou 9) ; les appuis ne choisissent
   aucune cible (PickMortarTarget, PickStrongpoint, PickBombTarget, SupportAssault n'existent pas).
9. **Staff : une seule entrée** (trou 15) : `SRP_Commander.ForceOrder(int kind, int variant, vector position, string
   author)` (ordre `m_bStaff` : sans seuil, plafond, délai, gel ni désorganisation, gratuit si `cmd_staff_gratuit` ;
   JAMAIS sans la sécurité : base, mer, soldats ennemis ou civils trop près, 120 soldats). Plus `SetFrozen`,
   `ForceOfficerDown`, `ForceOfficerReplace`, `CancelOperations`, `ReloadSettings` (relaie
   `SRP_FrontComponent.ReloadSettings`, qui relit les DEUX fichiers et tous les LoadSettings). Les consignes
   ForceMeans, ForceSupport, ForceReinforcement, ForceRetreat, ForceHarass sont abandonnées ; `ForceNextHeli` et
   `RequestTransfer` aussi (relecture du 26/09 : `ForceOrder(HELICO)` passe outre stock, repos et « personne sur
   place » ; `ForceOrder(COLONNE)` déplace des soldats entre localités). Les CORRECTIONS de moyens (pas des décisions)
   ont leurs actions dans `SRP_CmdScreens.RunStaff` : `stock-plein` (StaffRefillAll), `stock:<genre>:<R3|ile>:<0|plein>`
   (StaffSetStock), `renflouer:R3` (RequestResupply), `depot-deplacer:R3` (StaffMove), `depot-saboter:R3`
   (StaffSabotage), `coupure-lever:R3` (StaffClearCut), `depot-reveler:R3` (Reveal, sans radio),
   `interdictions:R3|tout` (ClearBans).
10. **Stocks (C1, trou 4).** Ordre des paramètres TOUJOURS `(kind, region)`, region = -1 pour les moyens de l'île.
    Dépense par tickets `Reserve` -> `Consume` (effet réel) ou `Release` (annulation, retour). Les appuis ne gardent
    que des numéros de ticket (`m_iTicket`) ; au redémarrage, SEULS les tickets ouverts (ressources), les colonnes en
    route et les camions de renfort hors colonne pas encore déchargés (manœuvres, `cmd_ma_c*` et `cmd_ma_t*`) sont
    rendus, une fois, dans `ReadFrom` (RE13). La capacité ne sauve rien.
11. **Frein C2 et effectifs (trous 5, 6, 7).** Les manœuvres SEULES tiennent le frein (`CanReinforceZone`, 10 min par
    zone, heure Unix, non sauvé) ; le cerveau l'interroge avant d'émettre un RENFORT ; le camion d'entrée #69 passe par
    `RequestEntryTruck` (même frein). Les soldats envoyés sont comptés à part des pertes (`SRP_FrontEnemyComponent`
    `AddSent` / `ReturnSent` / `ClearSent`, `AddBonus`, champs de `SRP_FrontLocality`). **Débit AU DÉPART, une seule
    fois** : camion HORS colonne -> `OnTruckLaunched` (appelé par les camions seulement si `m_iColumnId == 0`) ;
    colonne (papier ou réelle) -> `NewColumn` ; les camions d'une colonne ne débitent jamais. Arrivée d'une colonne
    sur le papier : `AddBonus(destination)` sans rien rendre ; colonne annulée : `ReturnSent(source)`. **Retour des
    soldats vivants : UNE seule porte**, `SRP_EnemyComponent.RetireTick` (groupe dont `m_sSourceLocality` n'est pas
    vide, si `renfort_rendre_survivants`) ; `ContactGroupsTick` ne fait que `RetireLater`, `OnColumnDelivered` ne rend
    rien. La source garde la moitié de son NOMINAL (deux tiers avec un officier prudent) et n'est jamais une garnison
    posée.
12. **Valeurs votées dans le squelette** (chaque point de décision rend la règle du front tant que son corps n'est pas
    codé) : `SRP_FrontEnemyComponent.IsCommanderActive()` = faux (SEULE définition de « Commandeur actif » ; le front
    applique ses règles : camion d'alerte #69, AdvanceAssault, vague réputée arrivée) ; `CaWindowDecision` = départ à D ; `PickByIntel` = -1 ; `PlannedWaves` = `m_iAttackMaxWaves` du
    front ; `ReviseWaves` = vagues prévues ; `WaveRoomGroups` = 1000 ; `WantPaperWave` = faux ; `CheckEnd` = AUCUNE ;
    `NightChance` = chance votée ; `ThreatAt` = menace de l'île ; `GarrisonFactorAt` = 1 ; capacité : `Room`,
    `GroupRoom`, `EngineRoom` = 1000, `Refusal` et `Ask` = "" (pas de borne en plus), `GarrisonCap` = décidé ;
    `ForceOrder` et les `Request*` rendent « … pas encore branché(s) ».
13. **C7** : si les menottes ACE ne marchent pas sur un officier IA inconscient, AUCUNE action de remplacement ; il ne
    peut alors qu'être tué (3 h). **C3** : blindé pris non ramené = disparaît au redémarrage (rien de sauvé).
    **C4** : un signalement de civil ne donne qu'une fouille (ENQUETE, ou HELICO si aucune unité n'est à 800 m).
    **C6** : `SetWantedGarrisons` + `GarrisonCap` ; une garnison posée n'est jamais réduite sous les yeux des joueurs.
    **C8** : fin aux deux tiers de pertes = `SRP_EAttackEnd.DEFENDUE` (menace -1, radio « zone défendue ») rendue par
    `CheckEnd` ; seule la rupture à 60 min (front) laisse la menace inchangée.
14. **Verrou du 2S1** : le `modded class SCR_GetInUserAction` n'est PAS dans le squelette (étape 11) ; texte exact au §8,
    à ajouter dans `SRP_CmdArmor.c` par le codeur des blindés.

## 2. Classes et méthodes publiques, fichier par fichier

Généré depuis les squelettes (`code/_gen/api_cmd.py` : signature exacte + la ligne `//!`, qui dit la règle et l'appelant). Les méthodes `protected` sont dans le code avec leur ligne `//!` ; les champs de réglage sont au §7.

### SRP_CmdSettings.c (682 lignes)


**class SRP_CmdSettings**
  - champ `static const string PATH_CMD = "$profile:SimpleRP/commandeur_reglages.txt"`
  - champ `static const string PATH_FRONT = "$profile:SimpleRP/front_reglages.txt"`
  - champ `static const string FRONT_PREFIX = "front_"`
- `static void Declare(string key, string defaultValue, string help)` — Déclare une clé (mise en minuscules) avec son défaut et son aide ; « front_* » -> front_reglages.txt, sinon commandeur_reglages.txt ; type deviné d'après le défaut (entier, décimal ou texte) ; une clé déjà déclarée est ignorée — modules du front (défaut = attribut.ToString()), SRP_MissionManagerComponent.DeclareFrontSettings
- `static void DeclareInt(string key, int value, string help)` — Déclare une clé entière (valeur du champ comme défaut) — DeclareSettings des modules
- `static void DeclareFloat(string key, float value, string help)` — Déclare une clé décimale (valeur du champ comme défaut) — DeclareSettings des modules
- `static void DeclareBool(string key, bool value, string help)` — Déclare un interrupteur (écrit 1 ou 0) — DeclareSettings des modules
- `static void DeclareString(string key, string value, string help)` — Déclare une clé texte (prefab, liste « a, b, c ») ; toute valeur est acceptée, vide compris — DeclareSettings
- `static bool IsDeclared(string key)` — La clé est-elle déclarée ? — modules (contrôles), SRP_CmdScreens
- `static bool IsFrontKey(string key)` — La clé va-t-elle dans front_reglages.txt ? — Reload
- `static string Reload(string author)` — Serveur : relit les DEUX fichiers, contrôle les valeurs, ajoute les clés déclarées absentes, puis journal ; rend le compte rendu (« Réglages relus (Jack) : 212 valeur(s) lue(s), 3 ajoutée(s), 0 problème ») ; ne relance aucun LoadSettings (c'est l'appelant qui le fait) — SRP_FrontComponent.Start (« démarrage »), ReloadSettings
- `static string GetStatusText()` — « commandeur_reglages.txt et front_reglages.txt lus à 20:01, 0 problème » (ou « pas encore lus ») — SRP_CmdScreens (page principale), SRP_FrontScreens
- `static int GetReadUnix()` — Heure Unix de la dernière lecture (0 = jamais) — Staff
- `static int GetProblems(notnull array<string> lines)` — Problèmes du dernier Reload (valeurs illisibles, clés inconnues, bornes) ; rend leur nombre — SRP_CmdScreens
- `static string GetString(string key)` — Texte brut : valeur lue, sinon le défaut déclaré ; clé non déclarée : "" et problème noté — Get*
- `static int GetInt(string key)` — Entier (un décimal est arrondi, oui/vrai = 1, non/faux = 0) ; illisible : défaut déclaré, sinon 0 — modules
- `static float GetFloat(string key)` — Décimal (virgule acceptée) ; illisible : défaut déclaré, sinon 0 — modules
- `static bool GetBool(string key)` — Interrupteur : vrai pour 1 (ou tout entier non nul), oui, vrai, true — modules
- `static void GetList(string key, notnull array<string> result)` — Liste « a, b, c » (le point-virgule vaut une virgule) : éléments sans espaces autour, vides retirés — modules
- `static int GetIntClamped(string key, int min, int max)` — Entier borné : une valeur hors bornes est ramenée dans [min, max] et signalée une fois par Reload — LoadSettings des modules
- `static float GetFloatClamped(string key, float min, float max)` — Décimal borné, même règle — LoadSettings des modules

### SRP_Commander.c (1202 lignes)

- `enum SRP_ECmdTrait { PRUDENT=0, AUDACIEUX=1, LENT=2, METHODIQUE=3 }` — Caractère d'un officier (CO7) ; METHODIQUE = caractère neutre, sans effet
- `enum SRP_ECmdMood { PRUDENTE=0, NORMALE=1, AGRESSIVE=2 }` — Humeur d'une région (CO7), recalculée à chaque cycle rapide
- `enum SRP_ECmdOfficerState { VIVANT=0, TUE=1, CAPTURE=2, DISPARU=3 }` — État d'un officier de région (OF2 à OF11)
- `enum SRP_ECmdRegionState { ACTIVE=0, DESORGANISEE=1, LIBEREE=2 }` — État d'une région (OF6, OF7, OF11)
- `enum SRP_ECmdReport { CONTACT=0, PERTE=1, CARRE_PERDU=2, CARRE_REPRIS=3, INFORMATEUR=4 }` — Nature d'un compte rendu en route vers le Commandeur (OF3)
- `enum SRP_ECmdOrder { RENFORT=0, MORTIER=1, FUMEE=2, FEINTE=3, ARTILLERIE=4, BOMBE=5, AVION=6, BLINDE=7, HELICO=8, ENQUETE=9, HARCELEMENT=10, COLONNE=11, DECROCHAGE=12 }` — Ordre du Commandeur (fusion tranchée, trou 3) : manœuvres (RENFORT, ENQUETE, HARCELEMENT, COLONNE, DECROCHAGE) ou appuis (MORTIER, FUMEE, FEINTE, ARTILLERIE, BOMBE, AVION, BLINDE, HELICO)
- `enum SRP_ECmdMissionStatus { EN_COURS=0, TOMBE=1, CAPTURE=2, ECHAPPE=3, SANS_OBJET=4 }` — Suivi de la mission OFFICIER (OF10) pour SRP_Missions
- `enum SRP_ECmdStock { OBUS=0, BLINDE=1, HELICO=2, ARTILLERIE=3, BOMBE=4 }` — Les SEULS stocks du Commandeur (arbitrage C1 : l'infanterie et les camions sont gratuits)
- `enum SRP_ECmdCapClass { COMBAT=0, GARNISON=1, POSTE=2, GARDE=3, PATROUILLE=4, HORS_COMPTE=5 }` — Classe de place d'un groupe ennemi (Q7, RE11, RE12) ; rangée dans SRP_EnemyGroup.m_iCapClass
- `enum SRP_ECmdShell { EXPLOSIF=0, FUMEE=1 }` — Obus de mortier (MO7, C5 : la fumée est gratuite)
- `enum SRP_ECmdArmorReason { CONTRE_ATTAQUE=0, RENFORT=1, RIPOSTE=2, STAFF=3 }` — Raison d'un blindé (AP5)
- `enum SRP_ECmdArmorState { ROUTE=0, ENGAGE=1, REPLI=2, PRENABLE=3, PRIS=4, LIVRE=5, DETRUIT=6 }` — Cycle de vie d'un blindé (AP4 à AP6, C3)
- `enum SRP_ECmdPieceState { PRETE=0, EN_BATTERIE=1, EN_TIR=2, MUETTE=3 }` — État d'une pièce de mortier ou d'une batterie d'une région (Staff)
- `enum SRP_ECmdAssaultMode { FACE=0, FLANC=1, DEUX_AXES=2 }` — Mode d'un assaut (MA8)
- `enum SRP_ECmdRole { AUCUN=0, ASSAUT=1, BASE_FEU=2, MANOEUVRE=3, COUVERTURE=4, REPLI=5, RENFORT=6, RAID=7, OFFICIER=8, GARDE_OFFICIER=9, SERVANT=10, EQUIPAGE=11, GARDE_DEPOT=12 }` — Rôle donné par le Commandeur à un groupe (SRP_EnemyGroup.m_iCmdRole). Tout rôle différent d'AUCUN exclut le groupe de l'entraide (SRP_EnemyAwareness.IsFree) : officier et ses gardes, servants, équipages, replis, assauts, raids.
- `enum SRP_ECmdColumnState { PAPIER=0, REELLE=1, ARRIVEE=2, PERDUE=3 }` — État d'une colonne (MA4)
- `enum SRP_ECmdColumnKind { REPLI=0, RENFORT=1, STAFF=2 }` — Nature d'une colonne (MA4, MA6, VU5)
- `enum SRP_ECmdLogKind { DECISION=0, APPUI=1, REFUS=2, OFFICIER=3, STOCK=4, RENSEIGNEMENT=5, STAFF=6, SYSTEME=7 }` — Nature d'une ligne du journal des décisions (VU4) ; teintes Staff : APPUI r, DECISION y, REFUS d, STAFF g, autres w
- `enum SRP_ECmdIntelSource { AUCUNE=0, PC=1, OFFICIER=2, MISSION=3 }` — Source d'un renseignement des joueurs (VU2, RE9) ; aussi le paramètre source de SRP_FrontRadio.IntelGained

**class SRP_CmdRing**
  Compteur glissant d'une heure, une case par minute Unix (tirs, pertes, carrés perdus et repris d'une zone)
  - champ `ref array<int> m_aCounts = {}` — 60 cases
  - champ `int m_iMinute` — minute Unix (nowUnix / 60) de la case 0
- `void Add(int nowUnix, int amount)` — Décale jusqu'à la minute nowUnix / 60 (cases dépassées remises à 0), puis ajoute amount — SRP_Commander.Integrate
- `int Sum(int nowUnix, int minutes)` — Somme des minutes récentes (1 à 60) — gravité, humeur, IsHeavySupportJustified, GetZoneShots/Losses
- `void Clear()` — Tout à 0 — remise à plat, restauration

**class SRP_CmdReport**
  Un compte rendu en route vers le Commandeur (OF3). Jamais sauvegardé (H3).
  - champ `int m_iKind = SRP_ECmdReport.CONTACT`
  - champ `int m_iRegion = -1`
  - champ `int m_iZone = -1`
  - champ `int m_iCell = -1`
  - champ `vector m_vPos` — déjà floutée pour un contact, exacte pour une perte ou un carré
  - champ `int m_iPlayers`
  - champ `bool m_bSeen`
  - champ `bool m_bVehicle`
  - champ `bool m_bArmed`
  - champ `int m_iShots`
  - champ `int m_iDueUnix` — livraison au plus tôt
  - champ `string m_sSource` — « poste », « Chotain », « unité mobile », « camion », « hélico », « civil », « assaut »
  - champ `bool m_bNeedsRadio` — unité d'une localité : passe par la radio de la localité (TryRadioCall)
  - champ `vector m_vRadioPos`
  - champ `bool m_bRadioLogged` — retard déjà écrit au journal
  - champ `bool m_bDirect` — hélico et vagues : parlent droit au Commandeur, même sans officier
  - champ `IEntity m_Informant` — le civil (C4), pour annuler s'il est mort, inconscient ou menotté

**class SRP_CmdContact**
  Ce que le Commandeur sait d'un groupe de joueurs (CO5, version du cerveau + m_iRegion, trou 3). Jamais sauvegardé.
  - champ `vector m_vPos` — position connue (floutée)
  - champ `vector m_vFirstPos` — gardée tant que la position reste à 100 m (joueurs immobiles, AP2)
  - champ `int m_iZone = -1`
  - champ `int m_iCell = -1`
  - champ `int m_iRegion = -1`
  - champ `int m_iPlayers` — joueurs ESTIMÉS
  - champ `bool m_bSeen` — vu (sinon seulement entendu)
  - champ `bool m_bInformantOnly` — ne vient que d'informateurs : fouille seulement, jamais d'appui (C4)
  - champ `bool m_bArmed` — véhicule armé (AP5)
  - champ `bool m_bVehicle`
  - champ `int m_iFirstUnix`
  - champ `int m_iLastUnix` — dernier compte rendu
  - champ `int m_iSeenUnix` — dernière fois VU (0 = jamais)
  - champ `int m_iReports`
  - champ `bool m_bInvestigated` — ENQUETE déjà donnée

**class SRP_CmdZoneIntel**
  Mémoire du Commandeur par zone (index = numéro de zone du socle). Jamais sauvegardée (H3).
  - champ `ref SRP_CmdRing m_Shots = new SRP_CmdRing()`
  - champ `ref SRP_CmdRing m_Losses = new SRP_CmdRing()`
  - champ `ref SRP_CmdRing m_CellsLost = new SRP_CmdRing()`
  - champ `ref SRP_CmdRing m_CellsRetaken = new SRP_CmdRing()`
  - champ `int m_iLastReportUnix`
  - champ `int m_iLastContactUnix`
  - champ `int m_iLastSeenUnix`
  - champ `int m_iLastInformantUnix` — C4 : un signalement par zone toutes les info_zone_repos_s
  - champ `int m_iArmedSeenUnix` — AP5 riposte
  - champ `vector m_vArmedPos`
  - champ `float m_fGravity` — MA9, recalculée au cycle rapide
  - (1 champs de réglage : voir §7)

**class SRP_CmdOrder**
  Un ordre du Commandeur, exécuté aussitôt ou à m_iNotBefore (officier lent). Jamais sauvegardé.
  - champ `int m_iKind = SRP_ECmdOrder.RENFORT`
  - champ `int m_iVariant = -1` — BLINDE : type (0 BTR-70, 1 BRDM-2, 2 Typhoon, -1 tirage) ; MORTIER, FUMEE : obus (0 = défaut)
  - champ `int m_iRegion = -1`
  - champ `int m_iZone = -1`
  - champ `vector m_vAim`
  - champ `int m_iSize` — RENFORT : soldats voulus (0 = au choix des manœuvres)
  - champ `float m_fDelayFactor = 1` — réflexe local d'une région désorganisée : desorg (appliqué par les manœuvres)
  - champ `int m_iNotBefore` — heure Unix (officier lent : + cmd_trait_lent_retard_s)
  - champ `string m_sReason`
  - champ `string m_sAuthor` — vide = le Commandeur
  - champ `bool m_bStaff` — VU5 : sans seuil, plafond ni délai ; jamais sans la sécurité

**class SRP_Commander : Managed**
  LE CERVEAU. Serveur seulement. Point d'accès unique : SRP_Commander.Get() (null si absent : les accroches statiques ne font alors rien). Managed : le pointeur statique retombe à null quand l'hôte le libère.
- `void SRP_Commander(SRP_FrontEnemyComponent host)` — Serveur, OnPostInit de l'hôte : s_Instance = this, m_Host ; crée m_Book, m_Resources, m_Maneuvers, m_Support (constructeurs seuls), puis DECLARE les clés (DeclareSettings ici et dans chaque sous-module, capacité et écrans compris) ; ne trace rien, ne lit rien — SRP_FrontEnemyComponent.OnPostInit
- `static SRP_Commander Get()` — Le Commandeur, null s'il est absent (client, composant ennemi absent, avant OnPostInit) — tout le mod
- `void Stop()` — Serveur, OnDelete de l'hôte, APRÈS SRP_FrontComponent.SaveFinalFromEnemy (sauvegarde finale complète) : Stop des sous-modules (ressources, appuis et leurs CallLater, capacité), puis s_Instance = null ; aucune écriture ici — SRP_FrontEnemyComponent.OnDelete
- `SRP_FrontEnemyComponent GetHost()` — L'hôte (composant ennemi du front) — sous-modules
- `SRP_CmdRegionBook GetBook()` — Régions et officiers — SRP_CmdScreens, SRP_Missions (OF10), SRP_FrontMarkers, sous-modules
- `SRP_CmdResources GetResources()` — Stocks et dépôts — SRP_CmdResources.Get, SRP_CmdScreens
- `SRP_CmdManeuvers GetManeuvers()` — Manœuvres — SRP_CmdManeuvers.Get
- `SRP_CmdSupport GetSupport()` — Appuis — SRP_CmdSupport.Get
- `void BuildRegions()` — Socle, AVANT Load : m_Book.Trace() (tracé OF1, lignes « region » de front_retouches.txt, rapport commandeur_regions.txt), m_aIntel dimensionné à GetZoneCount() + 1, SRP_CmdIntel.Reset(régions) — SRP_FrontComponent.Start
- `void Start()` — Serveur, 1er Tick de l'hôte qui voit le front prêt : m_aCellSeen copié, SRP_CmdCapacity.Get().Start() (limite 160, groupes 36/6), m_Book.Start (officiers manquants), m_Resources.Start, m_Maneuvers.Start, m_Support.Start, premières échéances des cycles ; journal « Commandeur prêt : 6 régions, 6 officiers » — SRP_FrontEnemyComponent.Tick
- `bool IsStarted()` — Start déjà fait — SRP_FrontEnemyComponent.Tick, IsCommanderActive
- `void Tick(int nowUnix)` — Toutes les 5 s (après AttackTick de l'hôte) : front absent ou pas prêt -> rien ; capacité (TOUJOURS, même gelé) ; version du front changée -> DiffCells, m_Book.OnFrontChanged (cachettes) ; CollectGroupReports, ProcessPending, ExpireKnowledge ; m_Book.Tick ; informateurs (info_periode_s) ; ressources toutes les 60 s ; m_Maneuvers.Tick, m_Support.Tick ; FastCycle et SlowCycle à leur échéance si non gelé — SRP_FrontEnemyComponent.Tick
- `void LoadSettings()` — Relit ses clés (champs publics ci-dessus) puis LoadSettings de m_Book, m_Resources (et dépôts), m_Maneuvers, m_Support (et blindés), SRP_CmdCapacity.Get(), SRP_CmdScreens — SRP_FrontEnemyComponent.LoadSettings (après SRP_CmdSettings.Reload)
- `string ReloadSettings(string author)` — Bouton « commandeur:relire » : SRP_FrontComponent.GetInstance().ReloadSettings(author) (relit les DEUX fichiers et tous les LoadSettings) ; rend le compte rendu, en rappelant que cmd_regions et les bornes de région ne changent qu'au redémarrage — SRP_CmdScreens.RunStaff
- `bool IsFrozen()` — Commandeur gelé : aucune décision, aucun appui ; la capacité et les règles du front continuent — tout le mod
- `string SetFrozen(bool frozen, string author)` — VU6 : gèle ou dégèle (sauvé, cmd_gel, MarkDirty(true)) ; au gel, les réflexes en file sont vidés ; journal StaffAction ; aucune radio ; rend le compte rendu — SRP_CmdScreens.RunStaff
- `string ForceOrder(int kind, int variant, vector position, string author)` — VU5, SEULE entrée pour forcer une décision : ordre m_bStaff (sans seuil, plafond ni délai, gratuit si cmd_staff_gratuit ; jamais sans la sécurité : base, soldats ou civils trop près, 120 soldats), passé directement au moyen (IssueOrder). variant : BLINDE -> type (0 BTR-70, 1 BRDM-2, 2 Typhoon, -1 tirage) ; MORTIER, FUMEE -> obus (0 = défaut) ; sinon -1. HELICO (outre stock, repos et « personne sur place ») et COLONNE (RequestColumn) passent aussi par ici : aucune autre porte Staff pour une décision (ForceNextHeli et RequestTransfer n'existent pas). Marche aussi gelé. Rend le résultat ou le refus — SRP_CmdScreens
- `string ForceOfficerDown(int region, bool captured, string author)` — VU5 : chute forcée de l'officier (tué ou capturé), même suite qu'une vraie chute (radio, désorganisation, renseignement si capturé) — SRP_CmdScreens.RunStaff
- `string ForceOfficerReplace(int region, string author)` — VU5 : nouvel officier tout de suite (fin de la désorganisation, correction silencieuse, VU2) — SRP_CmdScreens.RunStaff
- `string CancelOperations(string author)` — ia-reset et wipe : réflexes et comptes rendus vidés, m_Maneuvers.CancelAll (colonnes rendues, papier, raids, replis), m_Support.CancelAll (pièces, batteries, blindés retirés, tickets rendus), officiers posés remis « sur le papier » ; rend le compte rendu — SRP_AdminComponent (ia-reset, wipe)
- `void OnControllableDestroyed(notnull SCR_InstigatorContextData data)` — Serveur : victime = data.GetVictimEntity() ; officier (m_Book.OnPossibleOfficerDeath) -> fini ; joueur -> ignoré (CO5) ; soldat ennemi reconnu par son GROUPE enregistré (SRP_EnemyComponent.FindRecord, jamais la clé de faction, trou 22) -> compte rendu PERTE — SRP_FrontEnemyComponent.OnControllableDestroyed
- `void OnZoneCaptured(int zone, bool staff)` — Zone tombée (D1) ou forcée bleue : alerte alerte_zone de sa région (hors staff), m_Book.CheckLiberation(zone, staff, maintenant) (SEULE porte OF11), m_Resources.OnZoneOwnerChanged(zone, BLEU, staff) (qui relaie aux dépôts), m_Maneuvers.OnZoneCaptured(zone, staff) (MA11 hors staff) ; staff : état seul, sans alerte ni annonce (Q9) — SRP_FrontEnemyComponent.OnZoneCaptured
- `void OnZoneLost(int zone, int reason, bool staff)` — Zone repassée rouge : rattachement OF11 si sa région est libérée (m_Book.AttachZone), m_Resources .OnZoneOwnerChanged(zone, ROUGE, staff), m_Maneuvers.OnZoneLost ; staff : état seul (Q9) — SRP_FrontEnemyComponent.OnZoneLost
- `void OnLocalityTaken(string locality)` — Localité prise : cachettes de l'officier revues (m_Book), replis et colonnes réorientés (m_Maneuvers) — SRP_FrontEnemyComponent.OnLocalityTaken
- `void OnMissionSucceeded(int missionType, vector site, string missionId, string targetPrefab, string author)` — UNE entrée par mission réussie (trou 14) : renseignement si le site est sur un carré rouge (SRP_CmdIntel.OnMissionSucceeded, qui révèle aussi le dépôt pour ECOUTE et DOCUMENTS) ; quart de stock retiré pour SABOTAGE et CACHE (m_Resources.OnSabotageMission) ; alerte alerte_sabotage comptée UNE fois — SRP_FrontEnemyComponent.OnMissionSucceeded
- `bool OnSabotageUsed(IEntity target, int playerId, out string reply)` — Objet utilisé par un joueur (« Saboter ») : dépôt ennemi (m_Resources.GetDepots().TryUse) puis pièce d'appui (m_Support.OnSabotage) ; vrai = consommé, message dans reply — SRP_FrontEnemyComponent.OnSabotageUsed
- `void OnThreatChanged(int threat)` — La menace de l'île a changé : m_Resources.OnThreatChanged (pleins RE10) — SRP_FrontEnemyComponent.OnThreatChanged
- `void ResetCampaign(string reason)` — Nouvelle campagne (B3, K1, Staff) : régions retracées, officiers neufs, alertes à 0, rattachements effacés, connaissance vidée, puis ResetCampaign de m_Resources, m_Maneuvers, m_Support ; SRP_CmdIntel.Reset, SRP_CmdLog.Clear ; m_bFrozen = m_bFrozenAtStart — SRP_FrontEnemyComponent.ResetAll
- `void OnRestored()` — Après RestoreDaily (ReadFrom déjà fait) : officiers posés retirés hors de vue, connaissance et files vidées, m_aCellSeen recopié sans compte rendu, OnRestored des sous-modules — SRP_FrontEnemyComponent.OnRestored
- `void WriteTo(JsonSaveContext ctx)` — Clés cmd_v, cmd_gel, cmd_hash, puis m_Book (cmd_r*), m_Resources (cmd_res_*, puis dépôts cmd_dep_*), m_Maneuvers (cmd_ma_*), m_Support (cmd_ap_*), SRP_CmdIntel (cmd_vu_*) — SRP_FrontEnemyComponent.WriteTo
- `void ReadFrom(JsonLoadContext ctx)` — Relecture miroir, APRÈS BuildRegions, les zones et les localités (régions retrouvées par leur code ; somme de contrôle différente : avertissement). RE13 : tickets ouverts rendus (ressources), colonnes et camions hors colonne en route rendus à leur source (manœuvres), rien d'autre — SRP_FrontEnemyComponent.ReadFrom
- `static void NoteSighting(SRP_EnemyGroup record, IEntity player, vector perceived, bool identified)` — OF3 : un soldat du groupe perçoit un joueur (au plus une fois par passage de 10 s, par unité et par cible) : tampon du groupe (m_bCmdContact, m_bCmdSeen, m_aCmdPlayers, m_vCmdPos, m_bCmdVehicle, m_bCmdArmed) — SRP_EnemyComponent.AlertTick
- `static void NoteShot(SRP_EnemyGroup record)` — CO6 : un tir de joueur entendu par le groupe (m_iCmdShots, 500 au plus) — SRP_EnemyComponent (réaction au tir)
- `static void NoteAirSighting(vector position)` — AP7 : l'hélico voit des joueurs : compte rendu CONTACT vu, direct, flouté — SRP_EnemyComponent.ReportSpotted
- `static int ThreatAt(vector position, int islandThreat)` — CO9 : menace LOCALE = min(10, islandThreat + min(alerte_menace_max, alerte de la région / alerte_menace_par)) ; sans Commandeur : islandThreat — SRP_TerritoryComponent.GarrisonSoldiers (Nominal), SRP_FrontEnemyComponent
- `static float GarrisonFactorAt(vector position)` — OF5 : 1 + officier_bonus_garnison / 100 si l'officier de la région vit, sinon 1 — GarrisonSoldiers (Nominal)
- `static bool HasOfficerTarget()` — OF10 : une mission OFFICIER est possible (un officier VIVANT, non visé, avec une cachette du jour) — SRP_MissionManagerComponent (tirage des missions)
- `static void AddAlertAt(vector position, float amount, string reason)` — CO9 : alerte de la région sous la position (m_Book.AddRegionAlert) — sabotages hors mission, SRP_Missions
- `static int RadioDelayMinutes(int fallback)` — OF3 : retard radio d'une localité dont l'opérateur est tombé = cmd_radio_retard_min si le Commandeur existe, sinon fallback (une clé par chiffre : l'attribut m_iRadioDelayMinutes du territoire n'est plus que le repli) — SRP_TerritoryComponent.TryRadioCall
- `int GetRegionOfZone(int zone)` — Région qui tient ou tenait la zone (OF11 compris), -1 pour la base ou sans région — SRP_FrontScreens, SRP_FrontRadio, sous-modules
- `string GetRegionLabel(int region)` — « région de Saint-Philippe » ou « région d'Entre-Deux-Monts » (VU1) — SRP_FrontRadio, SRP_FrontScreens
- `int GetContacts(int region, int maxAgeSeconds, notnull array<ref SRP_CmdContact> result)` — Contacts de la région (-1 = toute l'île) mis à jour depuis maxAgeSeconds au plus ; rend leur nombre — SRP_CmdArmor (ENGAGE), SRP_CmdScreens
- `bool GetBestContact(int zone, bool seenOnly, int maxAgeSeconds, out vector position, out int players)` — Meilleur contact d'une zone (vu d'abord si seenOnly, puis le plus nombreux, puis le plus récent) — SRP_CmdManeuvers (CA2, MA8), SRP_CmdSupport
- `bool LastKnownContact(int zone, int maxAgeSeconds, out vector position, out int unixTime)` — Dernier contact connu d'une zone et son heure Unix — SRP_CmdManeuvers (MA5, MA8)
- `int KnownPlayersNear(vector position, float radius, int maxAgeSeconds)` — Joueurs ESTIMÉS connus à radius d'un point, contacts de moins de maxAgeSeconds — SRP_CmdManeuvers (MA8, CA1)
- `int GetKnownPlayers(int zone)` — Joueurs ESTIMÉS dans une zone (contacts récents) — gravité, SRP_CmdScreens
- `bool IsContactConfirmed(int zone, int withinSeconds)` — Contact de la zone confirmé (vu, ou 2 comptes rendus) depuis withinSeconds au plus — cycles, SRP_CmdManeuvers
- `bool IsHeavySupportJustified(int zone)` — CO6 : appui lourd justifié = joueur VU depuis moins de cmd_contact_confirme_s, ou pertes depuis moins de cmd_pertes_appui_s — cycles, SRP_CmdSupport
- `int GetZoneShots(int zone, int minutes)` — Tirs entendus dans la zone sur les minutes récentes — gravité, SRP_CmdScreens
- `int GetZoneLosses(int zone, int minutes)` — Pertes ennemies rapportées dans la zone sur les minutes récentes — gravité, SRP_CmdManeuvers (harcèlement)
- `bool WasArmedVehicleSeen(int zone, int withinSeconds, out vector position)` — AP5 riposte : véhicule armé vu dans la zone depuis withinSeconds — SlowCycle, SRP_CmdScreens
- `bool IsPlayerWatchedNear(vector position, float radius, out vector playerPos)` — MO4 : un soldat ennemi vivant et conscient IDENTIFIE en ce moment (4 s au plus) un joueur à moins de radius : sa position dans playerPos (seul usage d'une position vraie : l'observateur la voit) — SRP_CmdSupport.FireNext
- `int GetDosingPlayers(vector position)` — EQ4 SEULEMENT (dosage, jamais pour viser) : capture.CountValidPlayersNear(position, appui_rayon_joueurs_m) — SRP_CmdSupport, SRP_CmdArmor
- `float GetZoneGravity(int zone)` — Gravité de la zone : point clé menacé + carrés perdus + pertes + tirs + joueurs connus + contre-attaque en assaut (cmd_grav_*) — SRP_CmdManeuvers, SRP_CmdScreens
- `bool IsServedZone(int zone)` — Zone servie par les moyens (les cmd_grav_zones_servies plus graves ; la contre-attaque l'est toujours) — cycles, SRP_CmdSupport, SRP_CmdManeuvers
- `int GetZonesByGravity(notnull array<int> zones)` — Zones au contact triées par gravité décroissante ; rend leur nombre — FastCycle, SRP_CmdScreens
- `int GetNextFastSeconds()` — Secondes avant le prochain cycle rapide (0 = gelé ou pas prêt) — SRP_CmdScreens (« Rythme »)
- `int GetNextSlowSeconds()` — Secondes avant le prochain cycle lent — SRP_CmdScreens
- `string GetKnowledgeReport(vector from, float radius)` — Contacts connus autour d'un point (position floutée, joueurs estimés, âge, vu ou entendu, source) — Staff
- `string GetStaffReport()` — Résumé Staff : gel, cycles, comptes rendus perdus ou retardés, contacts, humeur de l'île — SRP_CmdScreens
  - (66 champs de réglage : voir §7)

### SRP_CmdRegions.c (669 lignes)


**class SRP_CmdOfficer**
  Un officier de région (OF2 à OF10). Champs « sauvé » : écrits par SRP_CmdRegionBook.WriteTo (clés cmd_rN_of_*).
  - champ `int m_iSerial` — numéro unique, jamais réutilisé (sauvé)
  - champ `string m_sName` — « Volkov » (sauvé)
  - champ `int m_iTrait = SRP_ECmdTrait.METHODIQUE` — sauvé
  - champ `int m_iState = SRP_ECmdOfficerState.VIVANT` — sauvé
  - champ `int m_iSinceUnix` — arrivée (sauvé)
  - champ `int m_iDownUnix` — chute (sauvé)
  - champ `int m_iHideDay = -1` — numéro de jour de la cachette (DayIndex, sauvé)
  - champ `string m_sHideLocality` — cachette du jour (sauvé)
  - champ `string m_sPrevLocality` — cachette précédente (sauvé)
  - champ `int m_iEscapedUnix` — évasion (mission OFFICIER échouée, sauvé)
  - champ `ref array<string> m_aHideChoices = {}` — cachettes candidates du jour (Staff : « tirée parmi … »)
  - champ `vector m_vHidePos`
  - champ `ref array<ref SRP_EnemyGroup> m_aGroups = {}` — case 0 : l'officier ; case 1 : ses gardes (toujours des ref)
  - champ `IEntity m_Entity` — l'officier posé
  - champ `int m_iNoPlayerSince`
  - champ `int m_iUnconsciousSince`
  - champ `int m_iCaptiveIdleSince`
  - champ `int m_iDeferUntil`
  - champ `bool m_bFleeing`
  - champ `vector m_vFleeTo`
  - champ `string m_sFleeLocality`
  - champ `bool m_bWeDelete` — retrait par nous : pas une chute

**class SRP_CmdRegion**
  Une région (OF1). Champs « sauvé » : clés cmd_rN_* retrouvées par le CODE de la région.
  - champ `int m_iIndex`
  - champ `string m_sCode` — « R1 » (le plus proche de la base) à « R6 »
  - champ `string m_sName` — plus grande localité (VU1)
  - champ `int m_iSeedZone = -1`
  - champ `vector m_vCentroid`
  - champ `ref array<int> m_aZones = {}` — zones rattachées, retouches et rattachements OF11 compris
  - champ `ref array<int> m_aHomeZones = {}` — tracé d'origine (zones de départ, RE2)
  - champ `int m_iState = SRP_ECmdRegionState.ACTIVE` — sauvé
  - champ `int m_iDesorgUntil` — heure Unix (sauvé)
  - champ `bool m_bDesorgCaptured` — sauvé
  - champ `int m_iLiberatedAt` — sauvé
  - champ `float m_fAlert` — CO9, lue par CurrentAlert (sauvé)
  - champ `int m_iAlertAt` — sauvé
  - champ `int m_iAlertStep` — dernier palier de 20 écrit au journal
  - champ `ref SRP_CmdOfficer m_Officer` — null = pas encore d'officier (sauvé)
  - champ `int m_iPrevTrait = -1` — caractère du prédécesseur (sauvé)
  - champ `int m_iMood = SRP_ECmdMood.NORMALE` — recalculée par le cerveau (CO7)
  - champ `int m_iReportsLost` — OF6 (Staff)
  - champ `int m_iReportsDelayed` — OF3 (Staff)
  - champ `int m_iLastReportUnix`
  - champ `string m_sRadioCutLocality` — « radio de Durras coupée encore 3 min » (Staff)
  - champ `int m_iRadioCutUntil`

**class SRP_CmdRegionBook**
  Le registre des régions et des officiers (tenu par SRP_Commander.m_Book), serveur.
- `void SRP_CmdRegionBook(SRP_Commander commander)` — Constructeur seul (aucune lecture) — SRP_Commander (constructeur)
- `void DeclareSettings()` — Déclare ses clés (défaut = valeur du champ) — SRP_Commander.DeclareSettings
- `void LoadSettings()` — Relit ses clés (cmd_regions et les bornes ne servent qu'au prochain tracé, donc au redémarrage) — SRP_Commander.LoadSettings
- `void Trace()` — OF1, AVANT Load, déterministe : graines (la plus éloignée de la base, puis la plus éloignée de la graine la plus proche), croissance, équilibrage (300 passes), numérotation R1..R6, retouches « region », noms, somme de contrôle (h x 131 + région + 2) modulo 8388593, m_iTzOffset, rapport WriteReport — SRP_Commander.BuildRegions
- `void Start(int nowUnix)` — Front prêt : officier créé pour chaque région non libérée qui n'en a pas (nouvelle campagne), cachette du jour choisie — SRP_Commander.Start
- `void Tick(int nowUnix)` — Toutes les 5 s : officiers (remplacement à la fin de la désorganisation, cachette du jour, pose, retrait, fuite, évasion, chute par mort, menottes ou abandon) — SRP_Commander.Tick
- `void OnFrontChanged(int nowUnix)` — La version du front a changé : cachettes tombées (localité de la cachette du jour prise -> nouvelle cachette) SEULEMENT ; les libérations OF11 passent par CheckLiberation — SRP_Commander.Tick
- `void CheckLiberation(int zone, bool staff, int nowUnix)` — OF11, SEULE porte de libération : la région de la zone (ACTIVE ou DESORGANISEE) dont toutes les zones sont bleues -> LIBEREE, officier DISPARU, unités retirées hors de vue, MarkDirty(true) ; SRP_FrontRadio .RegionLiberated et SRP_CmdLog SEULEMENT si !staff (Q9 : une zone forcée par le Staff ne donne ni radio ni évènement region_liberee, l'état seul change) — SRP_Commander.OnZoneCaptured(zone, staff), seul appelant
- `int GetRegionCount()` — Nombre de régions tracées — tout le Commandeur, écrans
- `SRP_CmdRegion GetRegion(int region)` — La région d'index donné, null hors bornes — écrans, sous-modules
- `int GetRegionOfZone(int zone)` — Région qui tient ou tenait la zone (OF11 compris), -1 pour la base ou sans région — tout le Commandeur
- `int GetRegionAt(vector position)` — Région sous une position (zone du carré), -1 en mer ou dans la base — accroches statiques, sous-modules
- `string GetRegionCode(int region)` — Code stable « R3 » (clé de sauvegarde) — sauvegardes, écrans
- `string GetRegionName(int region)` — Nom seul « Saint-Philippe » (plus grande localité, VU1) — écrans, radio
- `string GetRegionLabel(int region)` — « région de Saint-Philippe », « région d'Erquy » (VU1) — radio, écrans, journal
- `int FindRegion(string code)` — Région par code (« R3 », casse ignorée), -1 sinon — lecture de front.json, Staff
- `int GetRegionState(int region)` — SRP_ECmdRegionState de la région (DESORGANISEE levée d'elle-même à m_iDesorgUntil par Tick) — tout le Commandeur
- `bool IsRegionDisorganized(int region)` — OF6 à OF9 : région désorganisée — manœuvres (x renfort_facteur_desorg, OF8), appuis (ni mortier ni artillerie)
- `bool IsRegionAlive(int region)` — Région vivante (ACTIVE ou DESORGANISEE : elle a encore une zone rouge) — ressources, appuis
- `int GetRegionStartZones(int region)` — Zones de départ de la région (tracé d'origine, RE2 : parts des 48 obus) — SRP_CmdResources.Start
- `int GetRegionZones(int region, notnull array<int> zones)` — Zones rattachées à la région ; rend leur nombre — ressources, dépôts, écrans
- `int CountRegionRedZones(int region)` — Zones ROUGES de la région (revenu RE4, OF11) — ressources, écrans
- `int GetHash()` — Somme de contrôle du tracé (sauvée, comparée au chargement) — SRP_Commander.WriteTo, ReadFrom
- `float GetRegionAlert(int region)` — Alerte actuelle = max(0, alerte - (now - m_iAlertAt) x alerte_max / (alerte_retombee_min x 60)), calculée à la lecture (le temps serveur éteint compte) — cerveau, écrans
- `void AddRegionAlert(int region, float amount, string reason)` — Alerte + amount (alerte_max au plus), m_iAlertAt = maintenant ; ligne SRP_CmdLog à chaque palier de 20 — cerveau (comptes rendus, missions), SRP_Commander.AddAlertAt, dépôts (sabotage)
- `int ThreatAtZone(int zone, int islandThreat)` — CO9 : min(10, islandThreat + min(alerte_menace_max, floor(alerte / alerte_menace_par))) pour la région de la zone (islandThreat hors région) — SRP_Commander.ThreatAt
- `float GarrisonFactorAtZone(int zone)` — OF5 : 1 + officier_bonus_garnison / 100 si l'officier de la région de la zone vit, sinon 1 — SRP_Commander.GarrisonFactorAt
- `int GetRegionMood(int region)` — SRP_ECmdMood de la région (recalculée par le cerveau) — manœuvres (CA1), écrans
- `void SetRegionMood(int region, int mood)` — Range l'humeur calculée par le cerveau (CO7) — SRP_Commander.RecomputeMoods
- `void NoteReportTrouble(int region, bool lost, string locality, int untilUnix)` — OF3/OF6 : un compte rendu de la région a été perdu (désorganisée) ou retardé (radio coupée à locality) — cerveau
- `bool HasLivingOfficer(int region)` — L'officier de la région est VIVANT (posé ou « sur le papier ») — OF5, cerveau, écrans
- `int GetOfficerTrait(int region)` — SRP_ECmdTrait de l'officier (METHODIQUE sans officier) — cerveau, manœuvres (CA1, source prudente)
- `string GetOfficerName(int region)` — Nom de l'officier en poste (ou du dernier tombé), « » sans officier — radio, écrans
- `int GetOfficerSerial(int region)` — Numéro unique de l'officier en poste, 0 sans officier (VU2 : le renseignement tombe au changement) — SRP_CmdIntel.HasIntel
- `int GetOfficerState(int region)` — SRP_ECmdOfficerState de l'officier, -1 sans officier — écrans, missions
- `IEntity GetOfficerEntity(int region)` — L'officier posé (null s'il est « sur le papier ») — écrans, missions
- `bool GetOfficerHideout(int region, out vector position, out string locality)` — Cachette du jour (position et localité) ; faux sans officier vivant — missions (OF10), Staff (téléportation)
- `bool IsOfficerPosted(int region)` — L'officier est posé dans le monde — écrans
- `bool OnPossibleOfficerDeath(IEntity victim)` — OnControllableDestroyed : la victime est-elle un officier posé ? Si oui, OfficerDown(TUE) et vrai — SRP_Commander.OnControllableDestroyed
- `string OfficerDown(int region, bool captured, string cause, string author)` — OF6, OF7, OF9 : officier TUE ou CAPTURE, région DESORGANISEE jusqu'à maintenant + desorg_min (x desorg_capture_facteur si capturé), AddRegionAlert(alerte_officier), SRP_FrontRadio.OfficerFell, capturé : SRP_CmdIntel.OnOfficerCaptured (renseignement et dépôt révélé), SRP_CmdLog, MarkDirty(true) ; rend le compte rendu — Tick (chute vue), OnPossibleOfficerDeath, SRP_Commander.ForceOfficerDown
- `string NewOfficer(int region, string author)` — OF7 : nouvel officier (autre nom que les officier_noms_memoire derniers, autre caractère que le prédécesseur, pas un 3e du même caractère), région ACTIVE, cachette choisie ; silencieux pour les joueurs (VU2) — Tick, Start, SRP_Commander.ForceOfficerReplace
- `void AttachZone(int zone, int nowUnix)` — OF11 : une zone passe rouge alors que sa région est LIBEREE : rattachée à la région voisine non libérée qui partage le plus de côtés (sinon parcours en largeur) ; aucune : la région renaît DESORGANISEE ; sauvé, journal — SRP_Commander.OnZoneLost
- `void UnpostOfficers(string reason)` — Officiers posés retirés hors de vue (ils restent vivants « sur le papier ») — CancelOperations, OnRestored
- `bool PickOfficerTarget(vector near, out int region, out vector site, out string label)` — Officier à viser pour une mission OFFICIER près de near : région, point de la cachette du jour, libellé (« officier ennemi Volkov, Chotain ») ; faux sans candidat (vivant, non visé par une mission active) — SRP_MissionManagerComponent
- `int GetOfficerMissionStatus(int region, int sinceUnix)` — SRP_ECmdMissionStatus de l'officier depuis sinceUnix (tombé, capturé, échappé, en cours, sans objet) — SRP_MissionManagerComponent (fin de la mission OFFICIER)
- `void WriteTo(JsonSaveContext ctx)` — cmd_rn, cmd_rserial, cmd_rnames, puis par région cmd_rN_code, _state, _desorg, _desorgCap, _lib, _alert, _alertAt, _prevTrait, _of (1 si officier), _of_serial, _of_name, _of_trait, _of_state, _of_since, _of_down, _of_day, _of_loc, _of_prev, _of_escaped ; rattachements cmd_rmc, cmd_rmN_z (code de zone), cmd_rmN_r (code de région) — SRP_Commander.WriteTo
- `void ReadFrom(JsonLoadContext ctx)` — Relecture miroir par CODE de région et de zone (rattachements appliqués) — SRP_Commander.ReadFrom
- `void ResetCampaign(string reason)` — Nouvelle campagne : tracé refait, officiers neufs, alertes à 0, rattachements effacés, désorganisations levées — SRP_Commander.ResetCampaign
- `void OnRestored()` — Après une restauration : officiers retirés hors de vue, cachettes recalculées — SRP_Commander.OnRestored
- `string GetRegionReport(int region)` — Page Staff d'une région (officier, cachette, désorganisation, alerte, rapports, zones) — SRP_CmdScreens
- `string GetNearReport(vector from, float radius)` — Officiers et cachettes autour d'un point (distance, posé ou non, gardes) — SRP_CmdScreens.StaffNearReport
  - (35 champs de réglage : voir §7)

### SRP_CmdResources.c (504 lignes)


**class SRP_CmdRegionSupply**
  La part d'une région (RE2) : obus et coupure du dépôt. Clés cmd_res_rN_* retrouvées par le code de région.
  - champ `int m_iRegion = -1`
  - champ `string m_sCode` — « R3 »
  - champ `int m_iRefZones` — zones de départ (tracé d'origine)
  - champ `int m_iShellShare` — part entière des res_obus_plein à menace 0
  - champ `float m_fShells` — sauvé
  - champ `int m_iCutUntil` — heure Unix, 0 = pas de coupure (RE8, sauvé)
  - champ `int m_iRedCache` — zones rouges au dernier comptage

**class SRP_CmdTicket**
  Un engagement de moyens (RE13). Sauvé ; relu = rendu au stock (Release « redémarrage »).
  - champ `int m_iId`
  - champ `int m_iKind = SRP_ECmdStock.OBUS`
  - champ `int m_iRegion = -1`
  - champ `int m_iLeft` — unités encore engagées
  - champ `string m_sWhat` — « mortier sur Régina (S07) »
  - champ `int m_iOpenedAt` — heure Unix
  - champ `bool m_bFree` — décision forcée par le Staff (cmd_staff_gratuit) : rien n'a été retiré

**class SRP_CmdTransfer**
  Un renflouement d'obus en route (RE2). Sauvé ; relu = rendu au donneur.
  - champ `int m_iFrom = -1`
  - champ `int m_iTo = -1`
  - champ `int m_iAmount`
  - champ `int m_iArriveAt` — heure Unix

**class SRP_CmdResources**
  Les stocks du Commandeur (serveur).
- `void SRP_CmdResources(SRP_Commander commander)` — Constructeur seul : m_Depots = new SRP_CmdDepots(this) — SRP_Commander (constructeur)
- `static SRP_CmdResources Get()` — Les stocks du Commandeur, null s'il est absent — tous les moyens, écrans
- `void DeclareSettings()` — Déclare ses clés puis m_Depots.DeclareSettings — SRP_Commander.DeclareSettings
- `void LoadSettings()` — Relit ses clés puis m_Depots.LoadSettings ; les pleins sont bornés aux nouveaux pleins — SRP_Commander.LoadSettings
- `void Start(int nowUnix)` — Front et régions prêts : une SRP_CmdRegionSupply par région (zones de départ), parts d'obus (48 x zones ÷ total, plus forts restes), rien lu -> tout plein, sinon rattrapage du revenu (RE6) ; m_Depots.Start — SRP_Commander.Start
- `void Stop()` — Arrêt : m_Depots.Stop — SRP_Commander.Stop
- `void Tick(int nowUnix)` — Toutes les 60 s : zones rouges recomptées, revenu avancé, renflouements (arrivées, nouveaux si le Commandeur n'est pas gelé), départ du dernier joueur (CA7), m_Depots.Tick, MarkDirty(false) si changé — SRP_Commander.Tick
- `SRP_CmdDepots GetDepots()` — Les dépôts cachés (RE8, RE9) — SRP_Commander, SRP_CmdIntel, SRP_CmdScreens, SRP_FrontMarkers
- `float GetStock(int kind, int region)` — Stock actuel (décimal : revenu continu) ; region ignorée hors OBUS — moyens, écrans
- `float GetFull(int kind, int region)` — Plein actuel : part x (1 + res_bonus_menace_centiemes / 100 x menace) ; 0 d'obus pour une région sans zone rouge ; bombe sans bonus sauf res_bombe_bonus_menace — moyens, écrans
- `float GetRevenueFactor(int kind, int region)` — Facteur de revenu (plancher res_plancher_centiemes, coupure du dépôt) — écrans
- `int MinutesToNextUnit(int kind, int region)` — Minutes avant la prochaine unité entière, -1 si plein ou revenu nul — écrans
- `int CountOpen(int kind)` — Tickets ouverts d'un stock (EQ3 : un blindé à la fois) — SRP_CmdArmor, écrans
- `float GetStockLevel()` — CA7 : moyenne des remplissages (obus de l'île, blindés, hélico, artillerie), de 0 à 1 — CheckLastLeave, écrans
- `float GetFillRatio(int region)` — Remplissage d'une région (obus ÷ plein), ou des moyens lourds de l'île pour region = -1 ; de 0 à 1 — humeur (CO7), SRP_CmdManeuvers (CA1)
- `int GetRegionSupplyState(int region)` — Staff seulement : 0 affaiblie, 1 normale, 2 renforcée (res_etat_*_centiemes ; coupure = affaiblie) — écrans
- `bool CanReserve(int kind, int region, int amount, out string refusal)` — Contrôle sans rien retirer : OBUS -> région valide avec une zone rouge ; Math.Floor(stock) >= amount ; rend faux avec la raison — moyens (avant de poser une pièce)
- `int Reserve(int kind, int region, int amount, string what, bool staff)` — Retire amount tout de suite et ouvre un ticket (staff et cmd_staff_gratuit : ticket gratuit, rien retiré) ; 0 si refusé (SRP_CmdLog.Refusal) ; sinon SRP_CmdLog « %1 : %2 engagé(s), reste %3 », MarkDirty(false) ; rend le numéro du ticket — SRP_CmdSupport, SRP_CmdArmor
- `void Consume(int ticket, int amount)` — Effet réel : le ticket perd amount (perdu pour de bon), fermé à 0 — moyens (obus tiré, blindé détruit ou pris, sortie d'hélico finie, salve partie, bombe larguée)
- `void Release(int ticket, string reason)` — Annulation ou retour : le reste du ticket revient au stock (borné au plein), ticket fermé, journal — moyens, ReadFrom (« redémarrage », RE13)
- `int GetTicketLeft(int ticket)` — Unités encore engagées sur le ticket, 0 s'il est fermé — moyens
- `void OnSabotageMission(int region, int missionType, string prefabName, string author)` — RE8 « pareil » : mission SABOTAGE ou CACHE réussie dans la région : retrait de res_sabotage_centiemes du plein du stock visé (CACHE -> OBUS ; SABOTAGE selon le prefab : Mortier OBUS, Antichar BLINDE, _AA ou Radar HELICO, Transmissions ou Generateur ARTILLERIE, sinon OBUS) ; SRP_CmdLog seulement — SRP_Commander.OnMissionSucceeded
- `void CutRegion(int region, string reason)` — RE8 : revenu de la région coupé de res_depot_coupure_centiemes pendant res_depot_coupure_heures (relancé, jamais cumulé), MarkDirty(true) — SRP_CmdDepots.Sabotage
- `void RequestResupply(int region, string author)` — RE2 : renflouement tout de suite, sans seuil (donneur : région vivante qui a le plus d'obus au-dessus de res_renflouement_donneur_centiemes) — CheckResupply, Staff (SRP_CmdScreens, action renflouer:R3)
- `void OnZoneOwnerChanged(int zone, int owner, bool staff)` — Une zone a changé de camp : revenu avancé avec les anciennes zones, zones rouges recomptées ; région sans zone rouge : obus à 0 et renflouements vers elle rendus ; puis m_Depots.OnZoneOwnerChanged(zone, owner, staff) ; staff (Q9) : même état, aucune annonce — SRP_Commander.OnZoneCaptured / OnZoneLost
- `void OnThreatChanged(int threat)` — RE10 : la menace a changé : revenu avancé, puis chaque stock borné au nouveau plein — SRP_Commander.OnThreatChanged
- `bool IsOffensiveAllowedByStocks()` — Pas d'offensive si le niveau noté au départ du dernier joueur est sous res_offensive_bas_centiemes — SRP_CmdManeuvers.NightChance
- `int OffensiveChanceBonus()` — + res_offensive_bonus si le niveau noté atteint res_offensive_plein_centiemes, sinon 0 (le plafond de 60 reste au front) — SRP_CmdManeuvers.NightChance
- `void WriteTo(JsonSaveContext ctx)` — cmd_res_v, cmd_res_updated, cmd_res_blinde, cmd_res_helico, cmd_res_artillerie, cmd_res_bombe, cmd_res_niveau_depart, cmd_res_depart_unix ; cmd_res_n puis cmd_res_rN_code, _obus, _coupe ; cmd_res_tn puis cmd_res_tN_kind, _region, _left, _what, _free ; cmd_res_vn puis cmd_res_vN_from, _to, _amount ; puis m_Depots.WriteTo — SRP_Commander.WriteTo
- `void ReadFrom(JsonLoadContext ctx)` — Relecture miroir (lecteur tolérant : clé absente = défaut) ; nombre de régions changé : tout plein et ERREUR ; RE13 : chaque ticket lu est rendu, chaque renflouement lu revient au donneur (une seule fois) ; journal ; puis m_Depots.ReadFrom — SRP_Commander.ReadFrom
- `void ResetCampaign(string author)` — Nouvelle campagne : stocks pleins (menace 0), coupures, tickets et renflouements vidés, m_Depots.Reset — SRP_Commander.ResetCampaign
- `void OnRestored()` — Après une restauration (ReadFrom de la copie fait) : revenu avancé jusqu'à maintenant (RE6) — SRP_Commander
- `void StaffSetStock(int kind, int region, float value, string author)` — Fixe un stock (borné au plein) ; SRP_CmdLog.StaffAction — SRP_CmdScreens (stock:<genre>:<R3|ile>:<0|plein>)
- `void StaffRefillAll(string author)` — Tous les stocks pleins ; SRP_CmdLog.StaffAction — SRP_CmdScreens (stock-plein)
- `string GetStaffReport()` — « Obus : 31 sur 48 (…) · Blindés : 1,6 sur 2, prochain dans 48 min · Hélico : 3 sur 3 · … » — SRP_CmdScreens
- `string StockText(int kind, int region)` — « 31/48 obus », « 1/2 blindés » : stock d'une région ou de l'île en clair — SRP_CmdLog (coût, reste), écrans
- `static string KindLabel(int kind)` — Nom d'un stock : « obus », « blindé », « sortie d'hélico », « tir d'artillerie lourde », « bombe » — journal
  - (22 champs de réglage : voir §7)

### SRP_CmdDepot.c (247 lignes)


**class SRP_CmdDepot**
  Le dépôt d'une région. Champs « sauvé » : clés cmd_dep_rN_*.
  - champ `int m_iRegion = -1`
  - champ `string m_sCode` — code de la région (sauvé)
  - champ `vector m_vPos` — vector.Zero = à reposer (sauvé)
  - champ `int m_iZone = -1` — sauvé (par code de zone)
  - champ `int m_iPrevZone = -1` — zone du dépôt précédent, évitée (sauvé)
  - champ `bool m_bRevealed` — sauvé
  - champ `string m_sRevealedBy` — « PC ennemi saisi à Perelle (S08) » (sauvé)
  - champ `bool m_bRevealPending` — révéler dès la prochaine pose (sauvé)
  - champ `int m_iMoves` — déplacements (sauvé)
  - champ `IEntity m_Entity` — décor posé
  - champ `IEntity m_OldEntity` — décor saboté, effacé hors de vue
  - champ `ref array<ref SRP_EnemyGroup> m_aGuards = {}` — toujours des ref
  - champ `int m_iQuietSince` — plus aucun joueur à res_depot_gardes_retrait_m depuis (heure Unix)
  - champ `int m_iNextTryAt` — prochaine tentative de pose (heure Unix)

**class SRP_CmdDepots**
  Les dépôts ennemis (serveur).
- `void SRP_CmdDepots(SRP_CmdResources resources)` — Constructeur seul — SRP_CmdResources (constructeur)
- `void DeclareSettings()` — Déclare ses clés — SRP_CmdResources.DeclareSettings
- `void LoadSettings()` — Relit ses clés — SRP_CmdResources.LoadSettings
- `void Start(int regions)` — Un dépôt par région (ceux relus par ReadFrom gardés) — SRP_CmdResources.Start
- `void Stop()` — Arrêt (rien à écrire) — SRP_CmdResources.Stop
- `void Reset(string author)` — Nouvelle campagne ou remise : décors effacés hors de vue, gardes retirées (RetireLater), tout est à reposer — SRP_CmdResources.ResetCampaign
- `void Tick(int nowUnix)` — Toutes les 60 s, par dépôt : région sans zone rouge -> effacé ; à reposer -> PickSite puis Place ; ancien décor effacé hors de vue ; décor absent après redémarrage -> reposé ; gardes à l'approche ou retirées — SRP_CmdResources.Tick
- `bool TryUse(IEntity entity, int playerId, out string message)` — En tête de OnObjectiveUsed (par SRP_Commander.OnSabotageUsed) : l'entité est-elle le décor d'un dépôt ? Si oui, Sabotage(nom du joueur, « sabotage ») et message au joueur ; vrai = consommé
- `string Reveal(int region, string reason)` — RE9 : dépôt posé et caché -> révélé (m_sRevealedBy = reason) ; en déplacement -> m_bRevealPending ; déjà révélé -> rien. Les annonces sont faites par SRP_CmdIntel (IntelGained) ; rend la référence du carré (« 074 042 »), "" si rien n'est encore posé — SRP_CmdIntel.Grant, SRP_CmdScreens (depot-reveler:R3, sans radio)
- `bool IsRevealed(int region)` — Le dépôt de la région est connu des joueurs — écrans, carte
- `vector GetDepotPos(int region)` — Position du dépôt de la région (vector.Zero si rien n'est posé) — Staff (téléportation), carte
- `int GetRevealedDepots(notnull array<int> regions, notnull array<vector> positions)` — Dépôts révélés : régions et positions ; rend leur nombre — SRP_FrontMarkers.TickEnemyDepots, PC
- `void OnZoneOwnerChanged(int zone, int owner, bool staff)` — Une zone a changé de camp : zone d'un dépôt passée BLEU -> Sabotage(« », « zone prise ») (radio, évènement depot_ennemi, coupure du revenu) ; staff (Q9, zone forcée) : le dépôt change de place EN SILENCE, comme StaffMove (ni CutRegion, ni radio, ni depot_ennemi, ni alerte) — SRP_CmdResources.OnZoneOwnerChanged
- `void StaffMove(int region, string author)` — Staff : le dépôt de la région change de place (caché), sans coupure — SRP_CmdScreens (depot-deplacer:R3), OnZoneOwnerChanged (staff)
- `void StaffSabotage(int region, string author)` — Staff : sabotage forcé (même suite qu'un vrai) — SRP_CmdScreens (depot-saboter:R3, 2 clics)
- `void StaffClearCut(int region, string author)` — Staff : coupure du revenu levée — SRP_CmdScreens (coupure-lever:R3)
- `string GetStaffReport()` — Par région : carré, révélé ou non (par qui), déplacements, coupure restante — SRP_CmdScreens
- `void WriteTo(JsonSaveContext ctx)` — cmd_dep_n, puis cmd_dep_rN_code, _x, _y, _z, _zone (code), _prev (code), _revele, _par, _attente, _moves — SRP_CmdResources.WriteTo
- `void ReadFrom(JsonLoadContext ctx)` — Relecture miroir par code ; zone qui n'est plus rouge : le dépôt change de place sans coupure — SRP_CmdResources.ReadFrom
  - (9 champs de réglage : voir §7)

### SRP_CmdCapacity.c (421 lignes)


**class SRP_CmdDemand**
  Une demande de place en attente (file RE12), valable cap_demande_s, renouvelée à chaque essai. Non sauvée.
  - champ `string m_sKey` — « renfort-S07 », « officier-R3 », « depot-R3 », « mission-M07 »…
  - champ `int m_iClass = SRP_ECmdCapClass.COMBAT`
  - champ `int m_iSoldiers`
  - champ `int m_iGroups`
  - champ `vector m_vPos`
  - champ `int m_iSince` — heure Unix de la 1re demande
  - champ `int m_iUntil` — heure Unix de fin de validité

**class SRP_CmdCapacity**
  La place des soldats ennemis (serveur).
- `static SRP_CmdCapacity Get()` — Le singleton, créé à la demande avec les valeurs de Jack (même sans Commandeur, même gelé) — toutes les poses
- `void DeclareSettings()` — Déclare ses clés (défaut = valeur du champ) — SRP_Commander.DeclareSettings
- `void LoadSettings()` — Relit ses clés ; si déjà démarrée : ApplyEngineLimit et SRP_EnemyComponent.SetMaxGroups(cap_groupes_max, cap_groupes_reserve_missions) — SRP_Commander.LoadSettings
- `void Start()` — Serveur, front prêt : ApplyEngineLimit (160), SRP_EnemyComponent.SetMaxGroups(36, 6) ; contrôle de cohérence cap_soldats_max + cap_civils_prevus + cap_pilotes_prevus + cap_marge_jeu <= cap_limite_jeu, sinon ERREUR « réglages incohérents » au journal — SRP_Commander.Start
- `void Stop()` — Arrêt : demandes et parts oubliées — SRP_Commander.Stop
- `void Tick(int nowUnix)` — Toutes les 5 s (TOUJOURS, même Commandeur gelé) : demandes expirées retirées ; RE12 : si les demandes COMBAT dépassent Room(COMBAT), SRP_EnemyComponent.ReleaseForRoom(manque, cap_retrait_rondes_m, cap_retrait_jeeps_m, cap_retrait_par_passe) ; chaque minute : ApplyEngineLimit si la limite lue a bougé — SRP_Commander.Tick
- `void ApplyEngineLimit()` — GetGame().GetAIWorld().SetLimitOfActiveAIs(cap_limite_jeu) si la limite lue diffère ; relit et journalise « Limite du jeu : %1 avant, %2 voulue, %3 lue » (ERREUR si elle n'a pas pris : la clé aiLimit de la configuration du serveur l'emporte peut-être) ; AIWorld absent : nouvel essai (12 au plus) — Start, Tick
- `static int ClassOf(SRP_EnemyGroup record)` — Classe d'un groupe : m_iCapClass s'il est posé (>= 0), sinon devinée (propriétaire « helico » HORS_COMPTE ; tâche « jeep » ou propriétaire « ambiance » ou « survivant » PATROUILLE ; tâche « vague », « assaut », « camion », « chauffeur » ou propriétaire « camion » COMBAT ; « mission », « officier », « depot », « livraison » ou tâche « garde » GARDE ; « poste front » POSTE ; sinon GARNISON), en if/else — comptes, TagImportance
- `static void Tag(SRP_EnemyGroup record, int cls)` — Après CHAQUE groupe posé : record.m_iCapClass = cls, puis TagImportance — toutes les poses
- `static void TagImportance(SRP_EnemyGroup record)` — SEUL appel à SetImportance pour l'ennemi : cap_importance_ennemis (3, CRITICAL), cap_importance_rondes (2, HIGH) pour PATROUILLE — Tag, reclassement (débarqués qui rejoignent une garnison)
- `static void TagCivilianGroup(SCR_AIGroup group)` — SEUL appel à SetImportance pour un civil : cap_importance_civils (2, HIGH) — SRP_Civilians (après la pose)
- `static string ClassLabel(int cls)` — « renforts », « défense des localités », « postes », « gardes », « rondes », « hors compte » — Staff, refus
- `int CountGround()` — Soldats au sol, toutes classes sauf HORS_COMPTE — Room, Staff
- `int CountClass(int cls)` — Soldats d'une classe — Room, Staff
- `int CountGroundGroups()` — Groupes vivants hors HORS_COMPTE — GroupRoom, Staff
- `int CountClassGroups(int cls)` — Groupes d'une classe — GroupRoom, Staff
- `int Room(int cls, string owner)` — Soldats qu'une classe peut encore poser : cap_soldats_max - au sol, moins les réserves inutilisées des classes plus prioritaires (COMBAT garde cap_reserve_postes ; POSTE garde les renforts et les demandes COMBAT ; GARNISON, GARDE, PATROUILLE gardent aussi les demandes des classes au-dessus), borné par EngineRoom ; HORS_COMPTE : 1000 — Refusal, poses qui dimensionnent
- `int GroupRoom(int cls, string owner)` — Groupes qu'une classe peut encore poser : cap_groupes_max, moins cap_groupes_reserve_missions hors « mission », moins les groupes vivants, moins les réserves inutilisées (renforts sauf COMBAT, postes sauf POSTE si un poste est possible) — Refusal
- `int EngineRoom()` — Place du jeu : GetLimitOfActiveAIs - GetCurrentNumOfActiveAIs - SRP_EnemyComponent.CountPendingSpawn - cap_marge_jeu ; 1000 sans AIWorld — Room, CivilianRefusal
- `string Refusal(int cls, int soldiers, int groups, string owner)` — "" si soldiers <= Room(cls, owner) et groups <= GroupRoom(cls, owner) ; sinon la raison en clair pour le journal (« place : 11 demandées, 4 possibles pour les renforts ») — Ask, poses qui ne font pas la queue
- `string Ask(string key, int cls, int soldiers, int groups, vector position, string owner)` — Porte de pose recommandée : Refusal ; "" -> ClearDemand(key) et on pose ; sinon NoteDemand(key, …) (file RE12) et la raison est rendue — toutes les poses du mod
- `void NoteDemand(string key, int cls, int soldiers, int groups, vector position)` — Crée ou renouvelle la demande key (valable cap_demande_s) — Ask, poses qui attendent
- `void ClearDemand(string key)` — Retire la demande key (posée ou abandonnée) — Ask, poseurs
- `int GetPendingSoldiers(int cls)` — Soldats demandés en attente pour une classe (demandes valables) — Staff, SetWantedGarrisons
- `string CivilianRefusal(int count)` — Civils : "" si count <= EngineRoom() - max(0, cap_soldats_max - CountGround()) (les civils ne prennent jamais la place du jeu que les 120 soldats peuvent encore demander) ; sinon la raison — SRP_Civilians (avant la pose)
- `void SetWantedGarrisons(notnull array<string> keys, notnull array<int> sizes, notnull array<bool> near, notnull array<bool> attacked)` — À chaque passage de 5 s de l'ennemi : parts de chaque localité voulue (keys, sizes = effectif voulu, near = joueur à cap_garnison_proche_m, attacked = SRP_FrontEnemyComponent.IsLocalityUnderAttack) : attaquées d'abord à leur taille, les autres au prorata (poids double si near, 3 passes), plancher cap_garnison_min, plafond cap_garnison_max — SRP_FrontEnemyComponent.Tick
- `int GarrisonCap(string locality, int decided)` — Soldats qu'une localité peut poser ou compléter : min(decided, part, cap_garnison_max) ; localité sans part : min(decided, cap_garnison_max) (la pose reste bornée par Ask) ; ne réduit jamais une garnison posée — SRP_TerritoryComponent (pose F11), SRP_EnemyGarrison
- `int GetGarrisonMax()` — cap_garnison_max (60) : plafond de l'effectif Nominal et Effective d'une localité (OF5) — SRP_FrontEnemyComponent, SRP_TerritoryComponent.GarrisonSoldiers
- `int GetMaxSoldiers()` — cap_soldats_max — SRP_EnemyComponent (anciens GetMaxSoldiers), écrans
- `int GetMaxGroups()` — cap_groupes_max — SRP_EnemyComponent.SetMaxGroups, écrans
- `string GetStaffReport()` — « Soldats ennemis au sol : 112 sur 120 (renforts 18, défense des localités 71, postes 6, gardes 8, rondes 9) · réserves libres : renforts 2, postes 6 · en attente : renforts 11 · éléments : 28 sur 36 · limite du jeu : 139 actifs sur 160 » — SRP_CmdScreens
- `string GetLastRefusal()` — Dernier refus de place (« place : … »), "" sinon — SRP_CmdScreens
  - (22 champs de réglage : voir §7)

### SRP_CmdManeuvers.c (1017 lignes)


**class SRP_CmdColumn**
  Une colonne de soldats entre deux localités (MA4, MA6, Staff), sur le papier puis réelle. En route : sauvée (clés cmd_ma_cN_*) et rendue à sa source à la relecture (RE13).
  - champ `int m_iId`
  - champ `int m_iKind = SRP_ECmdColumnKind.RENFORT`
  - champ `int m_iState = SRP_ECmdColumnState.PAPIER`
  - champ `string m_sFrom` — localité source
  - champ `string m_sTo` — localité visée ("" = vers un contact)
  - champ `int m_iSoldiers`
  - champ `int m_iZone = -1` — zone du contact (renfort)
  - champ `vector m_vContact`
  - champ `ref array<vector> m_aRoute = {}` — points de route (SRP_Placement.BuildRoadRoute) plus les extrémités
  - champ `float m_fLength` — longueur à plat
  - champ `int m_iDepartUnix`
  - champ `int m_iNextTryUnix` — prochaine tentative de passage au réel
  - champ `int m_iTrucks` — camions posés
  - (1 champs de réglage : voir §7)

**class SRP_CmdSentTruck**
  Un camion de renfort HORS colonne lancé et pas encore déchargé (RE13). Sauvé (clés cmd_ma_tN_*) ; relu = ses soldats sont rendus à leur source (ReturnSent), une seule fois.
  - champ `int m_iId`
  - champ `string m_sFrom` — localité source, débitée au départ (AddSent)
  - champ `int m_iLoad` — soldats à bord

**class SRP_CmdPaperUnit**
  Une unité de contre-attaque « sur le papier », loin des joueurs (CA6). Non sauvée (H3).
  - champ `int m_iZone = -1`
  - champ `int m_iWave`
  - champ `int m_iAxis`
  - champ `int m_iSoldiers`
  - champ `vector m_vPos`
  - champ `vector m_vOrigin`
  - champ `int m_iObjCell = -1`
  - champ `int m_iLastUnix`

**class SRP_CmdRetreat**
  Un décrochage en cours (#40, MA5, MA6). Non sauvé (H3).
  - champ `string m_sLocality`
  - champ `string m_sTo`
  - champ `vector m_vExit`
  - champ `vector m_vContact`
  - champ `ref array<ref SRP_EnemyGroup> m_aGroups = {}`
  - champ `ref SRP_EnemyGroup m_Cover` — binôme qui couvre
  - champ `int m_iStartUnix`
  - champ `int m_iColumnId`
  - champ `bool m_bMoved`

**class SRP_CmdAssault**
  Plan d'assaut d'une vague ou d'un raid (MA8) ; la SEULE classe SRP_CmdAssault (trou 3). L'état d'appui d'une contre-attaque est sur SRP_CounterAttack (m_bFeint, m_bSupportMortar, m_bSupportHeavy).
  - champ `int m_iZone = -1` — -1 pour un raid
  - champ `int m_iMode = SRP_ECmdAssaultMode.FACE`
  - champ `vector m_vObjective`
  - champ `vector m_vFirePos`
  - champ `vector m_vOriginA`
  - champ `vector m_vOriginB`
  - champ `bool m_bHasB`
  - champ `ref array<ref SRP_EnemyGroup> m_aFire = {}`
  - champ `ref array<ref SRP_EnemyGroup> m_aMove = {}`
  - champ `ref array<ref SRP_EnemyGroup> m_aAxisB = {}`
  - champ `int m_iStep`
  - champ `int m_iStepUnix`
  - champ `bool m_bSmoked`

**class SRP_CmdRaid**
  Un raid de harcèlement (MA10). Non sauvé (H3).
  - champ `int m_iCell = -1`
  - champ `vector m_vTarget`
  - champ `vector m_vOrigin`
  - champ `ref array<ref SRP_EnemyGroup> m_aGroups = {}`
  - champ `ref SRP_CmdAssault m_Plan`
  - champ `int m_iStartUnix`
  - champ `int m_iContactUnix`
  - champ `int m_iPosted`

**class SRP_CmdTrack**
  Les deux derniers relevés de contact d'une zone ou d'une région (joueurs « retranchés » MA8, « chemin » MA11)
  - champ `vector m_vA`
  - champ `int m_iA`
  - champ `vector m_vB`
  - champ `int m_iB`

**class SRP_CmdManeuvers**
  Les manœuvres (serveur).
- `void SRP_CmdManeuvers(SRP_Commander commander)` — Constructeur seul — SRP_Commander (constructeur)
- `static SRP_CmdManeuvers Get()` — Les manœuvres, null sans Commandeur — SRP_FrontEnemyComponent, SRP_TerritoryComponent, camions
- `void DeclareSettings()` — Déclare ses clés — SRP_Commander.DeclareSettings
- `void LoadSettings()` — Relit ses clés — SRP_Commander.LoadSettings
- `void Start(int nowUnix)` — Front prêt : sous-cadences initialisées — SRP_Commander.Start
- `void Tick(int nowUnix)` — Toutes les 5 s : Commandeur inactif (!m_Commander.GetHost().IsCommanderActive()) -> seulement finir proprement (FinishOnly : replis, raids qui rentrent, colonnes réelles ; colonnes papier annulées, ReturnSent à leur source) ; sinon ColumnsTick, PaperTick, RetreatsTick, RaidsTick, AwakeTick (10 s) ; 60 s : RetreatCheckTick, EmptiedTick, PathTick, ContactGroupsTick ; 300 s : HarassTick ; m_bDirty -> SRP_FrontComponent.MarkDirty(false) — SRP_Commander.Tick
- `bool CanReinforceZone(int zone)` — Frein C2 : aucun renfort vers la zone depuis renfort_frein_min minutes (camion d'entrée compris) — cerveau (avant d'émettre un ordre RENFORT), RequestEntryTruck
- `int GetReinforceWaitSeconds(int zone)` — Secondes avant qu'un renfort soit de nouveau possible vers la zone (0 = possible) — Staff
- `string RequestReinforcement(int zone, vector aim, int size, string reason, string author, bool staff)` — Ordre RENFORT : frein C2 (sauf staff), source (PickSource), délai (x renfort_facteur_desorg si désorganisée), camion localité (DispatchFrom) ou camion de contact (SendContactTruck) si la route tient dans le délai, sinon colonne ; place par SRP_CmdCapacity (COMBAT) au moment de la pose des camions ; frein noté dès l'envoi ; rend "" ou la raison — SRP_Commander.IssueOrder
- `bool RequestEntryTruck(SRP_SectorState state, int players, int nowUnix)` — Camion d'entrée #69 (joueur à 500 m d'une localité), Commandeur actif : même frein, même source, même délai ; chargement d'aujourd'hui (LoadFor) ; vrai si un camion part (sinon la règle votée ne pose rien de plus) — SRP_TerritoryComponent
- `string RequestSearch(vector aim, int zone, string reason, bool staff)` — Ordre ENQUETE (contact « informateur seul », C4) : l'unité mobile la plus proche va fouiller (OrderSearch) ; jamais d'appui — SRP_Commander.IssueOrder
- `string RequestColumn(vector aim, string reason, string author, bool staff)` — Ordre COLONNE (Staff, par ForceOrder seul, trou 15) : colonne de la localité ennemie la plus proche d'aim vers la suivante (papier, réelle à colonne_distance_m) — SRP_Commander.IssueOrder
- `string RequestRetreat(vector aim, string reason, string author, bool staff)` — Ordre DECROCHAGE : StartRetreat de la garnison posée la plus proche d'aim à 3 km — SRP_Commander.IssueOrder
- `string RequestHarass(vector aim, string reason, string author, bool staff)` — Ordre HARCELEMENT (MA10) sur le carré bleu du front le plus proche d'aim : quelques obus (SRP_CmdSupport) ou raid de 1 à 2 groupes (PATROUILLE) — SRP_Commander.IssueOrder, HarassTick
- `bool IsLocalityEmptied(string locality)` — MA7 : localité vidée par un décrochage tant qu'un joueur est à repli_vide_distance_m : ni posée ni complétée — SRP_TerritoryComponent, SRP_FrontEnemyComponent
- `bool IsKeptFull(string locality)` — MA11 : localité tenue au complet (m_iFullUntil de SRP_FrontLocality) — SRP_FrontEnemyComponent (GarrisonScore)
- `bool IsOnPlayersPath(int cell)` — MA11 : carré de poste sur le chemin estimé des joueurs (20 min) — SRP_FrontEnemyComponent.PostsTick
- `void OnZoneCaptured(int zone, bool staff)` — MA11 (hors staff, région organisée) : localité hostile suivante tenue au complet reorg_complet_heures, pertes et envoyés remis à zéro — SRP_Commander.OnZoneCaptured
- `void OnZoneLost(int zone, int reason, bool staff)` — Zone repassée rouge : replis et raids qui la visaient arrêtés — SRP_Commander.OnZoneLost
- `void OnLocalityTaken(string locality)` — Localité prise : ses replis arrêtés (RetireLater), colonnes réorientées vers la localité hostile la plus proche à 3 km, drapeaux vidée et tenue au complet effacés — SRP_Commander.OnLocalityTaken
- `void OnLossReported(int zone, vector position)` — MA5 : une perte rapportée dans la zone, point de départ du contrôle de décrochage #40 — SRP_Commander.Integrate
- `void NoteContact(int region, int zone, vector position, int nowUnix)` — Relevé de contact connu (position floutée) pour les pistes « retranché » (MA8) et « chemin » (MA11) — SRP_Commander.Integrate
- `int OnTruckLaunched(string source, int load)` — Un camion de renfort HORS colonne part (contact, localité, entrée #69) : AddSent(source, load) (MA2, débit au départ, une seule fois) et inscription au registre des camions en route (RE13) ; rend le numéro à ranger dans SRP_EnemyTruck.m_iSentId (0 si rien n'est inscrit : source vide ou load <= 0). Jamais appelé pour un camion de colonne (m_iColumnId != 0 : débitée par NewColumn) — SRP_EnemyTruckComponent
- `void OnTruckUnloaded(int sentId)` — Le camion sentId est déchargé (passagers débarqués) ou perdu (détruit, retiré) : il sort du registre RE13 ; RIEN n'est rendu ici (les débarqués vivants rentrent par SRP_EnemyComponent.RetireTick, les morts restent des envoyés qui se regarnissent) ; sentId 0 : rien — SRP_EnemyTruckComponent
- `void OnColumnDelivered(int columnId, int survivors, bool joined)` — Colonne réelle livrée (la source a été débitée par NewColumn au départ) : joined = soldats entrés dans la garnison posée de la destination : leurs groupes gardent m_sSourceLocality = source (ils rentreront à leur source par SRP_EnemyComponent.RetireTick s'ils sont retirés vivants) et rien n'est rendu ni crédité ici ; sinon AddBonus(destination, survivants), groupes absorbés (m_sSourceLocality vidé avant leur retrait) ; aucun survivant : PERDUE — SRP_EnemyTruckComponent
- `void OnGroupGone(SRP_EnemyGroup record)` — Un groupe disparaît (supprimé, anéanti) : retiré des replis, raids, plans et débarqués — SRP_EnemyComponent
- `bool CaWindowDecision(int clockSec, int dueSec, int zone, int nowUnix)` — CA1, chaque minute de la fenêtre D ± ca_fenetre_min (horloge F6 en secondes) : vrai = lancer maintenant ; jamais à moins de ca_ecart_min d'une précédente ; note = 50 x remplissage + présence connue. Squelette : départ à D (règle votée) — SRP_FrontEnemyComponent.AttackClock
- `int PickByIntel(notnull array<int> candidates)` — CA2 (hors les 70 sur 100 de la dernière prise) : cible choisie sur renseignement (dépôt #49, zone peu défendue, carrés) ; -1 = aucune note positive (le front tire au hasard) — SRP_FrontEnemyComponent.PickTargetZone
- `int PlannedWaves(int zone)` — CA3 : vagues prévues = SRP_FrontEnemyComponent.m_iAttackMaxWaves (clé front_attaque_vagues, UNE clé par chiffre, la même que Commandeur gelé), une de moins si la région est désorganisée (OF8), jamais sous 1 ; la révision de la 2e vague reste bornée par ca_vagues_min / ca_vagues_max (ReviseWaves). Squelette : les vagues votées du front — SRP_FrontEnemyComponent.LaunchAttack
- `int ReviseWaves(SRP_CounterAttack attack)` — CA3 : à la 2e vague, total revu d'après la 1re (ca_vague_bonne_centiemes, ca_vague_brisee_centiemes), borné de ca_vagues_min à ca_vagues_max — SRP_FrontEnemyComponent.SendWave
- `int WaveRoomGroups()` — Groupes d'une vague permis par la place COMBAT (Room(COMBAT) / 6, moins ca_marge_place). Squelette : pas de borne en plus — SRP_FrontEnemyComponent.SendWave
- `bool WantPaperWave(vector origin, vector zoneCenter)` — CA6 : aucun joueur à ca_papier_distance_m de l'origine ni du centre de la zone -> vague sur le papier — SRP_FrontEnemyComponent.SendWave
- `void AddPaperWave(SRP_CounterAttack attack, vector origin, int soldiers, int wave, int axis)` — CA6 : ajoute une unité sur le papier (soldats, vague, axe) — SRP_FrontEnemyComponent.SendWave
- `void OnWavePosted(SRP_CounterAttack attack, int wave, array<ref SRP_EnemyGroup> groups, vector originA, vector originB, bool hasB)` — MA8 : plan d'assaut de la vague posée (face, flanc, deux axes), m_iCmdRole ASSAUT, attack.m_iPostedSoldiers — SRP_FrontEnemyComponent.SendWave
- `void Support(SRP_CounterAttack attack, int nowUnix)` — CA4 et MO8, à chaque passage pendant l'annonce : mortier ca_mortier_avance_s avant l'assaut, artillerie (amas de ca_amas_joueurs sur ca_amas_rayon_m) sinon blindé ca_lourd_avance_s avant, feinte (ca_feinte sur 100, 2e axe réel), Su-57 (ca_avion_chance) ; région désorganisée : rien (OF8) ; tout par SRP_CmdSupport.Request* — SRP_FrontEnemyComponent.AttackTick
- `void DriveAssault(SRP_CounterAttack attack, int nowUnix)` — MA8, CA6 : conduit l'assaut (plans, objectifs reprenables, groupes bloqués passés sur le papier) ; remplace AdvanceAssault quand le Commandeur est actif (IsCommanderActive) — SRP_FrontEnemyComponent.AttackTick
- `bool WaveArrived(SRP_CounterAttack attack)` — La dernière vague est arrivée (chaque groupe entré dans la zone ou hors de combat, chaque unité papier entrée), sans délai réputé — SRP_FrontEnemyComponent.AttackTick
- `int CheckEnd(SRP_CounterAttack attack, int nowUnix)` — CA5, C8 : SRP_EAttackEnd.DEFENDUE si les soldats en état de combattre tombent à (100 - ca_abandon_centiemes) centièmes des posés après ca_abandon_vagues vagues (ou toutes posées) ; AUCUNE sinon (la rupture ROMPUE à 60 min reste au front, m_iAttackMaxMinutes) — SRP_FrontEnemyComponent.AttackTick
- `void EndAttack(SRP_CounterAttack attack, int result)` — Fin d'attaque (SRP_EAttackEnd) : groupes réels en repli puis RetireLater, ReleaseAwake, papier effacé, capture.ClearPaperAssault(zone), SRP_CmdSupport.EndAssault(zone), m_iLastCaUnix, m_iCaCount — SRP_FrontEnemyComponent.EndAttack
- `int NightChance(int chance, out bool cancel)` — CA7 (relais de SRP_CmdResources, seule implémentation) : cancel = !IsOffensiveAllowedByStocks() ; rend chance + OffensiveChanceBonus() (le plafond de 60 reste appliqué par le front). Squelette : chance votée — SRP_FrontEnemyComponent.RunOffensive
- `string CancelAll(string author)` — ia-reset et wipe : colonnes annulées et rendues à leur source (ReturnSent), papier, raids et replis retirés hors de vue — SRP_Commander
- `string GetReport(vector from, float radius)` — Colonnes, replis, raids, freins et dernier renfort autour d'un point — SRP_CmdScreens.StaffNearReport
- `string GetColumnsReport()` — Colonnes en route (source, destination, soldats, papier ou réelle, distance restante) — SRP_CmdScreens
- `string GetReinforcementText(int region)` — « dernier à 21:10 vers Régina (S07) · prochain possible dans 4 min » pour une région — SRP_CmdScreens (page région)
- `void WriteTo(JsonSaveContext ctx)` — cmd_ma_v, cmd_ma_harass, cmd_ma_ca_last, cmd_ma_ca_count, cmd_ma_cols puis cmd_ma_cN_from, _to, _soldiers (colonnes en route, papier ou réelles), cmd_ma_tn puis cmd_ma_tN_from, _n (camions hors colonne lancés et pas encore déchargés, m_aSentTrucks) — SRP_Commander.WriteTo
- `void ReadFrom(JsonLoadContext ctx)` — Relecture ; RE13 : chaque colonne lue et chaque camion lu (cmd_ma_t*) rendent leurs soldats à leur source (ReturnSent), une seule fois ; colonnes et registre des camions vidés ensuite — SRP_Commander.ReadFrom
- `void ResetCampaign(string reason)` — Nouvelle campagne : tout vidé (colonnes, papier, replis, raids, freins, pistes, horloges) — SRP_Commander.ResetCampaign
- `void OnRestored()` — Après une restauration : papier, raids et replis oubliés ; colonnes et camions relus déjà rendus — SRP_Commander.OnRestored
  - (108 champs de réglage : voir §7)

### SRP_CmdSupport.c (777 lignes)


**class SRP_CmdMortarTeam**
  L'équipe de mortier d'une région (MO1, MO2, MO9). Non sauvée (pièce reposée à la demande).
  - champ `int m_iRegion = -1`
  - champ `IEntity m_Piece`
  - champ `ref SRP_EnemyGroup m_Servants`
  - champ `vector m_vPos`
  - champ `int m_iPlacedUnix`
  - champ `int m_iLastNeededUnix`
  - champ `int m_iNoServantSinceUnix`
  - champ `bool m_bSabotaged`
  - champ `bool m_bSilenced`
  - champ `bool m_bFiring`

**class SRP_CmdBattery**
  La batterie 2S1 d'une région (AP1). Non sauvée.
  - champ `int m_iRegion = -1`
  - champ `IEntity m_Vehicle`
  - champ `ref SRP_EnemyGroup m_Guard`
  - champ `vector m_vPos`
  - champ `int m_iPlacedUnix`
  - champ `int m_iLastNeededUnix`
  - champ `bool m_bDestroyed`

**class SRP_CmdFireMission**
  Une mission de tir en cours (mortier, rideau, feinte, artillerie). Non sauvée : son ticket est rendu au redémarrage par SRP_CmdResources.
  - champ `int m_iKind = SRP_ECmdOrder.MORTIER` — MORTIER, FUMEE, FEINTE ou ARTILLERIE
  - champ `int m_iShell = SRP_ECmdShell.EXPLOSIF`
  - champ `int m_iRegion = -1`
  - champ `int m_iZone = -1`
  - champ `vector m_vAim`
  - champ `float m_fError` — MO4 : écart courant, en mètres
  - champ `int m_iShellsLeft`
  - champ `int m_iFired`
  - champ `int m_iSkippedInRow` — coups sautés de suite (sécurité)
  - champ `int m_iTicket` — ticket de SRP_CmdResources (0 = gratuit)
  - champ `bool m_bStaff`
  - champ `bool m_bSalvo` — salve d'artillerie commencée
  - champ `string m_sReason`
  - champ `ref array<vector> m_aFixed = {}` — points imposés (rideau, feinte)

**class SRP_CmdStrike**
  Une frappe récente (EQ3 : pas deux frappes au même endroit en appui_meme_endroit_min). Sauvée (cmd_ap_frappes).
  - champ `vector m_vPos`
  - champ `int m_iUnix`

**class SRP_CmdSupport**
  Les appuis du Commandeur (serveur).
- `void SRP_CmdSupport(SRP_Commander commander)` — Constructeur seul : m_Armor = new SRP_CmdArmorManager(this) — SRP_Commander (constructeur)
- `static SRP_CmdSupport Get()` — Les appuis, null sans Commandeur — cerveau, manœuvres, écrans
- `SRP_CmdArmorManager GetArmor()` — Les blindés — SRP_CmdScreens, SRP_Commander
- `void DeclareSettings()` — Déclare ses clés puis m_Armor.DeclareSettings — SRP_Commander.DeclareSettings
- `void LoadSettings()` — Relit ses clés puis m_Armor.LoadSettings (garde AP4 : T14, K17, Kurganets, T90 refusés) — SRP_Commander.LoadSettings
- `void Start(int nowUnix)` — Front prêt : tableaux par région dimensionnés (interdictions relues gardées) ; journal des clés de faction présentes — SRP_Commander.Start
- `void Stop()` — Arrêt : tous les CallLater de tir retirés (FireNext, DropShell, DropBomb…) — SRP_Commander.Stop
- `void Tick(int nowUnix)` — Toutes les 5 s : équipes (silence MO9, retrait MO2), batteries (garde à l'approche, destruction, retrait), m_Armor.Tick, hélico (sortie finie -> Consume), frappes périmées — SRP_Commander.Tick
- `string RequestMortar(int region, vector aim, int shell, int shells, string reason, bool staff)` — MO1 à MO9 : EXPLOSIF (Reserve d'obus ; refus : gel, désorganisation, interdiction 24 h, appui_mortier_tirs_heure, appui_mortier_seuil_joueurs, stock, carré MO6, même endroit, sécurité, pièce déjà en tir) ou FUMEE (C5 : gratuite, hors plafonds) ; shells = 0 -> appui_mortier_obus_min..max ; pièce posée par EnsureTeam, premier coup après appui_mortier_delai_min_s..max_s — SRP_Commander.IssueOrder, SRP_CmdManeuvers
- `string RequestSmoke(int region, vector from, vector threat, string reason, bool staff)` — MO7 : rideau de appui_fumee_obus fumigènes (appui_fumee_largeur_m, perpendiculaire à from -> threat, à appui_fumee_devant_joueurs_m devant les joueurs) par la pièce de la région (gratuit, C5) — SRP_CmdManeuvers (assaut, décrochage), SRP_Commander (FUMEE)
- `string RequestFeint(int region, vector aim, string reason, bool staff)` — MO8 : rideau puis appui_feinte_obus_explosifs obus explosifs sur l'entrée du faux axe (aim) ; l'assaut vient d'ailleurs — SRP_CmdManeuvers.Support, SRP_Commander (FEINTE)
- `string MortarRefusal(int region, vector aim, int shell)` — Contrôle d'un tir de mortier sans rien lancer ("" = possible) — SRP_CmdManeuvers (feinte, harcèlement), Staff
- `string RequestArtillery(int region, vector aim, int attachedZone, string reason, bool staff)` — AP1, AP2, EQ1 : 2S1 posé à appui_artillerie_batterie_min_m..max_m, coups de réglage puis salve de appui_artillerie_salve_min..max obus dans appui_artillerie_rayon_m ; attachedZone = zone de la contre-attaque servie (-1 sinon, EQ3) ; refus : gel, désorganisation, interdiction, 2 h, 8 joueurs, stock, grosse opération, carré, même endroit, sécurité appui_securite_salve_m — SRP_Commander (AP2), SRP_CmdManeuvers (CA4)
- `string RequestBomb(vector aim, string reason, bool staff)` — AP3, EQ1 : Su-57 puis bombe (appui_bombe_avion_avance_s), à appui_bombe_dispersion_m près ; 24 h glissantes, appui_bombe_seuil_joueurs à 3 km, stock, pas de grosse opération, sécurité appui_securite_bombe_m (revue au largage) — SRP_Commander (AP3)
- `string RequestFlyby(vector position, string reason, bool staff)` — Passage d'avion sans dégât (appui_avion_passages_24h sur 24 h glissantes) — SRP_CmdManeuvers (CA4), Staff
- `string RequestArmor(int region, vector target, int armorReason, int zone, int variant, bool staff)` — AP4, AP5 : relais de m_Armor.Launch (armorReason = SRP_ECmdArmorReason ; variant 0 BTR-70, 1 BRDM-2, 2 Typhoon, -1 tirage selon les poids) — SRP_Commander (AP5), SRP_CmdManeuvers (CA4)
- `string RequestHeli(vector position, string reason, bool staff)` — AP7 : carré rouge, SRP_HeliSearch.Count() == 0, repos appui_helico_repos_min, stock HELICO (Reserve), puis SRP_HeliSearch.Launch(position, reason) ; aucun hélico armé (AP8) ; staff (ForceOrder(HELICO), seule porte du Staff, trou 15) : outre stock, repos et « personne sur place », jamais outre la sécurité — SRP_Commander (AP7)
- `bool IsBigOperationRunning(int attachedZone)` — EQ3 : une grosse opération est en cours (blindé actif, salve ou bombe en cours, contre-attaque sur une autre zone que attachedZone) — cerveau, SRP_CmdManeuvers (CA1), m_Armor
- `void EndAssault(int zone)` — Contre-attaque finie sur la zone (CA5, zone perdue, défendue) : rideau en attente annulé, blindé de cette attaque en repli — SRP_CmdManeuvers.EndAttack
- `bool IsMortarReady(int region)` — La région peut tirer au mortier (pas muette, pas en tir, appuis actifs) — cerveau, écrans
- `bool IsArtilleryReady(int region)` — La région peut recevoir un tir lourd (batterie non détruite, plafond EQ3 passé) — cerveau, écrans
- `int GetMortarState(int region)` — SRP_ECmdPieceState du mortier de la région — SRP_CmdScreens
- `int GetMortarSilencedUntil(int region)` — Fin de l'interdiction de 24 h du mortier de la région (heure Unix, 0 = aucune) — SRP_CmdScreens
- `vector GetMortarPos(int region)` — Position de la pièce posée (vector.Zero sinon) — SRP_CmdScreens (Staff)
- `int GetBatteryState(int region)` — SRP_ECmdPieceState de la batterie de la région — SRP_CmdScreens
- `int GetBatteryDestroyedUntil(int region)` — Fin de l'interdiction de 24 h de l'artillerie de la région (heure Unix, 0 = aucune) — SRP_CmdScreens
- `vector GetBatteryPos(int region)` — Position de la batterie posée (vector.Zero sinon) — SRP_CmdScreens (Staff)
- `string GetCapsText()` — « mortier 1 tir sur 2 cette heure · 1 blindé à la fois · artillerie lourde dans 1 h 20 · bombe dans 14 h · grosse opération : aucune » (EQ3) — SRP_CmdScreens
- `void GetReport(notnull array<string> lines)` — Lignes Staff : par région mortier et batterie, blindés (m_Armor.Report), plafonds — SRP_CmdScreens
- `string GetNearReport(vector from, float radius)` — Pièces, batteries et blindés autour d'un point — SRP_CmdScreens.StaffNearReport
- `bool OnSabotage(IEntity target, int playerId, out string reply)` — « Saboter » sur une pièce de mortier : m_bSabotaged (silence MO9 au passage suivant), message au joueur ; vrai = consommé — SRP_Commander.OnSabotageUsed
- `string ClearBans(int region, string author)` — Staff : interdictions de 24 h levées (région -1 = toutes) — SRP_CmdScreens (actions interdictions:R3, interdictions:tout)
- `string CancelAll(string author)` — ia-reset et wipe : missions arrêtées (tickets rendus : Release), pièces, batteries et blindés retirés (SRP_EnemyComponent.Delete, décors supprimés) — SRP_Commander.CancelOperations
- `void WriteTo(JsonSaveContext ctx)` — cmd_ap_v, cmd_ap_n puis cmd_ap_rN_code, _mortier_ban, _artillerie_ban ; cmd_ap_mortier_tirs (« unix,unix »), cmd_ap_artillerie_dernier, cmd_ap_bombe_dernier, cmd_ap_avion (« unix,unix,unix »), cmd_ap_helico_dernier, cmd_ap_frappes (« x;z;unix|… ») — SRP_Commander.WriteTo
- `void ReadFrom(JsonLoadContext ctx)` — Relecture miroir par code de région ; heures passées nettoyées — SRP_Commander.ReadFrom
- `void ResetCampaign(string reason)` — Nouvelle campagne : CancelAll, interdictions, plafonds et frappes vidés — SRP_Commander.ResetCampaign
- `void OnRestored()` — Après une restauration : missions de tir arrêtées, pièces et batteries retirées hors de vue — SRP_Commander
- `static bool IsArmedVehicle(IEntity vehicle)` — AP5, renseignement : le véhicule a au moins une place de tourelle (TurretCompartmentSlot) — cerveau (NoteSighting), SRP_CmdArmor
- `static bool IsBatteryVehicle(IEntity vehicle)` — AP1 : le véhicule est un 2S1 (nom de prefab contenant « Tracked/2S1/ », constante : le client ne lit pas le fichier du serveur) — modded SCR_GetInUserAction (SRP_CmdArmor.c), TickBatteries
- `static void PlaySoundLocal(string acp, string eventName, vector pos)` — MO3, chaque machine sauf un serveur dédié : son de départ (SCR_AudioSourceConfiguration, projet acp, évènement eventName) joué à pos — SRP_FrontEnemyComponent.RpcDo_SRPCmdSound
- `static void LaunchLocal(RplId shell, vector dir)` — Repli appui_obus_relance_clients : relance locale de l'obus répliqué (Replication.FindItem, Launch) — SRP_FrontEnemyComponent.RpcDo_SRPCmdLaunch
  - (82 champs de réglage : voir §7)

### SRP_CmdArmor.c (257 lignes)


**class SRP_CmdArmorType**
  Un type de blindé (réglages appui_blinde_N_*)
  - champ `string m_sName` — « BTR-70 »
  - champ `ResourceName m_sPrefab`
  - champ `int m_iWeight` — poids du tirage (AP4)
  - champ `int m_iBounty` — prime en euros s'il est ramené (C3)
  - champ `bool m_bAllowed = true` — faux : prefab refusé par la garde AP4

**class SRP_CmdArmor**
  Un blindé engagé. Non sauvé (C3, H3).
  - champ `IEntity m_Vehicle`
  - champ `ref SRP_EnemyGroup m_Crew`
  - champ `ref SRP_CmdArmorType m_Type`
  - champ `int m_iTicket` — ticket BLINDE de SRP_CmdResources (0 = gratuit)
  - champ `int m_iReason = SRP_ECmdArmorReason.RENFORT`
  - champ `int m_iZone = -1`
  - champ `int m_iRegion = -1`
  - champ `vector m_vTarget`
  - champ `vector m_vSpawn`
  - champ `vector m_vHalt`
  - champ `vector m_vLastPos`
  - champ `int m_iState = SRP_ECmdArmorState.ROUTE`
  - champ `int m_iStateUnix`
  - champ `int m_iLastMoveUnix`
  - champ `int m_iNoPlayerSinceUnix`
  - champ `int m_iEmptySinceUnix`
  - champ `int m_iRetargetUnix`
  - champ `bool m_bPaid`
  - champ `bool m_bStaff`

**class SRP_CmdArmorManager**
  Les blindés du Commandeur (serveur).
- `void SRP_CmdArmorManager(SRP_CmdSupport support)` — Constructeur seul — SRP_CmdSupport (constructeur)
- `void DeclareSettings()` — Déclare ses clés — SRP_CmdSupport.DeclareSettings
- `void LoadSettings()` — Relit ses clés puis refait m_aTypes (garde AP4 à la lecture, ligne de journal par prefab refusé) — SRP_CmdSupport.LoadSettings
- `string Launch(int region, vector target, int reason, int zone, int variant, bool staff)` — Lance un blindé (contrôles déjà faits par RequestArmor, ou Staff) : type (variant, sinon tirage par poids), départ, halte, véhicule, équipage (Ask COMBAT « blinde », Tag), Reserve(BLINDE) hors Staff gratuit, OrderMove ; journal ; rend "" ou la raison — SRP_CmdSupport.RequestArmor
- `void Tick(int nowUnix)` — Toutes les 5 s : cycle de vie de chaque blindé (voir l'en-tête) — SRP_CmdSupport.Tick
- `int CountActive()` — Blindés actifs (ROUTE, ENGAGE, REPLI) : EQ3 « un à la fois » (appui_blinde_simultanes) — SRP_CmdSupport
- `void EndAssault(int zone)` — Contre-attaque finie : le blindé CONTRE_ATTAQUE de la zone passe en REPLI — SRP_CmdSupport.EndAssault
- `string GetEngagedText()` — « engagé : BTR-70 vers Régina (S07) » ou « aucun » — SRP_CmdScreens (page principale)
- `void Report(notnull array<string> lines)` — Lignes Staff : un blindé par ligne (type, état, carré, raison, depuis) — SRP_CmdSupport.GetReport
- `string GetNearReport(vector from, float radius)` — Blindés autour d'un point — SRP_CmdSupport.GetNearReport
- `int GetTypeCount()` — Nombre de types lus — Staff
- `SRP_CmdArmorType GetType(int index)` — Type d'index donné, null hors bornes — Staff
- `int FindTypeByWord(string word)` — Type désigné par un mot du Staff (« btr », « brdm », « typhoon », casse ignorée), -1 sinon — SRP_CmdScreens (forcer)
- `void DeleteAll()` — ia-reset, wipe, nouvelle campagne : équipages et véhicules retirés, tickets rendus (sauf un blindé PRIS ou LIVRE avec un joueur à bord, jamais supprimé) — SRP_CmdSupport.CancelAll
  - (27 champs de réglage : voir §7)

### SRP_CmdScreens.c (468 lignes)


**class SRP_CmdLogEntry**
  Une ligne du journal des décisions (mémoire seulement)
  - champ `int m_iUnix`
  - champ `string m_sHour` — « 21:14 »
  - champ `int m_iRegion = -1` — -1 = toute l'île
  - champ `int m_iKind = SRP_ECmdLogKind.DECISION`
  - champ `string m_sText`

**class SRP_CmdLog**
  Le journal des décisions du Commandeur (VU4), statique.
- `static void Decision(int region, bool heavy, string what, string reason, string cost, string left)` — Une décision prise : « what — reason — coût : cost — reste : left » ; APPUI si heavy (artillerie, bombe, blindé), sinon DECISION ; mémoire + Journal("COMMANDEUR", « [région] … ») — tout le Commandeur, pour CHAQUE décision
- `static void Refusal(int region, string what, string why)` — Une décision envisagée puis écartée : « écarté : what — why » ; mémoire (vu_refus_menu) ; Discord seulement si vu_refus_discord ; une même clé what + why n'est réécrite qu'après vu_refus_repos_min — tout le Commandeur
- `static void Note(int region, int kind, string text)` — Un fait (officier, stock, dépôt, renseignement, système) de nature kind (SRP_ECmdLogKind) — tout le Commandeur, SRP_CmdSettings
- `static void StaffAction(string author, string text)` — Action du Staff : « [Staff] Jack : Commandeur gelé » — SRP_CmdScreens.RunStaff, sous-modules (Staff*)
- `static int CountEntries(int region, bool withRefusals)` — Nombre d'entrées d'une région (-1 = toutes), refus compris ou non — SRP_CmdScreens (pagination)
- `static void GetEntries(int region, bool withRefusals, int first, int count, notnull array<ref SRP_CmdLogEntry> result)` — Entrées filtrées, la plus récente d'abord, à partir de first (count au plus) — SRP_CmdScreens
- `static SRP_CmdLogEntry GetLast()` — Dernière entrée hors refus, null sinon — SRP_CmdScreens (« Dernière décision »)
- `static void Clear()` — Tout effacer (nouvelle campagne, construction du Commandeur : les statiques survivent aux parties du Workbench) — SRP_Commander

**class SRP_CmdIntel**
  Le renseignement des joueurs, par région (VU2, RE9), statique. État sauvé par SRP_Commander.WriteTo (cmd_vu_*).
- `static void Reset(int regions)` — Dimensionne l'état aux régions tracées, tout effacé — SRP_Commander.BuildRegions, ResetCampaign
- `static void OnKeyPointSeized(int zone)` — D4 réussie, AVANT le changement de camp : Grant(région de la zone, PC, libellé de la zone, dépôt révélé) ; un 2e appel rafraîchit l'heure — SRP_FrontEnemyComponent.OnKeyPointSeized
- `static void OnOfficerCaptured(int region)` — OF9 : Grant(région, OFFICIER, nom de l'officier, dépôt révélé) — SRP_CmdRegionBook.OfficerDown (capturé)
- `static void OnMissionSucceeded(int missionType, vector site, string missionId)` — Mission réussie : rien si le site n'est pas sur un carré ROUGE ; type (EnumToString de SRP_EMissionType) dans vu_renseignement_missions -> Grant(région, MISSION, missionId, dépôt si le type est dans vu_depot_missions) — SRP_Commander.OnMissionSucceeded
- `static bool HasIntel(int region)` — La région est renseignée : noté ET (numéro d'officier noté == officier en poste, ou région sans officier) ET (vu_renseignement_heures = 0 ou pas encore échu) — SRP_CmdScreens (PC), carte
- `static int GetIntelSince(int region)` — Heure Unix du renseignement, 0 sans — SRP_CmdScreens
- `static string GetIntelSourceText(int region)` — « PC ennemi saisi à Perelle (S08) », « officier ennemi Volkov capturé », « mission M07 réussie », "" sans — SRP_CmdScreens
- `static void WriteTo(JsonSaveContext ctx)` — cmd_vu_n puis cmd_vu_rN_code, _since, _source, _detail, _serial — SRP_Commander.WriteTo
- `static void ReadFrom(JsonLoadContext ctx)` — Relecture par code de région — SRP_Commander.ReadFrom

**class SRP_CmdScreens**
  Les écrans du Commandeur (Staff et PC), statique ; serveur.
- `static void DeclareSettings()` — Déclare les clés vu_* (défaut = valeur du champ) — SRP_Commander.DeclareSettings
- `static void LoadSettings()` — Relit les clés vu_* — SRP_Commander.LoadSettings
- `static string StaffTab(int playerId, vector staffPos, bool inGame, array<string> parts)` — Page demandée selon parts[1] : « region:R3 », « journal:<page>:<filtre> » (tout, refus, R1..R6), « forcer », « cible:<moyen> », « ressources » ; sinon la page principale (gel, dernière décision, rythme, soldats 96/120 et IA 131/160, moyens de l'île et lien « ressources », plafonds, réglages, régions, journal, forcer) ; Commandeur absent : « L|Commandeur pas encore prêt (démarrage du front) » — SRP_AdminComponent.BuildMenu
- `static string RunStaff(int playerId, vector staffPos, bool inGame, string action, array<string> parts, string author, out bool wantTeleport, out vector teleportTo)` — Exécute une action : gel -> SetFrozen ; relire -> SRP_Commander.ReloadSettings ; forcer et frappe -> moyen parts[2], cible parts[3] (« moi » = staffPos si inGame, « j<id> » = personnage du joueur) -> SRP_Commander.ForceOrder(kind, variant, position, author) (SEULE entrée pour forcer une DÉCISION, trou 15 ; hélico et colonne compris) ; officier-tue, officier-capture -> ForceOfficerDown ; officier-nouveau -> ForceOfficerReplace ; tp-cachette, tp-depot -> wantTeleport. Corrections des moyens (pas des décisions) : stock-plein -> SRP_CmdResources.StaffRefillAll ; stock:<genre>:<R3|ile>:<0|plein> -> StaffSetStock(kind, region, 0 ou GetFull) ; renflouer:R3 -> RequestResupply ; depot-deplacer:R3 -> GetDepots().StaffMove ; depot-saboter:R3 -> StaffSabotage ; coupure-lever:R3 -> StaffClearCut ; depot-reveler:R3 -> Reveal(région, « Staff ») (repère seul, sans radio) ; interdictions:R3 ou interdictions:tout -> SRP_CmdSupport.ClearBans(région ou -1) ; toujours SRP_CmdLog.StaffAction ; rend le texte affiché en tête — SRP_AdminComponent.Run (qui téléporte si wantTeleport)
- `static bool IsInternalPage(string action)` — Actions qui ouvrent une page sans rien exécuter (region, journal, forcer, cible, ressources) — SRP_AdminComponent.Execute
- `static bool NeedsConfirm(string action)` — Actions à deux clics (officier-tue, officier-capture, frappe, stock-plein, stock, depot-saboter, depot-reveler) — SRP_AdminComponent.NeedsConfirm
- `static string PageAfter(string action, array<string> parts)` — Page à rouvrir après une action : officier-*, tp-*, renflouer, depot-*, coupure-lever et interdictions:<R> -> « commandeur:region:<R> », forcer et frappe -> « commandeur:forcer », stock-plein et stock -> « commandeur: ressources », le reste -> « commandeur » — SRP_AdminComponent.PageAfter
- `static string StaffHeaderPart()` — « , COMMANDEUR GELÉ » s'il est gelé, sinon "" (collé après SRP_FrontScreens.StaffHeaderPart) — SRP_AdminComponent.Header
- `static string StaffTerritoryLine()` — « C|y|Commandeur : GELÉ — interrupteur séparé, onglet Commandeur » ou « L|Commandeur : actif — … » — SRP_FrontScreens.StaffTerritoire
- `static string StaffNearReport(vector from, float radius)` — Page « rapide:ia-groupes » : officiers et cachettes (m_Book), mortier, 2S1, blindés (appuis), colonnes (manœuvres) à radius (vu_proximite_m par défaut) — SRP_AdminComponent
- `static string PCZoneSuffix(int zone)` — PC, zones ENNEMIES seulement (I3) : « · région de Saint-Philippe » (+ « · désorganisée ») ; région renseignée : « · officier ennemi Volkov · troupes renforcées | normales | affaiblies » (GetZoneStrength : prévu x 100 / nominal, seuils vu_troupes_renforcees et vu_troupes_affaiblies) ; "" sans région — SRP_FrontScreens.PCTerritoire
- `static string PCIntelSection(int playerId)` — PC, section « H|Renseignement sur l'ennemi » : une ligne par région (renseignées, désorganisées, autres, libérées), dépôt seulement s'il est révélé, puis le mode d'emploi ; "" sans Commandeur — SRP_FrontScreens.PCTerritoire
- `static string RegionOf(string regionName)` — « région de X », ou « région d'X » devant a, e, i, o, u, y, é, è, h — radio, écrans, SRP_CmdRegionBook
- `static string OfficerLabel(string surname)` — « officier ennemi Volkov » (VU1) — radio, écrans
- `static string DurationText(int seconds)` — « 2 h 10 » ou « 12 min » — écrans, journal
- `static string CellText(vector position)` — « 074 042 » : référence du carré sous la position — écrans, journal
- `static string TraitWord(int trait)` — « prudent », « audacieux », « lent », « méthodique » — écrans, journal
- `static string MoodWord(int mood)` — « prudente », « normale », « agressive » — écrans
- `static string Esc(string text)` — Texte libre sans « | » ni retour à la ligne (comme SRP_AdminComponent.Esc) — toutes les pages
  - (15 champs de réglage : voir §7)

## 3. Cycles (serveur, heure Unix)

**Démarrage** (s'insère dans l'ordre du §3 de CONTRATS_FRONT) :
1. t = 0, `SRP_FrontEnemyComponent.OnPostInit` (serveur, hors éditeur) : `m_Commander = new SRP_Commander(this)` —
   `s_Instance`, sous-modules (constructeurs seuls), `SRP_CmdLog.Clear()`, `DeclareSettings()` : 386 clés du Commandeur
   (cerveau, régions, ressources, dépôts, manœuvres, appuis, blindés, capacité, écrans). Rien n'est lu ni tracé.
2. t = 5 s, `SRP_FrontComponent.Start`, étape 2 : `SRP_CmdSettings.Reload("démarrage")` (écrit
   `commandeur_reglages.txt` s'il manque) puis `SRP_FrontEnemyComponent.LoadSettings()` -> `m_Commander.LoadSettings()`
   (chaîne : m_Book, m_Resources + m_Depots, m_Maneuvers, m_Support + m_Armor, capacité, écrans).
3. Étape 4 (après la géométrie, AVANT Load) : `SRP_Commander.Get().BuildRegions()` -> `m_Book.Trace()` (tracé OF1,
   lignes « region S12 -> R3 », rapport `commandeur_regions.txt`), `m_aIntel` dimensionné, `SRP_CmdIntel.Reset(régions)`.
4. Étape 5, `Load` -> `SRP_FrontEnemyComponent.ReadFrom(ctx)` -> `SRP_Commander.ReadFrom(ctx)` (après zones et
   localités) : régions et officiers par code, stocks, dépôts, manœuvres, appuis, renseignement ; RE13 : tickets ouverts
   rendus, colonnes et camions en route rendus à leur source (une seule fois).
5. Premier `SRP_FrontEnemyComponent.Tick` qui voit le front prêt : `m_Commander.Start()` -> capacité
   (`ApplyEngineLimit` 160, `SetMaxGroups(36, 6)`, contrôle de cohérence), `m_Book.Start` (officiers manquants,
   cachettes), `m_Resources.Start` (parts d'obus, rattrapage RE6, dépôts), `m_Maneuvers.Start`, `m_Support.Start`,
   premières échéances des cycles ; journal « Commandeur prêt ».
6. Arrêt (ordre des OnDelete non garanti) : `SRP_FrontEnemyComponent.OnDelete` -> d'abord
   `SRP_FrontComponent.SaveFinalFromEnemy()` (écriture complète, chaîne fe_* et cmd_* comprise), puis
   `m_Commander.Stop()` (CallLater de tir retirés, s_Instance = null). Le socle refuse ensuite d'écrire un front.json
   sans l'ennemi (garde `m_bEnemyChained` de `SRP_FrontComponent.Save`).

**`SRP_Commander.Tick(nowUnix)`**, appelé par `SRP_FrontEnemyComponent.Tick` (5 s) juste après AttackTick :

| Ordre | Quoi | Cadence | Gelé (VU6) |
|---|---|---|---|
| 1 | `SRP_CmdCapacity.Get().Tick(now)` (demandes, RE12, limite du jeu à la minute) | 5 s | oui (Q7) |
| 2 | version d'état du socle changée : `DiffCells`, `m_Book.OnFrontChanged` (cachettes seulement ; la libération OF11 passe par `CheckLiberation`, appelée par `OnZoneCaptured`) | 5 s | oui |
| 3 | `CollectGroupReports`, `ProcessPending`, `ExpireKnowledge` | 5 s | oui |
| 4 | `m_Book.Tick` (officiers : remplacement, cachette, pose, retrait, fuite, chute) | 5 s | oui |
| 5 | `InformantsTick` | `info_periode_s` | oui (les signalements n'agissent pas gelé) |
| 6 | `m_Resources.Tick` (revenu, renflouements, CA7, `m_Depots.Tick`) | 60 s | oui (renflouement : non) |
| 7 | `m_Maneuvers.Tick` (sous-cadences 10 s, 60 s, 300 s) | 5 s | fin propre seulement (`IsCommanderActive()` faux) |
| 8 | `m_Support.Tick` (pièces, batteries, `m_Armor.Tick`, hélico) | 5 s | oui (aucun tir neuf) |
| 9 | `FastCycle` (réflexes échus, mortier, hélico, fouilles) | `cmd_cycle_rapide_min_s`..`max_s` | non |
| 10 | `SlowCycle` (blindé, artillerie lourde, bombe) | `cmd_cycle_lent_min_s`..`max_s` | non |

Contre-attaque : ni dans les cycles (trou 19) ; `SRP_FrontEnemyComponent.AttackClock` appelle
`SRP_CmdManeuvers.CaWindowDecision` chaque minute de la fenêtre D ± `ca_fenetre_min` quand `IsCommanderActive()`.
CallLater : seulement `SRP_CmdSupport` pour enchaîner les coups (FireNext, DropShell, DropBomb, salve) ; retirés par
`Stop` et `CancelAll`. Jamais de GetTickCount pour ce qui est sauvé (seules exceptions héritées :
`TryRadioCall`, camions et file d'apparition, non sauvés).

## 4. Flux (appels directs, AUCUN ScriptInvoker)

- **Zone changée** (SRP_ZoneFall -> `SRP_FrontEnemyComponent` -> Commandeur) :
  - `OnZoneCaptured(zone, staff)` : alerte `alerte_zone` de la région (hors staff), `m_Book.CheckLiberation(zone,
    staff, maintenant)` (SEULE porte OF11 : région toute bleue -> LIBEREE, officier DISPARU ; `SRP_FrontRadio
    .RegionLiberated` seulement si !staff), `m_Resources.OnZoneOwnerChanged(zone, BLEU, staff)` ->
    `m_Depots.OnZoneOwnerChanged(zone, BLEU, staff)` (dépôt de la zone : sabotage « zone prise » ; staff : déplacé en
    silence, ni coupure, ni radio, ni depot_ennemi), `m_Maneuvers.OnZoneCaptured` (MA11, hors staff).
  - `OnZoneLost(zone, reason, staff)` : `m_Book.AttachZone` si la région est libérée (OF11), `m_Resources
    .OnZoneOwnerChanged(zone, ROUGE, staff)`, `m_Maneuvers.OnZoneLost`. Staff : état seul, sans alerte ni annonce (Q9).
  - `OnLocalityTaken(nom)` : cachettes revues (`m_Book`), replis et colonnes réorientés (`m_Maneuvers`).
- **Saisie D4** : `SRP_FrontEnemyComponent.OnKeyPointSeized` -> `SRP_CmdIntel.OnKeyPointSeized(zone)` AVANT le
  changement de camp -> renseignement de la région (PC) + dépôt révélé -> `SRP_FrontRadio.IntelGained`.
- **Mission réussie** : `End(SUCCES)` -> `SRP_FrontEnemyComponent.OnMissionSucceeded` -> `SRP_Commander
  .OnMissionSucceeded` -> `SRP_CmdIntel.OnMissionSucceeded` (carré rouge seulement ; dépôt si ECOUTE ou DOCUMENTS),
  `m_Resources.OnSabotageMission` (SABOTAGE, CACHE : un quart du plein), `m_Book.AddRegionAlert(alerte_sabotage)` UNE
  fois.
- **« Saboter »** (UNE entrée : en tête de `OnObjectiveUsed`, que `SRP_SabotageAction` appelle déjà ; aucun appel
  direct depuis l'action) : `OnObjectiveUsed` -> `SRP_FrontEnemyComponent.OnSabotageUsed` -> `SRP_Commander.OnSabotageUsed` ->
  `m_Depots.TryUse` (dépôt : `CutRegion`, radio `EnemyDepotSabotaged`, évènement depot_ennemi, alerte) puis
  `m_Support.OnSabotage` (pièce de mortier : silence MO9 au passage suivant).
- **Menace** : `SRP_FrontComponent.AddThreat` -> `SRP_FrontEnemyComponent.OnThreatChanged` -> `SRP_Commander
  .OnThreatChanged` -> `m_Resources.OnThreatChanged` (pleins RE10).
- **Renfort** (MA1, C2, trou 5) : soldat qui voit -> `SRP_Commander.NoteSighting` (tampon du groupe) -> compte rendu
  (`CollectGroupReports`, 30 à 60 s, radio de la localité) -> `Integrate` -> `OfficerReflex` (zone rouge,
  `m_Maneuvers.CanReinforceZone`, taille `ReinforcementSize` sur les joueurs ESTIMÉS, officier lent + 75 s) ->
  `IssueOrder(RENFORT)` -> `SRP_CmdManeuvers.RequestReinforcement` (source, délai x `renfort_facteur_desorg`, camion
  localité / camion de contact / colonne) -> `SRP_EnemyTruckComponent.DispatchFrom` ou `SendContactTruck` (pose :
  `SRP_CmdCapacity.Get().Ask(COMBAT)`, `Tag`, `OnTruckLaunched` -> `AddSent` + registre des camions en route ;
  débarquement ou perte -> `OnTruckUnloaded`) ou colonne (`NewColumn` -> `AddSent` au départ). Retour des débarqués
  vivants : `SRP_EnemyComponent.RetireTick` seul.
- **Appuis** : cycles du cerveau (cible = contact connu) -> `IssueOrder` -> `SRP_CmdSupport.Request*` (sécurité, EQ3,
  EQ4 par `SRP_Commander.GetDosingPlayers`, MO6, interdictions, `Reserve`) ; en contre-attaque : `SRP_CmdManeuvers
  .Support` -> mêmes `Request*`.
- **Officier tombé** : `m_Book.OfficerDown` -> région désorganisée, alerte, `SRP_FrontRadio.OfficerFell` ; capturé :
  `SRP_CmdIntel.OnOfficerCaptured` -> renseignement + `m_Depots.Reveal` -> `SRP_FrontRadio.IntelGained` ; `MarkDirty(true)`.
- **Pièce ou batterie réduite au silence** : `m_Support` -> interdiction 24 h (sauvée) -> `SRP_FrontRadio
  .SupportSilenced` (si `SRP_CmdScreens.s_bRadioSilence`).
- **Blindé ramené** (C3) : `m_Armor` -> `SRP_TreasuryComponent.GetInstance().Add(prime, raison)` ->
  `SRP_FrontRadio.ArmorBroughtBack`.
- **Place** : toute pose -> `SRP_CmdCapacity.Get().Ask(clé, classe, soldats, groupes, position, propriétaire)` ;
  après la pose -> `SRP_CmdCapacity.Tag(record, classe)` ; le front ennemi -> `SetWantedGarrisons` à chaque passage et
  `GarrisonCap` à chaque pose de localité.
- **Staff** : `SRP_AdminComponent` (onglet « commandeur ») -> `SRP_CmdScreens.StaffTab / RunStaff` ->
  `SRP_Commander.SetFrozen / ForceOrder / ForceOfficerDown / ForceOfficerReplace / ReloadSettings` (décisions :
  ForceOrder seul) ; corrections de moyens : `SRP_CmdResources.StaffRefillAll / StaffSetStock / RequestResupply`,
  `SRP_CmdDepots.StaffMove / StaffSabotage / StaffClearCut / Reveal`, `SRP_CmdSupport.ClearBans` ; ia-reset et wipe
  -> `SRP_Commander.CancelOperations`.

## 5. Sauvegarde dans front.json (clés `cmd_*`, à plat, écrites par la chaîne unique)

`SRP_FrontComponent.Save` -> `SRP_FrontEnemyComponent.WriteTo(ctx)` (clés `fe_*`) -> `SRP_Commander.WriteTo(ctx)`.
Relecture miroir par `ReadFrom`, APRÈS `BuildRegions`, les zones et les localités. Régions, dépôts, renseignement et
interdictions sont retrouvés par le CODE de région (« R3 ») ; les zones par leur code (« S07 ») ; une clé absente
garde son défaut (lecteur tolérant : `ReadValue` rend faux). Aucun extra, aucun fichier à part (trou 16).

| Clés | Écrites par | Contenu |
|---|---|---|
| `cmd_v`, `cmd_gel`, `cmd_hash` | SRP_Commander | format (1), gel VU6, somme de contrôle du tracé (différente : avertissement, états gardés par code) |
| `cmd_rn`, `cmd_rserial`, `cmd_rnames` | SRP_CmdRegionBook | nombre de régions, prochain numéro d'officier, noms récents (« a,b,c ») |
| `cmd_rN_code`, `_state`, `_desorg`, `_desorgCap`, `_lib`, `_alert`, `_alertAt`, `_prevTrait` | SRP_CmdRegionBook | région N |
| `cmd_rN_of`, `_of_serial`, `_of_name`, `_of_trait`, `_of_state`, `_of_since`, `_of_down`, `_of_day`, `_of_loc`, `_of_prev`, `_of_escaped` | SRP_CmdRegionBook | officier de la région N |
| `cmd_rmc`, `cmd_rmN_z`, `cmd_rmN_r` | SRP_CmdRegionBook | rattachements OF11 (code de zone, code de région) |
| `cmd_res_v`, `cmd_res_updated`, `cmd_res_blinde`, `cmd_res_helico`, `cmd_res_artillerie`, `cmd_res_bombe`, `cmd_res_niveau_depart`, `cmd_res_depart_unix` | SRP_CmdResources | stocks de l'île, dernière avance du revenu, CA7 |
| `cmd_res_n`, `cmd_res_rN_code`, `_obus`, `_coupe` | SRP_CmdResources | obus et coupure de chaque région |
| `cmd_res_tn`, `cmd_res_tN_kind`, `_region`, `_left`, `_what`, `_free` | SRP_CmdResources | tickets ouverts (RENDUS à la relecture, RE13) |
| `cmd_res_vn`, `cmd_res_vN_from`, `_to`, `_amount` | SRP_CmdResources | renflouements en route (rendus au donneur) |
| `cmd_dep_n`, `cmd_dep_rN_code`, `_x`, `_y`, `_z`, `_zone`, `_prev`, `_revele`, `_par`, `_attente`, `_moves` | SRP_CmdDepots | dépôt de chaque région (reposé au chargement) |
| `cmd_ma_v`, `cmd_ma_harass`, `cmd_ma_ca_last`, `cmd_ma_ca_count` | SRP_CmdManeuvers | dernier harcèlement, dernière contre-attaque, compte |
| `cmd_ma_cols`, `cmd_ma_cN_from`, `_to`, `_soldiers` | SRP_CmdManeuvers | colonnes en route, papier ou réelles (RENDUES à leur source à la relecture) |
| `cmd_ma_tn`, `cmd_ma_tN_from`, `_n` | SRP_CmdManeuvers | camions de renfort hors colonne lancés et pas encore déchargés (RENDUS à leur source à la relecture, RE13) |
| `cmd_ap_v`, `cmd_ap_n`, `cmd_ap_rN_code`, `_mortier_ban`, `_artillerie_ban` | SRP_CmdSupport | interdictions de 24 h par région (MO9, AP1) |
| `cmd_ap_mortier_tirs`, `cmd_ap_artillerie_dernier`, `cmd_ap_bombe_dernier`, `cmd_ap_avion`, `cmd_ap_helico_dernier`, `cmd_ap_frappes` | SRP_CmdSupport | plafonds EQ3 (listes « unix,unix », frappes « x;z;unix|… ») |
| `cmd_vu_n`, `cmd_vu_rN_code`, `_since`, `_source`, `_detail`, `_serial` | SRP_CmdIntel | renseignement de chaque région (VU2) |

Sur `SRP_FrontLocality` (écrits par le socle, clés `lN_*`) : `m_iSent`, `m_iSentAt`, `m_iBonus`, `m_iBonusAt`,
`m_iEmptiedAt`, `m_iFullUntil` (manœuvres, par la façade `SRP_FrontEnemyComponent`).
Jamais sauvé (H3) : connaissance, comptes rendus, ordres, frein C2, papier, raids, replis, débarqués au combat
(les combats en cours sont annulés : ils restent des envoyés, regarnis en `front_regarnissage_heures`), missions de tir, blindés,
pièces et batteries posées, demandes de place, parts des localités, journal des décisions (le salon Staff et le
fichier du mois gardent l'historique).
Taille estimée : 4 à 6 Ko (6 régions).

## 6. Évènements, radio et journal

Textes publics : **`SRP_FrontRadio` seul** (trou 13), 8 fonctions déjà au squelette du front, appelées ainsi :

| Fonction (SRP_FrontRadio) | Appelée par | Évènement du pont (`PushZoneEvent`) |
|---|---|---|
| `OfficerFell(int region, string surname, bool captured, int zone)` | `SRP_CmdRegionBook.OfficerDown` | `officier_tombe` (radio seule si `vu_officier_discord` = 0) |
| `RegionLiberated(int region, string surname)` | `SRP_CmdRegionBook.CheckLiberation` (seulement si !staff, Q9) | `region_liberee` |
| `IntelGained(int region, int source, string detail, bool depotRevealed, string depotRef, int zone)` | `SRP_CmdIntel.Grant` (source = `SRP_ECmdIntelSource`) | `renseignement` |
| `EnemyDepotSabotaged(int region, int hours, int zone)` | `SRP_CmdDepots.Sabotage` | `depot_ennemi` |
| `SupportSilenced(int region, bool battery)` | `SRP_CmdSupport` (MO9, AP1) | aucun (radio seule, si `vu_radio_silence`) |
| `ArmorBroughtBack(string vehicleName, int reward)` | `SRP_CmdArmorManager` (C3) | `blinde_ramene` |
| `AttackBroken(int zone, int retaken)` / `AttackCalledOff(int zone)` | `SRP_FrontEnemyComponent.EndAttack` | `zone_assaut` / aucun |

La radio lit les noms par `SRP_Commander.Get().GetRegionLabel(region)` et `GetBook().GetOfficerName(region)`, et les
réglages par `SRP_CmdScreens.s_bRadioSilence`, `s_bOfficerDiscord`, `s_iDepotIcon`, `s_iDepotColor`. Jamais le type
« commandement » (réservé aux captures sans mission, G2), jamais un mot confidentiel (IA, automatique, garnison,
groupe, patrouille, tirage), jamais de signe pour cent. Pas d'annonce pour un remplaçant d'officier (VU2), pour une
zone forcée par le Staff (Q9), ni pour les gros moyens (EQ2 : jamais d'interception).

Journal du Commandeur : **`SRP_CmdLog` seul** (catégorie COMMANDEUR par `SRP_EnemyComponent.Journal`), mémoire de
`vu_journal_memoire` lignes pour le menu Staff, flux Discord « etat-major » (Staff seul, secours journal-serveur),
jamais public, jamais dans l'onglet Journal du PC. `SRP_CmdSettings` y écrit ses problèmes (`SYSTEME`) en plus de la
console. Une décision = `SRP_CmdLog.Decision(region, heavy, what, reason, cost, left)` ; une décision écartée =
`SRP_CmdLog.Refusal(region, what, why)` ; un fait = `Note(region, kind, text)` ; une action du Staff = `StaffAction`.

## 7. Clés de réglages (commandeur_reglages.txt)

Lecteur unique : `SRP_CmdSettings` (§1.4). 386 clés, toutes dans `$profile:SimpleRP/commandeur_reglages.txt` (aucune ne commence par `front_`). Défaut = valeur initiale du champ = choix de Jack (QCM, arbitrages C1 à C8) ou proposition marquée au §10.7 ; bornes appliquées par `GetIntClamped` / `GetFloatClamped` (valeur hors bornes ramenée et signalée) ; « Lu par » = la classe qui déclare la clé (DeclareSettings) et la relit (LoadSettings), relue au démarrage et par « Relire les réglages » sans republier. Réglages de STRUCTURE (`cmd_regions`, `cmd_region_min_zones`, `cmd_region_max_zones`) : pris en compte au redémarrage. Source : `code/_gen/cmd_settings.py`.

| Clé | Défaut | Bornes | Champ | Règle | Lu par |
|---|---|---|---|---|---|
| `cmd_gel_au_depart` | 0 |  | `m_bFrozenAtStart` | VU6 | SRP_Commander |
| `cmd_staff_gratuit` | 1 |  | `m_bStaffFree` | VU5 | SRP_Commander |
| `cmd_cycle_rapide_min_s` | 60 | 10 à 3600 | `m_iFastMinS` | CO4 | SRP_Commander |
| `cmd_cycle_rapide_max_s` | 120 | 10 à 3600 | `m_iFastMaxS` | CO4 | SRP_Commander |
| `cmd_cycle_lent_min_s` | 900 | 60 à 14400 | `m_iSlowMinS` | CO4 | SRP_Commander |
| `cmd_cycle_lent_max_s` | 1200 | 60 à 14400 | `m_iSlowMaxS` | CO4 | SRP_Commander |
| `cmd_rapport_min_s` | 30 | 5 à 600 | `m_iReportMinS` | OF3 | SRP_Commander |
| `cmd_rapport_max_s` | 60 | 5 à 600 | `m_iReportMaxS` | OF3 | SRP_Commander |
| `cmd_rapport_tirs_max` | 60 | 1 à 500 | `m_iReportShotsMax` | CO6 | SRP_Commander |
| `cmd_radio_retard_min` | 5 | 0 à 60 | `m_iRadioDelayMin` | OF3 | SRP_Commander |
| `cmd_flou_vu_min_m` | 50 | 0 à 1000 | `m_iBlurSeenMinM` | CO5 | SRP_Commander |
| `cmd_flou_vu_max_m` | 100 | 0 à 1000 | `m_iBlurSeenMaxM` | CO5 | SRP_Commander |
| `cmd_flou_entendu_min_m` | 100 | 0 à 2000 | `m_iBlurHeardMinM` | CO5 | SRP_Commander |
| `cmd_flou_entendu_max_m` | 150 | 0 à 2000 | `m_iBlurHeardMaxM` | CO5 | SRP_Commander |
| `cmd_oubli_s` | 1200 | 60 à 7200 | `m_iForgetS` | CO5 | SRP_Commander |
| `cmd_fusion_contacts_m` | 200 | 10 à 2000 | `m_iMergeM` | CO5 | SRP_Commander |
| `cmd_contact_confirme_s` | 300 | 10 à 3600 | `m_iConfirmS` | CO6 | SRP_Commander |
| `cmd_pertes_appui_s` | 600 | 10 à 7200 | `m_iLossJustifyS` | CO6 | SRP_Commander |
| `cmd_contact_vise_s` | 120 | 10 à 3600 | `m_iAimS` | CO6 | SRP_Commander |
| `cmd_tirs_intenses_3min` | 30 | 1 à 1000 | `m_iIntenseShots` | CO6 | SRP_Commander |
| `cmd_trait_poids` | 1, 1, 1, 1 |  | `m_sTraitWeights` | CO7 | SRP_Commander |
| `cmd_trait_lent_retard_s` | 75 | 0 à 1800 | `m_iSlowTraitDelayS` | CO7 | SRP_Commander |
| `cmd_trait_prudent_taille` | 75 | 10 à 300 | `m_iPrudentSizeCent` | CO7 | SRP_Commander |
| `cmd_trait_audacieux_taille` | 125 | 10 à 300 | `m_iBoldSizeCent` | CO7 | SRP_Commander |
| `cmd_trait_prudent_reserve_obus` | 50 | 0 à 100 | `m_iPrudentShellKeepCent` | CO7 | SRP_Commander |
| `cmd_humeur_stock_bas` | 33 | 0 à 100 | `m_iMoodStockLowCent` | CO7 | SRP_Commander |
| `cmd_humeur_stock_haut` | 66 | 0 à 100 | `m_iMoodStockHighCent` | CO7 | SRP_Commander |
| `cmd_humeur_pertes_region_60min` | 15 | 1 à 500 | `m_iMoodLosses60` | CO7 | SRP_Commander |
| `cmd_humeur_reprises_60min` | 2 | 1 à 100 | `m_iMoodRetaken60` | CO7 | SRP_Commander |
| `cmd_humeur_prudente_taille` | 75 | 10 à 300 | `m_iMoodPrudentSizeCent` | CO7 | SRP_Commander |
| `cmd_humeur_agressive_taille` | 125 | 10 à 300 | `m_iMoodAggressiveSizeCent` | CO7 | SRP_Commander |
| `cmd_grav_point_cle` | 5 | 0 à 100 | `m_iGravKeyPoint` | MA9 | SRP_Commander |
| `cmd_grav_point_cle_m` | 150 | 10 à 1000 | `m_iGravKeyPointM` | MA9 | SRP_Commander |
| `cmd_grav_carre_perdu` | 3 | 0 à 100 | `m_iGravCellLost` | MA9 | SRP_Commander |
| `cmd_grav_perte` | 1 | 0 à 100 | `m_iGravLoss` | MA9 | SRP_Commander |
| `cmd_grav_tirs_par` | 10 | 1 à 1000 | `m_iGravShotsPer` | MA9 | SRP_Commander |
| `cmd_grav_joueurs_max` | 6 | 0 à 100 | `m_iGravPlayersMax` | MA9 | SRP_Commander |
| `cmd_grav_contre_attaque` | 2 | 0 à 100 | `m_iGravCounterAttack` | MA9 | SRP_Commander |
| `cmd_grav_fenetre_min` | 15 | 1 à 60 | `m_iGravWindowMin` | MA9 | SRP_Commander |
| `cmd_grav_zones_servies` | 1 | 1 à 20 | `m_iGravServed` | MA9 | SRP_Commander |
| `cmd_grav_zones_servies_nombreux` | 2 | 1 à 20 | `m_iGravServedMany` | MA9 | SRP_Commander |
| `cmd_grav_joueurs_nombreux` | 10 | 1 à 100 | `m_iGravManyPlayers` | MA9 | SRP_Commander |
| `cmd_mortier_repos_zone_s` | 600 | 0 à 7200 | `m_iMortarZoneRestS` | CO4 | SRP_Commander |
| `cmd_mortier_zones_par_cycle` | 3 | 1 à 20 | `m_iMortarZonesPerCycle` | CO4 | SRP_Commander |
| `cmd_helico_sans_troupe_m` | 800 | 0 à 5000 | `m_iHeliNoTroopM` | AP7 | SRP_Commander |
| `cmd_blinde_vehicule_s` | 1200 | 0 à 7200 | `m_iArmorVehicleS` | AP5 | SRP_Commander |
| `cmd_blinde_gravite` | 8 | 0 à 100 | `m_iArmorGravity` | AP5 | SRP_Commander |
| `cmd_artillerie_joueurs` | 4 | 1 à 50 | `m_iArtilleryPlayers` | AP2 | SRP_Commander |
| `cmd_artillerie_immobile_s` | 600 | 60 à 7200 | `m_iArtilleryStillS` | AP2 | SRP_Commander |
| `cmd_artillerie_immobile_m` | 100 | 10 à 1000 | `m_iArtilleryStillM` | AP2 | SRP_Commander |
| `cmd_bombe_joueurs` | 4 | 1 à 50 | `m_iBombPlayers` | AP3 | SRP_Commander |
| `alerte_renfort_plein` | 60 | 0 à 1000 | `m_iAlertFullReinforce` | CO9 | SRP_Commander |
| `renfort_taille_base` | 4 | 1 à 30 | `m_iReinforceBase` | MA1 | SRP_Commander |
| `renfort_taille_par_joueur` | 2 | 0 à 10 | `m_iReinforcePerPlayer` | MA1 | SRP_Commander |
| `renfort_taille_plein` | 10 | 1 à 30 | `m_iReinforceFull` | MA1 | SRP_Commander |
| `info_actif` | 1 |  | `m_bInformants` | C4 | SRP_Commander |
| `info_periode_s` | 30 | 5 à 600 | `m_iInfoPeriodS` | C4 | SRP_Commander |
| `info_portee_m` | 120 | 10 à 1000 | `m_iInfoRangeM` | C4 | SRP_Commander |
| `info_chance` | 25 | 0 à 100 | `m_iInfoChance` | C4 | SRP_Commander |
| `info_civil_repos_s` | 900 | 0 à 7200 | `m_iInfoCivilRestS` | C4 | SRP_Commander |
| `info_zone_repos_s` | 600 | 0 à 7200 | `m_iInfoZoneRestS` | C4 | SRP_Commander |
| `info_delai_min_s` | 120 | 0 à 3600 | `m_iInfoDelayMinS` | C4 | SRP_Commander |
| `info_delai_max_s` | 300 | 0 à 3600 | `m_iInfoDelayMaxS` | C4 | SRP_Commander |
| `info_flou_min_m` | 150 | 0 à 2000 | `m_iInfoBlurMinM` | C4 | SRP_Commander |
| `info_flou_max_m` | 300 | 0 à 2000 | `m_iInfoBlurMaxM` | C4 | SRP_Commander |
| `info_compte_m` | 50 | 0 à 500 | `m_iInfoCountM` | C4 | SRP_Commander |
| `cmd_regions` | 6 | 1 à 12 | `m_iRegionsWanted` | OF1 | SRP_CmdRegionBook |
| `cmd_region_min_zones` | 8 | 1 à 60 | `m_iRegionMinZones` | OF1 | SRP_CmdRegionBook |
| `cmd_region_max_zones` | 13 | 1 à 60 | `m_iRegionMaxZones` | OF1 | SRP_CmdRegionBook |
| `alerte_max` | 100 | 1 à 1000 | `m_iAlertMax` | CO9 | SRP_CmdRegionBook |
| `alerte_retombee_min` | 180 | 1 à 1440 | `m_iAlertDecayMin` | CO9 | SRP_CmdRegionBook |
| `alerte_vu` | 2 | 0 à 100 | `m_iAlertSeen` | CO9 | SRP_CmdRegionBook |
| `alerte_entendu` | 1 | 0 à 100 | `m_iAlertHeard` | CO9 | SRP_CmdRegionBook |
| `alerte_tirs_par` | 25 | 1 à 1000 | `m_iAlertShotsPer` | CO9 | SRP_CmdRegionBook |
| `alerte_perte` | 2 | 0 à 100 | `m_iAlertLoss` | CO9 | SRP_CmdRegionBook |
| `alerte_carre` | 4 | 0 à 100 | `m_iAlertCell` | CO9 | SRP_CmdRegionBook |
| `alerte_zone` | 15 | 0 à 100 | `m_iAlertZone` | CO9 | SRP_CmdRegionBook |
| `alerte_officier` | 30 | 0 à 1000 | `m_iAlertOfficer` | CO9 | SRP_CmdRegionBook |
| `alerte_sabotage` | 15 | 0 à 100 | `m_iAlertSabotage` | CO9 | SRP_CmdRegionBook |
| `alerte_informateur` | 2 | 0 à 100 | `m_iAlertInformant` | CO9 | SRP_CmdRegionBook |
| `alerte_vehicule` | 3 | 0 à 100 | `m_iAlertVehicle` | CO9 | SRP_CmdRegionBook |
| `alerte_menace_par` | 20 | 1 à 1000 | `m_iAlertThreatPer` | CO9 | SRP_CmdRegionBook |
| `alerte_menace_max` | 5 | 0 à 10 | `m_iAlertThreatMax` | CO9 | SRP_CmdRegionBook |
| `officier_gardes` | 2 | 0 à 6 | `m_iOfficerGuards` | OF2 | SRP_CmdRegionBook |
| `officier_cachettes` | 3 | 1 à 10 | `m_iOfficerHideouts` | OF2 | SRP_CmdRegionBook |
| `officier_changement_heure` | 6 | 0 à 23 | `m_iOfficerDayHour` | OF2 | SRP_CmdRegionBook |
| `officier_apparition_m` | 1500 | 100 à 5000 | `m_iOfficerSpawnM` | OF2 | SRP_CmdRegionBook |
| `officier_retrait_m` | 2500 | 100 à 10000 | `m_iOfficerDespawnM` | OF2 | SRP_CmdRegionBook |
| `officier_retrait_min` | 5 | 0 à 120 | `m_iOfficerDespawnMin` | OF2 | SRP_CmdRegionBook |
| `officier_marge_ia` | 10 | 0 à 100 | `m_iOfficerAiMargin` | OF2 | SRP_CmdRegionBook |
| `officier_fuite_m` | 250 | 10 à 2000 | `m_iOfficerFleeM` | OF10 | SRP_CmdRegionBook |
| `officier_echappe_m` | 1500 | 100 à 5000 | `m_iOfficerEscapeM` | OF10 | SRP_CmdRegionBook |
| `officier_abandon_m` | 300 | 10 à 2000 | `m_iOfficerAbandonM` | OF9 | SRP_CmdRegionBook |
| `officier_inconscient_min` | 10 | 1 à 120 | `m_iOfficerUnconsciousMin` | OF9 | SRP_CmdRegionBook |
| `officier_captif_retrait_min` | 10 | 1 à 120 | `m_iOfficerCaptiveRetireMin` | OF9 | SRP_CmdRegionBook |
| `officier_prefab` | (vide) |  | `m_sOfficerPrefab` | OF2 | SRP_CmdRegionBook |
| `officier_bonus_garnison` | 25 | 0 à 200 | `m_iOfficerGarrisonBonusCent` | OF5 | SRP_CmdRegionBook |
| `officier_noms` | Volkov, Orlov, Sokolov, Morozov, Lebedev, Kozlov, Novikov… |  | `m_sOfficerNames` | VU1 | SRP_CmdRegionBook |
| `officier_noms_memoire` | 12 | 0 à 29 | `m_iOfficerNamesMemory` | VU1 | SRP_CmdRegionBook |
| `desorg_min` | 180 | 1 à 2880 | `m_iDesorgMin` | OF7 | SRP_CmdRegionBook |
| `desorg_capture_facteur` | 2 | 1 à 10 | `m_iDesorgCaptureFactor` | OF9 | SRP_CmdRegionBook |
| `res_obus_plein` | 48 | 0 à 1000 | `m_iShellsFull` | RE3 | SRP_CmdResources |
| `res_blindes_plein` | 2 | 0 à 20 | `m_iArmorFull` | RE3 | SRP_CmdResources |
| `res_helico_plein` | 3 | 0 à 20 | `m_iHeliFull` | AP7 | SRP_CmdResources |
| `res_artillerie_plein` | 1 | 0 à 20 | `m_iArtilleryFull` | RE3 | SRP_CmdResources |
| `res_bombe_plein` | 1 | 0 à 10 | `m_iBombFull` | AP3 | SRP_CmdResources |
| `res_bonus_menace_centiemes` | 5 | 0 à 50 | `m_iThreatBonusCent` | RE10 | SRP_CmdResources |
| `res_bombe_bonus_menace` | 0 |  | `m_bBombThreatBonus` | AP3 | SRP_CmdResources |
| `res_heures_plein` | 4.0 | 0.1 à 72.0 | `m_fRefillHours` | RE5 | SRP_CmdResources |
| `res_plancher_centiemes` | 50 | 0 à 100 | `m_iFloorCent` | RE4 | SRP_CmdResources |
| `res_zones_depart` | 0 | 0 à 200 | `m_iRefZonesOverride` | RE4 | SRP_CmdResources |
| `res_depot_coupure_heures` | 24 | 0 à 240 | `m_iDepotCutHours` | RE8 | SRP_CmdResources |
| `res_depot_coupure_centiemes` | 50 | 0 à 100 | `m_iDepotCutCent` | RE8 | SRP_CmdResources |
| `res_renflouement_minutes` | 60 | 0 à 600 | `m_iResupplyMinutes` | RE2 | SRP_CmdResources |
| `res_renflouement_seuil_obus` | 4 | 0 à 100 | `m_iResupplyThreshold` | RE2 | SRP_CmdResources |
| `res_renflouement_centiemes` | 50 | 0 à 100 | `m_iResupplyCent` | RE2 | SRP_CmdResources |
| `res_renflouement_donneur_centiemes` | 50 | 0 à 100 | `m_iDonorKeepCent` | RE2 | SRP_CmdResources |
| `res_sabotage_centiemes` | 25 | 0 à 100 | `m_iSabotageCent` | RE8 | SRP_CmdResources |
| `res_offensive_bonus` | 10 | 0 à 100 | `m_iOffensiveBonus` | CA7 | SRP_CmdResources |
| `res_offensive_plein_centiemes` | 95 | 0 à 100 | `m_iOffensiveFullCent` | CA7 | SRP_CmdResources |
| `res_offensive_bas_centiemes` | 33 | 0 à 100 | `m_iOffensiveLowCent` | CA7 | SRP_CmdResources |
| `res_etat_affaiblie_centiemes` | 34 | 0 à 100 | `m_iStateWeakCent` | VU2 | SRP_CmdResources |
| `res_etat_renforcee_centiemes` | 90 | 0 à 100 | `m_iStateStrongCent` | VU2 | SRP_CmdResources |
| `res_depot_distance_bleu_m` | 1000 | 0 à 5000 | `m_iDepotBlueM` | RE8 | SRP_CmdDepots |
| `res_depot_distance_point_cle_m` | 400 | 0 à 3000 | `m_iDepotKeyPointM` | RE8 | SRP_CmdDepots |
| `res_depot_route_m` | 150 | 0 à 1000 | `m_iDepotRoadM` | RE8 | SRP_CmdDepots |
| `res_depot_distance_pose_m` | 1500 | 0 à 5000 | `m_iDepotPlaceM` | RE8 | SRP_CmdDepots |
| `res_depot_gardes_distance_m` | 1500 | 100 à 5000 | `m_iDepotGuardsM` | RE9 | SRP_CmdDepots |
| `res_depot_gardes_max` | 2 | 0 à 6 | `m_iDepotGuardsMax` | RE9 | SRP_CmdDepots |
| `res_depot_gardes_retrait_m` | 2500 | 100 à 10000 | `m_iDepotGuardsRetireM` | RE9 | SRP_CmdDepots |
| `res_depot_gardes_retrait_min` | 10 | 0 à 120 | `m_iDepotGuardsRetireMin` | RE9 | SRP_CmdDepots |
| `res_depot_prefab` | {361B78F0EC88F339}Prefabs/Props/Military/SupplyBox/Supply… |  | `m_sDepotPrefab` | RE8 | SRP_CmdDepots |
| `cap_limite_jeu` | 160 | 40 à 1000 | `m_iEngineLimit` | Q7 | SRP_CmdCapacity |
| `cap_soldats_max` | 120 | 10 à 500 | `m_iMaxSoldiers` | Q7 | SRP_CmdCapacity |
| `cap_reserve_renforts` | 20 | 0 à 200 | `m_iCombatReserve` | RE11 | SRP_CmdCapacity |
| `cap_reserve_postes` | 12 | 0 à 200 | `m_iPostReserve` | Q7 | SRP_CmdCapacity |
| `cap_groupes_max` | 36 | 1 à 200 | `m_iMaxGroups` | RE11 | SRP_CmdCapacity |
| `cap_groupes_reserve_missions` | 6 | 0 à 50 | `m_iMissionGroupReserve` | RE11 | SRP_CmdCapacity |
| `cap_groupes_reserve_renforts` | 5 | 0 à 50 | `m_iCombatGroupReserve` | RE11 | SRP_CmdCapacity |
| `cap_groupes_reserve_postes` | 4 | 0 à 50 | `m_iPostGroupReserve` | Q7 | SRP_CmdCapacity |
| `cap_marge_jeu` | 10 | 0 à 100 | `m_iEngineMargin` | Q7 | SRP_CmdCapacity |
| `cap_civils_prevus` | 25 | 0 à 200 | `m_iPlannedCivilians` | Q7 | SRP_CmdCapacity |
| `cap_pilotes_prevus` | 2 | 0 à 20 | `m_iPlannedPilots` | Q7 | SRP_CmdCapacity |
| `cap_retrait_rondes_m` | 800 | 0 à 10000 | `m_iReleasePatrolM` | RE12 | SRP_CmdCapacity |
| `cap_retrait_jeeps_m` | 1500 | 0 à 10000 | `m_iReleaseJeepM` | RE12 | SRP_CmdCapacity |
| `cap_retrait_par_passe` | 2 | 0 à 20 | `m_iReleasePerPass` | RE12 | SRP_CmdCapacity |
| `cap_demande_s` | 45 | 5 à 600 | `m_iDemandS` | RE12 | SRP_CmdCapacity |
| `cap_garnison_min` | 8 | 0 à 60 | `m_iGarrisonMin` | C6 | SRP_CmdCapacity |
| `cap_garnison_max` | 60 | 1 à 200 | `m_iGarrisonMax` | OF5 | SRP_CmdCapacity |
| `cap_garnison_proche_m` | 1000 | 0 à 10000 | `m_iGarrisonNearM` | C6 | SRP_CmdCapacity |
| `cap_garnison_poids_proche` | 2 | 1 à 10 | `m_iGarrisonNearWeight` | C6 | SRP_CmdCapacity |
| `cap_importance_ennemis` | 3 | 0 à 3 | `m_iImportanceEnemy` | RE12 | SRP_CmdCapacity |
| `cap_importance_rondes` | 2 | 0 à 3 | `m_iImportancePatrol` | RE12 | SRP_CmdCapacity |
| `cap_importance_civils` | 2 | 0 à 3 | `m_iImportanceCivilian` | RE12 | SRP_CmdCapacity |
| `renfort_actif` | 1 |  | `m_bReinforceOn` | MA1 | SRP_CmdManeuvers |
| `renfort_frein_min` | 10 | 0 à 120 | `m_iBrakeMin` | C2 | SRP_CmdManeuvers |
| `renfort_source_garde_centiemes` | 50 | 0 à 100 | `m_iSourceKeepCent` | C2 | SRP_CmdManeuvers |
| `renfort_source_garde_prudent_centiemes` | 67 | 0 à 100 | `m_iSourceKeepPrudentCent` | CO7 | SRP_CmdManeuvers |
| `renfort_source_distance_m` | 4000 | 500 à 20000 | `m_iSourceM` | MA2 | SRP_CmdManeuvers |
| `renfort_source_distance_poche_m` | 6000 | 500 à 20000 | `m_iSourcePocketM` | MA12 | SRP_CmdManeuvers |
| `renfort_arrivee_min_s` | 180 | 10 à 3600 | `m_iArriveMinS` | MA2 | SRP_CmdManeuvers |
| `renfort_arrivee_max_s` | 360 | 10 à 3600 | `m_iArriveMaxS` | MA2 | SRP_CmdManeuvers |
| `renfort_facteur_desorg` | 2.0 | 1.0 à 10.0 | `m_fDesorgFactor` | OF6 | SRP_CmdManeuvers |
| `renfort_chargement_min` | 2 | 1 à 20 | `m_iLoadMin` | C2 | SRP_CmdManeuvers |
| `renfort_chargement_max` | 14 | 1 à 30 | `m_iLoadMax` | MA3 | SRP_CmdManeuvers |
| `renfort_retrait_min` | 20 | 1 à 240 | `m_iContactTruckRetireMin` | MA1 | SRP_CmdManeuvers |
| `renfort_rendre_survivants` | 1 |  | `m_bReturnSurvivors` | MA2 | SRP_CmdManeuvers |
| `colonne_distance_m` | 1500 | 100 à 5000 | `m_iColumnRealM` | MA4 | SRP_CmdManeuvers |
| `colonne_vitesse` | 7.0 | 0.5 à 30.0 | `m_fColumnSpeed` | MA4 | SRP_CmdManeuvers |
| `colonne_joueur_min_m` | 700 | 0 à 5000 | `m_iColumnPlayerMinM` | MA4 | SRP_CmdManeuvers |
| `colonne_recul_pas_m` | 100 | 10 à 1000 | `m_iColumnBackStepM` | MA4 | SRP_CmdManeuvers |
| `colonne_recul_max_m` | 1000 | 0 à 5000 | `m_iColumnBackMaxM` | MA4 | SRP_CmdManeuvers |
| `colonne_soldats_camion` | 10 | 1 à 20 | `m_iColumnSoldiersPerTruck` | MA4 | SRP_CmdManeuvers |
| `colonne_camions_max` | 2 | 1 à 6 | `m_iColumnTrucksMax` | MA4 | SRP_CmdManeuvers |
| `colonne_essai_s` | 10 | 5 à 600 | `m_iColumnRetryS` | MA4 | SRP_CmdManeuvers |
| `colonne_pas_route_m` | 300 | 50 à 2000 | `m_iColumnRoadStepM` | MA4 | SRP_CmdManeuvers |
| `repli_actif` | 1 |  | `m_bRetreatOn` | MA5 | SRP_CmdManeuvers |
| `repli_section_centiemes` | 34 | 0 à 100 | `m_iRetreatSectionCent` | MA5 | SRP_CmdManeuvers |
| `repli_garnison_centiemes` | 34 | 0 à 100 | `m_iRetreatGarrisonCent` | MA5 | SRP_CmdManeuvers |
| `repli_contact_min` | 5 | 1 à 60 | `m_iRetreatContactMin` | MA5 | SRP_CmdManeuvers |
| `repli_couverture_s` | 75 | 0 à 600 | `m_iRetreatCoverS` | MA6 | SRP_CmdManeuvers |
| `repli_grenades` | 2 | 0 à 6 | `m_iRetreatGrenades` | MA6 | SRP_CmdManeuvers |
| `repli_sortie_m` | 250 | 50 à 2000 | `m_iRetreatExitM` | MA6 | SRP_CmdManeuvers |
| `repli_voisine_max_m` | 3000 | 500 à 20000 | `m_iRetreatNeighbourM` | MA6 | SRP_CmdManeuvers |
| `repli_angle_min` | 90 | 0 à 180 | `m_iRetreatAngleMin` | MA6 | SRP_CmdManeuvers |
| `repli_effacement_m` | 300 | 50 à 5000 | `m_iRetreatEraseM` | MA6 | SRP_CmdManeuvers |
| `repli_vide_distance_m` | 2000 | 100 à 10000 | `m_iEmptiedM` | MA7 | SRP_CmdManeuvers |
| `repli_centre_rayon_m` | 40 | 5 à 300 | `m_iRetreatCentreM` | MA5 | SRP_CmdManeuvers |
| `assaut_deux_axes_groupes` | 3 | 1 à 20 | `m_iTwoAxesGroups` | MA8 | SRP_CmdManeuvers |
| `assaut_deux_axes_soldats` | 12 | 1 à 200 | `m_iTwoAxesSoldiers` | MA8 | SRP_CmdManeuvers |
| `assaut_deux_axes_rapport` | 2.0 | 0.5 à 10.0 | `m_fTwoAxesRatio` | MA8 | SRP_CmdManeuvers |
| `assaut_angle_axes` | 60 | 0 à 180 | `m_iAxesAngle` | MA8 | SRP_CmdManeuvers |
| `assaut_retranche_m` | 40 | 5 à 500 | `m_iEntrenchedM` | MA8 | SRP_CmdManeuvers |
| `assaut_retranche_min` | 5 | 1 à 60 | `m_iEntrenchedMin` | MA8 | SRP_CmdManeuvers |
| `assaut_connus_rayon_m` | 300 | 50 à 3000 | `m_iKnownRadiusM` | MA8 | SRP_CmdManeuvers |
| `assaut_connus_min` | 10 | 1 à 60 | `m_iKnownMin` | MA8 | SRP_CmdManeuvers |
| `assaut_feu_min_m` | 250 | 50 à 2000 | `m_iFireMinM` | MA8 | SRP_CmdManeuvers |
| `assaut_feu_max_m` | 350 | 50 à 2000 | `m_iFireMaxM` | MA8 | SRP_CmdManeuvers |
| `assaut_suppression_s` | 90 | 5 à 600 | `m_iSuppressS` | MA8 | SRP_CmdManeuvers |
| `assaut_suppression_ouverture_s` | 45 | 5 à 600 | `m_iSuppressOpenS` | MA8 | SRP_CmdManeuvers |
| `assaut_suppression_hauteur` | 1.5 | 0.0 à 5.0 | `m_fSuppressHeight` | MA8 | SRP_CmdManeuvers |
| `assaut_fumee_distance_m` | 300 | 50 à 2000 | `m_iSmokeDistanceM` | MA8 | SRP_CmdManeuvers |
| `assaut_fumee_grenades` | 1 | 0 à 6 | `m_iSmokeGrenades` | MA8 | SRP_CmdManeuvers |
| `assaut_synchro_distance_m` | 400 | 50 à 3000 | `m_iSyncDistanceM` | MA8 | SRP_CmdManeuvers |
| `assaut_synchro_max_s` | 240 | 10 à 1800 | `m_iSyncMaxS` | MA8 | SRP_CmdManeuvers |
| `assaut_attaque_finale` | 1 |  | `m_bFinalAttack` | MA8 | SRP_CmdManeuvers |
| `harcelement_actif` | 1 |  | `m_bHarassOn` | MA10 | SRP_CmdManeuvers |
| `harcelement_intervalle_min` | 60 | 5 à 1440 | `m_iHarassIntervalMin` | MA10 | SRP_CmdManeuvers |
| `harcelement_chance` | 25 | 0 à 100 | `m_iHarassChance` | MA10 | SRP_CmdManeuvers |
| `harcelement_joueurs_min` | 2 | 1 à 50 | `m_iHarassPlayersMin` | MA10 | SRP_CmdManeuvers |
| `harcelement_portee_m` | 1500 | 100 à 10000 | `m_iHarassRangeM` | MA10 | SRP_CmdManeuvers |
| `harcelement_connus_min` | 20 | 1 à 120 | `m_iHarassKnownMin` | MA10 | SRP_CmdManeuvers |
| `harcelement_obus` | 3 | 1 à 20 | `m_iHarassShells` | MA10 | SRP_CmdManeuvers |
| `harcelement_part_obus` | 50 | 0 à 100 | `m_iHarassShellChance` | MA10 | SRP_CmdManeuvers |
| `harcelement_groupes_max` | 2 | 1 à 6 | `m_iHarassGroupsMax` | MA10 | SRP_CmdManeuvers |
| `harcelement_deux_groupes_des` | 6 | 1 à 50 | `m_iHarassTwoGroupsFrom` | MA10 | SRP_CmdManeuvers |
| `harcelement_depart_min_m` | 400 | 100 à 5000 | `m_iHarassFromMinM` | MA10 | SRP_CmdManeuvers |
| `harcelement_depart_max_m` | 800 | 100 à 5000 | `m_iHarassFromMaxM` | MA10 | SRP_CmdManeuvers |
| `harcelement_combat_min` | 10 | 1 à 120 | `m_iHarassFightMin` | MA10 | SRP_CmdManeuvers |
| `harcelement_pertes_retrait_centiemes` | 50 | 0 à 100 | `m_iHarassLossCent` | MA10 | SRP_CmdManeuvers |
| `harcelement_apres_ca_min` | 20 | 0 à 240 | `m_iHarassAfterCaMin` | MA10 | SRP_CmdManeuvers |
| `reorg_complet_heures` | 6 | 0 à 72 | `m_iFullHours` | MA11 | SRP_CmdManeuvers |
| `reorg_bonus_score` | 500 | 0 à 10000 | `m_iFullScoreBonus` | MA11 | SRP_CmdManeuvers |
| `reorg_chemin_distance_m` | 2500 | 100 à 20000 | `m_iPathM` | MA11 | SRP_CmdManeuvers |
| `reorg_chemin_angle` | 45 | 0 à 180 | `m_iPathAngle` | MA11 | SRP_CmdManeuvers |
| `reorg_chemin_intervalle_min` | 20 | 1 à 240 | `m_iPathIntervalMin` | MA11 | SRP_CmdManeuvers |
| `reorg_chemin_calme_min` | 30 | 0 à 240 | `m_iPathQuietMin` | MA11 | SRP_CmdManeuvers |
| `reorg_postes_distance_m` | 1500 | 100 à 10000 | `m_iPathPostsM` | MA11 | SRP_CmdManeuvers |
| `reorg_releve_m` | 150 | 10 à 5000 | `m_iTrackM` | MA11 | SRP_CmdManeuvers |
| `reorg_releve_min` | 10 | 1 à 120 | `m_iTrackMin` | MA11 | SRP_CmdManeuvers |
| `ca_fenetre_min` | 20 | 0 à 120 | `m_iWindowMin` | CA1 | SRP_CmdManeuvers |
| `ca_ecart_min` | 45 | 0 à 600 | `m_iGapMin` | CA1 | SRP_CmdManeuvers |
| `ca_seuil_avance` | 70 | 0 à 100 | `m_iEarlyScore` | CA1 | SRP_CmdManeuvers |
| `ca_seuil_audacieux` | 60 | 0 à 100 | `m_iEarlyScoreBold` | CA1 | SRP_CmdManeuvers |
| `ca_seuil_heure` | 40 | 0 à 100 | `m_iOnTimeScore` | CA1 | SRP_CmdManeuvers |
| `ca_presence_rayon_m` | 1000 | 100 à 5000 | `m_iPresenceM` | CA1 | SRP_CmdManeuvers |
| `ca_presence_min` | 10 | 1 à 120 | `m_iPresenceMin` | CA1 | SRP_CmdManeuvers |
| `ca_cible_depot` | 40 | 0 à 100 | `m_iTargetDepot` | CA2 | SRP_CmdManeuvers |
| `ca_cible_vide` | 30 | 0 à 100 | `m_iTargetEmpty` | CA2 | SRP_CmdManeuvers |
| `ca_cible_carre` | 2 | 0 à 100 | `m_iTargetCell` | CA2 | SRP_CmdManeuvers |
| `ca_cible_min` | 20 | 1 à 240 | `m_iTargetMin` | CA2 | SRP_CmdManeuvers |
| `ca_vagues_min` | 2 | 1 à 10 | `m_iWavesMin` | CA3 | SRP_CmdManeuvers |
| `ca_vagues_max` | 4 | 1 à 10 | `m_iWavesMax` | CA3 | SRP_CmdManeuvers |
| `ca_vague_bonne_centiemes` | 67 | 0 à 100 | `m_iWaveGoodCent` | CA3 | SRP_CmdManeuvers |
| `ca_vague_brisee_centiemes` | 34 | 0 à 100 | `m_iWaveBrokenCent` | CA3 | SRP_CmdManeuvers |
| `ca_marge_place` | 6 | 0 à 60 | `m_iRoomMargin` | CA3 | SRP_CmdManeuvers |
| `ca_mortier_avance_s` | 120 | 0 à 900 | `m_iMortarLeadS` | CA4 | SRP_CmdManeuvers |
| `ca_lourd_avance_s` | 60 | 0 à 900 | `m_iHeavyLeadS` | CA4 | SRP_CmdManeuvers |
| `ca_amas_joueurs` | 4 | 1 à 50 | `m_iClusterPlayers` | CA4 | SRP_CmdManeuvers |
| `ca_amas_rayon_m` | 70 | 10 à 500 | `m_iClusterM` | CA4 | SRP_CmdManeuvers |
| `ca_feinte` | 25 | 0 à 100 | `m_iFeintChance` | MO8 | SRP_CmdManeuvers |
| `ca_avion_chance` | 50 | 0 à 100 | `m_iFlybyChance` | CA4 | SRP_CmdManeuvers |
| `ca_abandon_centiemes` | 67 | 0 à 100 | `m_iAbandonCent` | C8 | SRP_CmdManeuvers |
| `ca_abandon_vagues` | 2 | 1 à 10 | `m_iAbandonWaves` | CA5 | SRP_CmdManeuvers |
| `ca_papier_distance_m` | 1500 | 100 à 5000 | `m_iPaperM` | CA6 | SRP_CmdManeuvers |
| `ca_eveil_distance_m` | 900 | 100 à 5000 | `m_iAwakeM` | CA6 | SRP_CmdManeuvers |
| `ca_effacement_distance_m` | 2000 | 100 à 10000 | `m_iEraseM` | CA6 | SRP_CmdManeuvers |
| `ca_effacement_min` | 3 | 1 à 60 | `m_iEraseMin` | CA6 | SRP_CmdManeuvers |
| `ca_pied_vitesse` | 1.3 | 0.1 à 10.0 | `m_fFootSpeed` | CA6 | SRP_CmdManeuvers |
| `ca_bloque_min` | 3 | 1 à 60 | `m_iStuckMin` | CA6 | SRP_CmdManeuvers |
| `ca_bloque_papier_min` | 10 | 1 à 120 | `m_iStuckPaperMin` | CA6 | SRP_CmdManeuvers |
| `ca_papier_joueur_min_m` | 1000 | 0 à 5000 | `m_iPaperPlayerMinM` | CA6 | SRP_CmdManeuvers |
| `appui_actif` | 1 |  | `m_bOn` | VU6 | SRP_CmdSupport |
| `appui_distance_base_m` | 1500 | 0 à 10000 | `m_iBaseM` | MO6 | SRP_CmdSupport |
| `appui_securite_explosif_m` | 150 | 0 à 1000 | `m_iSafeHeM` | MO6 | SRP_CmdSupport |
| `appui_securite_fumee_m` | 40 | 0 à 500 | `m_iSafeSmokeM` | MO7 | SRP_CmdSupport |
| `appui_securite_salve_m` | 220 | 0 à 1000 | `m_iSafeSalvoM` | AP1 | SRP_CmdSupport |
| `appui_securite_bombe_m` | 250 | 0 à 1000 | `m_iSafeBombM` | AP3 | SRP_CmdSupport |
| `appui_meme_endroit_m` | 400 | 0 à 3000 | `m_iSameSpotM` | EQ3 | SRP_CmdSupport |
| `appui_meme_endroit_min` | 20 | 0 à 240 | `m_iSameSpotMin` | EQ3 | SRP_CmdSupport |
| `appui_rayon_joueurs_m` | 3000 | 500 à 10000 | `m_iPlayersM` | EQ4 | SRP_CmdSupport |
| `appui_obus_hauteur_m` | 200 | 20 à 1000 | `m_iShellHeightM` | MO3 | SRP_CmdSupport |
| `appui_obus_coef_vitesse` | 1.0 | 0.1 à 5.0 | `m_fShellSpeedCoef` | MO3 | SRP_CmdSupport |
| `appui_obus_relance_clients` | 0 |  | `m_bShellClientRelaunch` | MO3 | SRP_CmdSupport |
| `appui_obus_explosif` | {98EC9C526AFBA282}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_HE… |  | `m_sShellHe` | MO1 | SRP_CmdSupport |
| `appui_obus_fumee` | {A544A2C131DE2C64}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_Sm… |  | `m_sShellSmoke` | MO7 | SRP_CmdSupport |
| `appui_mortier_seuil_joueurs` | 3 | 1 à 50 | `m_iMortarPlayers` | EQ4 | SRP_CmdSupport |
| `appui_mortier_tirs_heure` | 2 | 0 à 20 | `m_iMortarPerHour` | EQ3 | SRP_CmdSupport |
| `appui_mortier_fumee_compte` | 0 |  | `m_bSmokeCounts` | C5 | SRP_CmdSupport |
| `appui_mortier_delai_min_s` | 60 | 0 à 900 | `m_iMortarDelayMinS` | MO5 | SRP_CmdSupport |
| `appui_mortier_delai_max_s` | 120 | 0 à 900 | `m_iMortarDelayMaxS` | MO5 | SRP_CmdSupport |
| `appui_mortier_obus_min` | 4 | 1 à 30 | `m_iMortarShellsMin` | MO5 | SRP_CmdSupport |
| `appui_mortier_obus_max` | 6 | 1 à 30 | `m_iMortarShellsMax` | MO5 | SRP_CmdSupport |
| `appui_mortier_cadence_min_s` | 10 | 1 à 120 | `m_iMortarRateMinS` | MO5 | SRP_CmdSupport |
| `appui_mortier_cadence_max_s` | 15 | 1 à 120 | `m_iMortarRateMaxS` | MO5 | SRP_CmdSupport |
| `appui_mortier_erreur_depart_m` | 150 | 0 à 1000 | `m_iMortarErrorStartM` | MO4 | SRP_CmdSupport |
| `appui_mortier_erreur_pas_m` | 40 | 0 à 500 | `m_iMortarErrorStepM` | MO4 | SRP_CmdSupport |
| `appui_mortier_erreur_min_m` | 30 | 0 à 500 | `m_iMortarErrorMinM` | MO4 | SRP_CmdSupport |
| `appui_mortier_observateur_m` | 100 | 10 à 1000 | `m_iMortarObserverM` | MO4 | SRP_CmdSupport |
| `appui_mortier_distance_min_m` | 800 | 100 à 5000 | `m_iMortarPieceMinM` | MO2 | SRP_CmdSupport |
| `appui_mortier_distance_max_m` | 1200 | 100 à 5000 | `m_iMortarPieceMaxM` | MO2 | SRP_CmdSupport |
| `appui_mortier_portee_max_m` | 3500 | 500 à 10000 | `m_iMortarRangeM` | MO2 | SRP_CmdSupport |
| `appui_mortier_joueur_min_m` | 600 | 0 à 5000 | `m_iMortarPlayerMinM` | MO2 | SRP_CmdSupport |
| `appui_mortier_presence_m` | 2000 | 100 à 10000 | `m_iMortarPresenceM` | MO2 | SRP_CmdSupport |
| `appui_mortier_retrait_min` | 5 | 0 à 120 | `m_iMortarRetireMin` | MO2 | SRP_CmdSupport |
| `appui_mortier_servants` | 2 | 1 à 6 | `m_iMortarCrew` | MO1 | SRP_CmdSupport |
| `appui_mortier_servant_rayon_m` | 25 | 5 à 200 | `m_iMortarCrewM` | MO9 | SRP_CmdSupport |
| `appui_mortier_silence_confirm_s` | 10 | 0 à 120 | `m_iMortarSilenceConfirmS` | MO9 | SRP_CmdSupport |
| `appui_mortier_silence_heures` | 24 | 0 à 240 | `m_iMortarSilenceHours` | MO9 | SRP_CmdSupport |
| `appui_mortier_temps_vol_s` | 15 | 1 à 120 | `m_iMortarFlightS` | MO3 | SRP_CmdSupport |
| `appui_mortier_piece` | {6A5B0C0D0E0F7A05}Prefabs/Missions/SRP_Piece_Mortier.et |  | `m_sMortarPiece` | MO1 | SRP_CmdSupport |
| `appui_mortier_servants_groupe` | {56FD583BBC989204}Prefabs/Groups/OPFOR/RHS_AFRF/MSV/VKPO_… |  | `m_sMortarCrewGroup` | MO1 | SRP_CmdSupport |
| `appui_mortier_son` | {3A984BD46A47EEC8}Sounds/Weapons/Mortars/2B14/Weapons_Mor… |  | `m_sMortarSound` | MO3 | SRP_CmdSupport |
| `appui_mortier_son_evenement` | SOUND_SHOT |  | `m_sMortarSoundEvent` | MO3 | SRP_CmdSupport |
| `appui_fumee_obus` | 3 | 1 à 12 | `m_iSmokeShells` | MO7 | SRP_CmdSupport |
| `appui_fumee_devant_joueurs_m` | 80 | 0 à 500 | `m_iSmokeAheadM` | MO7 | SRP_CmdSupport |
| `appui_fumee_largeur_m` | 100 | 10 à 500 | `m_iSmokeWidthM` | MO7 | SRP_CmdSupport |
| `appui_fumee_dispersion_m` | 20 | 0 à 200 | `m_iSmokeSpreadM` | MO7 | SRP_CmdSupport |
| `appui_fumee_declenchement_m` | 400 | 50 à 3000 | `m_iSmokeTriggerM` | MO7 | SRP_CmdSupport |
| `appui_feinte_decalage_m` | 300 | 50 à 2000 | `m_iFeintOffsetM` | MO8 | SRP_CmdSupport |
| `appui_feinte_obus_explosifs` | 2 | 0 à 12 | `m_iFeintHeShells` | MO8 | SRP_CmdSupport |
| `appui_feinte_obus_fumee` | 3 | 0 à 12 | `m_iFeintSmokeShells` | MO8 | SRP_CmdSupport |
| `appui_artillerie_seuil_joueurs` | 8 | 1 à 50 | `m_iArtilleryPlayers` | EQ4 | SRP_CmdSupport |
| `appui_artillerie_intervalle_h` | 2 | 0 à 48 | `m_iArtilleryIntervalH` | EQ3 | SRP_CmdSupport |
| `appui_artillerie_reglage_min` | 1 | 0 à 5 | `m_iAdjustMin` | EQ1 | SRP_CmdSupport |
| `appui_artillerie_reglage_max` | 2 | 0 à 5 | `m_iAdjustMax` | EQ1 | SRP_CmdSupport |
| `appui_artillerie_reglage_ecart_m` | 100 | 0 à 1000 | `m_iAdjustOffsetM` | EQ1 | SRP_CmdSupport |
| `appui_artillerie_avance_min_s` | 30 | 0 à 600 | `m_iAdjustLeadMinS` | EQ1 | SRP_CmdSupport |
| `appui_artillerie_avance_max_s` | 60 | 0 à 600 | `m_iAdjustLeadMaxS` | EQ1 | SRP_CmdSupport |
| `appui_artillerie_premier_coup_min_s` | 20 | 0 à 600 | `m_iFirstShotMinS` | AP1 | SRP_CmdSupport |
| `appui_artillerie_premier_coup_max_s` | 40 | 0 à 600 | `m_iFirstShotMaxS` | AP1 | SRP_CmdSupport |
| `appui_artillerie_salve_min` | 25 | 1 à 100 | `m_iSalvoMin` | AP1 | SRP_CmdSupport |
| `appui_artillerie_salve_max` | 30 | 1 à 100 | `m_iSalvoMax` | AP1 | SRP_CmdSupport |
| `appui_artillerie_rayon_m` | 70 | 10 à 500 | `m_iSalvoRadiusM` | AP1 | SRP_CmdSupport |
| `appui_artillerie_cadence_min_s` | 2 | 1 à 30 | `m_iSalvoRateMinS` | AP1 | SRP_CmdSupport |
| `appui_artillerie_cadence_max_s` | 3 | 1 à 30 | `m_iSalvoRateMaxS` | AP1 | SRP_CmdSupport |
| `appui_artillerie_batterie_min_m` | 2000 | 500 à 10000 | `m_iBatteryMinM` | AP1 | SRP_CmdSupport |
| `appui_artillerie_batterie_max_m` | 3000 | 500 à 10000 | `m_iBatteryMaxM` | AP1 | SRP_CmdSupport |
| `appui_artillerie_portee_max_m` | 3500 | 500 à 20000 | `m_iBatteryRangeM` | AP1 | SRP_CmdSupport |
| `appui_artillerie_garde_m` | 1000 | 100 à 5000 | `m_iBatteryGuardM` | AP1 | SRP_CmdSupport |
| `appui_artillerie_presence_m` | 3000 | 100 à 10000 | `m_iBatteryPresenceM` | AP1 | SRP_CmdSupport |
| `appui_artillerie_retrait_min` | 10 | 0 à 120 | `m_iBatteryRetireMin` | AP1 | SRP_CmdSupport |
| `appui_artillerie_silence_heures` | 24 | 0 à 240 | `m_iBatterySilenceHours` | AP1 | SRP_CmdSupport |
| `appui_artillerie_batterie` | {D8136D90BE12445F}Prefabs/Vehicles/Tracked/2S1/Tank_2S1.et |  | `m_sBatteryPrefab` | AP1 | SRP_CmdSupport |
| `appui_artillerie_son` | (vide) |  | `m_sBatterySound` | AP1 | SRP_CmdSupport |
| `appui_artillerie_son_evenement` | (vide) |  | `m_sBatterySoundEvent` | AP1 | SRP_CmdSupport |
| `appui_bombe_seuil_joueurs` | 10 | 1 à 100 | `m_iBombPlayers` | AP3 | SRP_CmdSupport |
| `appui_bombe_intervalle_h` | 24 | 0 à 240 | `m_iBombIntervalH` | AP3 | SRP_CmdSupport |
| `appui_bombe_dispersion_m` | 3 | 0 à 100 | `m_iBombSpreadM` | AP3 | SRP_CmdSupport |
| `appui_bombe_avion_avance_s` | 10 | 0 à 120 | `m_iBombPlaneLeadS` | EQ1 | SRP_CmdSupport |
| `appui_bombe_prefab` | {B881CA7B63D4EDF5}Prefabs/Vehicles/Bombs/UMPK500/UMPK500_… |  | `m_sBombPrefab` | AP3 | SRP_CmdSupport |
| `appui_avion_prefab` | {FDD01BF3CEAB37E3}Prefabs/Vehicles/Airplanes/SU57/SU57_Fl… |  | `m_sPlanePrefab` | AP3 | SRP_CmdSupport |
| `appui_avion_passages_24h` | 3 | 0 à 50 | `m_iFlybysPerDay` | AP3 | SRP_CmdSupport |
| `appui_helico_repos_min` | 30 | 0 à 600 | `m_iHeliRestMin` | AP7 | SRP_CmdSupport |
| `appui_blinde_seuil_joueurs` | 6 | 1 à 50 | `m_iArmorPlayers` | EQ4 | SRP_CmdArmorManager |
| `appui_blinde_simultanes` | 1 | 0 à 5 | `m_iArmorAtOnce` | EQ3 | SRP_CmdArmorManager |
| `appui_blinde_1_nom` | BTR-70 |  | `m_sType1Name` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_1_prefab` | {447CFE8D73F95C3E}Prefabs/Vehicles/Wheeled/BTR70/BTR70_AF… |  | `m_sType1Prefab` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_1_poids` | 45 | 0 à 1000 | `m_iType1Weight` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_1_prime` | 2000 | 0 à 100000 | `m_iType1Bounty` | C3 | SRP_CmdArmorManager |
| `appui_blinde_2_nom` | BRDM-2 |  | `m_sType2Name` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_2_prefab` | {9AEDF323A812F9ED}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2_AF… |  | `m_sType2Prefab` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_2_poids` | 45 | 0 à 1000 | `m_iType2Weight` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_2_prime` | 2000 | 0 à 100000 | `m_iType2Bounty` | C3 | SRP_CmdArmorManager |
| `appui_blinde_3_nom` | Typhoon |  | `m_sType3Name` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_3_prefab` | {AB5DE68E3E654FCA}Prefabs/Vehicles/Wheeled/K4386/K4386_Ar… |  | `m_sType3Prefab` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_3_poids` | 10 | 0 à 1000 | `m_iType3Weight` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_3_prime` | 5000 | 0 à 100000 | `m_iType3Bounty` | C3 | SRP_CmdArmorManager |
| `appui_blinde_equipage` | 2 | 1 à 6 | `m_iCrewSize` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_equipage_prefab` | {0848BAB1B94FCBBA}Prefabs/Characters/Factions/OPFOR/RHS_A… |  | `m_sCrewPrefab` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_faction` | (vide) |  | `m_sFaction` | AP4 | SRP_CmdArmorManager |
| `appui_blinde_depart_min_m` | 1500 | 200 à 10000 | `m_iSpawnMinM` | AP5 | SRP_CmdArmorManager |
| `appui_blinde_depart_max_m` | 2500 | 200 à 10000 | `m_iSpawnMaxM` | AP5 | SRP_CmdArmorManager |
| `appui_blinde_halte_m` | 350 | 0 à 3000 | `m_iHaltM` | AP5 | SRP_CmdArmorManager |
| `appui_blinde_duree_max_min` | 25 | 1 à 240 | `m_iEngageMaxMin` | AP5 | SRP_CmdArmorManager |
| `appui_blinde_fin_sans_joueur_m` | 800 | 100 à 5000 | `m_iNoPlayerM` | AP5 | SRP_CmdArmorManager |
| `appui_blinde_fin_sans_joueur_min` | 5 | 0 à 120 | `m_iNoPlayerMin` | AP5 | SRP_CmdArmorManager |
| `appui_blinde_bloque_s` | 180 | 10 à 1800 | `m_iStuckS` | AP5 | SRP_CmdArmorManager |
| `appui_blinde_retrait_m` | 1200 | 100 à 10000 | `m_iRetireM` | AP5 | SRP_CmdArmorManager |
| `appui_blinde_base_rayon_m` | 300 | 50 à 2000 | `m_iBaseRadiusM` | C3 | SRP_CmdArmorManager |
| `appui_blinde_vide_s` | 10 | 0 à 600 | `m_iEmptyS` | C3 | SRP_CmdArmorManager |
| `vu_journal_memoire` | 200 | 10 à 2000 | `s_iLogMemory` | VU4 | SRP_CmdScreens |
| `vu_journal_par_page` | 15 | 5 à 50 | `s_iLogPerPage` | VU4 | SRP_CmdScreens |
| `vu_refus_menu` | 1 |  | `s_bRefusalsInMenu` | VU4 | SRP_CmdScreens |
| `vu_refus_discord` | 0 |  | `s_bRefusalsToDiscord` | VU4 | SRP_CmdScreens |
| `vu_refus_repos_min` | 10 | 0 à 240 | `s_iRefusalRepeatMin` | VU4 | SRP_CmdScreens |
| `vu_renseignement_heures` | 0 | 0 à 720 | `s_iIntelHours` | VU2 | SRP_CmdScreens |
| `vu_renseignement_missions` | toutes |  | `s_sIntelMissions` | VU2 | SRP_CmdScreens |
| `vu_depot_missions` | ECOUTE, DOCUMENTS |  | `s_sDepotMissions` | RE9 | SRP_CmdScreens |
| `vu_troupes_renforcees` | 110 | 0 à 500 | `s_iTroopsStrongCent` | VU2 | SRP_CmdScreens |
| `vu_troupes_affaiblies` | 70 | 0 à 500 | `s_iTroopsWeakCent` | VU2 | SRP_CmdScreens |
| `vu_radio_silence` | 1 |  | `s_bRadioSilence` | MO9 | SRP_CmdScreens |
| `vu_officier_discord` | 1 |  | `s_bOfficerDiscord` | VU3 | SRP_CmdScreens |
| `vu_depot_icone` | 31 | 0 à 200 | `s_iDepotIcon` | RE9 | SRP_CmdScreens |
| `vu_depot_couleur` | 4 | 0 à 50 | `s_iDepotColor` | RE9 | SRP_CmdScreens |
| `vu_proximite_m` | 3000 | 100 à 20000 | `s_iNearM` | VU5 | SRP_CmdScreens |

## 8. Méthodes attendues dans les fichiers EXISTANTS (codées par leurs propriétaires)

Signatures exactes. « (neuf) » = à ajouter ; « (retouché) » = existe, change de sens, de visibilité ou de corps.
Tout ce qui est déjà au §8 de CONTRATS_FRONT reste valable ; ne sont listés ici que les besoins du Commandeur.

**SRP_FrontEnemy.c — `SRP_FrontEnemyComponent`, hôte du Commandeur (propriétaire : ennemi du front)**
- `OnPostInit` (serveur, pas dans l'éditeur : `!SCR_Global.IsEditMode(owner)`) : `m_Commander = new SRP_Commander(this);`
  AVANT `DeclareSettings()`.
- `OnDelete` : d'abord `SRP_FrontComponent front = SRP_FrontComponent.GetInstance(); if (front)
  front.SaveFinalFromEnemy();`, puis `Remove(Tick)`, désabonnement, `if (m_Commander) m_Commander.Stop();`, enfin
  `s_Instance = null` (l'ordre des OnDelete n'est pas garanti : sans cela, front.json pourrait être réécrit sans les
  clés fe_* et cmd_*).
- `OnControllableDestroyed` : après super, `if (m_Commander) m_Commander.OnControllableDestroyed(instigatorContextData);`.
- `Tick` : front prêt et `m_Commander && !m_Commander.IsStarted()` -> `m_Commander.Start();` ; après AttackTick :
  `if (m_Commander && m_Commander.IsStarted()) m_Commander.Tick(nowUnix);` ; à chaque passage :
  `SRP_CmdCapacity.Get().SetWantedGarrisons(keys, sizes, near, attacked)`.
- `bool IsCommanderActive()` : `return m_Commander && m_Commander.IsStarted() && !m_Commander.IsFrozen();` — SEULE
  définition de « Commandeur actif » (SRP_CmdManeuvers.IsOn est supprimé) ; dans un contexte statique :
  `SRP_FrontEnemyComponent fe = SRP_FrontEnemyComponent.GetInstance(); if (fe && fe.IsCommanderActive())`.
- `LoadSettings` : après ses clés, `if (m_Commander) m_Commander.LoadSettings();`.
- Relais (ne font rien sans Commandeur) : `OnZoneCaptured` -> `m_Commander.OnZoneCaptured(zone, staff)` ; `OnZoneLost`
  -> `OnZoneLost(zone, reason, staff)` ; `OnLocalityTaken` -> `OnLocalityTaken(locality)` ; `OnKeyPointSeized` ->
  `SRP_CmdIntel.OnKeyPointSeized(zone)` ; `OnMissionSucceeded` -> `m_Commander.OnMissionSucceeded(missionType, site,
  missionId, targetPrefab, author)` ; `OnSabotageUsed` -> `return m_Commander.OnSabotageUsed(target, playerId, reply);` ;
  `OnThreatChanged` -> `OnThreatChanged(threat)` ; `ResetAll` -> `ResetCampaign(reason)` ; `OnRestored` ->
  `OnRestored()` ; `WriteTo` / `ReadFrom` -> `m_Commander.WriteTo(ctx)` / `ReadFrom(ctx)`.
- RPC : `RpcDo_SRPCmdSound` -> `SRP_CmdSupport.PlaySoundLocal(acp, eventName, pos);` ; `RpcDo_SRPCmdLaunch` ->
  `SRP_CmdSupport.LaunchLocal(shell, dir);`.
- Crochets de contre-attaque : TOUS gardés par `IsCommanderActive()` (sinon règle votée) : `AttackClock` ->
  `CaWindowDecision` (chaque minute de la fenêtre) ; `PickTargetZone` -> `PickByIntel` (hors les 70 sur 100) ;
  `LaunchAttack` -> `PlannedWaves`, `attack.m_iRegion = SRP_Commander.Get().GetRegionOfZone(zone)`,
  `attack.m_bDisorganized` ; `SendWave` -> `ReviseWaves` (2e vague), `WaveRoomGroups`, `WantPaperWave` /
  `AddPaperWave`, `OnWavePosted` ; `AttackTick` -> `Support` (pendant l'annonce), `DriveAssault` (au lieu
  d'AdvanceAssault), `WaveArrived`, `CheckEnd` (DEFENDUE aux deux tiers, C8) ; `EndAttack` ->
  `SRP_CmdManeuvers.Get().EndAttack(attack, result)` (PERDUE vient d'OnZoneLost seul, qui appelle EndAttack ; seul
  endroit d'AdoptAssailants) ; `RunOffensive` -> `NightChance(chance, cancel)` (plafond 60
  appliqué par le front après). Vagues posées : `SRP_CmdCapacity.Get().Ask("vague-" + code, SRP_ECmdCapClass.COMBAT, …)`
  puis `SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.COMBAT)`.
- `PostsTick` : `Ask("poste-" + carré, SRP_ECmdCapClass.POSTE, soldats, 1, point, "territoire")`, puis
  `Tag(record, SRP_ECmdCapClass.POSTE)` et `record.m_sHomeTask = "poste front"` ; carrés sur le chemin des joueurs
  d'abord (`SRP_CmdManeuvers.Get().IsOnPlayersPath(cell)` si `IsCommanderActive()`).
- `GetLocalityNominal` / `GetLocalityEffective` : plafond `SRP_CmdCapacity.Get().GetGarrisonMax()` (60).

**SRP_Front.c — `SRP_FrontComponent` (socle)**
- `Start`, après la géométrie et AVANT `Load` : `SRP_Commander commander = SRP_Commander.Get(); if (commander)
  commander.BuildRegions();`.
- `ReloadSettings(author)` : `SRP_CmdSettings.Reload(author)` puis les `LoadSettings` (déjà au contrat du front).

**SRP_Enemy.c — `SRP_EnemyGroup` et `SRP_EnemyComponent` (propriétaire : IA ennemie)**
- `SRP_EnemyGroup` (neuf, en plus de `int m_iAssaultZone = -1;` du front) :
  `int m_iCapClass = -1;` (SRP_ECmdCapClass posée par `SRP_CmdCapacity.Tag`, -1 = devinée) ;
  `int m_iCmdRole;` (SRP_ECmdRole, AUCUN = 0) ; `string m_sSourceLocality;` ; `bool m_bAwake;` ; `vector m_vCmdLast;` ;
  `int m_iCmdStillUnix;` ; tampon des comptes rendus (OF3) : `bool m_bCmdContact;` `bool m_bCmdSeen;`
  `ref array<int> m_aCmdPlayers = {};` `vector m_vCmdPos;` `int m_iCmdShots;` `bool m_bCmdVehicle;` `bool m_bCmdArmed;`
  `int m_iCmdNextReport;`.
- (neuf) `SRP_EnemyGroup SpawnVehicleCrew(ResourceName crewPrefab, IEntity vehicle, int seats, string owner)` —
  équipage assis (modèle SpawnHeliPilots), `SetForceStayInVehicle`, null si impossible.
- (neuf) `int CountPendingSpawn()` — soldats encore attendus par la file d'apparition (m_iExpected - vivants, groupes
  non fermés).
- (neuf) `int ReleaseForRoom(int soldiersWanted, float patrolMinDist, float jeepMinDist, int maxGroups)` — RE12 : retire
  hors de vue des patrouilles de fond et des jeeps loin des joueurs ; rend les soldats libérés.
- (neuf) `void SetMaxGroups(int maxGroups, int missionReserve)` — plafond d'éléments (36) et réserve des missions (6).
- (neufs) ordres : `void OrderSuppress(SRP_EnemyGroup record, vector target, float holdSeconds, float height)` ;
  `void OrderAttackPoint(SRP_EnemyGroup record, vector target)` ; `void OrderSmokeCover(SRP_EnemyGroup record, vector
  protect, int grenades)` ; `void OrderForcedMove(SRP_EnemyGroup record, vector target, float radius)` ;
  `void OrderRetreat(SRP_EnemyGroup record, vector smokeAt, int grenades, vector exit, vector destination)` ;
  `void OrderApproach(SRP_EnemyGroup record, vector via, vector target)` ; `static void HoldAwake(SRP_EnemyGroup
  record)` ; `static void ReleaseAwake(SRP_EnemyGroup record)` ; `static int FitOf(SRP_EnemyGroup record)`.
- (retouché) fin de `SpawnGroup`, `SpawnCharacterGroup`, `SpawnVehicleDriver`, `SpawnHeliCrew`, `SpawnVehicleCrew` :
  `SRP_CmdCapacity.TagImportance(record);` (classe devinée ; le poseur appelle ensuite `SRP_CmdCapacity.Tag(record,
  classe)`). Plus AUCUN autre `SetImportance` dans le mod (trou 18).
- (retouché) `int GetMaxSoldiers()` rend `SRP_CmdCapacity.Get().GetMaxSoldiers()` et `int CountBudgetSoldiers()` rend
  `SRP_CmdCapacity.Get().CountGround()` : ils ne servent PLUS qu'aux TEXTES (Staff, journal), jamais à refuser une
  pose (c'est `Ask` qui décide) ; `m_bBudget` n'est plus lu (champ gardé, sans effet).
- (retouché) menace LOCALE pour les moyens décidés par le Commandeur (CO2, CO9 : « les patrouilles et jeeps suivent la
  menace locale ») : `protected float ThreatFactor(vector position)` = `1 + SRP_Commander.ThreatAt(position,
  SRP_FrontComponent.GetInstance().GetThreat()) / 10.0` (sans Commandeur : menace de l'île, comme aujourd'hui) ;
  `protected bool RollThreat(int chance, vector position)` ; appelants : `SpawnWave` (camion de vague, position =
  target, SRP_Enemy.c:2067), `MaybeSpawnVehiclePatrol` (jeep, center, l.2404), patrouilles (l.3950 : facteur au
  joueur tiré). Remplace `ThreatFactor()` et `RollThreat(int)` (SRP_Enemy.c:964-978, menace de l'île).
- (retouché) `RetireTick` : SEULE porte de retour des envoyés vivants (groupe à `m_sSourceLocality` non vide,
  `SRP_CmdManeuvers.Get().m_bReturnSurvivors`, vrai sans Commandeur) -> `SRP_FrontEnemyComponent.ReturnSent(source,
  vivants)`.
- (retouché) `AlertTick`, pour chaque joueur perçu, juste après `record.m_iSpotTick = now;` :
  `SRP_Commander.NoteSighting(record, target.m_Entity, target.m_vWorldPos, identified);`.
- (retouché) `ReportSpotted(vector position)` : en tête `SRP_Commander.NoteAirSighting(position);`.
- (retouché) `MaybeCallHeli` : en tête `if (SRP_Commander.Get()) return false;` (l'hélico ne part plus que par l'ordre
  HELICO du cerveau ; Commandeur gelé : pas d'hélico, trou 11).
- (retouché) `Delete(SRP_EnemyGroup record)` et `Prune()` (groupe retiré de la liste) :
  `SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get(); if (maneuvers) maneuvers.OnGroupGone(record);`.
- (retouché) `MaybeSpawnVehiclePatrol`, `SpawnPatrolNear`, `SpawnRoadPatrol` : `Ask(…, SRP_ECmdCapClass.PATROUILLE, …)`
  avant, `Tag(record, SRP_ECmdCapClass.PATROUILLE)` après ; `SpawnWave`, `SpawnWaveFrom` : l'appelant tague COMBAT.

**SRP_EnemySenses.c** — dans le `modded SCR_AIDangerReaction_WeaponFired` (l.870-889), après
`record.m_mShotHeard.Set(playerId, perception.GetTime());` : `SRP_Commander.NoteShot(record);`.

**SRP_EnemyAwareness.c**
- (retouché) `protected static bool IsFree(…)` : en tête `if (record.m_iCmdRole != SRP_ECmdRole.AUCUN ||
  record.m_iAssaultZone >= 0) return false;` (avec le test « poste front » du front).
- (retouché) `static bool FlankPoint(vector helper, vector source, bool hasSource, vector contact, out vector flank)` —
  devient public (retirer `protected`).

**SRP_EnemyGarrison.c**
- (neuf) `static SRP_EnemyGroup FindTownHQ(SRP_EnemyComponent enemies, SRP_SectorState state)` ; (neuf) `static bool
  IsCentreGroup(SRP_EnemyGroup record)`.
- (supprimé) `static int SoldierCap(SRP_EnemyComponent enemies, SRP_SectorState state)` (l.477) et son appel (l.335) :
  l'ancien plafond (réserve des camions retirée de GetMaxSoldiers) doublait `Ask` / `GarrisonCap` et
  `cap_reserve_renforts` ; la place ne se décide plus que par `GarrisonCap` puis `Ask`. `record.m_bBudget = true`
  (l.411) n'a plus d'effet.
- (retouché) pose : effectif = `Math.Min(SRP_FrontEnemyComponent.GetLocalityEffective(nom),
  SRP_CmdCapacity.Get().GarrisonCap(nom, décidé))` ; `Ask("garnison-" + nom, classe, soldats, groupes, centre,
  "territoire")` avec classe = COMBAT si `IsLocalityUnderAttack(nom)`, sinon GARNISON ; `Tag` de chaque groupe ; rien si
  `SRP_CmdManeuvers.Get().IsLocalityEmptied(nom)` (MA7) ; complément au plein si `IsKeptFull(nom)` (MA11).

**SRP_EnemyTrucks.c — `SRP_EnemyTruckComponent` et `SRP_EnemyTruck`**
- `SRP_EnemyTruck` (neuf) : `string m_sSource;` `int m_iTargetZone = -1;` `int m_iColumnId;` `int m_iSentId;`
  (numéro rendu par `OnTruckLaunched`, 0 pour un camion de colonne).
- (neuf) `bool DispatchFrom(SRP_SectorState state, string source, vector sourcePos, int load, int arriveSeconds, string
  reason)` ; (neuf) `bool SendContactTruck(vector contact, string source, vector sourcePos, int load, int arriveSeconds,
  int zone)` ; (neuf) `int LaunchColumn(SRP_CmdColumn column, vector at, array<vector> remaining, int load)`.
- (retouché, rendus publics) `static void PreventLod(IEntity member)`, `static void AllowLod(IEntity member)`,
  `int LoadFor(int players)`, `int CapLoad(int load)`, `int CountLaunched()`.
- (supprimé) `int GetSoldierReserve(SRP_SectorState state, int players)` (l.322) ; dans le refus de pose des camions
  (l.1365-1381), retirer les tests « plafond de soldats des garnisons et camions » et « IA actives du jeu … marge »
  (`m_iAIHeadroom`, texte « limite du jeu (128) ») : seul `Ask("camion-…", SRP_ECmdCapClass.COMBAT, …)` décide (la
  marge du jeu est déjà dans `SRP_CmdCapacity.EngineRoom`, `cap_marge_jeu`) ; `cargo.m_bBudget` (l.1433) n'a plus
  d'effet.
- (retouché) pose d'un camion : `SRP_CmdCapacity.Get().Ask("camion-" + cible, SRP_ECmdCapClass.COMBAT, charge + 1, 2,
  point, propriétaire)` ; chauffeur et passagers `Tag(COMBAT)` ; passagers qui rejoignent une garnison `Tag(GARNISON)` ;
  `truck.m_iSentId = SRP_CmdManeuvers.Get().OnTruckLaunched(m_sSource, charge)` SEULEMENT si la source est connue ET
  `m_iColumnId == 0` (une colonne est débitée à son départ par `NewColumn` : jamais deux débits) ; passagers débarqués,
  camion détruit ou retiré -> `OnTruckUnloaded(truck.m_iSentId)` ; passagers débarqués : `record.m_sSourceLocality =
  m_sSource` (retour par RetireTick) ; livraison d'une colonne -> `OnColumnDelivered(m_iColumnId, survivants,
  rejoint)`.

**SRP_Territory.c — `SRP_TerritoryComponent` (localités)**
- (retouché, public) `int GarrisonSoldiers(SRP_EnemyComponent enemies, SRP_SectorDef sector)` : menace LOCALE
  `SRP_Commander.ThreatAt(sector.GetCenter(), GetThreat())` au lieu de `m_iThreat`, total x
  `SRP_Commander.GarrisonFactorAt(sector.GetCenter())`, plafond `SRP_CmdCapacity.Get().GetGarrisonMax()`.
- (retouché) `bool TryRadioCall(vector position, int now)` : `minutes = Math.ClampInt(SRP_Commander.RadioDelayMinutes(
  m_iRadioDelayMinutes), 0, 120);`.
- (retouché) `GarrisonTrucksTick` : si `SRP_FrontEnemyComponent.GetInstance().IsCommanderActive()` (tester le null),
  le camion d'entrée passe par
  `SRP_CmdManeuvers.Get().RequestEntryTruck(state, entryPlayers, System.GetUnixTime())` (même frein C2) et le camion
  d'alerte n'est plus envoyé (le camion de contact du cerveau le remplace) ; sinon, règles d'aujourd'hui.

**SRP_Missions.c — `SRP_MissionManagerComponent`**
- (neuf) `array<ResourceName> GetOfficerPrefabs()` (rend `m_aOfficerPrefabs`) ; (neuf) `bool IsOfficerTargeted(string
  regionCode)` (une mission OFFICIER active vise cette région).
- `SRP_Mission` (neuf) : `string m_sOfficerRegion;` `int m_iOfficerSince;`.
- (retouché) mission OFFICIER (OF10) : si `SRP_Commander.HasOfficerTarget()` : `SRP_Commander.Get().GetBook()
  .PickOfficerTarget(près, région, site, libellé)` ; site = cachette du jour ; la mission ne pose PAS d'officier (le
  registre des régions le pose) ; fin par `GetOfficerMissionStatus(région, m_iOfficerSince)` (TOMBE, CAPTURE =
  réussite ; ECHAPPE = échec). Sinon, la mission d'aujourd'hui.
- (retouché) gardes de mission, escorte, officier de mission (ancien mode) : `Ask("mission-" + id,
  SRP_ECmdCapClass.GARDE, …, "mission")` puis `Tag(GARDE)` ; vagues (`SpawnWave`) : `Ask("vague-" + id, COMBAT, …)` puis
  `Tag(COMBAT)`.
- (déjà au contrat du front) `End(SUCCES)` -> `OnMissionSucceeded` ; `OnObjectiveUsed` -> `OnSabotageUsed` en tête
  (SEULE entrée ; `SRP_SabotageAction` inchangé) ; Q9 : `CaptureDifficulty(zone)` et `OnZoneCaptured(code, staff)`
  (staff -> mission ANNULEE, sans prime ni G2), voir §8 de CONTRATS_FRONT.

**SRP_Civilians.c — `SRP_CivilianManagerComponent`**
- (neuf) `void GetInformantCandidates(notnull array<IEntity> characters)` — civils à pied vivants (promeneurs, postés ;
  pas les conducteurs), sans filtre : le cerveau filtre (carré rouge, région ACTIVE, repos, vue).
- (retouché) avant une pose : `SRP_CmdCapacity.Get().CivilianRefusal(nombre)` (rien si refus) ; après :
  `SRP_CmdCapacity.TagCivilianGroup(groupe)`.

**SRP_Delivery.c** — garnison de livraison : `Ask("livraison-" + id, SRP_ECmdCapClass.GARDE, …, "livraison")` puis
`Tag(GARDE)`.

**SRP_HeliSearch.c** — après `SpawnHeliCrew` : `SRP_CmdCapacity.Tag(crew, SRP_ECmdCapClass.HORS_COMPTE);`.
`static string Launch(vector center, string author)` et `static int Count()` inchangés (appelés par
`SRP_CmdSupport.RequestHeli`).

**SRP_Treasury.c** — rien de neuf : `void Add(int amount, string reason)` (l.214) sert à la prime C3.
`IsEnemyFactionKey` n'est PAS ajouté (trou 22 : une victime ennemie est reconnue par son groupe enregistré).

**SRP_FrontRadio.c — `SRP_FrontRadio` et `SRP_FrontMarkers` (propriétaire : carte)**
- Les 6 annonces du Commandeur (§6) : libellé par `SRP_Commander.Get().GetRegionLabel(region)` (rien si le Commandeur
  est absent) ; `IntelGained` : `source` est un `SRP_ECmdIntelSource` ; `SupportSilenced` seulement si
  `SRP_CmdScreens.s_bRadioSilence` ; `OfficerFell` sans évènement si `!SRP_CmdScreens.s_bOfficerDiscord`.
- `TickEnemyDepots` : `SRP_CmdResources resources = SRP_CmdResources.Get();` puis
  `resources.GetDepots().GetRevealedDepots(regions, positions)` ; icône `SRP_CmdScreens.s_iDepotIcon`, couleur
  `SRP_CmdScreens.s_iDepotColor`, texte « Dépôt ennemi — » + libellé de région.

**SRP_FrontScreens.c** — `PCTerritoire` : `SRP_CmdScreens.PCZoneSuffix(zone)` sur chaque zone ENNEMIE et
`SRP_CmdScreens.PCIntelSection(playerId)` ; `StaffTerritoire` : `SRP_CmdScreens.StaffTerritoryLine()` sous le gel.

**SRP_Admin.c — `SRP_AdminComponent` (propriétaire : carte)**
- 8e onglet « commandeur » : page `SRP_CmdScreens.StaffTab(playerId, staffPos, inGame, parts)` ; actions « commandeur:* »
  : `SRP_CmdScreens.IsInternalPage(action)`, `NeedsConfirm(action)`, `RunStaff(playerId, staffPos, inGame, action,
  parts, author, wantTeleport, teleportTo)` (téléporte si `wantTeleport`), `PageAfter(action, parts)`.
- En-tête : `SRP_FrontScreens.StaffHeaderPart() + SRP_CmdScreens.StaffHeaderPart()`.
- Page « rapide:ia-groupes » : `SRP_CmdScreens.StaffNearReport(position, SRP_CmdScreens.s_iNearM)`.
- ia-reset et wipe : `SRP_Commander commander = SRP_Commander.Get(); if (commander) commander.CancelOperations(author);`.

**SRP_Discord.c** — catégorie COMMANDEUR -> webhook « etat-major » de discord.json (secours : journal-serveur),
jamais un salon public ; couleurs « officier ennemi », « renseignement », « dépôt ennemi », « blindé ennemi ».
**SRP_JournalComponent.c** — la catégorie COMMANDEUR n'entre pas dans `s_aRecent` (onglet Journal du PC).
**SRP_Bridge.c** — `PushZoneEvent` accepte `officier_tombe`, `region_liberee`, `renseignement`, `depot_ennemi`,
`blinde_ramene` (file `m_iMaxEvents` portée à 40).
**Hors mod** — bot.py : styles de ces 5 évènements (ALERT_STYLES) ET de `offensive` (H2, émis par le front :
`"offensive": ("🏳️", RED, None)`, oublié jusqu'ici, il tombait sur le style « info »), traitement de tous les évènements d'un envoi par
paquets de 10 ; setup_discord.py : salon « état-major-ennemi » (Staff seul, catégorie STAFF).

**SRP_CmdArmor.c — à AJOUTER par le codeur des blindés (étape 11), texte exact** (override vérifié :
`SCR_GetInUserAction.CanBePerformedScript`, vanilla Game/UserActions/SCR_GetInUserAction.c:55 ; `GetMainParent`,
SCR_EntityHelper.c:325 ; `SetCannotPerformReason`, BaseUserAction.c:21 ; aucun champ ajouté) :

```c
//------------------------------------------------------------------------------------------------
//! AP1 : verrou d'entrée de la batterie 2S1 du Commandeur (toutes machines ; aucun champ ajouté)
modded class SCR_GetInUserAction
{
	override bool CanBePerformedScript(IEntity user)
	{
		IEntity root = SCR_EntityHelper.GetMainParent(GetOwner(), true);
		if (SRP_CmdSupport.IsBatteryVehicle(root))
		{
			SetCannotPerformReason("Pièce verrouillée");
			return false;
		}
		return super.CanBePerformedScript(user);
	}
}
```

## 9. Prefabs et fichiers du profil

- **Aucun composant neuf, aucun GUID de composant** : rien à ajouter à `SRP_GameMode.et` pour le Commandeur.
- `Prefabs/Missions/SRP_Piece_Mortier.et` (+ `.meta`) : NEUF, enfant de `SRP_Cible_Mortier.et` (modèle 2B14, action
  « Saboter »). Le GUID `{6A5B0C0D0E0FC001}` de la clé `appui_mortier_piece` est écrit à la main dans le plan : à
  remplacer par le GUID donné par le Workbench (ou corriger la clé dans `commandeur_reglages.txt`).
- Profil, écrits par le serveur (rien à livrer) : `commandeur_reglages.txt` et `front_reglages.txt` (SRP_CmdSettings,
  au premier démarrage, complétés d'eux-mêmes) ; `commandeur_regions.txt` (rapport du tracé, réécrit à chaque
  démarrage) ; `front_retouches.txt` accepte « region S12 -> R3 » (lu par `SRP_CmdRegionBook`, ignoré par le socle) ;
  `discord.json` : clé « etat-major ».
- Prefabs par défaut dans les réglages (vérifiés dans le plan des appuis, modifiables sans republier) : obus 82 mm
  explosif et fumigène, groupe des servants, 2S1, UMPK500, Su-57, BTR-70, BRDM-2, K4386 armé, équipage RHS, décor du
  dépôt `SRP_Cache.et` (déjà dans SRP_GameMode.et).

## 10. Points délicats

1. **Compilation d'ensemble** : les 10 fichiers ne dépendent que du jeu, du code existant et des squelettes du front
   (`SRP_FrontEnemyComponent.m_iAttackMaxWaves`, `SRP_CounterAttack.m_iPlannedWaves`, `SRP_EAttackEnd`,
   `SRP_SectorState`). Aucune méthode NEUVE d'un fichier existant n'est appelée par ces squelettes : les appels du §8
   s'ajoutent en même temps que les corps.
2. **`SRP_CmdSettings` est vivant dès la compilation** : au premier démarrage, `commandeur_reglages.txt` est écrit avec
   386 clés groupées par section, et `front_reglages.txt` avec les clés que le front déclare. Ses problèmes partent à la
   console et dans `SRP_CmdLog.Note` (qui reste muet tant que le corps de `SRP_CmdLog` n'est pas codé).
3. **`SRP_Commander : Managed`** : ne pas le tenir par une référence forte ailleurs que `m_Commander` ; les
   sous-modules gardent un pointeur simple vers lui. Toujours tester `SRP_Commander.Get()`, `SRP_CmdResources.Get()`,
   `SRP_CmdManeuvers.Get()`, `SRP_CmdSupport.Get()` (null sans Commandeur, sur un client, ou avant OnPostInit).
   La capacité, elle, existe toujours (`SRP_CmdCapacity.Get()` la crée) ; mais c'est `SRP_Commander.Start` qui pose la
   limite de 160 IA et le plafond de groupes : sans composant ennemi du front, ils ne sont pas posés.
4. **Instance jetable du Workbench** : le Commandeur ne doit être créé ni dans l'éditeur ni sur un client ; son
   constructeur ne fait que déclarer ; tout le reste attend `Start` (front prêt).
5. **Gel VU6 ≠ absence** : gelé, le Commandeur continue de tenir la capacité, les comptes rendus, les officiers et les
   stocks ; il ne décide plus (cycles), ne tire plus, et les crochets de contre-attaque rendent la main au front
   (`IsCommanderActive()` faux). Sans Commandeur, `MaybeCallHeli` et les camions d'aujourd'hui reprennent ; avec un Commandeur,
   même gelé, plus d'hélico par tirage (trou 11).
6. **RE13, une seule restitution** : tickets (ressources), colonnes et camions hors colonne en route (manœuvres) sont
   rendus dans `ReadFrom` ;
   `RestoreDaily` refait `ReadFrom(copie)` puis `OnRestored` : la copie peut contenir des tickets, ils sont rendus eux
   aussi (voulu : l'ennemi « rentre au stock »). Aucun autre registre « en route » (les appuis n'en ont pas).
7. **Valeurs proposées sans chiffre de Jack** : `cmd_grav_contre_attaque` = 2 (échelle du barème du cerveau),
   `vu_refus_repos_min` = 10, `renfort_chargement_max` = 14, `appui_securite_salve_m` = 220 ; tout est réglable.
8. **Ordre (kind, region)** des stocks : deux entiers, une inversion compile et lit le mauvais stock (trou 4) ; relire
   chaque appel à `GetStock`, `GetFull`, `CanReserve`, `Reserve`, `StaffSetStock`.
9. **Capacité** : `Ask` est la porte recommandée (elle note la demande pour la file RE12) ; `Refusal` seul ne fait pas la
   queue. Une pose oubliée garde sa classe devinée (`ClassOf`) et son importance (`TagImportance` dans les Spawn* de
   `SRP_EnemyComponent`), mais échappe au contrôle de place : relire chaque pose du §8.
10. **Textes** : aucun signe pour cent, `string.Format` à 9 paramètres au plus, jamais le vrai prénom de Jack (« Jack » partout), jamais les
    mots confidentiels dans un texte public ; le journal COMMANDEUR n'est jamais public.
11. **Consignes `par_fichier` déclarées CADUQUES par la relecture du 26/09** (ce document et CONTRATS_FRONT priment) :
    `SRP_Missions.c.md` l.82-84 (prime payée même au Staff, `authorised = staff || mission`, PushEvent direct) ;
    `SRP_Missions.c.md` l.101-103 (appel direct `SRP_CmdSupport.GetInstance().OnSabotage` dans `SRP_SabotageAction`) ;
    `SRP_Territory.c.md` l.30-32 (pose `GarrisonSoldiers − CurrentLosses`) ; `SRP_Bridge.c.md` l.72 (`IsAtFront`, devenu
    `IsNearFront`) ; toute mention de `SRP_CmdManeuvers.IsOn`, `ForceNextHeli`, `RequestTransfer`, `ca_vagues`,
    `res_heures_bombe`, `SoldierCap`, `GetSoldierReserve`, `SRP_FrontEnemyComponent.GetAttackState`.
12. **RE13, restitutions (rappel complet)** : tickets ouverts (ressources), colonnes en route papier ou réelles
    (`cmd_ma_c*`) et camions hors colonne pas encore déchargés (`cmd_ma_t*`), chacun UNE fois dans `ReadFrom`. Les
    débarqués au combat au moment de l'arrêt ne sont pas rendus (« les combats en cours sont annulés ») : ils restent
    des envoyés qui se regarnissent.
