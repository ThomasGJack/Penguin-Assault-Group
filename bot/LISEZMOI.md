# PAG-Bot — bot Discord permanent du Penguin Assault Group

Un seul bot, allumé en permanence, qui fait le lien entre le Discord et le serveur Arma Reforger :

- sa **présence** affiche en continu `🟢 12/40 en jeu · 3 mission(s) · 8,8 % de l'île`, ou `🔴 serveur injoignable` ;
- les membres **Staff** administrent le serveur depuis Discord : `/ban`, `/unban`, `/kick`, `/grade`, `/certif`, `/instructeur`, `/staff`, `/avertir`, `/dire`, `/mp`, `/sauver`, `/info` (les joueurs hors ligne aussi, sur leur fiche) ;
- tout le monde a `/serveur`, `/joueurs`, `/lier`, `/delier` ;
- les **rôles Discord suivent le grade en jeu** pour les comptes liés : Recrue, Soldat, Sous-officier, Officier, plus Instructeur pour ceux qui ont une habilitation.

Comment ça marche : le serveur de jeu ne peut pas recevoir de connexion, donc c'est lui qui appelle le bot toutes les 15 s (composant `SRP_BridgeComponent` du mod). Il envoie son état et repart avec les commandes en attente. Le bot doit donc être joignable **depuis Internet** sur son port, et ne peut pas tourner derrière une box sans redirection de port.

## 0. Le plus simple : sur ton PC Windows, tout automatique

1. Double-clique **installer.bat**. Il installe Python s'il manque, les dépendances, crée `config.json` avec un secret aléatoire, te demande le jeton du bot (saisie masquée, rangé dans les variables d'environnement de ton compte, jamais dans un fichier), ouvre le port dans le pare-feu et propose de démarrer le bot à chaque ouverture de session.
2. À la fin, il affiche les deux lignes `bridge_url` et `bridge_secret` à coller dans le `discord.json` du serveur de jeu.
3. **lancer.bat** démarre le bot tout de suite et le relance s'il tombe.

