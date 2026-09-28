#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Installation du Discord du serveur SimpleRP (Penguin Assault Group).

Prérequis :
  - Python 3.9+ et le paquet requests :  pip install requests
  - Un bot ajouté au serveur avec la permission Administrateur (invitation via le Developer Portal)
  - Le jeton du bot dans la variable d'environnement DISCORD_TOKEN, l'identifiant du serveur dans DISCORD_GUILD
  Le jeton n'est jamais écrit sur le disque par ce script.

Le script est idempotent : relancé, il ne recrée pas ce qui existe déjà (rôles et salons retrouvés par leur nom).
Il écrit webhooks.json (les URL des flux automatiques du mod) à côté de lui.
"""
import json
import os
import sys
import time

try:
    import requests
except ImportError:
    sys.exit("Installe d'abord requests : pip install requests")

# La console Windows n'est pas toujours en UTF-8 : les émojis des noms de salons ne doivent jamais faire planter le script
for stream in (sys.stdout, sys.stderr):
    try:
        stream.reconfigure(encoding="utf-8", errors="replace")
    except (AttributeError, ValueError):
        pass

TOKEN = os.environ.get("DISCORD_TOKEN")
GUILD = os.environ.get("DISCORD_GUILD")
if not TOKEN or not GUILD:
    sys.exit("Définis DISCORD_TOKEN et DISCORD_GUILD dans l'environnement (voir l'en-tête du script)")

API = "https://discord.com/api/v10"
HEADERS = {"Authorization": f"Bot {TOKEN}", "Content-Type": "application/json"}

# ------------------------------------------------------------------------------------------------
# Réglages
# ------------------------------------------------------------------------------------------------
SERVER_NAME = "PAG — Penguin Assault Group"
SEP = "┃"                 # séparateur entre l'émoji et le nom d'un salon texte
COLOR_EMBED = 0x1F4E79    # bleu foncé des encadrés

# Permissions Discord (bits)
P_VIEW = 1 << 10
P_SEND = 1 << 11
P_MANAGE_MESSAGES = 1 << 13
P_READ_HISTORY = 1 << 16
P_CONNECT = 1 << 20
P_SPEAK = 1 << 21
P_MOVE = 1 << 24
P_ADMIN = 1 << 3

ROLES = [
    # nom, couleur, permissions de base, hoist (affiché à part)
    ("Staff",         0xE74C3C, P_ADMIN, True),
    ("Officier",      0x1F4E79, P_VIEW | P_SEND | P_READ_HISTORY | P_CONNECT | P_SPEAK | P_MOVE | P_MANAGE_MESSAGES, True),
    ("Instructeur",   0xE67E22, P_VIEW | P_SEND | P_READ_HISTORY | P_CONNECT | P_SPEAK, True),
    ("Sous-officier", 0x2E86C1, P_VIEW | P_SEND | P_READ_HISTORY | P_CONNECT | P_SPEAK, True),
    ("Soldat",        0x27AE60, P_VIEW | P_SEND | P_READ_HISTORY | P_CONNECT | P_SPEAK, True),
    ("Recrue",        0x82E0AA, P_VIEW | P_SEND | P_READ_HISTORY | P_CONNECT | P_SPEAK, True),
    ("Visiteur",      0x95A5A6, P_VIEW | P_READ_HISTORY, False),
]

EVERYONE = "@everyone"
PLAYERS = ["Recrue", "Soldat", "Sous-officier", "Instructeur", "Officier", "Staff"]
SOLDIERS = ["Soldat", "Sous-officier", "Instructeur", "Officier", "Staff"]
NCO = ["Sous-officier", "Instructeur", "Officier", "Staff"]
OFFICERS = ["Officier", "Staff"]
STAFF = ["Staff"]
# Accès libre : quiconque rejoint le Discord voit et parle dans les salons publics, sans attendre un rôle
PUBLIC = [EVERYONE, "Visiteur"] + PLAYERS

# Catégories et salons : (catégorie, [(clé, nom affiché, type, lecture pour, écriture pour, webhook?)])
# La clé sert au script (textes, webhooks.json) ; le nom affiché est ce que voient les joueurs.
STRUCTURE = [
    ("🏠 ACCUEIL", [
        ("bienvenue",      "👋" + SEP + "bienvenue",      "text",  PUBLIC, STAFF, False),
        ("regles",         "📜" + SEP + "règles",         "text",  PUBLIC, STAFF, False),
        ("annonces",       "📢" + SEP + "annonces",       "text",  PUBLIC, OFFICERS, False),
        ("presentations",  "🙋" + SEP + "présentations",  "text",  PUBLIC, PUBLIC, False),
    ]),
    ("💬 VIE DU SERVEUR", [
        ("general",        "💬" + SEP + "général",        "text",  PUBLIC, PUBLIC, False),
        ("rp",             "🎭" + SEP + "rp",             "text",  PUBLIC, PUBLIC, False),
        ("objections",     "📮" + SEP + "objections",     "text",  PUBLIC, PUBLIC, False),
        ("suggestions",    "💡" + SEP + "suggestions",    "text",  PUBLIC, PUBLIC, False),
        ("partages",       "📸" + SEP + "partages",       "text",  PUBLIC, PUBLIC, False),
    ]),
    ("🎯 OPÉRATIONS", [
        ("ordres-du-jour", "📋" + SEP + "ordres-du-jour", "text",  PUBLIC, OFFICERS, False),
        ("missions",       "🎯" + SEP + "missions",       "text",  PUBLIC, STAFF, True),
        ("territoire",     "🚩" + SEP + "territoire",     "text",  PUBLIC, STAFF, True),
        ("logistique",     "🚚" + SEP + "logistique",     "text",  PUBLIC, STAFF, True),
    ]),
    ("🎓 FORMATIONS", [
        ("formations",     "🎓" + SEP + "formations",     "text",  NCO, NCO, False),
        ("promotions",     "🏅" + SEP + "promotions",     "text",  PUBLIC, STAFF, True),
        ("dotations",      "🎒" + SEP + "dotations",      "text",  PUBLIC, STAFF, False),
    ]),
    ("⭐ COMMANDEMENT", [
        ("commandement",   "⭐" + SEP + "commandement",   "text",  NCO, OFFICERS, False),
        ("effectifs",      "👥" + SEP + "effectifs",      "text",  OFFICERS, STAFF, True),
    ]),
    ("🛡 STAFF", [
        ("staff",          "🛡" + SEP + "staff",          "text",  STAFF, STAFF, False),
        ("journal-serveur", "📖" + SEP + "journal-serveur", "text", STAFF, STAFF, True),
        ("alertes",        "🚨" + SEP + "alertes",        "text",  STAFF, STAFF, True),
    ]),
    ("🔊 VOCAL", [
        ("vocal-general",  "🔊 Général",                  "voice", PUBLIC, PUBLIC, False),
        ("vocal-staff",    "🛡 Staff",                    "voice", STAFF, STAFF, False),
    ]),
]

# Salons renommés d'une version à l'autre : (catégorie, ancien nom affiché, nouveau nom affiché, type)
RENAMED = [
    ("🏠 ACCUEIL", "📝" + SEP + "candidatures", "🙋" + SEP + "présentations", "text"),
]

# Salons d'anciennes versions à supprimer s'ils existent encore : (catégorie, nom affiché, type)
# Le jeu se joue à la radio en jeu : plus de vocaux d'équipe ni de formation.
OBSOLETE = [
    ("🔊 VOCAL", "📻 PC", "voice"),
    ("🔊 VOCAL", "🔊 Alpha", "voice"),
    ("🔊 VOCAL", "🔊 Bravo", "voice"),
    ("🔊 VOCAL", "🔊 Charlie", "voice"),
    ("🔊 VOCAL", "🎓 Formation", "voice"),
]

GAME_SERVER_NAME = "[FR] PAG | Milsim semi-RP Armée Française"

RULES_TEXT = """La PAG est un serveur milsim semi-RP de l'armée française : on y joue un soldat, pas un joueur. L'accès est libre, le respect de ces règles ne l'est pas.

