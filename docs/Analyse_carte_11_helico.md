# Hélicoptère de recherche ennemi (carte #11) : analyse complète du 24/09/2026

*Rapport pour Jack. Je n'ai modifié aucun fichier du mod, du jeu ni des journaux. Chaque point porte une étiquette : **PROUVÉ** (vérifié dans le code ou les journaux), **PROBABLE** (fortement appuyé), **SUPPOSÉ** (hypothèse à vérifier).*

---

## 1. En bref

1. **À deux, l'hélico ne peut jamais venir tout seul.** Le réglage demande 3 joueurs à moins de 3 km de l'alerte, et rien ne le change sur le serveur (PROUVÉ). À trois, il n'y a qu'un seul tirage à 12 % par alerte, souvent perdu sans laisser de trace.
2. **Les deux Mi-8 que tu as vus sur PAG ont été lancés depuis le menu Staff** (21/09 à 23 h 15 et 23/09 à 19 h 10). On n'a encore jamais vu une sortie sur alerte en conditions réelles sur le serveur.
3. **La « boule qui se téléporte » : le projecteur n'est pas accroché à l'hélico.** La source de lumière est posée en l'air, à 80 m du sol éclairé, avec un halo et un faux cône. Une lampe ronde est ajoutée 14 m au-dessus de la cible. Le tout s'allume dès que l'hélico est à 2 km, et le point éclairé fait des sauts de plusieurs centaines de mètres.
4. **« Lumière divine » : la lampe est environ 18 à 39 fois plus puissante que les projecteurs d'hélico du jeu**, et elle ne faiblit presque pas avec la distance.
5. **La refonte proposée :** un faisceau étroit et tamisé qui part de sous le nez et qui glisse au lieu de sauter. Il faut d'abord un essai court dans le Workbench, parce que le 20/09 une lumière posée sur l'hélico était invisible depuis le sol.

---

## 2. Pourquoi il n'est pas apparu

### Comment il est censé venir

Quand l'ennemi **voit** des joueurs, le serveur ouvre une « alerte ». Environ 30 s après le premier repérage, il fait **un seul contrôle**, puis il lance le dé (SRP_Enemy.c l.1806-1809). Voici ce que le contrôle demande (valeurs réelles : SRP_GameMode.et et GameMode.layer ne changent aucun de ces réglages) :

| Condition | Valeur | Preuve |
|---|---|---|
| Aucun hélico en vol, et 30 min écoulées depuis la dernière sortie sur alerte | repos 30 min | l.1831, 1866-1869 |
| Joueurs vivants à moins de 3 km de l'alerte (sans compter ceux qui sont à la base, ni ceux qui se sont connectés ou ont réapparu depuis moins de 3 min) | **3 minimum** | l.236-237, 557-582, 1833 |
| Alerte à plus de 1 500 m de la base | 1 500 m | l.1835-1837 |
| Alerte à moins de 600 m d'une mission **en cours** ou d'un secteur ennemi | 600 m | l.1839-1858 |
| Tirage au dé | **12 %** | l.233-234, 1860 |

Le mode essai (1 joueur, tirage toujours gagnant) ne fonctionne que dans le Workbench (`#ifdef WORKBENCH`, l.506-521). Il est donc inactif sur le serveur (PROUVÉ).

### Les causes, de la plus lourde à la plus légère

**1. Le seuil de 3 joueurs (PROUVÉ).** À deux, le contrôle s'arrête tout de suite, sans rien écrire (l.1833). Tes journaux listent les joueurs connectés (lignes « Creating player / Connecting player ») :
- **2 joueurs au plus :** plusieurs longues séances entre le 21 et le 23/09.
- **3 joueurs ou plus :** quelques séances seulement sur la même période.

Sur une bonne partie de tes séances, l'hélico ne pouvait donc **pas** venir sur alerte. Ce seul réglage suffit à expliquer ton retour.

