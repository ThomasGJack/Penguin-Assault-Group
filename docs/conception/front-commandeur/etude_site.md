# Rapport : comment le territoire est exporté et affiché hors du jeu (pont, bot, site, Discord)

Je n'ai rien modifié. Je n'ai lu ni `config.json` ni `discord.json`. Un écart à signaler : une de mes commandes contenait un `cd` vers le dossier Scripts du mod. Elle n'a fait que lire, mais c'est contraire à la consigne ; ensuite je n'ai utilisé que des chemins absolus.

Le dossier `donnees_web/` n'existe que sur le VPS : il n'y en a aucune copie locale. Les formats décrits au point 3 viennent donc du code, pas des fichiers réels.

## 1. Ce que le pont envoie (`SRP_Bridge.c`)

**Fréquence**
- `m_iSyncSeconds` vaut 5 s par défaut et 5 s au minimum (l.64-65 et 120-124). L'envoi se fait par `CallLater(Sync, …, true)`.
- Une seule requête est en vol à la fois (`Sync()`, l.270-276). Le délai d'attente est de 10 s (l.118).
- Si le bot a renvoyé des commandes, un nouvel envoi part 1,5 s plus tard (l.629-630).

**Corps du message** : un seul `POST api/sync`, construit à la main par `BuildState()` (l.291+). Il contient, dans l'ordre :
- `secret`, `time`, `caps` (l.295) ;
- `players[]` : chacun a un champ `grid` ;
- `missions[]` : chacune a `grid` et `claimed` ;
- `territory` ;
- `sectors[]` ;
- `hour` ;
- une fois sur douze seulement, soit environ toutes les 60 s : `mission_types` et `places[]` avec `{name, x, z}` en mètres arrondis (compteur `m_iMapPlacesCountdown`, l.67 et 402-405) ;
- `euros`, `stocks`, `boxes[]`, `fleet[]` (chacun avec `grid`), `requests`, `events[]`, `results[]`, `links[]`, `votereq[]`.

**Champs du territoire**
- `territory` vaut `{"ours":N,"total":N,"threat":N}` (l.355-371). Attention : `ours` compte les secteurs dont le propriétaire est différent de `ENNEMI`.
- Chaque élément de `sectors[]` (l.373-395) est de la forme `{"name","grid","ours","attack"[, "depot":"gx gz"],"stock"}` :
  - `grid` est le carré du **centre** du secteur ;
  - `ours` vaut `owner == NOUS` ;
  - `attack` vaut `m_iAttackState != 0` (0 = rien, 1 = attaque annoncée, 2 = en cours).
- Le rayon du secteur n'est pas envoyé. Il vaut 150, 250 ou 350 m selon le type (`SRP_Territory.c` l.23 et 432-438).
- L'énumération n'a que deux états : `ENNEMI=0`, `NOUS=1` (`SRP_Territory.c` l.54-58).
- Il y a environ 100 secteurs : `m_iAutoMax 100` dans `Prefabs/MP/Modes/Plain/SRP_GameMode.et` l.184 (la valeur par défaut du code est 12, `SRP_Territory.c` l.444-445).

**Taille (estimation, non mesurée)**
- Un secteur fait environ 80 octets, 100 avec un dépôt. Les 100 secteurs font donc environ 8 à 10 Ko.
- Un joueur fait environ 300 octets.
- Au total, un envoi fait environ 15 à 30 Ko. Le bot accepte jusqu'à 2 Mo (`client_max_size`, `bot.py` l.1913).
- Le journal du bot ne consigne pas les requêtes, je n'ai donc pas de taille réelle.

**Convention de carré** (`SRP_Fleet.c` l.802-818, `GridOf`)
- `gx = floor(x/100)`, `gz = floor(z/100)`. Chaque valeur est écrite sur 3 chiffres (`Pad3`) et les deux sont séparées par une espace : `"074 043"`.
- Une valeur négative est ramenée à 0. Il n'y a pas de plafond à 127.
- x va vers l'est, z vers le nord. L'origine du monde est en bas à gauche (sud-ouest).
- Côté bot, `campagne._carre("074 043")` renvoie `(74, 43)` (`campagne.py` l.41-47).
- Côté web : `centre(g)` donne `[gz*100+50, gx*100+50]` et `coins(g)` le rectangle de 100 m (`carte/index.html` l.169-170). Leaflet utilise l'ordre `[lat=z, lng=x]`.

