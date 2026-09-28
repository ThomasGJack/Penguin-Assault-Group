# Carte #69 : refaire l'IA des ennemis, sirène comprise
## Plan de codage à valider (25/09)

Le travail est découpé en **3 livraisons**. Chacune se compile et s'essaie seule, par-dessus la précédente. On commence par ce qui rapporte le plus et risque le moins.

Rien n'est codé tant que tu n'as pas validé ce plan. Tu compiles toi-même, Workbench fermé pendant que j'écris les fichiers.

Il n'y a **aucun réglage de difficulté dans le menu Staff**. Chaque nombre est un réglage du game mode, que tu peux changer dans le panneau d'attributs de SRP_GameMode. Le réglage fin se fait au ressenti sur le serveur, en s'aidant du journal de repérage.

---

## Livraison 1 : ils voient (risque faible, aucun soldat en plus)

**Ce qui change en jeu**
- **De jour, à découvert :**
  - debout à environ 100 m, tu es repéré et visé en quelques secondes ;
  - accroupi ou allongé, c'est nettement plus dur ;
  - chaque posture a son réglage.
- **Sentinelle et patrouille :** une sentinelle immobile voit mieux qu'une patrouille qui marche.
- **Buisson (expérimental, avec un interrupteur) :**
  - accroupi ou allongé, immobile dans un buisson, tu es repéré plus lentement ;
  - ça ne compte que si tous les joueurs proches du soldat (250 m) sont cachés : personne ne profite du buisson d'un autre ;
  - les hautes herbes ne sont pas prises en compte.
- **De nuit, sans jumelles de vision nocturne :**
  - la vraie lumière compte : lune, crépuscule, nuages ;
  - ce qui te trahit : ta lampe allumée, tes tirs (quelques secondes après chaque tir), les fusées, et la proximité (30 à 40 m).
- **Sous le feu :** sous un tir nourri, leurs balles se dispersent nettement.
- **Réaction aux tirs :** un tir entendu fait vraiment venir le groupe. Aujourd'hui, à moins de 300-400 m d'un tir, il se croyait déjà arrivé et ne bougeait pas.
- **Défaut de la version 1.8 du jeu, corrigé.** Les soldats arrivent maintenant un par un (2 par seconde pour toute la carte), et le mod effaçait les ordres des groupes encore vides. Même cause, réparée aussi :
  - l'alerte « un garde est tué » des missions ;
  - la garnison de livraison déclarée « neutralisée » trop tôt ;
  - le convoi ;
  - le compte de départ de la reddition.
- **L'officier :** il ne vise plus parfaitement et reste à son poste (30 m).
- **Plafond de groupes : 20 → 24.** Les groupes en attente de leurs soldats comptent maintenant dans le plafond. Sans ce relèvement, il y aurait moins d'ennemis qu'aujourd'hui.
- **Staff > Rapide > Essais, deux pages en lecture seule :**
  - « état des groupes autour de moi » ;
  - « journal de repérage » : qui a vu qui, à quelle distance, dans quelle posture, de jour ou de nuit, lumière, buisson, délai.

**Ce que tu essaies** (des contrôles rapides, pas une séance de mesure)
1. **Compilation :** sans erreur. Au premier ennemi posé, une ligne ENNEMI donne les réglages CRX du serveur.
2. **Pose :** Rapide > Vers une localité, puis revenir.
   - Chaque groupe passe de 0/N à N/N soldats et garde ses ordres.
   - Aucune ligne « pose abandonnée ».
3. **Jour :**
   - debout à 100 m d'un poste, en mode invincible : on te tire dessus en quelques secondes ;
   - allongé au même endroit : nettement plus dur ;
   - dans un buisson : plus lent (le journal indique « buisson oui »).
4. **Nuit (23 h) :**
   - sans lampe à 100-150 m : pas vu ;
   - à 30-40 m : vu ;
   - lampe allumée : vu de loin ;
   - si tu tires : vu quelques secondes.
5. **Réaction à un tir :** un coup de fusil à 200 m d'une ronde, hors de sa vue, la fait venir.
6. **Tir sous le feu :** mitrailler un groupe, ses tirs s'écartent.
7. **Rien ne doit changer pour :** les civils, les lampes des patrouilles, les fusées, la reddition, les jeeps, l'hélico et l'alerte des missions.