**2. Un seul tirage à 12 % par alerte (mécanisme PROUVÉ, chiffres par mission SUPPOSÉS).** Tout repérage à moins de 400 m d'une alerte existante est fusionné dans cette alerte, et elle suit les joueurs (l.1771-1780). Elle ne s'éteint qu'après 6 min sans repérage (l.1757). Une mission donne donc en général une à deux alertes, soit un à deux tirages. Le nombre d'alertes par mission est une hypothèse : il peut être plus élevé si vous êtes dispersés.

**3. Le tirage est perdu même quand les conditions ne sont pas réunies (PROUVÉ).** L'alerte est marquée « tirée » **avant** le contrôle (l.1808). Si le 3e joueur est à plus de 3 km, à la base, ou vient de se reconnecter à cet instant, le tirage est grillé pour toute l'alerte, sans deuxième chance et sans aucune ligne écrite. Le délai de 3 min après une connexion n'existe que depuis la 1.0.26 (22/09). Il pèse surtout après un plantage : le personnage revient sur le terrain, et le jeu a planté plusieurs fois sur la période.

**4. Le filtre de lieu (code PROUVÉ, effet réel SUPPOSÉ).** Un repérage pendant l'approche, ou par une patrouille à 700 m de la mission, ne compte pas. Une mission déjà réussie n'est plus « en cours ». Les ennemis posés à la main dans l'éditeur ne déclenchent jamais d'alerte (SUPPOSÉ).

**5. Rien n'est visible (PROUVÉ).** Seul le tirage raté écrit une ligne, dans la catégorie ENNEMI. Cette catégorie est classée « bavarde » : elle ne part pas sur Discord (SRP_Discord.c l.300-302, 340-343). Tous les autres refus sont muets.

### Ce que montrent tes journaux

- **21/09 à 23 h 15 min 15 :** un Mi-8 apparaît à 2 001 m d'un point situé à 197 m de la base. Une alerte ne peut pas appeler l'hélico à moins de 1 500 m de la base. C'est donc un **lancement Staff** (PROBABLE, très fort : le départ se fait toujours à 2 000 m du point visé).
- **Une autre sortie, le 23/09 :** deux joueurs seulement étaient connectés, donc l'appel automatique était impossible. C'est un **lancement Staff** (PROUVÉ). L'appareil est parti au-dessus de la mer, au bord sud de la carte.
- **22/09 à 16 h 04 et 16 h 13 :** ces Mi-8 ne venaient pas du serveur de la PAG.
- Le 20/09, dans le Workbench et en mode essai, l'hélico est bien venu sur alerte (21 h 46 min 40). **Sur le serveur dédié, ce chemin n'a jamais été vu fonctionner.** Il faudra le vérifier (étape 8).

### Probabilité réelle

| Situation | Chance que l'hélico vienne sur alerte |
|---|---|
| **2 joueurs** | **0 %** (seul le bouton Staff le fait venir) |
| **3 joueurs**, tous comptés au moment du tirage | 12 % par mission s'il y a 1 alerte, 22,6 % s'il y en a 2. Sur une soirée de 4 missions : 40 à 64 % |
| 3 joueurs, mais le 3e manque au tirage une fois sur cinq | 9,6 à 18,3 % par mission |

### Ce qu'on peut changer

- **Seuil :** ajouter `m_iHeliMinPlayers 2` dans SRP_GameMode.et. Aucun script à compiler, mais il faut republier depuis le Workbench.
- **Tirage non gaspillé :** l'alerte n'est marquée « tirée » que si toutes les conditions sont remplies. Sinon, le contrôle recommence 10 s plus tard.
- **Chance qui monte :** chaque tirage raté augmente la chance suivante, remise à zéro après une sortie. Par exemple la nuit : 25 → 40 → 55 → 70 %, soit 94 % cumulés au 4e essai. Attention, le compteur avance **par tirage** et non par mission.
- **Distance :** passer de 600 à 900 m, en mesurant aussi depuis le centre de la garde de la mission.
- **Suivi :** créer une catégorie de journal « HELICO », non bavarde, envoyée dans #journal-serveur, avec la raison de chaque décision (« 2 joueurs sur 3 requis », « repos encore 12 min », « alerte à 850 m de la mission », « tirage 25 % raté, prochaine chance 40 % »). Ajouter aussi, dans le menu Staff, une ligne d'état et un bouton « Forcer le prochain tirage ».