**Sur le Discord**
**1.** Respect entre membres : pas d'insulte, pas de harcèlement, pas de politique, pas de contenu choquant.
**2.** Chaque salon a son usage : les questions dans 💬┃général, les réclamations dans 📮┃objections, les idées dans 💡┃suggestions.
**3.** Pas de publicité, pas de lien douteux, pas de mentions en rafale.
**4.** Les rôles Discord suivent tes grades en jeu, ils ne se demandent pas.
**5.** Une décision du Staff se conteste en privé ou dans 📮┃objections, jamais en public.

**En jeu**
**6.** Tu arrives Recrue, tu acceptes la charte à la première connexion, puis tu passes ta FGI avec un instructeur.
**7.** Le rôle avant tout : on reste dans son personnage, on parle comme sur un vrai réseau radio, on suit la chaîne de commandement.
**8.** Grades et certifications sont des règles de jeu : sans certification, on ne conduit pas, on ne pilote pas, on ne prend pas la mitrailleuse.
**9.** Pas de tir ami volontaire, pas de tir sur les civils, pas de pillage ni de destruction gratuite. Un crime de guerre est sanctionné.
**10.** Le matériel est commun : on range les paquets, on ramène les véhicules, on signale la casse.
**11.** Pas de triche, pas d'exploitation de bug, pas de déconnexion pour esquiver un combat ou une sanction.
**12.** Le Staff a le dernier mot en jeu. Un désaccord se règle après, dans 📮┃objections.