Deux limites à connaître : le PC doit rester allumé, et ta box doit **rediriger le port 8787 TCP vers ce PC** (interface d'administration de la box, rubrique NAT ou redirection de ports). Sans ça, le serveur LobbyHost ne peut pas joindre le bot et la présence reste rouge. Si ton adresse publique change, il faut mettre à jour `bridge_url`.

## 1. Où le faire tourner en permanence

Il faut une machine allumée 24 h/24 avec une adresse publique. Trois options, de la plus simple à la moins chère :

1. **Un petit VPS** (Hetzner, OVH, Ionos… autour de 4 € par mois, Ubuntu 22.04 ou 24.04). C'est la solution propre.
2. **Une machine virtuelle gratuite** (Oracle Cloud « Always Free », par exemple). Gratuit mais plus de manipulations.
3. **Ton PC** avec `lancer.bat`, pour tester seulement : dès qu'il s'éteint, le bot disparaît et la présence passe en rouge.

## 2. Le bot sur le Developer Portal

1. https://discord.com/developers/applications → ton application existante (celle du script d'installation) ou une nouvelle « PAG-Bot ».
2. Onglet **Bot** : active **Server Members Intent** (indispensable pour poser les rôles). Copie le jeton.
3. Onglet **OAuth2 → URL Generator** : coche `bot` et `applications.commands`, permissions `Manage Roles`, `Send Messages`, `View Channels`. Ouvre l'URL, ajoute le bot au serveur.
4. Dans Discord, **Paramètres du serveur → Rôles** : monte le rôle du bot **au-dessus** de Recrue, Soldat, Sous-officier, Officier, Instructeur. Un bot ne peut poser que des rôles plus bas que le sien.

## 3. Installation sur un VPS Debian 12 (ou Ubuntu)

Depuis ton PC Windows, tout se fait en un double-clic :

1. Lance `deployer_vps.bat` et donne l'adresse IP du VPS.
2. `ssh` demande le mot de passe du VPS (deux fois : envoi des fichiers, puis installation).
3. Sur le VPS, le script demande le **jeton du bot** en saisie masquée. Il est rangé dans `/etc/pag-bot.env`, lisible par root seulement.
4. À la fin, le script affiche la ligne `bridge_url` à mettre dans `discord.json` sur LobbyHost.
5. Lance `arreter_bot_local.bat` pour arrêter le bot de ton PC : deux bots avec le même jeton se marchent dessus.

Ce que fait `vps/installer_vps.sh` : paquets Python, utilisateur système `pagbot`, dossier `/opt/pag-bot`, environnement virtuel, service systemd `pag-bot` durci, démarrage automatique au reboot, et règle ufw pour le serveur de jeu si ufw est actif.

Relancer `deployer_vps.bat` plus tard met le bot à jour (le `config.json` et le jeton du VPS sont conservés). `deployer_vps.ps1 -Ip ... -Jeton` redemande le jeton après un « Reset Token ».

Sur le VPS : `journalctl -u pag-bot -f` pour suivre le journal, `systemctl restart pag-bot` pour relancer.

Test depuis ton PC : `http://IP-DU-VPS:8787/api/status` doit répondre `{"online": false, ...}` tant que le serveur de jeu ne l'a pas contacté.

## 4. Côté serveur de jeu (LobbyHost)

Dans le `discord.json` du profil du serveur (`profile/SimpleRP/discord.json`, celui qui contient déjà les webhooks), ajoute deux clés :

```json
  "bridge_url": "http://IP-DU-VPS:8787/",
  "bridge_secret": "LE-MEME-SECRET-QUE-DANS-config.json"
```

Redémarre le serveur. Le journal affiche `Passerelle bot Discord démarrée`, puis `Passerelle bot : contact établi` au premier échange réussi. La présence du bot passe au vert dans la minute.

Le mod doit être en version 1.0.12 ou plus (composant `SRP_BridgeComponent` sur le game mode).

## 5. Lier un compte Discord à un soldat

1. Sur Discord : `/lier` → le bot donne un code de six caractères, valable dix minutes.
2. En jeu, dans le chat : `/lier LECODE`.
3. Au contact suivant, le bot confirme en message privé et pose les rôles de grade. `/delier` retire la liaison.

Les liaisons sont dans `state.json` à côté du bot.

## 6. Sécurité

- Le jeton du bot ne va que dans le service systemd (ou `token.txt`, à protéger par `chmod 600`). Jamais dans `config.json`, jamais dans un dépôt.
- Le `secret` protège l'API : sans lui, toute requête est refusée avec un 403. Choisis-le long.
- Le trafic entre LobbyHost et le VPS est en HTTP simple. Pour le chiffrer, mets Caddy ou nginx devant le bot avec un certificat Let's Encrypt et un nom de domaine, puis passe `bridge_url` en `https://`.
- Seul le rôle Staff Discord peut envoyer des commandes au jeu. Le retour de chaque commande est aussi posté dans `🛡┃staff`.

## 7. Ce que fait le bot

**Soins et guides, pour tous, en salon ou en message privé** :
- `/soins` : fiche de soins interactive. Trois pages de menus (premier regard, signes vitaux, voies aériennes et soins déjà faits), puis « Quoi faire ? » : le bot rend les gestes dans l'ordre, marqués 🪖 tout soldat, ⛑️ infirmier ou 🩺 médecin, avec ce qu'il reste à examiner et les pièges à éviter.
- `/guide` : les guides de la PAG, à feuilleter chapitre par chapitre. Guide médical (11 chapitres) et procédure radio.
- Tout le contenu est dans `medical.py`, sans dépendance à Discord : le formulaire (`PAGES`), le moteur (`plan`) et les guides (`GUIDES`). À déployer à côté de `bot.py`. Les chiffres viennent du code d'ACE Dev et des réglages de la PAG : après un changement de réglage médical dans le mod, relire l'en-tête de `medical.py`.

**Pour tous** : `/serveur`, `/joueurs`, `/fiche` (sa propre fiche, avec la progression vers le béret noir), `/lier`, `/delier`, `/formation` (demande transmise aux instructeurs, qui la prennent par bouton).

**Encadrement** (rôles `panel_roles`, par défaut Staff et Officier) :
- `/panel`, dans un salon (réponse visible de soi seul) ou en message privé au bot. Accueil avec l'état du serveur, liste des joueurs, fiche d'un joueur et ses actions : avertir, message privé, grade, certification, instructeur, fiche complète. Expulser, bannir et lever un ban sont réservés à `ban_roles`. Section Serveur : annonce, sauvegarde, heure, missions.
- `/operation titre date heure description` : annonce avec inscription par menu de rôles, rappel automatique 30 minutes avant, clôture 4 heures après.

**Demandes de mission** (rôles `request_roles`, par défaut Staff et Officier) :
- En jeu, un soldat prend l'action « Demander une mission » au tableau des missions : mission du tableau ou capture d'une zone au contact du front, puis l'effectif parmi les joueurs en ligne.
- Sur Discord, `/ordonner capture` ne propose que les zones ennemies au contact du front (liste envoyée par le jeu), affichées « Régina (S07) » ; le jeu reçoit le code de la zone.
- Le bot publie la demande dans `⭐┃commandement` (mention du rôle `request_ping_role`) avec le bouton « Prendre l'assignation de la mission ». Un seul officier par demande : c'est le jeu qui arbitre.
- L'officier reçoit en message privé le panel de décision : **Accorder** (à pied, ou motorisé avec les véhicules garés à la base, plus des consignes), **Refuser** (motif), **Rendre l'assignation**, puis **Clôturer l'opération** une fois accordée. Il doit accepter les messages privés du serveur.
- Le groupe est prévenu en jeu de chaque décision. Les deux panels suivent l'opération jusqu'à l'issue : mission réussie ou perdue, zone capturée, demande annulée, serveur redémarré.
- Sans réponse après `request_reminder_minutes` (10), le bot relance le salon. Le soldat peut annuler sa demande au tableau.

**Staff** : les commandes directes `/ban /unban /kick /avertir /grade /certif /instructeur /staff /dire /mp /sauver /info`.

**Automatique** :
- Tableau de bord permanent dans `📊┃tableau-de-bord`, mis à jour chaque minute.
- Rôles Discord suivant le jeu pour les comptes liés : grade, Instructeur, Logisticien, Vétéran (100 missions).
- Alertes en direct dans `🚨┃alertes` : crime de guerre (avec mention du Staff), zone prise ou perdue, attaque annoncée et assaut, zone défendue, coupée ou reliée, offensive de la nuit, victoire et nouvelle campagne, front gelé ou ramené, dépôt détruit, faits du Commandeur ennemi (officier tombé, région libérée, renseignement, dépôt ennemi saboté, blindé ramené), véhicule perdu, mission réussie ou échouée, sanctions données en jeu. Tous les évènements d'un envoi sont publiés, par messages de 10 encadrés (`alerts_per_message`, au plus `events_per_sync_max` par envoi). La copie publique des annonces de zone part dans `🚩┃territoire` par le fil du mod ; les carrés un par un restent côté Staff (`journal-serveur`).
- Image du front chaque soir à 23 h (heure de Paris) dans `🚩┃territoire` : juste la carte, sans chiffre, et seulement si le front a bougé depuis l'image précédente : carrés, camp ou nom d'une zone (les paquets des dépôts et les coupures ne comptent pas ; `front_image_if_unchanged` pour la poster quand même). À la victoire, l'image de l'île toute bleue. Le rôle du bot doit pouvoir écrire et joindre des fichiers dans ce salon.
- Journal des sanctions dans `📕┃sanctions`.
- Classement hebdomadaire le lundi à 9 h dans `🏅┃promotions` : temps de service et missions.

Les salons et les rôles qui manquent sont créés par le bot au démarrage (sauf `🚩┃territoire` et le salon Staff `🎖┃état-major-ennemi`, créés par `setup_discord.py` avec leur webhook).

## 8. Le front sur le site (#75)

Le pont du jeu envoie la grille des carrés de 200 m (bloc `front` de `/api/sync`) : en entier une fois par minute, puis seulement les carrés qui changent. Le bot répond sur la première ligne `#front <campagne> <version>` (il tient cette version) ou `#front plein` (il redemande la grille entière, par exemple après un redémarrage du bot). Un ancien mod sans bloc `front` reste lu comme avant (secteurs) : on peut mettre le bot et le site en ligne AVANT le mod. Si un ancien mod est remis APRÈS le premier front (retour arrière), le site et le tableau de bord reviennent aux secteurs au lieu de montrer le front figé ; `front.json` est gardé et reprend au retour du mod à front.

Fichiers, dans `donnees_web/` :
- `front.json` : la grille, le découpage et l'état des zones, les 50 derniers mouvements du front. Le site le lit même serveur éteint (part de l'île tenue, zones, carte).
- `frise.json` : les photos de la frise de la carte, une à chaque zone prise ou perdue (15 s après, le temps que tous ses carrés arrivent) et une par jour à 23 h. Photos du jour gardées 30 jours (`front_photo_days`), photos de zone toute la campagne (`front_zone_photo_days` = 0).
- `archives/` : `secteurs-<date>.json`, l'historique des anciens secteurs, archivé une seule fois au premier front reçu ; `front-<campagne>.json`, chaque campagne finie (victoire ou nouvelle campagne du Staff), avec sa frise. Page Campagne, bloc « Campagnes précédentes » ; une campagne de front se rejoue sur `/carte/?archive=<nom>`.