**À vérifier de ton côté, sans rien coder :**
- Dans #journal-serveur (et #alertes), cherche une ligne `[STAFF] … hélicoptère de recherche (prototype) au-dessus de sa position` vers 23 h 15 le 21/09 et vers 19 h 10 le 23/09. Cela suppose que le pont Discord fonctionne sur LobbyHost.
- Dans #missions, les lignes « garde de N groupe(s) pour X joueur(s) en approche » disent combien de joueurs le serveur comptait à chaque mission.
- Sur LobbyHost, le fichier `$profile:SimpleRP/journal/journal_2026-09.log` contient les lignes « Alerte : des joueurs ont été vus » et « l'ennemi ne fait pas venir son hélicoptère cette fois ». La page Journal du PC en jeu affiche aussi les 80 dernières lignes.

---

## 3. Le projecteur

Le code en jeu est bien celui que j'ai analysé : SRP_HeliSearch.c et SRP_Projecteur.et sont identiques dans la 1.0.26 publiée (empreinte md5 43516f5e…). C'est la version « source à 80 m + nappe » écrite le 20/09, dont personne n'avait regardé le rendu avant publication.

### Comment il est construit aujourd'hui

Chaque joueur fabrique chez lui **deux lumières qui ne sont pas attachées à l'hélico** :
- **Le faisceau :** il part sous l'hélico, mais dès que le point éclairé est à plus de 80 m (toujours le cas), sa source est **déplacée à 80 m du sol éclairé**, en l'air, sur la ligne hélico → sol (SRP_HeliSearch.c l.640, 759-766, PROUVÉ). Elle flotte à environ 18-80 m de haut, souvent à 100-450 m de l'appareil.
- **La « nappe » :** une lampe **ronde**, qui éclaire dans toutes les directions, posée 14 m au-dessus du point éclairé, rayon 42 m (l.777-792, PROUVÉ).

### D'où vient la « boule qui se téléporte »

1. **La source flottante porte un halo et un faux cône (PROBABLE, cause principale).** Le jeu dessine automatiquement un halo d'objectif (lens flare : l'éclat rond qu'une caméra voit autour d'une lampe) à l'endroit de la source. Le prefab hérite de ce halo (échelle 0,5, type automatique) et porte en plus un « faux volume » (VolLightIntensity 0,35 à l'échelle 1 : le cône laiteux dessiné dans l'air, le plus fort des 54 lumières du jeu). Sa pointe brillante se trouve à la source, donc dans le vide au-dessus de vous.
2. **La nappe ajoute une lueur ronde au-dessus du groupe (PROBABLE, effet plus faible).** Avec l'atténuation normale, elle éclaire peu. Elle fait surtout une lueur sans source visible.
3. **Elle s'allume dès l'approche (PROUVÉ).** L'ordre « allumé » part dès la phase 1 (l.219), alors que l'hélico est encore à 2 km, et le point visé est votre position (l.119). Une lumière apparaît donc chez vous avant que l'hélico soit là, avec un faisceau presque rasant. Le 23/09, il est parti de la mer, seulement 59 m au-dessus de votre sol.
4. **Le point éclairé saute (PROUVÉ).**
   - Au début de la fouille, il saute jusqu'à ~265 m, puis ~106 m une seconde après.
   - Quand l'hélico repère un soldat, le faisceau se colle à lui en 0,2 s.
   - Quand il le perd, le point repart sur une boucle qui a continué d'avancer : saut de ~300 m ou plus, traversé en 1 à 2,5 s (vitesse de départ ~500 m/s).
   - Même sans repérage, le point n'est envoyé qu'une fois par seconde : petits à-coups de 10 à 16 m.
   - Le lissage se fait « par image » (l.748, 754), donc la vitesse change selon les images par seconde de chaque joueur.