**Retour du bot et files d'attente**
- La réponse est en texte, une commande par ligne, au format `id\tcmd\targs` (l.607-624).
- Les `events` sont vidés dès qu'un envoi réussit (l.600). La file garde au plus 20 éléments et perd les plus anciens (`MAX_EVENTS`, l.88).
- Les `links` et `votereq` ne sont retirés qu'après accusé de réception (`m_iLinksSent`/`m_iVotesSent`, l.529-549 et 588-599). Ce mécanisme peut servir tel quel pour un envoi de deltas fiable.

## 2. Affichage sur le web

**Carte `/carte/`** (`carte/index.html`)
- **Repère** : `L.CRS.Simple` avec la transformation `(1/64, 0, -1/64, 256)` (l.131). Le monde fait 16 384 m, soit 1 m par pixel au zoom 6.
  - Tuiles satellite jusqu'au zoom natif 6 (l.133).
  - Photo aérienne à 0,25 m par pixel au zoom 8 (l.136).
  - Bornes `[[0,0],[12800,12800]]`.
- **Quadrillage** : lignes de 1 km toujours visibles, lignes de 100 m à partir du zoom 5, étiquettes `"074"` (l.139-156).
- **Secteurs** (l.203-216) :
  - un losange (`.drapeau`) posé sur le **carré central** : vert `#3ddc84` s'il est à nous, rouge `#ff4d4d` s'il est ennemi, orange clignotant en cas d'attaque ;
  - il n'y a **ni cercle, ni rayon, ni surface** ;
  - le dépôt est un petit carré doré, qui clignote quand du stock attend.
- **Liste latérale** : seulement les secteurs à nous ou attaqués (l.215). Le compteur affiche « N / total » (l.226).
- **Rafraîchissement** : `api/carte` toutes les 5 s (l.330-331). À chaque fois, toutes les couches sont effacées et redessinées (`clearLayers`).
- **Frise d'historique** (l.230-267) : lue sur `api/historique` toutes les 60 s. Pour revenir en arrière, elle part de l'état en direct et inverse les événements **par nom de secteur** (`voyage()`, l.233-239). Elle liste aussi les 12 derniers mouvements.
- **Trajets** (l.269-314) :
  - suite des carrés de 100 m parcourus par chaque soldat, un trait coupé après 10 min sans point ou un saut de plus de 1,5 km ;
  - lus sur `api/trajets?heures=` ou `?debut=&fin=` ;
  - image JPEG sur `api/trace.jpg`, fabriquée par `trace_image.py` (même repère : `pixel = (x*e, (16384-z)*e)`, l.67-68).
- **Côté bot** : `map_state()` (`bot.py` l.1830-1845) filtre `sectors` en `{nom, carre, nous, attaque, depot, stock}`, limité à 200 secteurs, et **seulement quand le serveur est en ligne**. Serveur hors ligne, la carte n'affiche aucun secteur.

**Accueil** (`site_web/index.html`)
- `/api/accueil` est lu toutes les 20 s (l.310-314).
- L'anneau en `conic-gradient` (l.72 et 288-289) et la jauge « secteurs tenus » (l.229 et 284-285) viennent de `territory.ours / total` (`bot.py` l.1892-1897).

**Campagne** (`site_web/campagne.html`, `/campagne/api`, `bot.py` l.1955-1970, lu toutes les 10 s)
- Bloc « Territoire » : N / total et pourcentage « de l'île » (l.99-101). Ce chiffre compte en réalité des secteurs, pas une surface.
- Bloc « Menace ennemie ».
- Bloc « **Derniers mouvements du front** » : les 8 derniers événements (l.77 et 116).
- Pastilles de noms des secteurs (l.83 et 119-120).