**Risques**
- **Les seuils du moteur sont cachés :** les valeurs de départ sont une estimation, le réglage fin se fait sur le serveur.
- **La nuit peut rendre l'ennemi trop aveugle.** Filets : la révélation de près ; le réglage « nuit minimum » à remonter.
- **Coût serveur :** un passage toutes les 2 s sur les soldats. Faible, mais à surveiller.

---

## Livraison 2 : ils réagissent en section, la ville sonne l'alerte (risque moyen)

**Ce qui change en jeu**
- **Au contact :** les groupes de 4 et plus avancent par binômes, une moitié fixe pendant que l'autre bouge.
- **Grenades :** rien à coder, le jeu les lance à courte distance si le soldat en porte. On le vérifie aux essais.
- **Aide :** environ 20 s après le premier repérage, le groupe voisin le plus proche vient aider en passant par le flanc. Un interrupteur donne l'approche directe.
- **Contact perdu pendant 1 min :**
  - ratissage d'environ 10 min, par le groupe venu aider et un groupe proche ;
  - puis chacun retourne à son poste ou à sa ronde.
- **Les autres groupes ne sont pas prévenus :** tu n'as demandé que le voisin le plus proche (question 7).
- **Sirène :**
  - Où : un vrai poteau de sirène militaire, visible et destructible, posé au centre de chaque localité ennemie gardée, à l'écart des routes.
  - Quand : un soldat de la garnison **voit** un joueur dans la localité. Il faut être vu : un bruit entendu ne suffit pas.
  - Quoi : environ 8 s plus tard (le temps de donner l'alerte), la sirène sonne environ 1 min. Tous les joueurs l'entendent, même ceux qui se connectent pendant ce temps.
  - Pause : 10 min de repos par localité.
  - Détruire le poteau la fait taire : environ 5 balles de fusil ou une grenade.
  - Jamais pour une mission en campagne, jamais pour un repérage par l'hélico.
  - Rien sur le Discord public ; l'état des sirènes n'apparaît que dans les pages Staff.

**Ce que tu essaies** (Workbench avec l'outil multijoueur, ou serveur à deux joueurs)
1. **Pendant un contact :**
   - un groupe passe en « aide » et arrive par le côté ;
   - après 1 min sans contact : « ratissage » pendant environ 10 min ;
   - puis retour au poste ou à la ronde.
2. **Au contact :** couvert, riposte, progression par binômes, grenades à courte portée.
3. **La sirène :**
   - se faire voir : la sirène sonne chez les deux joueurs ;
   - un 3e joueur qui se connecte l'entend encore ;
   - tirer sur le poteau : silence ;
   - anéantir en moins de 8 s le groupe qui t'a vu : pas de sirène ;
   - se refaire voir avant 10 min : pas de sirène.
4. **Jamais de sirène :** si tu es repéré par l'hélico, ou par la garde d'une mission en campagne.

**Risques**
- **Réplication du poteau :** on la vérifie avec un second joueur. Il existe un repli simple.
- **Portée du son au loin :** inconnue.
- **Aide et ratissage en ville :** un groupe peut se coincer, ou le contournement peut sembler artificiel. L'interrupteur donne l'approche directe.

---

## Livraison 3 : effectifs et camions de renfort (le plus risqué, donc en dernier)

**Garnisons**
- **Effectifs :**
  - hameau environ 6, village environ 10, ville environ 16 ;
  - +1 soldat par joueur au-delà de 2 (6 au plus) ;
  - +1 soldat par tranche de 2 points de menace, arrondi au-dessus : la menace ajoute toujours des soldats ;
  - même nombre de jour et de nuit (à 1 près).
- **Composition :**
  - Ville : un QG de section au centre (chef, sergent, opérateur radio, infirmier, tireur d'élite) et des sections.
  - Village : une section de 6 au centre (mitrailleuse et antichar) et des binômes.
  - Hameau : une section de 4.
- **Postes d'entrée de 2 hommes :**
  - 1 poste au hameau et au village, 2 à la ville ;
  - la nuit, 1 de plus, pris sur les sections : le total ne change pas ;
  - la nuit, moins de rondes ; au jour, retour aux rondes, jamais pendant une alerte.
- **Bâtiments :** une partie des soldats est placée dans les bâtiments, au mieux (certains peuvent rester devant).

**Camions** (ils remplacent partout l'ancien renfort motorisé, fini la jeep abandonnée à 300 m)
- **Déclenchement :** dès 2 joueurs, quand un joueur arrive à 500 m du centre d'une localité ennemie.
- **Arrivée :** un camion arrive 2 à 4 min plus tard par la route et se gare en ville, à la vue.
- **Débarquement :** tout le monde descend, une moitié vient vers vous, l'autre défend la ville.
- **Départ :** le camion repart et disparaît, jamais sous vos yeux.
- **Chargement :**
  - jusqu'à 4 joueurs : 2 à 6 soldats ;
  - dès 5 joueurs : 12 soldats, et un 2e camion plein si vous êtes vus.
- **Pris sous le feu** (camion touché, un homme touché, tir nourri, joueur à 30 m) : ils sautent et attaquent.
- **Opérateur radio :** le tuer retarde de 5 min le prochain appel (camion d'alerte et hélico), même si on vous voit bien plus tard. Aucun message aux joueurs.
- **Staff :** « forcer un camion », petit ou plein.

**Ce que tu essaies**
1. **Effectifs :** approcher un hameau, un village puis une ville. La page Staff donne les effectifs attendus et le même total à 23 h. Des soldats sont réellement dans les bâtiments.
2. **Camion d'entrée :** entrer à 500 m. Un camion arrive en 2 à 4 min (le journal donne l'heure prévue et l'heure réelle), débarque et repart. Le suivre aux jumelles : il ne disparaît jamais à la vue.
3. **Embuscade :** tirer sur le camion en route, ils sautent et attaquent. Barrer la rue : ils finissent à pied.
4. **Radio, à 5 joueurs :**
   - vus : 2e camion ;
   - opérateur radio tué avant : camion et hélico retardés de 5 min.
5. **Mission de cargaison :** un camion au lieu de la jeep.
6. **Non-régression :** la sirène, les jeeps, l'hélico.
7. **Charge :** soirée sur le serveur, surveiller les IA actives et les images par seconde.

**Risques**
- **Conduite de l'IA en ville.** Filets :
  - relance si le camion est bloqué, puis débarquement sur place ;
  - disparition toujours hors de vue ;
  - un interrupteur coupe tout.
- **Budget d'IA :** garnisons plus grosses et camions.
- **Bâtiments :** placement au mieux, un réglage à 0 le coupe.

---

## Tes réponses (25/09)
Déjà tranché avant : de 2 à 4 joueurs, un seul camion, à l'entrée dans la zone, sans camion d'alerte.
1. **Camion plein :** 10 soldats (marge sous la limite de 128 IA).
2. **Missions :** même tirage qu'aujourd'hui (une chance sur deux, plus avec la menace), mais avec le nouveau camion.
3. **Renfort à pied du secteur voisin :** coupé, le camion d'alerte le remplace.
4. **Sirène :** 8 s après avoir été vu, et elle sonne même si l'opérateur radio est mort (lui ne compte que pour le camion d'alerte et l'hélico).
5. **Poteau :** environ 5 balles ou une grenade ; après la prise de la ville il reste en place, muet, et reprend du service si l'ennemi reprend la ville.
6. **Autres groupes proches :** ils deviennent PLUS VIGILANTS sur place pendant quelques minutes, sans quitter leur poste (seul le voisin le plus proche vient aider).
7. **Sections :** les gardes et les vagues de mission passent aussi en sections de 4 à 6 ; les patrouilles hors mission restent légères.
8. **Tireur d'élite :** en ville seulement, avec le QG.
9. **Tirs entendus :** corrigé pour tous les ennemis (une patrouille qui entend un tir va vraiment voir, jusqu'à 300 m environ).
10. **Opérateur radio :** le retard de 5 min part du premier appel qui suit sa mort.
11. **Budget :** 24 groupes (livraison 1), puis 28 (livraison 3) ; 90 soldats au plus pour les garnisons et les camions ; 5 villes gardées en même temps au plus.