5. **Au départ, elle reste allumée jusqu'à 2,5 km (PROUVÉ).** Elle continue même de suivre le soldat repéré, pendant environ 85-90 s (l.335-344).
6. **Autre suspect possible (SUPPOSÉ) :** les fusées éclairantes. L'hélico en fait tirer une toutes les 45 s la nuit. Elles apparaissent d'un coup à 170 m et descendent lentement pendant environ 1 min.

### D'où vient la « lumière divine », trop forte

- **Puissance :** LV 10,5 (LV = puissance de la lampe, où +1 ≈ deux fois plus fort), contre 5,2 à 6,3 pour les projecteurs d'hélico du jeu. C'est 18 à 39 fois plus (valeurs PROUVÉES, échelle ×2 PROBABLE).
- **Presque aucune baisse avec la distance :** l'atténuation vaut 0,05, sous le minimum de l'éditeur (0,1). La valeur physique est 2, et les lumières du jeu qui la précisent sont entre 0,2 et 1,1 (PROUVÉ).
- **Couleur bleutée** (0,92 / 0,96 / 1), alors que les phares du jeu sont blanc chaud.
- **Les phares d'origine du Mi-8 sont forcés allumés** (atterrissage, roulage, phares : l.185-198). Ils portent à 100 m pour un vol à 120 m : ils n'éclairent rien au sol, mais ajoutent des cônes laiteux sous l'appareil (réglage PROUVÉ, effet visuel PLAUSIBLE).
- **Aucune différence entre jour et nuit :** le projecteur s'allume aussi en plein jour (PROUVÉ).
- **Le repérage ne colle pas à la lumière :** le serveur repère à 30 m de son propre point, alors que la tache visible fait environ 16 m (en réalité une ellipse allongée, parce que le faisceau arrive en biais). On peut donc être repéré sans être éclairé, ou l'inverse (PROUVÉ).

### Le piège à éviter (les analyses se contredisaient, voici mon choix)

Trois analyses proposaient simplement « une seule lumière attachée sous le nez ». Or, le 20/09, une lumière placée sous l'hélico (LV 11) **se voyait depuis Zeus mais pas depuis le soldat au sol, même dans le faisceau**, et le réglage FEATURE n'avait rien changé (notes du 20/09). La source posée à 80 m servait justement à contourner ce problème. Explication la plus probable (SUPPOSÉE) : le jeu coupe une lumière qu'il juge trop faible vu la distance entre la caméra et la source (réglage ClipEV).

Le seul exemple du jeu qui éclaire le sol de loin est la lampe de la caméra Game Master : posée à 150 m, puissance LV 13 à 17, atténuation physique, **sans halo ni faux cône**.

**Mon choix : faire un essai court avant d'écrire la refonte, et prévoir dès le départ une lumière d'appoint invisible au cas où.**

### La refonte proposée

On sépare **ce qu'on voit** de **ce qui éclaire**, comme le fait la lampe industrielle du jeu.

- **Faisceau visible, attaché sous le nez.** La lumière devient une pièce de l'hélico (AddChild). Elle est orientée comme une tourelle, avec une vitesse de rotation limitée (~15-20°/s en fouille, ~40-45°/s en poursuite) et un lissage qui ne dépend plus des images par seconde. Elle porte un petit halo au nez et un trait très léger dans l'air.
- **Lumière d'appoint, seulement si l'essai le demande.** Elle est posée sur le même axe, à 40-50 m du sol. C'est un spot et non une lampe ronde, **sans halo, sans faux cône et sans source visible** : elle éclaire le sol, mais on ne la voit pas comme un objet.
- **Côté serveur :**
  - le point visé avance en continu (~8-10 m/s) et la fouille se fait **autour de l'hélico** (130-250 m, faisceau à 35-55° sous l'horizon) ;
  - il glisse sur un soldat repéré en ~1 s ;
  - quand il perd la vue, il reste 3 s sur le dernier endroit puis fouille autour (il ne te suit plus à travers un toit) ;
  - le repérage se fait dans la tache réellement éclairée ;
  - il s'allume **seulement au-dessus de la zone et la nuit**, et s'éteint au départ, cible oubliée ;
  - pendant la fouille, les phares du Mi-8 sont coupés : on garde les feux de navigation et d'anticollision.