**Fiche soldat** : « Secteurs pris » (`soldat.html` l.62-98), calculé par `campagne.fiche_publique` (`campagne.py` l.243-251).

## 3. Stockage et annonces Discord

**`donnees_web/secteurs.json`** (géré par `campagne.py`)
- Structure : `{"etat": {nom: {"nous","carre","depuis"}}, "historique": [{"t","secteur","carre","nous","soldats":[{"code","nom"}]}]}`.
- Un événement n'est créé qu'au **changement de propriétaire d'un secteur désigné par son nom** (l.119-140).
- Les « présents » sont les soldats à 3 carrés (300 m) ou moins du centre (`AUTOUR`, l.31).
- L'historique garde 2 000 événements (`HISTORIQUE_MAX`, l.29) ; le public en voit 500 (l.207).
- Le fichier est écrit au plus une fois par minute (l.190). À côté : `soldats.json`, `missions.json`, `trajets/AAAA-MM-JJ.json` gardés 30 jours.

**Annonces**
- **Côté mod**, par `SRP_BridgeComponent.PushEvent` :
  - `secteur_pris` à la capture (`SRP_Territory.c` l.1728) et à la défense réussie (l.1914) ;
  - `secteur_perdu` (l.1786) ;
  - `attaque` (l.1829 et 2027) ;
  - `commandement` pour une capture sans mission (l.1717-1719).
- **Côté bot** : `ALERT_STYLES` (`bot.py` l.121-128) et `handle_events` (l.577-597) publient un embed dans `🚨┃alertes`, ou dans le salon commandement pour `commandement` et `controle`. Seuls les **10 premiers** événements d'un envoi sont traités (l.583).
- **En parallèle**, la même ligne de journal `TERRITOIRE` part sur le webhook « territoire » via `SRP_Discord.c` (l.323-326), en bleu, rouge ou orange selon des mots-clés (l.393-404). Une ligne qui contient un mot confidentiel ne part que dans journal-serveur (liste l.246-248).
  - Point constaté : la ligne de **perte** contient « groupe(s) » et « garnison ». Avec la liste par défaut, elle ne part donc pas dans le flux public, alors que l'événement du pont la publie en entier dans le salon des alertes.
- **Ailleurs dans le bot** :
  - présence du bot « X/Y secteurs » (l.423-424) ;
  - `status_embed` (l.819-823) ;
  - autocomplétion de `/ordonner` capture : la liste des secteurs non tenus (l.2512-2518) ;
  - image du tracé d'opération (l.758-774).

## 4. Ce qu'une carte de front par carrés changerait

**Volume selon la taille des carrés**
- Seules les tailles 200, 400 et 800 m divisent 12 800 m exactement.

| Taille | Cases | 1 bit par case | 2 bits par case | 1 caractère par case |
|---|---|---|---|---|
| 100 m | 128×128 = 16 384 | 2 048 octets, 4 096 caractères hex | 8 192 hex, 5 462 base64 | 16 Ko |
| 200 m | 64×64 = 4 096 | 1 024 hex | 2 048 hex | 4 096 caractères |
| 400 m | 32×32 = 1 024 | — | 512 hex | 1 024 caractères |
| 500 m | 26×26 = 676 (dernière rangée partielle) | — | — | — |
| 1 km | 13×13 = 169 (dernière rangée partielle) | — | — | — |

- Le mode 2 bits permet 4 états : bleu, rouge, neutre, contesté.
- Un codage par plages (RLE) par rangée descend probablement à 1-3 Ko, car les zones sont des blocs d'un seul tenant.
- Liste des seules cases bleues sous forme `"gx gz"` : environ 10 octets par case, donc 30 Ko pour 3 000 cases.
- Je n'ai pas vérifié si l'Enforce 1.8 a un encodeur base64. L'hexadécimal ou « 1 caractère par case » sont triviaux à écrire.
- Une taille multiple de 100 m garde la convention `"074 043"` : grand carré = `floor(gx/k)`.