**Sanctions**, selon la gravité et la récidive : rappel, rétrogradation, exclusion temporaire, bannissement."""

WELCOME_TEXT = """Un serveur **Arma Reforger** milsim semi-RP, armée française, sur Everon : une base, des missions, un territoire à tenir, des grades qui se gagnent en jouant.

**🪖 Pour jouer, rien à demander**
L'accès est libre. Cherche **PAG** dans la liste des serveurs Reforger, le nom complet est `""" + GAME_SERVER_NAME + """`. En jeu, tu acceptes la charte puis tu passes ta FGI avec un instructeur. Présente-toi dans 🙋┃présentations si tu veux, on aime savoir qui arrive.

**🎖 Les rôles ici suivent tes grades en jeu**
Recrue, Soldat, Sous-officier, Officier ; Instructeur pour ceux qui forment ; Staff pour l'équipe. Le Staff les met à jour au fil de ta carrière."""

ANNOUNCE_TEXT = """La **PAG** ouvre ses portes : un serveur **Arma Reforger milsim semi-RP**, armée française, sur Everon.

Ici, tu ne spawn pas avec un fusil pour aller chercher du frag. Tu arrives en **Recrue**, tu acceptes la charte, tu passes ta **FGI** avec un instructeur, et tu construis ta carrière : dix grades du Soldat de 2e classe au Capitaine, seize certifications à décrocher, du conducteur VL au chef de groupe, du sapeur au pilote.

**Ce qui vous attend sur le terrain**
🎯 Seize types de missions générées en continu : caches ennemies, convois à intercepter, officiers à neutraliser, zones minées, documents à récupérer, colis médical, villages à libérer.
🚩 Un territoire à conquérir secteur par secteur, tenu par l'ennemi, avec garnisons qui patrouillent, contre-attaques annoncées et production à ramener à la base.
🚚 Une base vivante : quatre dépôts en paquets physiques, un coffre en euros, un terminal logistique, un parc automobile persistant. Un véhicule perdu est perdu. Chaque ravitaillement se gagne.
🎖 Tout est persistant : ta fiche, tes heures, tes états de service, ton inventaire, ta position.

**Matériel** : AMF pour l'armée française, RHS pour l'ennemi, ACE pour le médical, CRX pour une IA qui se bat vraiment.

**Accès libre, pas de candidature**
Cherche **PAG** dans la liste des serveurs, nom complet `""" + GAME_SERVER_NAME + """`, et connecte-toi. Aucune expérience milsim exigée, seulement l'envie de jouer le jeu.

*Serrés, on tient.*"""

# ------------------------------------------------------------------------------------------------
def call(method, path, **kwargs):
    for attempt in range(5):
        response = requests.request(method, API + path, headers=HEADERS, timeout=30, **kwargs)
        if response.status_code == 429:
            wait = float(response.json().get("retry_after", 1))
            time.sleep(wait + 0.2)
            continue
        if response.status_code >= 400:
            sys.exit(f"Erreur Discord {response.status_code} sur {method} {path} : {response.text}")
        time.sleep(0.35)
        return response.json() if response.text else None
    sys.exit("Trop de limitations de débit, réessaie dans une minute")