- **Transmission :** envoi deux fois par seconde avec la vitesse du point. Mieux : un composant sur un hélico dérivé dont l'état est transmis par le jeu. Il n'arrive alors qu'aux joueurs proches (même ceux qui se connectent en cours de route), et cela prépare le Mi-8 RHS. Pour un balayage calculé à l'identique chez tout le monde, il faut l'horloge commune du jeu (`Replication.Time()`).

### Valeurs de départ (toutes à régler à l'œil : le modèle de luminosité est SUPPOSÉ)

| Réglage | Aujourd'hui | Départ | Plage à essayer |
|---|---|---|---|
| Couleur | blanc bleuté | blanc chaud (1 ; 0,96 ; 0,88) | neutre possible |
| Largeur du cône (angle total) | 23° | 10° | 7-14° |
| Bord de la tache (0 = très doux, 1 = net) | 0,5 | 0,3 | 0-0,7 |
| Baisse avec la distance | 0,05 | 2 (physique) ; repli 1 | 1-2 |
| Puissance LV | 10,5 | essais à 13, 15 et 17 avec l'atténuation 2 ; repli : ≈ 4 + log2(distance) avec l'atténuation 1 (~11,6 à 200 m) | 9-17 |
| Seuil d'affichage (ClipEV) | −12 | −10 | −10 à −16 |
| Faux cône dans l'air | 0,35 / échelle 1 | 0,006 / échelle 0,4 ; **0** sur l'appoint | 0,002-0,04 |
| Halo | 0,5 automatique | 0,15 au nez ; **coupé** sur l'appoint | 0,1-0,4 |
| Portée | 170 m | 500 m | 350-600 m |
| Ombres des arbres | non | à essayer | selon le coût |
| Point d'attache | aucun | sous le nez, près des phares d'atterrissage (0 ; 0,86 ; 3,52), un peu plus bas | à caler |

**Protocole dans le Workbench.** De nuit (sans lune, avec lune, puis par brouillard), place un soldat à 150, 250 et 400 m de l'hélico, face au faisceau puis dos à l'hélico, et vérifie avec deux joueurs que vous voyez la même chose. **La mesure clé :** la distance caméra → source au-delà de laquelle le sol n'est plus éclairé vu du soldat (essais à 80, 150, 250 et 400 m). Pendant les essais, le script écrira dans la console la position de la source, sa distance à la caméra et son état.

---

## 4. Le vol en réseau et les autres points de la carte