Pages et adresses : `/carte/` (carrés bleus, rouges et orange quand ils sont contestés, ligne de front, noms des zones, frise), `/carte/api/front` (grille complète, jamais gardée en cache : la carte ne la redemande que quand `front_v` change), `/carte/api/frise`, `/carte/api/front.jpg` (l'image du soir, Pillow requis), `/campagne/api/archives` et `/campagne/api/archives/<nom>`. Le site ne montre rien des forces ennemies.

Réglages de `config.json` (valeurs par défaut dans `bot.py`) : `front_channel`, `front_image_hour`, `front_image_minute`, `front_image_if_unchanged`, `front_photo_days`, `front_zone_photo_days`, `front_photo_if_unchanged`, `front_photo_delay_seconds`, `front_movements_max`, `front_delta_max`, `front_sources`, `alerts_per_message`, `events_per_sync_max`.

Zones forcées par le Staff (`zone-bleue`, `zone-rouge`, `zone-reset`) : c'est une correction (Q9). Le mod les annonce dans le bloc `front` (clé `stf`, codes des zones) ; le site tient leur nouvel état mais n'en fait ni mouvement « prise / perdue » ni photo de la frise.

**Essais au Workbench** : le `discord.json` du profil Workbench contient lui aussi `bridge_url` et `bridge_secret`. Sans précaution, un essai y écrirait le front d'essai dans `front.json`, la frise et les archives du vrai site. Mets dans `config.json` du bot `"front_sources": ["IP-DU-SERVEUR-LOBBYHOST"]` : seuls ces serveurs nourrissent le front et la mémoire du site (fiches, trajets) ; les autres restent servis (commandes, alertes) et le journal le signale une fois. Liste vide = tous. En attendant, retire `bridge_url` et `bridge_secret` du `discord.json` du Workbench pendant les essais du mod neuf.

Commandeur ennemi (#76) : ses décisions ne passent jamais par le bot. Le mod les poste lui-même dans le salon Staff `🎖┃état-major-ennemi` (webhook « etat-major » de `discord.json`, créé par `setup_discord.py` ; sans lui, elles vont dans `journal-serveur`). Seuls les faits visibles des joueurs arrivent au bot comme alertes.

**Dépendance au mod** : les demandes de mission demandent le mod qui annonce la capacité « demande » (après la 1.0.19). Téléportation, soin, heure, missions, alertes, carré GPS, missions et infractions sur la fiche, pseudos avec espaces demandent la version du mod avec le pont v2. Avec un mod plus ancien, le bot masque ces boutons et fonctionne avec le reste.