def embed(title, description):
    return {"embeds": [{"title": title, "description": description, "color": COLOR_EMBED}]}


def refresh_notice(channel_id, bot_id, title, text):
    """Remplace les messages du bot dans ce salon par un encadré à jour."""
    for m in call("GET", f"/channels/{channel_id}/messages?limit=50") or []:
        if m.get("author", {}).get("id") == bot_id:
            call("DELETE", f"/channels/{channel_id}/messages/{m['id']}")
    call("POST", f"/channels/{channel_id}/messages", json=embed(title, text))


def post_once(channel_id, bot_id, title, text):
    """Publie un encadré une seule fois : s'il y a déjà un message du bot dans ce salon, ne fait rien."""
    for m in call("GET", f"/channels/{channel_id}/messages?limit=50") or []:
        if m.get("author", {}).get("id") == bot_id:
            return
    call("POST", f"/channels/{channel_id}/messages", json=embed(title, text))
    print(f"    encadré publié : {title}")


def migrate_renamed():
    """Renomme les salons dont le nom a changé d'une version à l'autre (contenu conservé)."""
    channels = call("GET", f"/guilds/{GUILD}/channels")
    cat_by_name = {c["name"]: c for c in channels if c["type"] == 4}
    for category_name, old_name, new_name, kind in RENAMED:
        cat = cat_by_name.get(category_name)
        if not cat:
            continue
        channel_type = 0 if kind == "text" else 2
        for c in channels:
            if c.get("parent_id") == cat["id"] and c["type"] == channel_type and c["name"] == old_name:
                call("PATCH", f"/channels/{c['id']}", json={"name": new_name})
                print(f"  salon renommé : {old_name} -> {new_name}")


def migrate_legacy_names():
    """Renomme les catégories et salons créés sans émoji par l'ancienne version du script."""
    channels = call("GET", f"/guilds/{GUILD}/channels")
    cat_by_name = {c["name"]: c for c in channels if c["type"] == 4}
    for category_name, items in STRUCTURE:
        legacy = category_name.split(" ", 1)[1]
        if category_name not in cat_by_name and legacy in cat_by_name:
            cat = cat_by_name[legacy]
            call("PATCH", f"/channels/{cat['id']}", json={"name": category_name})
            print(f"catégorie renommée : {legacy} -> {category_name}")
            cat["name"] = category_name
            cat_by_name[category_name] = cat
        cat = cat_by_name.get(category_name)
        if not cat:
            continue
        for key, display, kind, readers, writers, wants_webhook in items:
            channel_type = 0 if kind == "text" else 2
            if kind == "text":
                legacy_ch = display.split(SEP, 1)[1]
            else:
                legacy_ch = display.split(" ", 1)[1]
            for c in channels:
                if c.get("parent_id") == cat["id"] and c["type"] == channel_type and c["name"] == legacy_ch:
                    call("PATCH", f"/channels/{c['id']}", json={"name": display})
                    print(f"  salon renommé : {legacy_ch} -> {display}")
                    c["name"] = display
                    break