| Point | État | Risque | Proposition |
|---|---|---|---|
| **Vol sur le serveur dédié** | Le serveur décide du mouvement, et le composant réseau d'hélico du jeu le lisse chez les joueurs (PLAUSIBLE). Tu l'as vu deux fois sur le serveur sans te plaindre du vol : indice faible qu'il est fluide. | Rotors immobiles ou son absent chez les joueurs, petites saccades verticales (jamais vérifié) | Le regarder aux jumelles à 300 m. En cas de saccades, couper la prédiction sur les copies des joueurs. Si les rotors sont figés, démarrer le moteur autrement |
| **Équipage** | En 1.8, les soldats d'un groupe apparaissent un par un. L'embarquement ne passe qu'une fois, à 3 s. Des équipiers restent au point de départ : le 21/09 sur terre, 3 min après ; le 23/09 en mer, 1 min 46 après (PROBABLE). La règle « pilotes tués » compte tous les équipiers vivants, où qu'ils soient (PROUVÉ) | Tuer les pilotes ne fait pas tomber l'hélico. Cockpit peut-être vide le 23/09 (SUPPOSÉ) | Créer 2 pilotes RHS directement dans la cabine. Ne compter que les occupants de l'appareil. Vérifier la clé de faction (« AFRF » dans le mode de jeu, « RHS_AFRF » chez RHS) |
| **Point de départ** | Tiré au hasard à 2 km : parfois en mer ou hors de la carte (PROUVÉ, 23/09) | Équipage posé dans l'eau | Essayer 8 à 16 directions et garder une direction sur la terre ferme, dans la carte, de préférence côté secteur ennemi |
| **Se cacher sous le feuillage** | La ligne de vue traverse les feuilles et les buissons. Seuls les troncs, les murs et les toits bloquent (PROUVÉ, fichier projet du jeu) | Sous un arbre, on est aussi visible qu'à découvert | Détecter les arbres et buissons à moins de 6 m, diviser la chance de repérage (ou la mettre à 0), et perdre la cible après 3 s. Les ombres des arbres rendraient la protection visible |
| **Chute, feux sur l'épave** | La chute n'envoie que « projecteur éteint ». Les phares du Mi-8 forcés allumés ne sont pas coupés, l'équipage n'est jamais retiré et l'épave n'est pas nettoyée par nous (PLAUSIBLE : le jeu peut couper des feux abîmés ou nettoyer l'épave, non vérifié). Jamais testé | Épave qui brûle phares allumés, ennemis « fantômes » qui occupent une place de groupe | Couper les feux à la chute (anticollision 10 s puis coupée), retirer l'équipage hors de vue, supprimer l'épave après 20 min sans joueur à moins de 300 m |
| **Mi-8 RHS** | L'appareil actuel est le Mi-8 du jeu, que RHS se contente d'étiqueter (PROUVÉ). Les vrais Mi-8 RHS sont `{5BF04078A1EC66D8}` noir sans pylônes, `{4A465E25A866520E}` avec pylônes et `{80C975F8482DEE97}`. Mêmes places de pilote, aucun projecteur | Aucun, c'est surtout l'apparence | Changer `m_sHeliPrefab` (sans code), ou mieux un hélico dérivé portant le composant du projecteur. Recaler le point d'attache |
| **Temps sur zone** | L'approche (~90 s) est comptée dans les 6 min : il tourne en réalité ~4 min 30 (PROUVÉ) | Un joueur qui arrive un peu tard le rate | Compter les 6 min depuis son arrivée, +60 s tant qu'il tient un soldat, 10 min au plus |
| **Fin de mission, alertes successives** | Aucun lien avec les missions. Il se recentre sur l'alerte la plus proche, pas la plus récente (PROUVÉ) | Il tourne au-dessus d'un site déjà nettoyé | Le rappeler à la fin de la mission ; se recentrer sur l'alerte la plus récente |
| **Cible dans une maison, inconsciente** | Le faisceau suit la cible à travers le toit pendant 6 s. Un joueur inconscient reste suivi (PROUVÉ) | Sentiment d'injustice | Rester sur le dernier point vu, puis fouiller autour |
| **Ordre « éteint » perdu** | Message non garanti : s'il se perd, la lumière reste jusqu'à 8 s, y compris après la chute (PROUVÉ) | Mineur | Réglé par la transmission d'état de la refonte |
| **Charge serveur** | Le vol coûte peu (PLAUSIBLE, non mesuré). Le vrai pic : un groupe RHS complet créé pour n'en garder que 2 | Faible | Réglé par les 2 pilotes directs |

---

## 5. Ce qui est écarté

- **Le serveur tournerait un autre code :** non. Le code de l'hélico et le projecteur sont identiques dans la 1.0.26 publiée. Le mode de jeu publié ne diffère que par `m_bPermitUnconsciousVON`.
- **Hélico désactivé, Mi-8 invalide, mode essai actif sur le serveur :** non. L'hélico sait apparaître sur le serveur.
- **Un vol « fantôme » bloquerait les sorties suivantes, collision avec le relief, hélico non reçu par les joueurs :** non.
- **Les Mi-8 du 22/09 :** ils étaient sur un autre serveur.
- **Le réseau (1 message par seconde) comme cause des sauts, ou un va-et-vient de l'hélico dans la zone de diffusion :** non, les sauts viennent du point visé et du lissage.
- **Un mod d'hélicoptères IA (REAPER) qui gênerait :** non, il n'est pas chargé par le serveur.
- **La propriété « Visualization Normal » :** c'est un affichage de débogage de l'éditeur (vraisemblable). On la passera quand même à Disabled.
- **Reprendre un mod de projecteur du Workshop, ou une tourelle comme sur le BRDM-2 :** pas adapté.
- **Pistes faibles, en une ligne :** « monter LV aggrave forcément », faux si on coupe le halo et le faux cône ; « lumière statique déplacée = saccades », non prouvé ; « FEATURE jamais testé seul », faux, il a été testé le 20/09 ; refonte « LV 8, atténuation 0,35 » telle quelle, risque de faisceau invisible.

---

## 6. Plan de refonte, dans l'ordre

**Pourquoi la lumière d'abord :** si l'hélico vient plus souvent avec la lumière actuelle, vous verrez plus souvent la « lumière divine ». Si tu veux quand même qu'il vienne à deux tout de suite, le seul réglage `m_iHeliMinPlayers 2` peut être publié à tout moment. Un pansement rapide est aussi possible avant la refonte : allumer seulement au-dessus de la zone, éteindre au départ, supprimer la nappe et couper le halo et le faux cône de la source flottante.

| Étape | Ce que je fais | Ce que tu testes |
|---|---|---|
| **0. Vérifications sans code** | — | Les lignes Discord et du journal (section 2). Et trois questions : la boule suivait-elle la tache, ou descendait-elle lentement pendant 1 min (fusée) ? Faisait-il nuit ou jour le 23/09 à 19 h 10 ? As-tu vu des pilotes à bord ? |
| **1. Essai de visibilité** | Un petit code d'essai jetable : spot sous le nez, halo et faux cône coupés, LV 13/15/17, atténuation 2, ClipEV −10/−14/−16, affichage des mesures dans la console | Workbench de nuit, soldat à 150, 250 et 400 m, face puis dos à l'hélico. **Le sol est-il éclairé ?** Le résultat décide s'il faut la lumière d'appoint |
| **2. Nouveau projecteur chez les joueurs** | Lumière attachée, appoint si besoin, orientation lissée, allumage sur zone et la nuit, phares du Mi-8 coupés | Plus de boule, le faisceau part du nez, plus aucun saut, et deux joueurs voient la même chose |
| **3. Nouveau point visé côté serveur** | Fouille autour de l'hélico, glissement vers le soldat, perte de vue, repérage = tache éclairée | Tu es repéré seulement dans la lumière ; dans une maison, le faisceau décroche |
| **4. Équipage, départ, chute** | 2 pilotes RHS en cabine, départ sur terre, feux coupés et nettoyage après la chute | Tuer les pilotes le fait tomber, tirer sur le rotor de queue aussi ; épave éteinte ; plus de nageurs |
| **5. Apparition et suivi** | Seuil, tirage non gaspillé, chance qui monte, journal HELICO, état et bouton dans le menu Staff, temps sur zone, rappel en fin de mission | Workbench en mode essai : les lignes de décision apparaissent, le bouton force bien le tirage |
| **6. Feuillage** | Couvert sous les arbres, ombres en option | Accroupi sous un arbre dans le faisceau |
| **7. Mi-8 RHS** | Hélico dérivé du Mi-8 RHS noir | Places, point d'attache du projecteur, livrée |
| **8. Publication et essai sur le serveur** | Republication | À 2 puis 3 joueurs : forcer un tirage, voir la ligne sur Discord, vérifier rotors, son et fluidité. **Premier vrai test de la chaîne alerte → hélico sur le serveur** |

⚠️ La prochaine compilation embarquera aussi les correctifs en attente, jamais compilés (droits #67, contrôle routier #61, radio, consoles). Il faudra les essayer en même temps, ou les publier avant.

---

## 7. Les choix à faire avant de coder

1. **Joueurs minimum pour qu'il vienne sur alerte :** A) 2 (conseillé, vous jouez souvent à deux) · B) 1 · C) 3 (actuel).
2. **Fréquence :** A) une chance qui monte à chaque tirage raté et revient à zéro après une sortie (nuit 25 → 40 → 55 → 70 %, jour 10 → 25 → 40 → 55 → 70 %), conseillé · B) 25 % fixe · C) garder 12 %. *Repos entre deux sorties : 20, 30 (actuel) ou 45 min ?*
3. **Jour et nuit :** A) jour et nuit, plus probable la nuit, projecteur éteint le jour (il cherche à vue), conseillé · B) la nuit seulement · C) pareil jour et nuit, projecteur toujours allumé (actuel).
4. **Rendu du projecteur :** tache A) comme des pleins phares de voiture, on voit bien dedans et c'est noir autour (conseillé) · B) forte, éblouissante quand on regarde l'hélico · C) réglable en jeu par le Staff. *Faisceau dans l'air :* trait très léger (conseillé), aucun, ou visible seulement par brouillard. *Couleur :* blanc chaud (conseillé), blanc neutre, ou bleuté actuel.
5. **Quand il te repère :** A) le faisceau glisse sur toi en ~1 s puis te suit ; si tu passes sous un toit, il reste 3 s sur ton dernier endroit puis fouille autour (conseillé) · B) pareil, mais il reste 2 s sur toi avant de prévenir les patrouilles · C) il saute sur toi tout de suite (actuel). *Balayage :* lent et méthodique (~15°/s) ou nerveux (~30°/s) ?
6. **Sous un arbre ou un buisson :** A) invisible au projecteur · B) très difficile à repérer (chance divisée par 10) · C) comme aujourd'hui (les feuilles ne cachent rien). *Ombres des arbres dans le faisceau :* les essayer, ou non ?
7. **Arrivée et présence :** prévenir les joueurs A) rien, on l'entend arriver · B) interception radio « l'ennemi demande un hélicoptère » ~60 s avant · C) message à l'écran. *Temps sur zone :* 6 min comptées depuis son arrivée, prolongées tant qu'il tient un soldat, 10 min au plus (conseillé), ou tel quel (~4 min 30). *En fin de mission :* il repart tout de suite, ou il finit sa ronde (actuel) ?
8. **L'appareil :** A) Mi-8 RHS noir sans pylônes (conseillé) · B) Mi-8 RHS noir avec pylônes · C) garder l'actuel. *Équipage :* 2 pilotes seulement (conseillé), ou 2 pilotes + un mitrailleur de porte qui vous tire dessus ? *Après une chute :* épave et corps retirés 20 min après, hors de vue (conseillé), ou épave laissée sur place ?

---

*Fichiers de travail (extraits, calculs, aucun fichier du mod touché) :*
- `%USERPROFILE%\AppData\Local\Temp\claude\C--Users-goule-Documents\e1f1bd8a-58e8-4a5d-8f58-bc003292c2d8\scratchpad\heli11_apparition\` (sessions.py, proba.py, publie\, rhs\)
- `…\scratchpad\heli11_projecteur\` (prefabs de lumière du jeu)
- `…\scratchpad\heli11_net\` (publie\, vanilla\, rhs\)
- `…\scratchpad\heli11_recherche\` (API du moteur, mods étudiés)
- `…\scratchpad\heli11_sceptique_apparition\pak`, `…\heli11_sceptique_net\`, `…\heli11_sceptique_proj\` (réextractions de la 1.0.26 et de RHS)
- Code analysé : `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_HeliSearch.c` et `SRP_Enemy.c`