**Côté mod**
- Ajouter par exemple :
  - `"front":{"v":version,"k":taille,"d":["gx gz état",…]}` pour les changements ;
  - un instantané complet une fois par minute (même principe que `places`, l.402-405), au retour du contact (`m_bOnline` qui repasse à vrai), ou sur une commande du bot.
- Ne pas reconstruire 16 384 concaténations toutes les 5 s : garder les rangées en cache et ne refaire que celles qui ont changé.
- Utiliser l'accusé de réception de type `m_iLinksSent` pour les deltas.
- Ajouter `"front"` dans `caps` (l.295), pour que le bot sache si le mod le gère.
- Si un 3e état (neutre ou contesté) apparaît, `territory.ours` (différent d'`ENNEMI`) et `sectors[].ours` (égal à `NOUS`) **ne donneront plus le même résultat**.

**Côté bot**
- Garder la grille en mémoire et la **persister**, par exemple dans `donnees_web/front.json`, pour l'afficher même serveur hors ligne (aujourd'hui `map_state` n'affiche rien hors ligne).
- Calculer en Python :
  - les zones : composantes connexes, soit un nom défini par zone, soit un regroupement automatique ;
  - la ligne de front : les arêtes entre une case bleue et une case rouge, fusionnées en polylignes.
  Sur 16 000 cases, ce calcul est négligeable.
- Servir le tout sur un point d'accès à part, par exemple `/carte/api/front`, avec un numéro de version. Il ne faut pas renvoyer la grille dans `api/carte` à chaque sondage de 5 s de chaque visiteur.

**Côté carte web**
- Ne pas tracer 16 384 `L.rectangle` en SVG. À la place :
  - un canvas de 128×128 transformé en `L.imageOverlay` sur `[[0,0],[12800,12800]]`, avec `image-rendering: pixelated`, bleu et rouge semi-transparents ;
  - la ligne de front en `L.polyline` ;
  - le contour et le nom des zones en `L.polygon` ou en étiquettes.
- Ne redessiner cette couche **que si la version change** (aujourd'hui tout est effacé toutes les 5 s).
- Masquer la mer avec un masque terre fixe, qu'on peut tirer de `carte/relief_everon.png` (7,3 Mo, présent localement).

**Historique**
- Le format actuel, indexé par nom de secteur, ne convient pas aux cases : 2 000 événements par case seraient consommés en une soirée.
- Deux pistes :
  - des événements par zone : « zone X passée bleue, +N cases », avec les soldats présents ;
  - des instantanés datés de la grille (environ 2 Ko en 1 bit, donc environ 60 Ko pour un par jour sur 30 jours), la frise passant alors d'un instantané à l'autre au lieu d'inverser des événements.
- La liste « Secteurs pris » des fiches soldats (`fiche_publique`) devra suivre le même choix.

**Annonces Discord**
- Il faut regrouper par zone ou par « poussée du front ». Sinon la file de 20 événements du mod et la limite de 10 par envoi du bot perdent des messages.
- Les textes de l'embed d'état, de la présence du bot, de l'accueil et de la campagne passent de « X/Y secteurs » à « % de l'île » ou « zones tenues ».

**Réutilisable tel quel**
- La convention de carré et `GridOf`, `_carre`, `centre`/`coins`.
- Le repère Leaflet, les tuiles et le quadrillage.
- Le canal de 5 s, le protocole de commandes en retour et le mécanisme d'accusé de réception.
- La persistance atomique de `campagne._ecrire` et les trajets.
- L'anneau, les jauges et le bloc « Derniers mouvements du front » : ils prennent n'importe quel pourcentage ou liste.
- Le curseur de la frise : seule la source change.
- `trace_image.pixel()` pour dessiner le front sur l'image de l'opération.
- L'autocomplétion de `/ordonner` : à nourrir avec les noms de zones.

Fichiers à modifier et à redéployer : `bot.py` et `campagne.py` (poussés par `deployer_vps.ps1` l.26), `carte/index.html`, `site_web/index.html`, `site_web/campagne.html`, `site_web/soldat.html`, et côté mod `SRP_Bridge.c`.