def main():
    guild = call("GET", f"/guilds/{GUILD}")
    print(f"Serveur : {guild['name']}")
    if guild["name"] != SERVER_NAME:
        call("PATCH", f"/guilds/{GUILD}", json={"name": SERVER_NAME})
        print(f"  renommé en {SERVER_NAME}")

    # Rôles
    existing = {r["name"]: r for r in call("GET", f"/guilds/{GUILD}/roles")}
    role_ids = {}
    everyone = existing["@everyone"]
    # @everyone ne voit rien par défaut : chaque salon donne explicitement ses accès
    call("PATCH", f"/guilds/{GUILD}/roles/{everyone['id']}", json={"permissions": "0"})
    for name, color, perms, hoist in ROLES:
        if name in existing:
            role_ids[name] = existing[name]["id"]
            print(f"  rôle existant : {name}")
            continue
        role = call("POST", f"/guilds/{GUILD}/roles", json={"name": name, "color": color, "permissions": str(perms), "hoist": hoist, "mentionable": True})
        role_ids[name] = role["id"]
        print(f"  rôle créé : {name}")
    role_ids[EVERYONE] = everyone["id"]

    bot_id = call("GET", "/users/@me")["id"]
    migrate_legacy_names()
    migrate_renamed()

    # Salons obsolètes
    channels = call("GET", f"/guilds/{GUILD}/channels")
    cat_names = {c["id"]: c["name"] for c in channels if c["type"] == 4}
    for cat_name, display, kind in OBSOLETE:
        channel_type = 0 if kind == "text" else 2
        for c in channels:
            if c["type"] == channel_type and c["name"] == display and cat_names.get(c.get("parent_id")) == cat_name:
                call("DELETE", f"/channels/{c['id']}")
                print(f"  salon supprimé : {display}")

    # Salons
    channels = call("GET", f"/guilds/{GUILD}/channels")
    by_name = {}
    for c in channels:
        by_name[(c.get("parent_id"), c["name"], c["type"])] = c
    categories = {c["name"]: c for c in channels if c["type"] == 4}
    webhooks = {}

    for category_name, items in STRUCTURE:
        if category_name in categories:
            category = categories[category_name]
        else:
            category = call("POST", f"/guilds/{GUILD}/channels", json={"name": category_name, "type": 4})
            print(f"catégorie créée : {category_name}")
        for key, display, kind, readers, writers, wants_webhook in items:
            channel_type = 0 if kind == "text" else 2
            overwrites = [{"id": role_ids[EVERYONE], "type": 0, "allow": "0", "deny": str(P_VIEW | P_CONNECT)}]
            for role in set(readers + writers):
                if role == EVERYONE:
                    allow = P_VIEW | P_READ_HISTORY | P_CONNECT
                    if EVERYONE in writers:
                        allow |= P_SEND | P_SPEAK
                    overwrites[0] = {"id": role_ids[EVERYONE], "type": 0, "allow": str(allow), "deny": "0"}
                    continue
                allow = P_VIEW | P_READ_HISTORY | P_CONNECT
                if role in writers:
                    allow |= P_SEND | P_SPEAK
                deny = 0 if role in writers else (P_SEND | P_SPEAK)
                overwrites.append({"id": role_ids[role], "type": 0, "allow": str(allow), "deny": str(deny)})
            lookup = (category["id"], display, channel_type)
            if lookup in by_name:
                channel = by_name[lookup]
                call("PATCH", f"/channels/{channel['id']}", json={"permission_overwrites": overwrites})
                print(f"  salon existant, permissions rafraîchies : {display}")
            else:
                channel = call("POST", f"/guilds/{GUILD}/channels", json={"name": display, "type": channel_type, "parent_id": category["id"], "permission_overwrites": overwrites})
                print(f"  salon créé : {display}")
            if key == "regles":
                refresh_notice(channel["id"], bot_id, "📜 Charte du serveur", RULES_TEXT)
            if key == "bienvenue":
                refresh_notice(channel["id"], bot_id, f"👋 Bienvenue à la {SERVER_NAME}", WELCOME_TEXT)
            if key == "annonces":
                post_once(channel["id"], bot_id, "🪖 Ouverture du Penguin Assault Group", ANNOUNCE_TEXT)
            if wants_webhook:
                hooks = call("GET", f"/channels/{channel['id']}/webhooks") or []
                # Les webhooks ont été renommés « PAG » à la main dans Discord : on accepte les deux noms
                hook = next((h for h in hooks if h["name"] in ("PAG", "SimpleRP") and h.get("token")), None)
                if not hook:
                    hook = call("POST", f"/channels/{channel['id']}/webhooks", json={"name": "PAG"})
                    print(f"    webhook créé sur {display}")
                webhooks[key] = f"https://discord.com/api/webhooks/{hook['id']}/{hook['token']}"

    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "webhooks.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(webhooks, f, indent=2, ensure_ascii=False)
    print(f"\nwebhooks.json écrit ({out}) : à copier plus tard dans le profil du serveur de jeu sous SimpleRP/discord.json (pont du mod).")
    print("Terminé. Vérifie les rôles et les salons.")


if __name__ == "__main__":
    main()
