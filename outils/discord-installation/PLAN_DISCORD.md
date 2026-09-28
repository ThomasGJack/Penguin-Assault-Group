# Discord du serveur — plan par défaut (à valider)

Nom proposé : **GOAF — Groupe Opérationnel Armé Français** (modifiable en tête du script).

## Rôles (du plus bas au plus haut)

| Rôle | Couleur | Qui | Droits |
|---|---|---|---|
| Visiteur | gris | rôle facultatif | accès libre : tout arrivant voit et écrit déjà dans les salons publics |
| Recrue | vert clair | charte acceptée en jeu | salons joueurs |
| Soldat | vert | 2e classe et plus | salons joueurs + vocaux d'équipe |
| Sous-officier | bleu | Sergent à Adjudant | + #commandement en lecture, salons instructeurs |
| Officier | bleu foncé | Lieutenant, Capitaine | + #commandement en écriture, gestion des vocaux |
| Instructeur | orange | certif Instructeur | + #formations en écriture |
| Staff | rouge | l'équipe serveur | administration complète, salons Staff |
| Bot SimpleRP | — | le bot | webhooks |

## Salons

**ACCUEIL** (lecture seule, sauf présentations)
- #bienvenue — présentation du serveur, comment rejoindre, lien du Workshop
- #règles — la charte du serveur (le même texte que `charte.txt`)
- #annonces — mises à jour, événements (Officiers et Staff écrivent)
- #présentations — ouvert à tous, facultatif : l'accès au serveur est libre, sans candidature

**VIE DU SERVEUR**
- #général — discussion
- #rp — récits, comptes rendus de mission, photos
- #objections — pour signaler un problème de règle ou de joueur au Staff (visible Staff + auteur… en pratique : Staff lit tout)
- #suggestions

**OPÉRATIONS**
- #ordres-du-jour — briefing des soirées (Officiers)
- #missions — flux automatique du mod : missions créées, prises, réussies
- #territoire — flux automatique : captures, pertes, attaques
- #logistique — flux automatique : livraisons, coffre, parc

**FORMATIONS** (Sous-officiers, Instructeurs, Officiers)
- #formations — demandes et plannings de FGI et certifs
- #promotions — flux automatique : grades, certifs accordées

**COMMANDEMENT** (Officiers, Staff)
- #commandement
- #effectifs — flux automatique : connexions, déconnexions, morts en absence

**STAFF** (Staff)
- #staff
- #journal-serveur — flux automatique brut du journal du mod
- #alertes — flux automatique : avertissements, bans, infractions, erreurs

**VOCAL**
- 🔊 PC (poste de commandement)
- 🔊 Alpha, 🔊 Bravo, 🔊 Charlie (équipes)
- 🔊 Formation
- 🔊 Staff (Staff uniquement)

## Webhooks créés par le script
missions, territoire, logistique, promotions, effectifs, journal-serveur, alertes → les URL sont écrites par le script dans `webhooks.json`, à copier dans le profil du serveur de jeu (`SimpleRP/discord.json`) pour le pont du mod.
