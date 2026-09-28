# Discord SimpleRP : installation

1. Dans le Developer Portal, onglet OAuth2 : coche `bot`, permission `Administrator`, ouvre l'URL generee et ajoute le bot a ton serveur.
   (Forme de l'URL : https://discord.com/oauth2/authorize?client_id=TON_APPLICATION_ID&scope=bot&permissions=8)
2. Dans Discord : Parametres utilisateur > Avance > Mode developpeur, puis clic droit sur le serveur > Copier l'identifiant du serveur.
3. Double-clique `lancer.bat` : il demande l'identifiant du serveur, puis le jeton du bot (saisie masquee). Rien n'est ecrit sur le disque.
4. Le script renomme le serveur (SERVER_NAME en tete de setup_discord.py, a modifier avant si tu veux un autre nom), cree 7 roles, 7 categories, les salons, les permissions, les textes de #bienvenue et #regles, et 7 webhooks.
5. `webhooks.json` est ecrit a cote du script : URL secretes des flux automatiques (missions, territoire, logistique, promotions, effectifs, journal-serveur, alertes). A copier plus tard dans le profil du serveur de jeu sous `SimpleRP/discord.json` quand le pont du mod existera.
6. Relancer le script est sans danger : il retrouve rôles et salons par leur nom et ne les recree pas.
