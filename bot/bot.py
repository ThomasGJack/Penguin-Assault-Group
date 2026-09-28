#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PAG-Bot : bot Discord permanent de la PAG (Penguin Assault Group), passerelle avec le serveur Arma Reforger.

Le serveur de jeu (mod SimpleRP, composant SRP_BridgeComponent) contacte ce bot à intervalle régulier :
POST /api/sync avec l'état du serveur ; la réponse contient les commandes Discord en attente.

Pour tous      /serveur /joueurs /fiche /lier /delier /formation
Encadrement    /panel (menu de gestion : joueurs, sanctions, grades, certifications, annonces…), /operation
Staff          /ban /unban /kick /grade /certif /instructeur /staff /avertir /dire /mp /sauver /info

Automatique    présence du bot, tableau de bord permanent, rôles Discord suivant le jeu, alertes en direct,
               rappel des opérations, journal des sanctions, classement hebdomadaire, image du front chaque soir.

Front (#75) : le pont envoie la grille des carrés de 200 m (bloc « front », accusé par la ligne « #front ») ; campagne.py
la garde dans donnees_web/front.json, le site la montre même serveur éteint. Un ancien mod (secteurs) reste lu.

Réglages : config.json à côté du script ; jeton dans la variable d'environnement DISCORD_TOKEN.
"""
import asyncio
import io
import datetime as dt
import html
import json
import logging
import os
import re
import secrets
import sys
import time
import urllib.parse
from zoneinfo import ZoneInfo

import aiohttp
import discord
from aiohttp import web
from discord import app_commands, ui

import medical
import suivi
import campagne

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(HERE, "config.json")
STATE_PATH = os.path.join(HERE, "state.json")
PARIS = ZoneInfo("Europe/Paris")
GOLD = 0xC9A227
BLUE = 0x1F4E79
RED = 0xC0392B
GREEN = 0x3E8E5A
SKY = 0x2E86DE       # zones à nous (bleu contre rouge, comme sur la carte)
ORANGE = 0xE67E22    # attaques ennemies sur nos zones

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("pag-bot")

# ------------------------------------------------------------------------------------------------
# Réglages
# ------------------------------------------------------------------------------------------------
DEFAULTS = {
    "guild_id": 0,
    "secret": "",
    "port": 8787,
    "max_players": 40,
    "staff_role": "Staff",
    "panel_roles": ["Staff", "Officier"],        # qui peut ouvrir /panel et créer une opération
    "ban_roles": ["Staff"],                      # qui peut expulser, bannir, donner le Staff
    "feedback_channel": "🛡┃staff",
    "sync_roles": True,
    "sync_staff_role": False,
    "grade_roles": {"recrue": "Recrue", "soldat": "Soldat", "sous_officier": "Sous-officier", "officier": "Officier"},
    "instructor_role": "Instructeur",
    "logi_role": "Logisticien",
    "veteran_role": "Vétéran",
    "veteran_missions": 100,
    "offline_after_seconds": 90,
    "board_users": [],                           # identifiants Discord autorisés à gérer /suivi, EN PLUS du propriétaire du bot
    "updates_channel": "🛠┃mises-à-jour",        # où le bot annonce une mise à jour publiée avec /suivi publier
    "public_url": "",                            # adresse publique du bot, pour les liens (ex. http://1.2.3.4:8787)
    "map_enabled": True,                         # carte de situation web : http://<serveur>:<port>/carte/ (publique, sans connexion)
    "map_dir": "carte",                          # dossier de la page et des tuiles, à côté de bot.py
    "public_port": 0,                            # port web public en plus de "port" (80 = adresse sans numéro de port) ; 0 = aucun. Il ne sert QUE les pages, jamais le pont du jeu
    "public_host": "0.0.0.0",                    # adresse d'écoute du port public ; "127.0.0.1" quand un serveur web (Caddy, HTTPS) est placé devant
    "site_enabled": True,                        # page d'accueil du site (http://<serveur>:<port>/) : portes vers la carte, le suivi, le Discord
    "discord_invite": "",                        # lien d'invitation Discord affiché sur l'accueil ("" = bouton caché)
    "server_name": "",                           # nom du serveur dans la liste d'Arma Reforger, affiché sur l'accueil ("" = rien)
    "vote_url": "https://top-serveurs.net/arma-reforger/vote/fr-pag-milsim-semi-rp-armee-francaise-discordgg4qqgkqdhju",  # page de vote (/vote y ajoute ?pseudo=)
    "top_serveurs_token": "",                    # jeton de votes de la fiche serveur (page d'administration top-serveurs) ; "" = pas de prime automatique
    "vote_reward": 150,                          # euros versés au coffre de la base par vote réclamé
    "vote_channel": "",                          # salon où le bot annonce chaque vote ("" = aucun)
    # Salons gérés par le bot (créés s'ils manquent : nom, catégorie, modèle de permissions à copier)
    "dashboard_channel": "📊┃tableau-de-bord",
    "operations_channel": "📅┃opérations",
    "sanctions_channel": "📕┃sanctions",
    "formations_channel": "🎓┃formations",
    "alerts_channel": "🚨┃alertes",
    "ranking_channel": "🏅┃promotions",
    "shares_channel": "📸┃partages",
    "command_channel": "⭐┃commandement",         # demandes de mission venues du jeu
    "command_category": "⭐ COMMANDEMENT",
    "request_roles": ["Staff", "Officier"],      # qui peut prendre l'assignation d'une demande
    "request_ping_role": "Officier",             # rôle mentionné à chaque nouvelle demande ("" = personne)
    "request_reminder_minutes": 10,              # relance si personne n'a pris la demande
    "community_category": "💬 VIE DU SERVEUR",
    "community_template": "💬┃général",
    "public_category": "🎯 OPÉRATIONS",
    "staff_category": "🛡 STAFF",
    "public_template": "📜┃règles",
    "staff_template": "🛡┃staff",
    "dashboard_seconds": 60,
    "ranking_weekday": 0,        # 0 = lundi
    "ranking_hour": 9,
    # Front (#75) : image du soir, frise du site, alertes
    "front_channel": "🚩┃territoire",            # salon de l'image du front du soir et de celle de la victoire
    "front_image_hour": 23,                      # heure de Paris de l'image du soir et de la photo du jour de la frise
    "front_image_minute": 0,
    "front_image_if_unchanged": False,           # poster l'image même si le front n'a pas bougé depuis la précédente
    "front_photo_days": 30,                      # photos du jour gardées dans la frise (jours)
    "front_zone_photo_days": 0,                  # photos de zone prise ou perdue gardées (jours) ; 0 = toute la campagne
    "front_photo_if_unchanged": False,           # photo du jour même identique à la précédente
    "front_photo_delay_seconds": 15,             # délai entre la bascule d'une zone et sa photo (le temps que ses carrés arrivent)
    "front_movements_max": 50,                   # mouvements du front gardés (la page Campagne montre les 8 derniers)
    "front_delta_max": 2000,                     # garde-fou : au-delà, le bot refuse les changements et redemande la grille entière
    "front_sources": [],                         # adresses IP des serveurs de jeu qui nourrissent le front et la mémoire du site ; [] = tous (un Workbench d'essai aussi)
    "alerts_per_message": 10,                    # encadrés par message d'alertes (limite de Discord : 10)
    "events_per_sync_max": 60,                   # garde-fou : évènements traités par envoi du jeu
}

GRADES = ["recrue", "soldat", "premiere", "caporal", "caporal-chef", "sergent", "sergent-chef", "adjudant", "lieutenant", "capitaine"]
GRADE_LABELS = ["Recrue", "Soldat", "1re classe", "Caporal", "Caporal-chef", "Sergent", "Sergent-chef", "Adjudant", "Lieutenant", "Capitaine"]
CERTIFS = [("fgi", "FGI"), ("or", "Opérateur radio"), ("infirmier", "Infirmier"), ("medecin", "Médecin"), ("sapeur", "Sapeur"),
           ("logi", "Logisticien"), ("vl", "Conducteur VL"), ("pl", "Conducteur PL"), ("blinde", "Chef d'engin blindé"),
           ("pilote", "Pilote"), ("mg", "Mitrailleur"), ("lg", "Lance-grenades"), ("at", "Antichar"), ("tp", "Tireur de précision"),
           ("cdg", "Chef de groupe")]
OP_ROLES = [("fusilier", "Fusilier", "🪖"), ("mule", "Mule", "🎒"), ("grenadier", "Grenadier", "💥"), ("mitrailleur", "Mitrailleur", "🔫"),
            ("tp", "Tireur de précision", "🎯"), ("infirmier", "Infirmier ou médecin", "⛑️"), ("cdg", "Chef de groupe", "⭐"),
            ("or", "Opérateur radio", "📻"), ("logi", "Logisticien ou conducteur", "🚚"), ("equipage", "Équipage ou pilote", "🚁"),
            ("reserve", "Réserve, selon les besoins", "➕")]
ALERT_STYLES = {   # type d'évènement envoyé par le jeu -> (émoji, couleur, rôle à mentionner ou None)
    "crime": ("⚖️", RED, "staff_role"), "attaque": ("🚨", RED, None), "secteur_perdu": ("🏳️", RED, None),
    "secteur_pris": ("🚩", GREEN, None), "vehicule": ("💥", GOLD, None), "mission": ("🎯", BLUE, None),
    "sanction": ("📕", RED, None), "info": ("ℹ️", BLUE, None),
    "controle": ("🚧", BLUE, None),     # contrôle routier établi par un soldat : va au salon commandement, pour information
    "commandement": ("📣", GOLD, None),  # à l'attention des officiers (capture faite sans mission activée au tableau…)
    "defense": ("🛡️", GREEN, None),     # ancien mod : secteur défendu
    # Front (#75) : annonces de zone (la copie publique part dans 🚩┃territoire par le fil du mod) ; aucune mention de rôle (G15)
    "zone_prise": ("🚩", SKY, None), "zone_perdue": ("🏳️", RED, None),
    "zone_attaque": ("🚨", ORANGE, None), "zone_assaut": ("⚔️", ORANGE, None),
    "zone_defendue": ("🛡️", SKY, None), "zone_coupee": ("✂️", GOLD, None), "zone_reliee": ("🔗", SKY, None),
    "victoire": ("🏆", GOLD, None), "campagne": ("🗺️", BLUE, None), "front": ("🧊", BLUE, None),
    "offensive": ("🏳️", RED, None),     # offensive ennemie de la nuit (H2)
    "depot_detruit": ("💥", RED, None),  # dépôt de zone détruit à la reprise (G7)
    # Commandeur ennemi (#76) : faits visibles des joueurs ; ses décisions, elles, restent dans le salon état-major (Staff)
    "officier_tombe": ("🎖️", GOLD, None), "region_liberee": ("🏁", GREEN, None), "renseignement": ("🔎", BLUE, None),
    "depot_ennemi": ("💥", GOLD, None), "blinde_ramene": ("🛡️", GREEN, None),
}
COMMAND_EVENTS = {"controle", "commandement"}           # ces évènements préviennent le commandement au lieu du salon des alertes


def load_config():
    if not os.path.exists(CONFIG_PATH):
        sys.exit(f"Pas de {CONFIG_PATH} : copie config.example.json en config.json et remplis-le")
    with open(CONFIG_PATH, encoding="utf-8") as f:
        cfg = dict(DEFAULTS)
        cfg.update(json.load(f))
    if not cfg["guild_id"] or not cfg["secret"]:
        sys.exit("config.json : guild_id et secret sont obligatoires")
    return cfg


def load_token():
    token = os.environ.get("DISCORD_TOKEN", "").strip()
    if token:
        return token
    path = os.path.join(HERE, "token.txt")
    if os.path.exists(path):
        with open(path, encoding="utf-8") as f:
            return f.read().strip()
    sys.exit("Jeton du bot introuvable : variable DISCORD_TOKEN ou fichier token.txt")


CFG = load_config()
TOKEN = load_token()


# ------------------------------------------------------------------------------------------------
# État persistant
# ------------------------------------------------------------------------------------------------
class State:
    def __init__(self):
        self.data = {"links": {}, "ops": {}, "dashboard_message": 0, "seen": {}, "week_base": {}, "week_id": "", "requests": {}}
        if os.path.exists(STATE_PATH):
            try:
                with open(STATE_PATH, encoding="utf-8") as f:
                    self.data.update(json.load(f))
            except (OSError, ValueError) as ex:
                log.warning("state.json illisible (%s) : on repart d'un état vide", ex)

    @property
    def links(self):
        return self.data["links"]          # discord_id (str) -> {"identity", "name"}

    @property
    def ops(self):
        return self.data["ops"]            # message_id (str) -> opération

    def save(self):
        tmp = STATE_PATH + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(self.data, f, indent=1, ensure_ascii=False)
        os.replace(tmp, STATE_PATH)

    def discord_for_identity(self, identity):
        for did, info in self.links.items():
            if info.get("identity") == identity:
                return int(did)
        return None


STATE = State()


# ------------------------------------------------------------------------------------------------
# File de commandes vers le jeu et dernier état reçu
# ------------------------------------------------------------------------------------------------
class Bridge:
    def __init__(self):
        self.pending = []
        self.waiting = {}
        self.last_state = None
        self.last_sync = 0.0
        self.link_codes = {}
        self.next_id = int(time.time()) % 100000

    def enqueue(self, author, command):
        self.next_id += 1
        cid = str(self.next_id)
        fut = asyncio.get_event_loop().create_future()
        self.pending.append({"id": cid, "author": author, "command": command.replace("\n", " ").replace("\t", " ")})
        self.waiting[cid] = fut
        return fut

    def online(self):
        return self.last_state is not None and time.time() - self.last_sync < CFG["offline_after_seconds"]

    def caps(self):
        """Capacités annoncées par le jeu (mod récent) : tp, soigner, heure… Vide avec un mod ancien."""
        return set((self.last_state or {}).get("caps", []))

    def players(self):
        return (self.last_state or {}).get("players", []) if self.online() else []

    def take_pending_lines(self):
        lines = [f"{p['id']}\t{p['author']}\t{p['command']}" for p in self.pending]
        self.pending = []
        return "\n".join(lines)

    def deliver_results(self, results):
        for r in results or []:
            fut = self.waiting.pop(str(r.get("id")), None)
            if fut and not fut.done():
                fut.set_result((bool(r.get("ok")), r.get("message", "")))

    def new_link_code(self, discord_id):
        now = time.time()
        for code in [c for c, v in self.link_codes.items() if v["expires"] < now]:
            del self.link_codes[code]
        code = secrets.token_hex(3).upper()
        self.link_codes[code] = {"discord_id": discord_id, "expires": now + 600}
        return code


BRIDGE = Bridge()


def target_token(pseudo, identity=""):
    """Le mot qui désigne un joueur dans une commande. Avec un mod récent : « @identité », sinon le pseudo
    dont les espaces sont écrits « + » (un pseudo peut contenir des espaces, la commande est découpée sur eux)."""
    pseudo = str(pseudo).strip()
    if "cible" not in BRIDGE.caps():
        return pseudo
    if not identity:
        for p in BRIDGE.players():
            if p.get("name", "").lower() == pseudo.lower():
                identity = p.get("identity", "")
                break
    if identity:
        return "@" + identity
    return pseudo.replace(" ", "+")


def pretty(command):
    """La commande telle qu'on l'affiche : « @identité » et « + » redeviennent un pseudo lisible."""
    words = command.split(" ")
    if len(words) > 1:
        if words[1].startswith("@"):
            for p in (BRIDGE.last_state or {}).get("players", []):
                if p.get("identity") == words[1][1:]:
                    words[1] = p.get("name", words[1])
                    break
        elif "cible" in BRIDGE.caps():
            words[1] = words[1].replace("+", " ")
    return " ".join(words)


async def game_command(author, command, timeout=60):
    """Envoie une commande au jeu. Retourne (ok, message) ; ok vaut None si le jeu n'a pas répondu à temps."""
    if not BRIDGE.online():
        return False, "le serveur de jeu ne contacte plus le bot"
    fut = BRIDGE.enqueue(author, command)
    try:
        return await asyncio.wait_for(fut, timeout=timeout)
    except asyncio.TimeoutError:
        return None, "pas de réponse en 60 s ; la commande reste en attente et s'exécutera au prochain contact"


# ------------------------------------------------------------------------------------------------
# Bot Discord
# ------------------------------------------------------------------------------------------------
intents = discord.Intents.default()
intents.members = True


class PagBot(discord.Client):
    def __init__(self):
        super().__init__(intents=intents)
        self.tree = app_commands.CommandTree(self)
        self.guild_obj = discord.Object(id=int(CFG["guild_id"]))
        self.channels_ready = False

    async def setup_hook(self):
        # Commandes globales (utilisables aussi en message privé pour /panel, /fiche…) ; on vide les anciennes copies de serveur
        self.tree.clear_commands(guild=self.guild_obj)
        await self.tree.sync(guild=self.guild_obj)
        await self.tree.sync()
        self.add_view(OperationView())
        self.add_view(FormationView())
        self.add_view(RequestTakeView())
        self.add_view(RequestPanelView())
        self.loop.create_task(self.presence_loop())
        self.loop.create_task(self.dashboard_loop())
        self.loop.create_task(self.outbox_loop())
        self.loop.create_task(self.operations_loop())
        self.loop.create_task(self.ranking_loop())
        self.loop.create_task(self.front_loop())
        try:
            vote_migrate()               # avant d'ouvrir le pont : les anciennes demandes rangées par pseudo ne doivent plus peser
        except Exception as ex:  # noqa: BLE001
            log.warning("votes (reprise des anciennes données) : %s", ex)
        self.loop.create_task(self.vote_loop())
        self.loop.create_task(start_web())

    async def vote_loop(self):
        await self.wait_until_ready()
        async with aiohttp.ClientSession() as session:
            while not self.is_closed():
                try:
                    await vote_tick(session)
                except Exception as ex:  # noqa: BLE001
                    log.warning("votes : %s", ex)
                await asyncio.sleep(60)

    async def on_ready(self):
        log.info("Connecté en tant que %s", self.user)
        for g in list(self.guilds):
            await self.leave_foreign(g)
        if not self.channels_ready:
            await self.ensure_channels()
            self.channels_ready = True

    async def on_guild_join(self, guild):
        await self.leave_foreign(guild)

    async def leave_foreign(self, guild):
        # Le bot ne sert que la PAG : invité sur un autre serveur, il le quitte aussitôt
        if guild.id != self.guild_obj.id:
            log.warning("Invité sur un autre serveur Discord (%s, %s) : je le quitte", guild.id, guild.name)
            try:
                await guild.leave()
            except discord.HTTPException:
                log.exception("Impossible de quitter le serveur %s", guild.id)

    # --- serveur, membres, salons ---------------------------------------------------------------
    def guild(self):
        return self.get_guild(int(CFG["guild_id"]))

    async def member_of(self, user_id):
        g = self.guild()
        if not g:
            return None
        m = g.get_member(user_id)
        if m:
            return m
        try:
            return await g.fetch_member(user_id)
        except discord.HTTPException:
            return None

    def channel_named(self, name):
        """Salon par son nom, ou par sa mention « <#id> » ou son identifiant (ce que renvoie un sélecteur Discord)."""
        g = self.guild()
        if not g:
            return None
        name = str(name).strip()
        digits = name[2:-1] if name.startswith("<#") and name.endswith(">") else name
        if digits.isdigit():
            ch = g.get_channel(int(digits))
            return ch if isinstance(ch, discord.TextChannel) else None
        return discord.utils.get(g.text_channels, name=name)

    async def ensure_channels(self):
        """Crée les salons du bot s'ils manquent, avec les permissions d'un salon modèle."""
        g = self.guild()
        if not g:
            return
        wanted = [(CFG["dashboard_channel"], CFG["public_category"], CFG["public_template"], "État du serveur de jeu, mis à jour en continu par le bot."),
                  (CFG["operations_channel"], CFG["public_category"], CFG["public_template"], "Opérations programmées : inscris-toi avec le menu sous chaque annonce."),
                  (CFG["sanctions_channel"], CFG["staff_category"], CFG["staff_template"], "Journal des avertissements, expulsions et bannissements."),
                  (CFG["shares_channel"], CFG["community_category"], CFG["community_template"], "Captures, clips et bons moments du serveur. On rit avec les copains, jamais contre eux."),
                  (CFG["command_channel"], CFG["command_category"], CFG["staff_template"], "Commandement : demandes de mission des soldats en jeu, à prendre et à décider par les officiers."),
                  (CFG["updates_channel"], CFG["public_category"], CFG["public_template"], "Notes de mise à jour du serveur, publiées par le bot.")]
        for name, cat_name, template_name, topic in wanted:
            if discord.utils.get(g.text_channels, name=name):
                continue
            category = discord.utils.get(g.categories, name=cat_name)
            template = discord.utils.get(g.text_channels, name=template_name)
            overwrites = template.overwrites if template else {}
            try:
                channel = await g.create_text_channel(name, category=category, overwrites=overwrites, topic=topic, reason="PAG-Bot : salon géré par le bot")
                log.info("salon créé : %s", name)
                if name == CFG["shares_channel"]:
                    embed = discord.Embed(title="📸 Partages de la PAG", color=GOLD,
                                          description="Ici, on partage les bons moments du serveur : captures, clips, ratés mémorables et belles actions.")
                    embed.add_field(name="🎬 Ce qu'on poste", inline=False,
                                    value="• Tes captures d'écran et tes clips de mission.\n• Les moments drôles, les ratés, les coups d'éclat.\n• Les belles images d'Everon.")
                    embed.add_field(name="🤝 L'esprit du salon", inline=False,
                                    value="On rit **avec** les copains, jamais contre eux. Pas de moquerie blessante, pas de règlement de comptes. Un désaccord va dans le salon objections.")
                    embed.add_field(name="💡 Astuce", inline=False,
                                    value="Une ligne de contexte sous ton image, c'est toujours mieux : qui, où, et ce qui s'est passé.")
                    embed.set_footer(text="La PAG · milsim semi-RP armée française")
                    message = await channel.send(embed=embed)
                    try:
                        await message.pin()
                    except discord.HTTPException:
                        pass
            except discord.HTTPException as ex:
                log.warning("création du salon %s impossible : %s", name, ex)
        for role_name in (CFG["logi_role"], CFG["veteran_role"], CFG["instructor_role"]):
            if role_name and not discord.utils.get(g.roles, name=role_name):
                try:
                    await g.create_role(name=role_name, mentionable=False, reason="PAG-Bot : rôle suivi depuis le jeu")
                    log.info("rôle créé : %s", role_name)
                except discord.HTTPException as ex:
                    log.warning("création du rôle %s impossible : %s", role_name, ex)

    # --- présence -------------------------------------------------------------------------------
    async def presence_loop(self):
        await self.wait_until_ready()
        while not self.is_closed():
            try:
                if not BRIDGE.online():
                    activity = discord.Activity(type=discord.ActivityType.watching, name="🔴 serveur injoignable")
                    status = discord.Status.dnd
                else:
                    st = BRIDGE.last_state
                    n = len(st.get("players", []))
                    text = f"🟢 {n}/{CFG['max_players']} en jeu · {len(st.get('missions', []))} mission(s) · {territory_short(st)}"
                    activity = discord.Activity(type=discord.ActivityType.playing, name=text)
                    status = discord.Status.online if n > 0 else discord.Status.idle
                await self.change_presence(activity=activity, status=status)
            except Exception as ex:  # noqa: BLE001
                log.warning("présence : %s", ex)
            await asyncio.sleep(30)

    # --- tableau de bord permanent --------------------------------------------------------------
    # --- boîte d'envoi : annonces déposées sur le serveur du bot --------------------------------
    # Un fichier JSON dans outbox/ = un message à publier, sans que personne d'autre que le bot ait le jeton :
    #   {"channel": "📢┃annonces", "content": "@everyone", "everyone": true,
    #    "embed": {"title": "…", "description": "…", "color": 13214247, "fields": [{"name": "…", "value": "…"}], "footer": "…"}}
    # Publié : le fichier part dans outbox/sent/. Refusé : dans outbox/failed/, avec la raison dans le journal.
    async def outbox_loop(self):
        await self.wait_until_ready()
        box = os.path.join(HERE, "outbox")
        for sub in ("", "sent", "failed"):
            os.makedirs(os.path.join(box, sub), exist_ok=True)
        handled = set()     # garde-fou : un fichier qu'on n'arrive pas à ranger n'est jamais publié deux fois
        while not self.is_closed():
            for name in sorted(os.listdir(box)):
                path = os.path.join(box, name)
                if not name.endswith(".json") or not os.path.isfile(path) or name in handled:
                    continue
                handled.add(name)
                target = "failed"
                try:
                    with open(path, encoding="utf-8") as f:
                        job = json.load(f)
                    ch = self.channel_named(str(job.get("channel", "")))
                    if not ch:
                        raise ValueError(f"salon introuvable : {job.get('channel')}")
                    embed = None
                    if job.get("embed"):
                        e = job["embed"]
                        embed = discord.Embed(title=str(e.get("title", ""))[:256] or None, description=str(e.get("description", ""))[:4096] or None,
                                              color=int(e.get("color", GOLD)))
                        for field in e.get("fields", [])[:25]:
                            embed.add_field(name=str(field.get("name", "\u200b"))[:256], value=str(field.get("value", "\u200b"))[:1024], inline=bool(field.get("inline", False)))
                        if e.get("footer"):
                            embed.set_footer(text=str(e["footer"])[:2048])
                    message = await ch.send(content=str(job.get("content", ""))[:2000] or None, embed=embed,
                                            allowed_mentions=discord.AllowedMentions(everyone=bool(job.get("everyone")), roles=True, users=True))
                    if job.get("pin"):
                        await message.pin()
                    log.info("boîte d'envoi : %s publié dans %s (message %s)", name, ch.name, message.id)
                    target = "sent"
                except Exception as ex:  # noqa: BLE001
                    log.warning("boîte d'envoi : %s refusé : %s", name, ex)
                try:
                    os.replace(path, os.path.join(box, target, f"{int(time.time())}-{name}"))
                except OSError as ex:
                    log.warning("boîte d'envoi : %s non rangé : %s", name, ex)
            await asyncio.sleep(10)

    async def dashboard_loop(self):
        await self.wait_until_ready()
        await asyncio.sleep(8)
        while not self.is_closed():
            try:
                await self.update_dashboard()
            except Exception as ex:  # noqa: BLE001
                log.warning("tableau de bord : %s", ex)
            await asyncio.sleep(max(30, int(CFG["dashboard_seconds"])))

    async def update_dashboard(self):
        ch = self.channel_named(CFG["dashboard_channel"])
        if not ch:
            return
        embed = status_embed(full=True)
        mid = int(STATE.data.get("dashboard_message") or 0)
        if mid:
            try:
                msg = await ch.fetch_message(mid)
                await msg.edit(embed=embed)
                return
            except discord.NotFound:
                pass
        msg = await ch.send(embed=embed)
        STATE.data["dashboard_message"] = msg.id
        STATE.save()

    # --- rôles suivant le jeu -------------------------------------------------------------------
    async def sync_roles(self, state):
        if not CFG.get("sync_roles"):
            return
        g = self.guild()
        if not g:
            return
        grade_roles = CFG["grade_roles"]
        managed = set(grade_roles.values()) | {CFG["instructor_role"], CFG["logi_role"], CFG["veteran_role"]}
        if CFG.get("sync_staff_role"):
            managed.add(CFG["staff_role"])
        by_name = {r.name: r for r in g.roles}
        for p in state.get("players", []):
            did = STATE.discord_for_identity(p.get("identity"))
            if not did:
                continue
            member = await self.member_of(did)
            if not member:
                continue
            wanted = set()
            grade = int(p.get("grade", 0))
            if grade <= 0:
                wanted.add(grade_roles["recrue"])
            elif grade <= 4:
                wanted.add(grade_roles["soldat"])
            elif grade <= 7:
                wanted.add(grade_roles["sous_officier"])
            else:
                wanted.add(grade_roles["officier"])
            if p.get("instructeur") and p["instructeur"] != "aucune":
                wanted.add(CFG["instructor_role"])
            if "logisticien" in str(p.get("certifs", "")).lower():
                wanted.add(CFG["logi_role"])
            if int(p.get("missions", 0)) >= int(CFG["veteran_missions"]):
                wanted.add(CFG["veteran_role"])
            if CFG.get("sync_staff_role") and p.get("staff"):
                wanted.add(CFG["staff_role"])
            current = {r.name for r in member.roles}
            to_add = [by_name[n] for n in wanted - current if n in by_name]
            to_remove = [by_name[n] for n in (managed & current) - wanted if n in by_name]
            try:
                if to_add:
                    await member.add_roles(*to_add, reason="PAG-Bot : suit le jeu")
                if to_remove:
                    await member.remove_roles(*to_remove, reason="PAG-Bot : suit le jeu")
            except discord.Forbidden:
                log.warning("rôles : permission refusée pour %s (le rôle du bot doit être au-dessus des rôles gérés)", member)

    async def handle_links(self, links):
        for raw in links or []:
            parts = raw.split("|")
            if len(parts) != 3:
                continue
            code, identity, name = parts
            entry = BRIDGE.link_codes.pop(code.upper(), None)
            if not entry or entry["expires"] < time.time():
                log.info("liaison refusée, code inconnu ou expiré : %s (%s)", code, name)
                continue
            did = entry["discord_id"]
            STATE.links[str(did)] = {"identity": identity, "name": name}
            STATE.save()
            log.info("liaison : Discord %s <-> %s", did, name)
            user = self.get_user(did)
            if user:
                try:
                    await user.send(f"✅ Ton compte Discord est lié au soldat **{name}**. Tes rôles suivront ton grade en jeu.")
                except discord.HTTPException:
                    pass

    # --- alertes en direct (évènements envoyés par le jeu) --------------------------------------
    async def handle_events(self, events):
        """TOUS les évènements d'un envoi (au plus events_per_sync_max), regroupés par salon et publiés par messages de
        alerts_per_message encadrés (limite de Discord) ; les mentions de rôle d'un message vont ensemble dans son texte.
        Commandement et contrôle vont à ⭐┃commandement, le reste à 🚨┃alertes (Staff)."""
        if not isinstance(events, list) or not events:
            return
        alerts = self.channel_named(CFG["alerts_channel"])
        command = self.channel_named(CFG["command_channel"])
        g = self.guild()
        try:
            most = max(1, int(CFG.get("events_per_sync_max", 60)))
            per_message = max(1, min(10, int(CFG.get("alerts_per_message", 10))))
        except (TypeError, ValueError):
            most, per_message = 60, 10
        groups = {}          # identifiant du salon -> (salon, [(encadré, mention ou None)])
        victory = False
        for ev in events[:most]:
            if not isinstance(ev, dict):
                continue
            kind = str(ev.get("type", "info"))
            victory = victory or kind == "victoire"
            ch = (command or alerts) if kind in COMMAND_EVENTS else alerts
            if not ch:
                continue
            emoji, color, ping_key = ALERT_STYLES.get(kind, ALERT_STYLES["info"])
            text = str(ev.get("text", ""))[:500]          # 10 encadrés de 500 caractères restent sous les 6 000 d'un message
            embed = discord.Embed(description=f"{emoji} {text}", color=color, timestamp=dt.datetime.now(dt.timezone.utc))
            mention = None
            if ping_key and g:
                role = discord.utils.get(g.roles, name=CFG.get(ping_key, ""))
                if role:
                    mention = role.mention
            groups.setdefault(ch.id, (ch, []))[1].append((embed, mention))
        for ch, items in groups.values():
            for start in range(0, len(items), per_message):
                chunk = items[start:start + per_message]
                mentions = []
                for _embed, mention in chunk:
                    if mention and mention not in mentions:
                        mentions.append(mention)
                try:
                    await ch.send(content=" ".join(mentions) or None, embeds=[e for e, _m in chunk], allowed_mentions=discord.AllowedMentions(roles=True))
                except discord.HTTPException as ex:
                    log.warning("alertes non publiées (%d) : %s", len(chunk), ex)
        if victory and CAMP.front.prendre_victoire():
            await self.post_front_image("victoire")      # B3 : l'île toute bleue, avant la remise à zéro de la campagne

    # --- front : image du soir, frise, victoire -------------------------------------------------
    async def front_loop(self):
        """Toutes les 30 s : photos de zone arrivées à échéance ; une fois par jour à front_image_hour:front_image_minute
        (heure de Paris), la photo du jour de la frise puis l'image du soir si le front a bougé ; l'image de la victoire ;
        l'écriture de front.json et frise.json. La date du soir est sauvegardée : pas de doublon au redémarrage."""
        await self.wait_until_ready()
        while not self.is_closed():
            try:
                now = time.time()
                front = CAMP.front
                front.traiter_attente(now)
                if front_actif():
                    paris = dt.datetime.now(PARIS)
                    today = paris.strftime("%Y-%m-%d")
                    hour, minute = int(CFG.get("front_image_hour", 23)), int(CFG.get("front_image_minute", 0))
                    if front.etat.get("soir") != today and (paris.hour, paris.minute) >= (hour, minute):
                        front.etat["soir"] = today
                        front.sale.add("front")
                        front.photo("jour", "", "", now)
                        # « a bougé » = ce que l'image dessine a changé (carrés, zones) ; pas les paquets des dépôts (rev)
                        if CFG.get("front_image_if_unchanged") or front.etat.get("image_sig") != front.signature_image():
                            await self.post_front_image("soir")
                if front.prendre_victoire():
                    await self.post_front_image("victoire")
                front.sauver()
            except Exception as ex:  # noqa: BLE001
                log.warning("front : %s", ex)
            await asyncio.sleep(30)

    async def post_front_image(self, reason):
        """L'image du front dans front_channel : « soir » (chaque soir) ou « victoire » (B3). Juste la carte, sans chiffre
        ni bilan (I7), et le lien vers la carte web. Rend vrai si l'image est partie."""
        ch = self.channel_named(CFG.get("front_channel", ""))
        if not ch:
            log.warning("image du front : salon %s introuvable", CFG.get("front_channel"))
            return False
        if not CAMP.front.a_front():
            return False
        import trace_image
        now = time.time()
        today = dt.datetime.fromtimestamp(now, PARIS).strftime("%d/%m/%Y")
        if reason == "victoire":
            title = "PAG · Everon libérée"
            subtitle = trace_image.sous_titre_front(now, PARIS, "Le front final, le")
            text = "🏆 Everon libérée — le front final"
        else:
            title = "PAG · Le front d'Everon"
            subtitle = trace_image.sous_titre_front(now, PARIS)
            text = f"🗺️ Le front d'Everon, au soir du {today}"
        # révision et empreinte lues AVANT de dessiner : un envoi du jeu arrivé pendant le dessin fera repartir l'image
        rev = CAMP.front.etat.get("rev")
        sig = CAMP.front.signature_image()
        try:
            image = await make_front_image(title, subtitle)
        except Exception as ex:  # noqa: BLE001 — Pillow absent, tuiles manquantes : pas d'image, et pas d'erreur pour le reste
            log.warning("image du front : %s", ex)
            return False
        if not image:
            return False
        base = str(CFG.get("public_url", "")).rstrip("/")
        if base:
            text += f"\n{base}/carte/"
        try:
            await ch.send(content=text, file=discord.File(io.BytesIO(image), filename="front_everon.jpg"), allowed_mentions=discord.AllowedMentions.none())
        except discord.HTTPException as ex:
            log.warning("image du front non publiée (le rôle du bot doit pouvoir écrire et joindre des fichiers dans %s) : %s", ch.name, ex)
            return False
        CAMP.front.etat["image_sig"] = sig        # l'empreinte de ce qui vient d'être dessiné (voir front_loop)
        CAMP.front.etat.pop("image_rev", None)    # ancienne comparaison par révision, remplacée par l'empreinte
        CAMP.front.sale.add("front")
        log.info("image du front publiée (%s, révision %s)", reason, rev)
        return True

    # --- demandes de mission --------------------------------------------------------------------
    async def post_request(self, rid, entry, req):
        ch = self.channel_named(CFG["command_channel"])
        if not ch:
            log.warning("demande %s : salon %s introuvable", rid, CFG["command_channel"])
            return
        content = "📨 **Nouvelle demande de mission**"
        g = self.guild()
        role = discord.utils.get(g.roles, name=CFG["request_ping_role"]) if g and CFG["request_ping_role"] else None
        if role:
            content += f" — {role.mention}"
        try:
            message = await ch.send(content=content, embed=request_embed(req), view=RequestTakeView(),
                                    allowed_mentions=discord.AllowedMentions(roles=True))
            entry["channel_msg"] = message.id
            log.info("demande %s publiée (%s, %s)", rid, req.get("kind"), req.get("title"))
        except discord.HTTPException as ex:
            log.warning("demande %s non publiée : %s", rid, ex)

    async def refresh_request_messages(self, rid, entry, channel_only=False):
        """Aligne le panel du salon commandement et celui de l'officier sur le dernier état connu."""
        req = entry.get("last", {})
        state = req.get("state", "attente")
        embed = request_embed(req, entry.get("officer_id", 0))
        ch = self.channel_named(CFG["command_channel"])
        if ch and entry.get("channel_msg"):
            try:
                view = RequestTakeView(taken=state != "attente") if state in REQUEST_OPEN else None
                await ch.get_partial_message(entry["channel_msg"]).edit(embed=embed, view=view)
            except discord.HTTPException as ex:
                log.warning("demande %s : panel du salon non mis à jour : %s", rid, ex)
        if channel_only or not entry.get("dm_msg"):
            return
        try:
            user = self.get_user(entry["officer_id"]) or await self.fetch_user(entry["officer_id"])
            dm = user.dm_channel or await user.create_dm()
            view = RequestPanelView(state) if state in ("assignee", "accordee") else None
            await dm.get_partial_message(entry["dm_msg"]).edit(embed=embed, view=view)
        except discord.HTTPException as ex:
            log.warning("demande %s : panel de l'officier non mis à jour : %s", rid, ex)

    async def tell_request_officer(self, entry, req):
        """Fin d'une demande : un mot à l'officier qui la suivait."""
        if not entry.get("officer_id"):
            return
        label = REQUEST_STATES.get(req.get("state"), ("close", 0))[0]
        try:
            user = self.get_user(entry["officer_id"]) or await self.fetch_user(entry["officer_id"])
            await user.send(f"📋 Demande `{req.get('id')}` — **{req.get('title', '?')}** : {label}. {req.get('note', '')}"[:1900])
        except discord.HTTPException:
            pass

    async def remind_request(self, rid, entry, req):
        ch = self.channel_named(CFG["command_channel"])
        if not ch or not entry.get("channel_msg"):
            return
        g = self.guild()
        role = discord.utils.get(g.roles, name=CFG["request_ping_role"]) if g and CFG["request_ping_role"] else None
        text = f"⏰ La demande de **{req.get('requester', '?')}** attend toujours un officier."
        if role:
            text += f" {role.mention}"
        try:
            await ch.send(text, reference=ch.get_partial_message(entry["channel_msg"]), allowed_mentions=discord.AllowedMentions(roles=True))
        except discord.HTTPException as ex:
            log.warning("demande %s : relance impossible : %s", rid, ex)

    # --- suivi pour le classement ---------------------------------------------------------------
    def track_stats(self, state):
        seen = STATE.data["seen"]
        for p in state.get("players", []):
            ident = p.get("identity")
            if not ident:
                continue
            seen[ident] = {"name": p.get("name", "?"), "minutes": int(p.get("minutes", 0)), "missions": int(p.get("missions", 0))}

    async def ranking_loop(self):
        await self.wait_until_ready()
        while not self.is_closed():
            try:
                now = dt.datetime.now(PARIS)
                week_id = now.strftime("%G-S%V")
                if not STATE.data.get("week_id"):
                    STATE.data["week_id"] = week_id
                    STATE.data["week_base"] = dict(STATE.data["seen"])
                    STATE.save()
                elif STATE.data["week_id"] != week_id and now.weekday() == int(CFG["ranking_weekday"]) and now.hour >= int(CFG["ranking_hour"]):
                    await self.post_ranking(STATE.data["week_id"])
                    STATE.data["week_id"] = week_id
                    STATE.data["week_base"] = dict(STATE.data["seen"])
                    STATE.save()
            except Exception as ex:  # noqa: BLE001
                log.warning("classement : %s", ex)
            await asyncio.sleep(600)

    async def post_ranking(self, week_label):
        base = STATE.data.get("week_base", {})
        rows = []
        for ident, cur in STATE.data["seen"].items():
            b = base.get(ident, {"minutes": 0, "missions": 0})
            dm = max(0, cur["minutes"] - b.get("minutes", 0))
            dmi = max(0, cur["missions"] - b.get("missions", 0))
            if dm > 0 or dmi > 0:
                rows.append((cur["name"], dm, dmi))
        ch = self.channel_named(CFG["ranking_channel"])
        if not ch or not rows:
            return
        medals = ["🥇", "🥈", "🥉"]
        by_time = sorted(rows, key=lambda r: r[1], reverse=True)[:10]
        embed = discord.Embed(title="🏆 Classement de la semaine", description=f"Semaine {week_label.split('-S')[-1]} à la PAG. Merci à tous ceux qui ont servi.", color=GOLD)
        embed.add_field(name="⏱️ Temps en service", inline=False,
                        value="\n".join(f"{medals[i] if i < 3 else f'`{i + 1:>2}`'} **{n}** · {m // 60} h {m % 60:02d}" for i, (n, m, _) in enumerate(by_time)))
        by_missions = [r for r in sorted(rows, key=lambda r: r[2], reverse=True)[:10] if r[2] > 0]
        if by_missions:
            embed.add_field(name="🎯 Missions réussies", inline=False,
                            value="\n".join(f"{medals[i] if i < 3 else f'`{i + 1:>2}`'} **{n}** · {k}" for i, (n, _, k) in enumerate(by_missions)))
        embed.set_footer(text="La PAG · classement hebdomadaire")
        await ch.send(embed=embed)

    # --- opérations : rappel et clôture ---------------------------------------------------------
    async def operations_loop(self):
        await self.wait_until_ready()
        while not self.is_closed():
            try:
                now = time.time()
                changed = False
                for mid, op in list(STATE.ops.items()):
                    if not op.get("reminded") and 0 < op["ts"] - now <= 1800:
                        op["reminded"] = True
                        changed = True
                        ch = self.channel_named(CFG["operations_channel"])
                        if ch and op["signups"]:
                            mentions = " ".join(f"<@{d}>" for d in op["signups"])
                            await ch.send(f"⏰ **{op['title']}** commence <t:{int(op['ts'])}:R>. Rassemblement à la base, tenue et dotation vérifiées.\n{mentions}",
                                          allowed_mentions=discord.AllowedMentions(users=True))
                    # Tracé de l'opération : une fois commencée depuis 30 min, dès que le serveur est vide (ou à la clôture, 4 h
                    # après le début), l'image des déplacements est postée dans le salon des opérations. Une seule fois.
                    if not op.get("trace") and now - op["ts"] > 1800 and (now - op["ts"] > 4 * 3600 or not (BRIDGE.online() and (BRIDGE.last_state or {}).get("players"))):
                        op["trace"] = True
                        changed = True
                        await self.post_operation_track(op, now)
                    if not op.get("closed") and now - op["ts"] > 4 * 3600:
                        op["closed"] = True
                        changed = True
                        ch = self.channel_named(CFG["operations_channel"])
                        if ch:
                            try:
                                msg = await ch.fetch_message(int(mid))
                                await msg.edit(embed=operation_embed(op), view=None)
                            except discord.HTTPException:
                                pass
                if changed:
                    STATE.save()
                if CAMP.sale:
                    CAMP.sauver()       # ce que le dernier envoi du jeu a laissé en attente (serveur qui vient de se vider)
            except Exception as ex:  # noqa: BLE001
                log.warning("opérations : %s", ex)
            await asyncio.sleep(60)

    async def post_operation_track(self, op, now):
        ch = self.channel_named(CFG["operations_channel"])
        if not ch:
            return
        try:
            image = await make_track_image(op["ts"], now, "PAG · " + str(op.get("title", "Opération"))[:60])
        except Exception as ex:  # noqa: BLE001 — Pillow absent, tuiles manquantes : pas d'image, et pas d'erreur pour le reste
            log.warning("tracé de l'opération : %s", ex)
            return
        if not image:
            return              # personne n'a bougé pendant l'opération : rien à montrer
        base = str(CFG.get("public_url", "")).rstrip("/")
        texte = f"🗺️ **{op.get('title', 'Opération')}** — le tracé des déplacements."
        if base:
            texte += f"\nÀ revoir sur la carte : {base}/carte/?debut={int(op['ts'])}&fin={int(now)}"
        try:
            await ch.send(content=texte, file=discord.File(io.BytesIO(image), filename="trace_operation.jpg"), allowed_mentions=discord.AllowedMentions.none())
        except discord.HTTPException as ex:
            log.warning("tracé de l'opération non publié : %s", ex)

    # --- journal des sanctions ------------------------------------------------------------------
    async def log_sanction(self, author, kind, target, detail, ok, result):
        ch = self.channel_named(CFG["sanctions_channel"])
        if not ch:
            return
        titles = {"avertir": "⚠️ Avertissement", "kick": "👢 Expulsion", "ban": "⛔ Bannissement", "unban": "✅ Bannissement levé"}
        embed = discord.Embed(title=titles.get(kind, kind), color=RED if kind != "unban" else GREEN, timestamp=dt.datetime.now(dt.timezone.utc))
        embed.add_field(name="Soldat", value=f"**{target}**", inline=True)
        embed.add_field(name="Par", value=author, inline=True)
        if detail:
            embed.add_field(name="Motif", value=detail[:1000], inline=False)
        state = "exécuté" if ok else ("en attente" if ok is None else "refusé par le jeu")
        embed.add_field(name="Résultat", value=f"{state} : {result}"[:1000], inline=False)
        try:
            await ch.send(embed=embed)
        except discord.HTTPException:
            pass

    async def feedback(self, text):
        ch = self.channel_named(CFG["feedback_channel"])
        if ch:
            try:
                await ch.send(text[:1900])
            except discord.HTTPException:
                pass


bot = PagBot()


# ------------------------------------------------------------------------------------------------
# Embeds partagés
# ------------------------------------------------------------------------------------------------
def percent_fr(value):
    """8.8 -> « 8,8 % » (écriture française)."""
    try:
        return f"{float(value):.1f}".replace(".", ",") + " %"
    except (TypeError, ValueError):
        return "– %"


def territory_short(st):
    """Le territoire en quelques mots pour la présence : « 8,8 % de l'île » avec le front, sinon (ancien mod) « 4/30 secteurs »."""
    summary = CAMP.front.resume() if front_actif() else None
    if summary:
        return f"{percent_fr(summary['part'])} de l'île"
    terr = (st or {}).get("territory") or {}
    return f"{terr.get('ours', 0)}/{terr.get('total', 0)} {'zones' if 'land' in terr else 'secteurs'}"


def status_embed(full=False):
    if not BRIDGE.online():
        embed = discord.Embed(title="🔴 Serveur de la PAG injoignable", description="Le serveur de jeu ne contacte plus le bot. Redémarrage en cours, ou serveur arrêté.", color=RED)
        embed.set_footer(text="La PAG · tableau de bord")
        embed.timestamp = dt.datetime.now(dt.timezone.utc)
        return embed
    st = BRIDGE.last_state
    players = st.get("players", [])
    terr = st.get("territory", {})
    stocks = st.get("stocks", {})
    embed = discord.Embed(title="🟢 Serveur de la PAG en ligne", color=GOLD)
    embed.add_field(name="👥 Joueurs", value=f"**{len(players)}** / {CFG['max_players']}", inline=True)
    summary = CAMP.front.resume() if front_actif() else None
    if summary:
        front_text = f"**{percent_fr(summary['part'])}** de l'île · {summary['zones_nous']}/{summary['zones_total']} zones · menace {terr.get('threat', 0)}"
        if summary["gele"]:
            front_text += " · front gelé"
        embed.add_field(name="🚩 Front", value=front_text, inline=True)
    else:
        embed.add_field(name="🚩 Territoire", value=f"**{terr.get('ours', 0)}** / {terr.get('total', 0)} {'zones' if 'land' in terr else 'secteurs'} · menace {terr.get('threat', 0)}", inline=True)
    embed.add_field(name="💶 Coffre", value=f"**{st.get('euros', 0)}** €", inline=True)
    embed.add_field(name="📦 Dépôts", value=f"🔫 {stocks.get('munitions', 0)} · ⛽ {stocks.get('carburant', 0)} · 🔧 {stocks.get('pieces', 0)} · 🍞 {stocks.get('vivres', 0)}", inline=False)
    if players:
        lines = [f"`{p.get('gradeName', '?'):<13}` **{p['name']}**" + (" 🛡" if p.get("staff") else "") for p in sorted(players, key=lambda p: -int(p.get("grade", 0)))]
        embed.add_field(name="🪖 En service", value="\n".join(lines[:25])[:1020], inline=False)
    elif full:
        embed.add_field(name="🪖 En service", value="Personne pour le moment.", inline=False)
    missions = st.get("missions", [])
    if missions:
        embed.add_field(name="🎯 Missions disponibles ou en cours", inline=False,
                        value="\n".join(f"**{m['id']}** {m['title']} · {m['site']} · difficulté {m.get('difficulty', '?')}" for m in missions[:10])[:1020])
    if full:
        boxes = st.get("boxes", [])
        if boxes:
            embed.add_field(name="📥 Production à ramasser", value="\n".join(f"**{b['sector']}** · {b['count']} {b['resource']}" for b in boxes[:10])[:1020], inline=False)
        fleet = st.get("fleet", [])
        if fleet:
            embed.add_field(name="🚙 Parc de véhicules", value="\n".join(f"**{v['name']}** · {v.get('state', '?')} · {v.get('grid', '?')}" for v in fleet[:12])[:1020], inline=False)
    embed.set_footer(text=f"La PAG · heure en jeu {st.get('time', '?')} · mis à jour")
    embed.timestamp = dt.datetime.now(dt.timezone.utc)
    return embed


def player_embed(p):
    embed = discord.Embed(title=f"🪖 {p['name']}", color=GOLD)
    embed.add_field(name="Grade", value=p.get("gradeName", "?"), inline=True)
    embed.add_field(name="Temps de service", value=f"{int(p.get('minutes', 0)) // 60} h {int(p.get('minutes', 0)) % 60:02d}", inline=True)
    if "missions" in p:
        target = int(CFG["veteran_missions"])
        done = int(p["missions"])
        bar = "▰" * min(10, done * 10 // target) + "▱" * (10 - min(10, done * 10 // target))
        embed.add_field(name="Missions réussies", value=f"**{done}** · {bar} béret noir à {target}", inline=False)
    embed.add_field(name="Certifications", value=p.get("certifs") or "aucune", inline=False)
    if p.get("instructeur") and p["instructeur"] != "aucune":
        embed.add_field(name="Instructeur de", value=p["instructeur"], inline=False)
    extra = []
    if "infractions" in p:
        extra.append(f"infractions : **{p['infractions']}**")
    if "votes" in p:
        extra.append(f"votes top-serveurs : **{p['votes']}**")
    if p.get("grid"):
        extra.append(f"position : `{p['grid']}`")
    if p.get("staff"):
        extra.append("🛡 Staff")
    if extra:
        embed.add_field(name="Divers", value=" · ".join(extra), inline=False)
    return embed


def operation_embed(op):
    closed = op.get("closed")
    embed = discord.Embed(title=("🔒 " if closed else "📅 ") + op["title"], description=op.get("desc") or None, color=BLUE if not closed else 0x555555)
    embed.add_field(name="Quand", value=f"<t:{int(op['ts'])}:F>\n<t:{int(op['ts'])}:R>", inline=True)
    embed.add_field(name="Inscrits", value=f"**{len(op['signups'])}**", inline=True)
    by_role = {}
    for did, role in op["signups"].items():
        by_role.setdefault(role, []).append(f"<@{did}>")
    for key, label, emoji in OP_ROLES:
        if key in by_role:
            embed.add_field(name=f"{emoji} {label} ({len(by_role[key])})", value=" ".join(by_role[key])[:1020], inline=False)
    footer = "Opération terminée" if closed else "Choisis ton rôle dans le menu ci-dessous · tenue et dotation selon le guide"
    embed.set_footer(text=f"La PAG · organisée par {op.get('author', '?')} · {footer}")
    return embed


# ------------------------------------------------------------------------------------------------
# Droits
# ------------------------------------------------------------------------------------------------
async def roles_of(inter: discord.Interaction):
    # Les droits se lisent TOUJOURS sur le serveur de la PAG : un rôle « Staff » créé sur un autre serveur ne donne rien
    chez_nous = isinstance(inter.user, discord.Member) and inter.guild_id == bot.guild_obj.id
    member = inter.user if chez_nous else await bot.member_of(inter.user.id)
    return {r.name for r in member.roles} if member else set()


async def can_panel(inter):
    return bool(await roles_of(inter) & set(CFG["panel_roles"]))


async def can_ban(inter):
    return bool(await roles_of(inter) & set(CFG["ban_roles"]))


async def display_name(inter):
    member = inter.user if isinstance(inter.user, discord.Member) else await bot.member_of(inter.user.id)
    return member.display_name if member else inter.user.name


# ------------------------------------------------------------------------------------------------
# Panel de gestion
# ------------------------------------------------------------------------------------------------
async def run_from_panel(inter: discord.Interaction, command: str, back_view, sanction=None):
    """Exécute une commande depuis le panel : message d'attente, résultat, puis retour à la vue d'origine."""
    author = await display_name(inter)
    waiting = discord.Embed(description=f"⏳ Envoyé au serveur : `{pretty(command)[:150]}`\nRéponse attendue en quelques secondes…", color=BLUE)
    if inter.response.is_done():
        await inter.edit_original_response(embed=waiting, view=None)
    else:
        await inter.response.edit_message(embed=waiting, view=None)
    ok, message = await game_command(author, command)
    icon = "✅" if ok else ("⏳" if ok is None else "❌")
    result = discord.Embed(description=f"{icon} {message}"[:4000], color=GREEN if ok else RED)
    result.set_footer(text=f"Commande : {pretty(command)[:100]}")
    await inter.edit_original_response(embed=result, view=back_view)
    await bot.feedback(f"{icon} **{author}** (panel) → `{pretty(command)[:200]}` : {message}")
    if sanction:
        await bot.log_sanction(author, sanction[0], sanction[1], sanction[2], ok, message)


class TextModal(ui.Modal):
    """Fenêtre de saisie générique : une liste de champs, puis un rappel avec les valeurs."""

    def __init__(self, title, fields, on_submit):
        super().__init__(title=title[:45], timeout=600)
        self._cb = on_submit
        self.inputs = []
        for label, placeholder, long, required, default in fields:
            item = ui.TextInput(label=label[:45], placeholder=placeholder[:100] or None, required=required, default=default or None,
                                style=discord.TextStyle.paragraph if long else discord.TextStyle.short, max_length=400 if long else 100)
            self.inputs.append(item)
            self.add_item(item)

    async def on_submit(self, inter: discord.Interaction):
        await self._cb(inter, [str(i.value).strip() for i in self.inputs])


class HomeView(ui.View):
    def __init__(self):
        super().__init__(timeout=900)

    @ui.button(label="Joueurs en ligne", emoji="👥", style=discord.ButtonStyle.primary, row=0)
    async def players(self, inter, _):
        await inter.response.edit_message(embed=players_embed(), view=PlayersView())

    @ui.button(label="Joueur hors ligne", emoji="🔎", style=discord.ButtonStyle.secondary, row=0)
    async def offline(self, inter, _):
        async def done(i, values):
            await i.response.edit_message(embed=discord.Embed(title=f"🪖 {values[0]}", description="Joueur hors ligne ou non listé. Les actions ci-dessous s'appliquent à sa fiche.", color=GOLD),
                                          view=PlayerView({"name": values[0]}, offline=True))
        await inter.response.send_modal(TextModal("Joueur hors ligne", [("Pseudo exact en jeu", "Jack Lania", False, True, "")], done))

    @ui.button(label="Serveur", emoji="🖥️", style=discord.ButtonStyle.secondary, row=0)
    async def server(self, inter, _):
        await inter.response.edit_message(embed=status_embed(full=True), view=ServerView())

    @ui.button(label="Actualiser", emoji="🔄", style=discord.ButtonStyle.secondary, row=0)
    async def refresh(self, inter, _):
        await inter.response.edit_message(embed=home_embed(), view=HomeView())


def home_embed():
    embed = status_embed()
    embed.title = "🛠️ Panel de gestion · " + (embed.title or "")
    embed.description = ((embed.description + "\n\n") if embed.description else "") + "Choisis une section. Les actions partent au serveur de jeu et reviennent avec leur résultat."
    return embed


def players_embed():
    players = BRIDGE.players()
    if not players:
        return discord.Embed(title="👥 Joueurs en ligne", description="Personne en ligne, ou serveur injoignable.", color=GOLD)
    lines = [f"`{p.get('gradeName', '?'):<13}` **{p['name']}**" + (" 🛡" if p.get("staff") else "") for p in players]
    return discord.Embed(title=f"👥 Joueurs en ligne · {len(players)}", description="\n".join(lines)[:4000] + "\n\nChoisis un joueur dans le menu.", color=GOLD)


class PlayersView(ui.View):
    def __init__(self):
        super().__init__(timeout=900)
        players = BRIDGE.players()[:25]
        if players:
            select = ui.Select(placeholder="Choisir un joueur…", options=[
                discord.SelectOption(label=p["name"][:100], description=p.get("gradeName", "")[:100], value=str(i)) for i, p in enumerate(players)])
            self._players = players

            async def chosen(inter):
                p = self._players[int(select.values[0])]
                await inter.response.edit_message(embed=player_embed(p), view=PlayerView(p))
            select.callback = chosen
            self.add_item(select)
        back = ui.Button(label="Retour", emoji="◀️", style=discord.ButtonStyle.secondary, row=1)

        async def go_back(inter):
            await inter.response.edit_message(embed=home_embed(), view=HomeView())
        back.callback = go_back
        self.add_item(back)


class PlayerView(ui.View):
    """Fiche d'un joueur et ses actions."""

    def __init__(self, player, offline=False):
        super().__init__(timeout=900)
        self.p = player
        self.name = player["name"]
        self.offline = offline
        caps = BRIDGE.caps()
        if offline:
            for child in list(self.children):
                if getattr(child, "custom_id", "") in ("pv_mp", "pv_kick", "pv_tp", "pv_heal", "pv_etat", "pv_arme"):
                    self.remove_item(child)
        else:
            if "tp" not in caps:
                self.remove_item(self.tp)
            if "soigner" not in caps:
                self.remove_item(self.heal)
            if "etat" not in caps:
                self.remove_item(self.etat)
            if "arme" not in caps:
                self.remove_item(self.arme)

    def again(self):
        return PlayerView(self.p, offline=self.offline)

    def cible(self):
        return target_token(self.name, self.p.get("identity", ""))

    @ui.button(label="Avertir", emoji="⚠️", style=discord.ButtonStyle.primary, row=0)
    async def warn(self, inter, _):
        async def done(i, v):
            await run_from_panel(i, f"avertir {self.cible()} {v[0]}", self.again(), ("avertir", self.name, v[0]))
        await inter.response.send_modal(TextModal(f"Avertir {self.name}", [("Motif", "Tir ami volontaire", True, True, "")], done))

    @ui.button(label="Message privé", emoji="✉️", style=discord.ButtonStyle.primary, row=0, custom_id="pv_mp")
    async def mp(self, inter, _):
        async def done(i, v):
            await run_from_panel(i, f"mp {self.cible()} {v[0]}", self.again())
        await inter.response.send_modal(TextModal(f"Message à {self.name}", [("Message", "", True, True, "")], done))

    @ui.button(label="Expulser", emoji="👢", style=discord.ButtonStyle.danger, row=0, custom_id="pv_kick")
    async def kick(self, inter, _):
        if not await can_ban(inter):
            return await inter.response.send_message("Expulser est réservé au Staff.", ephemeral=True)

        async def done(i, v):
            await run_from_panel(i, f"kick {self.cible()} {v[0]}", self.again(), ("kick", self.name, v[0]))
        await inter.response.send_modal(TextModal(f"Expulser {self.name}", [("Motif", "", True, True, "")], done))

    @ui.button(label="Bannir", emoji="⛔", style=discord.ButtonStyle.danger, row=0)
    async def ban(self, inter, _):
        if not await can_ban(inter):
            return await inter.response.send_message("Bannir est réservé au Staff.", ephemeral=True)

        async def done(i, v):
            days = v[0] if v[0].isdigit() else "0"
            label = "définitif" if days == "0" else f"{days} jour(s)"
            await run_from_panel(i, f"ban {self.cible()} {days} {v[1]}", self.again(), ("ban", self.name, f"{label} · {v[1]}"))
        await inter.response.send_modal(TextModal(f"Bannir {self.name}", [("Durée en jours (0 = définitif)", "7", False, True, "0"), ("Motif", "", True, True, "")], done))

    @ui.button(label="Lever un ban", emoji="✅", style=discord.ButtonStyle.secondary, row=1)
    async def unban(self, inter, _):
        if not await can_ban(inter):
            return await inter.response.send_message("Réservé au Staff.", ephemeral=True)
        await run_from_panel(inter, f"unban {self.cible()}", self.again(), ("unban", self.name, ""))

    @ui.button(label="Grade", emoji="🎖️", style=discord.ButtonStyle.secondary, row=1)
    async def grade(self, inter, _):
        await inter.response.edit_message(view=ChoiceView(self, [("Nouveau grade…", [(GRADE_LABELS[i], GRADES[i]) for i in range(len(GRADES))])],
                                                         lambda value: f"grade {self.cible()} {value}"))

    @ui.button(label="Certification", emoji="📜", style=discord.ButtonStyle.secondary, row=1)
    async def certif(self, inter, _):
        menus = [("Accorder une certification…", [(label, f"{key} on") for key, label in CERTIFS]),
                 ("Retirer une certification…", [(label, f"{key} off") for key, label in CERTIFS])]
        await inter.response.edit_message(view=ChoiceView(self, menus, lambda value: f"certif {self.cible()} {value}"))

    @ui.button(label="Instructeur", emoji="🎓", style=discord.ButtonStyle.secondary, row=1)
    async def instructor(self, inter, _):
        menus = [("Habiliter comme instructeur de…", [(label, f"{key} on") for key, label in CERTIFS]),
                 ("Retirer l'habilitation de…", [(label, f"{key} off") for key, label in CERTIFS])]
        await inter.response.edit_message(view=ChoiceView(self, menus, lambda value: f"instructeur {self.cible()} {value}"))

    @ui.button(label="Téléporter à la base", emoji="🧭", style=discord.ButtonStyle.secondary, row=2, custom_id="pv_tp")
    async def tp(self, inter, _):
        await run_from_panel(inter, f"tp {self.cible()} base", self.again())

    @ui.button(label="Soigner", emoji="⛑️", style=discord.ButtonStyle.secondary, row=2, custom_id="pv_heal")
    async def heal(self, inter, _):
        await run_from_panel(inter, f"soigner {self.cible()}", self.again())

    @ui.button(label="État", emoji="🩺", style=discord.ButtonStyle.secondary, row=2, custom_id="pv_etat")
    async def etat(self, inter, _):
        await run_from_panel(inter, f"etat {self.cible()}", self.again())

    @ui.button(label="Recréer l'arme", emoji="🔧", style=discord.ButtonStyle.secondary, row=2, custom_id="pv_arme")
    async def arme(self, inter, _):
        await run_from_panel(inter, f"arme-recreer {self.cible()}", self.again())

    @ui.button(label="Fiche complète", emoji="📄", style=discord.ButtonStyle.secondary, row=2)
    async def info(self, inter, _):
        await run_from_panel(inter, f"info {self.cible()}", self.again())

    @ui.button(label="Retour", emoji="◀️", style=discord.ButtonStyle.secondary, row=3)
    async def back(self, inter, _):
        await inter.response.edit_message(embed=players_embed(), view=PlayersView())


class ChoiceView(ui.View):
    """Un ou plusieurs menus déroulants, puis exécution de la commande construite à partir du choix."""

    def __init__(self, origin: PlayerView, menus, build):
        super().__init__(timeout=600)
        for placeholder, options in menus:
            self.add_item(self._menu(origin, placeholder, options, build))
        cancel = ui.Button(label="Annuler", emoji="◀️", style=discord.ButtonStyle.secondary)

        async def go_back(inter):
            await inter.response.edit_message(view=origin.again())
        cancel.callback = go_back
        self.add_item(cancel)

    @staticmethod
    def _menu(origin, placeholder, options, build):
        select = ui.Select(placeholder=placeholder, options=[discord.SelectOption(label=l[:100], value=v) for l, v in options[:25]])

        async def chosen(inter):
            await run_from_panel(inter, build(select.values[0]), origin.again())
        select.callback = chosen
        return select


class ServerView(ui.View):
    def __init__(self):
        super().__init__(timeout=900)
        caps = BRIDGE.caps()
        if "heure" not in caps:
            self.remove_item(self.hour)
        if "mission" not in caps:
            self.remove_item(self.mission)
        if "radio" not in caps:
            self.remove_item(self.radio)

    @ui.button(label="Annonce à tous", emoji="📢", style=discord.ButtonStyle.primary, row=0)
    async def say(self, inter, _):
        async def done(i, v):
            await run_from_panel(i, f"dire {v[0]}", ServerView())
        await inter.response.send_modal(TextModal("Annonce à tous les joueurs", [("Message affiché en jeu", "", True, True, "")], done))

    @ui.button(label="Sauvegarder", emoji="💾", style=discord.ButtonStyle.secondary, row=0)
    async def save(self, inter, _):
        await run_from_panel(inter, "sauver", ServerView())

    @ui.button(label="Changer l'heure", emoji="🕐", style=discord.ButtonStyle.secondary, row=0)
    async def hour(self, inter, _):
        async def done(i, v):
            await run_from_panel(i, f"heure {v[0]}", ServerView())
        await inter.response.send_modal(TextModal("Heure en jeu", [("Heure, de 0 à 23 (ex. 6 ou 21.5)", "8", False, True, "")], done))

    @ui.button(label="Missions", emoji="🎯", style=discord.ButtonStyle.secondary, row=0)
    async def mission(self, inter, _):
        missions = (BRIDGE.last_state or {}).get("missions", [])[:12]
        options = [("Créer une nouvelle mission", "creer")]
        for m in missions:
            options.append((f"Annuler {m['id']} · {m['title']}"[:100], f"annuler {m['id']}"))
            options.append((f"Réussir {m['id']} · {m['title']}"[:100], f"reussir {m['id']}"))
        view = ui.View(timeout=600)
        select = ui.Select(placeholder="Action sur les missions…", options=[discord.SelectOption(label=l, value=v) for l, v in options[:25]])

        async def chosen(i):
            await run_from_panel(i, f"mission {select.values[0]}", ServerView())
        select.callback = chosen
        view.add_item(select)
        await inter.response.edit_message(view=view)

    @ui.button(label="Diagnostic radio", emoji="📻", style=discord.ButtonStyle.secondary, row=1)
    async def radio(self, inter, _):
        await run_from_panel(inter, "radio", ServerView())

    @ui.button(label="Retour", emoji="◀️", style=discord.ButtonStyle.secondary, row=1)
    async def back(self, inter, _):
        await inter.response.edit_message(embed=home_embed(), view=HomeView())


# ------------------------------------------------------------------------------------------------
# Opérations : inscription par menu (vue persistante, survit au redémarrage du bot)
# ------------------------------------------------------------------------------------------------
class OperationView(ui.View):
    def __init__(self):
        super().__init__(timeout=None)

    @ui.select(custom_id="pag:op:role", placeholder="M'inscrire avec le rôle…",
               options=[discord.SelectOption(label=label, value=key, emoji=emoji) for key, label, emoji in OP_ROLES])
    async def choose(self, inter: discord.Interaction, select: ui.Select):
        op = STATE.ops.get(str(inter.message.id))
        if not op or op.get("closed"):
            return await inter.response.send_message("Cette opération est terminée.", ephemeral=True)
        op["signups"][str(inter.user.id)] = select.values[0]
        STATE.save()
        await inter.response.edit_message(embed=operation_embed(op), view=OperationView())

    @ui.button(custom_id="pag:op:leave", label="Me désinscrire", emoji="🚪", style=discord.ButtonStyle.secondary)
    async def leave(self, inter: discord.Interaction, _):
        op = STATE.ops.get(str(inter.message.id))
        if not op:
            return await inter.response.send_message("Opération introuvable.", ephemeral=True)
        op["signups"].pop(str(inter.user.id), None)
        STATE.save()
        await inter.response.edit_message(embed=operation_embed(op), view=OperationView())


# ------------------------------------------------------------------------------------------------
# Demandes de formation
# ------------------------------------------------------------------------------------------------
class FormationView(ui.View):
    def __init__(self):
        super().__init__(timeout=None)

    @ui.button(custom_id="pag:form:take", label="Je prends cette formation", emoji="🎓", style=discord.ButtonStyle.success)
    async def take(self, inter: discord.Interaction, _):
        roles = await roles_of(inter)
        if not roles & ({CFG["instructor_role"]} | set(CFG["panel_roles"])):
            return await inter.response.send_message("Réservé aux instructeurs et à l'encadrement.", ephemeral=True)
        embed = inter.message.embeds[0]
        embed.color = GREEN
        embed.add_field(name="Prise en charge", value=f"{inter.user.mention} · <t:{int(time.time())}:R>", inline=False)
        await inter.response.edit_message(embed=embed, view=None)
        requester = None
        for f in embed.fields:
            if f.name == "Demandeur":
                requester = f.value.strip("<@!>")
        if requester and requester.isdigit():
            user = bot.get_user(int(requester)) or await bot.fetch_user(int(requester))
            try:
                await user.send(f"🎓 Ta demande de formation est prise en charge par **{inter.user.display_name}**. Il te contactera pour fixer un créneau.")
            except discord.HTTPException:
                pass


# ------------------------------------------------------------------------------------------------
# Demandes de mission (suivi de mission)
# Le jeu envoie la liste « requests » à chaque contact. Pour chaque demande : un panel dans le salon
# commandement, avec le bouton « Prendre l'assignation de la mission ». L'officier qui clique reçoit en
# message privé le panel de décision : accorder (transport à pied ou motorisé, véhicules présents à la base,
# consignes), refuser (motif), rendre l'assignation, clôturer l'opération. Le jeu reste l'arbitre : chaque
# bouton envoie une commande « demande … », et les messages suivent l'état renvoyé par le jeu.
# ------------------------------------------------------------------------------------------------
REQUEST_LOCK = asyncio.Lock()
REQUEST_STATES = {   # état envoyé par le jeu -> (libellé, couleur)
    "attente": ("🟡 En attente d'un officier", GOLD), "assignee": ("🔵 Examinée par un officier", BLUE),
    "accordee": ("🟢 Accordée — opération en cours", GREEN), "refusee": ("🔴 Refusée", RED),
    "annulee": ("⚪ Annulée", 0x7F8C8D), "reussie": ("🏆 Réussie", GREEN), "echouee": ("⚫ Close sans succès", RED),
}
REQUEST_OPEN = ("attente", "assignee", "accordee")


def request_id_of(message):
    """L'identifiant de la demande, écrit dans le pied de l'embed : « Demande D123 »."""
    if not message or not message.embeds or not message.embeds[0].footer or not message.embeds[0].footer.text:
        return ""
    words = message.embeds[0].footer.text.split()
    return words[1] if len(words) >= 2 and words[0] == "Demande" else ""


def request_embed(req, officer_id=0):
    state = str(req.get("state", "attente"))
    label, color = REQUEST_STATES.get(state, REQUEST_STATES["attente"])
    territory = req.get("kind") == "territoire"
    title = ("🚩 Demande de capture : " if territory else "🎯 Demande de mission : ") + str(req.get("title", "?"))
    embed = discord.Embed(title=title[:250], color=color)
    embed.add_field(name="Type", value="Capture d'une zone ennemie" if territory else "Mission du tableau", inline=True)
    embed.add_field(name="Carré", value=f"`{req.get('grid') or '?'}`", inline=True)
    embed.add_field(name="État", value=label, inline=True)
    if req.get("details"):
        embed.add_field(name="Détails", value=str(req["details"])[:1000], inline=False)
    embed.add_field(name="Demandeur", value=str(req.get("requester", "?"))[:200], inline=True)
    members = [str(m) for m in req.get("members", [])]
    embed.add_field(name=f"Effectif ({len(members)})", value=("\n".join("• " + m for m in members) or "—")[:1000], inline=False)
    officer = str(req.get("officer", ""))
    if officer:
        embed.add_field(name="Officier", value=(f"<@{officer_id}>" if officer_id else officer)[:200], inline=True)
    if req.get("transport"):
        embed.add_field(name="Transport autorisé", value=str(req["transport"])[:1000], inline=False)
    if req.get("note"):
        note_label = {"accordee": "Consignes", "refusee": "Motif du refus"}.get(state, "Issue")
        embed.add_field(name=note_label, value=str(req["note"])[:1000], inline=False)
    embed.set_footer(text=f"Demande {req.get('id', '?')} · reçue à {str(req.get('created', ''))[11:16] or '?'} (heure du serveur)")
    return embed


async def can_take_request(inter):
    return bool(await roles_of(inter) & set(CFG["request_roles"]))


class RequestTakeView(ui.View):
    """Sous le panel du salon commandement."""

    def __init__(self, taken=False):
        super().__init__(timeout=None)
        self.take.disabled = taken

    @ui.button(custom_id="pag:req:take", label="Prendre l'assignation de la mission", emoji="🎖️", style=discord.ButtonStyle.success)
    async def take(self, inter: discord.Interaction, _):
        if not await can_take_request(inter):
            return await inter.response.send_message("Réservé aux officiers et au Staff.", ephemeral=True)
        rid = request_id_of(inter.message)
        if not rid:
            return await inter.response.send_message("Demande illisible.", ephemeral=True)
        await inter.response.defer(ephemeral=True, thinking=True)
        author = await display_name(inter)

        entry = STATE.data["requests"].get(rid)
        if not entry or entry.get("closed"):
            return await inter.followup.send("Cette demande est close.", ephemeral=True)
        if entry.get("officer_id") and entry["officer_id"] != inter.user.id:
            return await inter.followup.send(f"Déjà prise par <@{entry['officer_id']}>.", ephemeral=True)
        ok, message = await game_command(author, f"demande assigner {rid}")     # le jeu arbitre : un seul officier par demande
        if not ok:
            return await inter.followup.send(f"❌ {message}", ephemeral=True)

        async with REQUEST_LOCK:
            req = dict(entry.get("last", {}))
            req.update({"state": "assignee", "officer": author})
            try:
                dm = await inter.user.send(content="🎖️ Tu as pris l'assignation de cette demande. À toi de décider :",
                                           embed=request_embed(req, inter.user.id), view=RequestPanelView("assignee"))
            except discord.HTTPException:
                await game_command(author, f"demande liberer {rid}")
                return await inter.followup.send("Je ne peux pas t'écrire en message privé : autorise les messages privés de ce serveur "
                                                 "(clic droit sur le serveur → Paramètres de confidentialité), puis reprends l'assignation.", ephemeral=True)
            entry.update({"officer_id": inter.user.id, "dm_channel": dm.channel.id, "dm_msg": dm.id, "last": req, "state": "assignee"})
            STATE.save()
            await bot.refresh_request_messages(rid, entry, channel_only=True)
        await inter.followup.send("Assignation prise : le panel de décision t'attend en message privé.", ephemeral=True)
        await bot.feedback(f"🎖️ **{author}** prend l'assignation de la demande `{rid}`")


class RequestPanelView(ui.View):
    """Panel de décision, en message privé chez l'officier."""

    def __init__(self, state="assignee"):
        super().__init__(timeout=None)
        deciding = state == "assignee"
        self.grant.disabled = not deciding
        self.deny.disabled = not deciding
        self.release.disabled = not deciding
        self.close_op.disabled = state != "accordee"

    async def _entry(self, inter):
        rid = request_id_of(inter.message)
        entry = STATE.data["requests"].get(rid)
        if not rid or not entry or entry.get("closed"):
            await inter.response.send_message("Cette demande est close.", ephemeral=True)
            return None, None
        if entry.get("officer_id") != inter.user.id or not await can_take_request(inter):
            await inter.response.send_message("Cette demande n'est pas la tienne.", ephemeral=True)
            return None, None
        return rid, entry

    @ui.button(custom_id="pag:req:grant", label="Accorder", emoji="✅", style=discord.ButtonStyle.success, row=0)
    async def grant(self, inter: discord.Interaction, _):
        rid, entry = await self._entry(inter)
        if not rid:
            return
        vehicles = [v for v in (BRIDGE.last_state or {}).get("fleet", []) if v.get("base")] if BRIDGE.online() else []
        embed = discord.Embed(title="✅ Accorder — transport autorisé", color=GREEN,
                              description="1. Choisis **à pied** ou **motorisé**.\n2. En motorisé, coche les véhicules autorisés, parmi ceux garés à la base.\n"
                                          "3. **Valider** : tu peux ajouter des consignes, puis la décision part au groupe en jeu.")
        embed.set_footer(text=f"Demande {rid}")
        await inter.response.send_message(embed=embed, view=TransportView(rid, vehicles))

    @ui.button(custom_id="pag:req:deny", label="Refuser", emoji="⛔", style=discord.ButtonStyle.danger, row=0)
    async def deny(self, inter: discord.Interaction, _):
        rid, entry = await self._entry(inter)
        if not rid:
            return

        async def submit(modal_inter, values):
            await send_request_decision(modal_inter, f"demande refuser {rid} {values[0] or 'sans motif'}", "⛔ Demande refusée, le soldat est prévenu en jeu.")
        await inter.response.send_modal(TextModal("Refuser la demande", [("Motif, envoyé au soldat en jeu", "Effectif insuffisant, zone trop chaude…", True, True, "")], submit))

    @ui.button(custom_id="pag:req:release", label="Rendre l'assignation", emoji="↩️", style=discord.ButtonStyle.secondary, row=1)
    async def release(self, inter: discord.Interaction, _):
        rid, entry = await self._entry(inter)
        if not rid:
            return
        await inter.response.defer()
        author = await display_name(inter)
        ok, message = await game_command(author, f"demande liberer {rid}")
        async with REQUEST_LOCK:
            if ok:
                entry.update({"officer_id": 0, "dm_channel": 0, "dm_msg": 0, "state": "attente"})
                entry.setdefault("last", {}).update({"state": "attente", "officer": ""})
                STATE.save()
                await bot.refresh_request_messages(rid, entry, channel_only=True)
        if ok:
            await inter.edit_original_response(content="↩️ Assignation rendue : la demande est de nouveau ouverte dans le salon commandement.", view=None)
        else:
            await inter.followup.send(f"❌ {message}")

    @ui.button(custom_id="pag:req:close", label="Clôturer l'opération", emoji="🏁", style=discord.ButtonStyle.secondary, row=1)
    async def close_op(self, inter: discord.Interaction, _):
        rid, entry = await self._entry(inter)
        if not rid:
            return

        async def submit(modal_inter, values):
            await send_request_decision(modal_inter, f"demande clore {rid} {values[0]}", "🏁 Opération close, le groupe est prévenu en jeu.")
        await inter.response.send_modal(TextModal("Clôturer l'opération", [("Raison, envoyée au groupe en jeu", "Repli ordonné, fin de soirée…", True, True, "")], submit))


async def send_request_decision(inter, command, success_text):
    """Envoie une décision au jeu et répond à l'officier ; les panels suivent au prochain contact du jeu."""
    await inter.response.defer(thinking=True)
    author = await display_name(inter)
    ok, message = await game_command(author, command)
    if ok:
        await inter.followup.send(success_text)
    else:
        await inter.followup.send(f"{'⏳' if ok is None else '❌'} {message}")
    await bot.feedback(f"{'✅' if ok else '❌'} **{author}** (demande de mission) → `{command[:200]}` : {message}")


class TransportView(ui.View):
    """Choix du transport, puis consignes : dernier écran avant d'accorder."""

    def __init__(self, rid, vehicles):
        super().__init__(timeout=900)
        self.rid = rid
        self.motorised = False
        self.chosen = []
        mode = ui.Select(placeholder="Mode de transport", row=0, options=[
            discord.SelectOption(label="À pied", value="pied", emoji="🥾", default=True),
            discord.SelectOption(label="Motorisé", value="moto", emoji="🚙")])
        mode.callback = self.pick_mode
        self.add_item(mode)
        self.mode_select = mode
        options = [discord.SelectOption(label=f"{v.get('name', '?')} n° {v.get('id', '?')}"[:100], value=str(v.get("id", ""))[:100],
                                        description=f"carré {v.get('grid', '?')}"[:100]) for v in vehicles[:25] if v.get("id")]
        self.vehicle_select = None
        if options:
            sel = ui.Select(placeholder="Véhicules autorisés (en motorisé)", row=1, min_values=0, max_values=len(options), options=options)
            sel.callback = self.pick_vehicles
            self.add_item(sel)
            self.vehicle_select = sel
        self.no_vehicle = not options

    async def pick_mode(self, inter: discord.Interaction):
        self.motorised = self.mode_select.values[0] == "moto"
        for opt in self.mode_select.options:
            opt.default = opt.value == self.mode_select.values[0]
        note = None
        if self.motorised and self.no_vehicle:
            note = "⚠️ Aucun véhicule n'est garé à la base en ce moment : le motorisé sera accordé sans véhicule désigné."
        await inter.response.edit_message(content=note, view=self)

    async def pick_vehicles(self, inter: discord.Interaction):
        self.chosen = list(self.vehicle_select.values)
        for opt in self.vehicle_select.options:
            opt.default = opt.value in self.chosen
        await inter.response.defer()

    @ui.button(label="Valider", emoji="✅", style=discord.ButtonStyle.success, row=2)
    async def confirm(self, inter: discord.Interaction, _):
        rid = self.rid
        mode = "moto" if self.motorised else "pied"
        vehicles = ",".join(self.chosen) if self.motorised and self.chosen else "-"
        view = self

        async def submit(modal_inter, values):
            command = f"demande accorder {rid} {mode} {vehicles} {values[0]}".strip()
            await send_request_decision(modal_inter, command, "✅ Demande accordée : le groupe reçoit la décision, le transport et les consignes en jeu.")
            for item in view.children:
                item.disabled = True
            try:
                await inter.message.edit(view=view)
            except discord.HTTPException:
                pass
        await inter.response.send_modal(TextModal("Consignes (facultatif)", [("Consignes envoyées au groupe en jeu", "Itinéraire, règles d'engagement, heure de retour…", True, False, "")], submit))

    @ui.button(label="Annuler", style=discord.ButtonStyle.secondary, row=2)
    async def cancel(self, inter: discord.Interaction, _):
        await inter.response.edit_message(content="Rien n'a été envoyé. Le panel de décision reste au-dessus.", embed=None, view=None)


async def handle_requests(state):
    """À chaque contact du jeu : publie les nouvelles demandes, met à jour les panels, relance, clôt."""
    if "demande" not in state.get("caps", []):
        return
    async with REQUEST_LOCK:
        if state is not BRIDGE.last_state:
            return      # un état plus récent est arrivé pendant l'attente du verrou : lui seul fait foi
        known = STATE.data["requests"]
        changed = False
        seen = set()
        for req in state.get("requests", []):
            rid = str(req.get("id", ""))
            if not rid:
                continue
            seen.add(rid)
            entry = known.get(rid)
            if entry is None:
                if req.get("state") not in REQUEST_OPEN:
                    continue
                entry = {"channel_msg": 0, "officer_id": 0, "dm_channel": 0, "dm_msg": 0, "posted_at": time.time(), "reminded": False,
                         "closed": False, "state": "", "last": {}}
                known[rid] = entry
                await bot.post_request(rid, entry, req)
                entry["last"] = req
                entry["state"] = req.get("state", "")
                changed = True
                continue
            if entry.get("closed"):
                continue
            previous = entry.get("last", {})
            moved = any(previous.get(k) != req.get(k) for k in ("state", "officer", "note", "transport"))
            entry["last"] = req
            entry["state"] = req.get("state", "")
            if moved:
                if req.get("state") == "attente":
                    entry.update({"officer_id": 0})
                await bot.refresh_request_messages(rid, entry)
                if req.get("state") not in REQUEST_OPEN:
                    entry["closed"] = True
                    entry["closed_at"] = time.time()
                    await bot.tell_request_officer(entry, req)
                changed = True
            elif (req.get("state") == "attente" and not entry.get("reminded")
                  and time.time() - entry.get("posted_at", 0) > 60 * int(CFG["request_reminder_minutes"])):
                entry["reminded"] = True
                await bot.remind_request(rid, entry, req)
                changed = True

        # Demandes ouvertes que le jeu ne connaît plus : le serveur a redémarré
        for rid, entry in known.items():
            if entry.get("closed") or rid in seen:
                continue
            req = dict(entry.get("last", {}))
            req.update({"state": "annulee", "note": "Le serveur de jeu a redémarré : la demande n'existe plus."})
            entry.update({"last": req, "state": "annulee", "closed": True, "closed_at": time.time()})
            await bot.refresh_request_messages(rid, entry)
            changed = True
        for rid in [r for r, e in known.items() if e.get("closed") and time.time() - e.get("closed_at", 0) > 7 * 86400]:
            del known[rid]
            changed = True
        if changed:
            STATE.save()


async def handle_requests_safe(state):
    try:
        await handle_requests(state)
    except Exception:  # noqa: BLE001
        log.exception("demandes de mission : traitement interrompu")


# ------------------------------------------------------------------------------------------------
# /soins : fiche de soins interactive — /guide : les guides de la PAG
# Tout le savoir (formulaire, moteur « Quoi faire ? », chapitres) est dans medical.py, sans Discord.
# Ici : l'habillage. Une fiche = un message, trois pages de menus déroulants, puis le plan d'action.
# ------------------------------------------------------------------------------------------------
PIED_PAG = "La PAG · milsim semi-RP armée française"


def care_embed(fiche, page):
    titre_page = medical.PAGES[page][0]
    embed = discord.Embed(title="🩺 Fiche de soins", color=GOLD,
                          description="Renseigne **ce que tu vois** sur le blessé avec les menus ci-dessous, dans l'ordre que tu veux. "
                                      "Puis appuie sur **🧭 Quoi faire ?** : je te rends les gestes, dans l'ordre.\n"
                                      "Ce que tu ne sais pas encore, laisse-le : je te dirai quoi regarder.")
    lignes = medical.resume(fiche)
    n = 0
    for i, (nom, champs) in enumerate(medical.PAGES):
        bloc = "\n".join(f"**{t}** : {v}" for t, v in lignes[n:n + len(champs)])
        n += len(champs)
        embed.add_field(name=("▶ " if i == page else "") + f"{i + 1}. {nom}", value=bloc[:1024], inline=False)
    embed.set_footer(text=f"Page {page + 1}/{len(medical.PAGES)} : {titre_page} · {PIED_PAG}")
    return embed


def plan_embed(fiche):
    p = medical.plan(fiche)
    corps = "\n\n".join(f"**{i}.** {medical.QUI[q]} {t}" for i, (q, t) in enumerate(p["etapes"], 1))
    embed = discord.Embed(title=f"🧭 Quoi faire — {p['titre']}", description=corps[:4096], color=p["couleur"])
    if p["constat"]:
        embed.insert_field_at(0, name="Ce que ça veut dire", value="\n".join(p["constat"])[:1024], inline=False)
    if p["manque"]:
        embed.add_field(name="🔎 À regarder encore", value="\n".join("• " + x for x in p["manque"])[:1024], inline=False)
    if p["rappels"]:
        embed.add_field(name="⚠️ À ne pas oublier", value="\n".join("• " + x for x in p["rappels"])[:1024], inline=False)
    embed.add_field(name="Qui peut le faire", value="🪖 tout soldat · ⛑️ infirmier · 🩺 médecin", inline=False)
    embed.set_footer(text=f"Plan établi pour : {medical.QUI_NOM[p['role']]} · l'état change vite, refais la fiche après tes gestes · {PIED_PAG}")
    return embed


class CareView(ui.View):
    """Les menus d'une page de la fiche de soins."""

    def __init__(self, owner_id, fiche=None, page=0):
        super().__init__(timeout=1800)
        self.owner_id = owner_id
        self.fiche = fiche or medical.nouvelle_fiche()
        self.page = page
        for row, (cle, titre, multiple, options) in enumerate(medical.PAGES[page][1]):
            actuel = self.fiche.get(cle)
            choisi = set(actuel) if multiple else {actuel}
            select = ui.Select(placeholder=titre[:150], row=row, min_values=0 if multiple else 1, max_values=len(options) if multiple else 1,
                               options=[discord.SelectOption(label=lib[:100], value=v, emoji=e, default=v in choisi) for v, lib, e in options])
            select.callback = self._changer(cle, multiple, select)
            self.add_item(select)
        self.precedent.disabled = page == 0
        self.suivant.disabled = page == len(medical.PAGES) - 1

    async def interaction_check(self, inter: discord.Interaction):
        if inter.user.id != self.owner_id:
            await inter.response.send_message("Cette fiche n'est pas la tienne : ouvre la tienne avec `/soins`.", ephemeral=True)
            return False
        return True

    def _changer(self, cle, multiple, select):
        async def callback(inter: discord.Interaction):
            if multiple:
                valeurs = list(select.values)
                neutres = {"aucun", "rien"}
                if len(valeurs) > 1:      # « aucun » ne se combine pas : le dernier ajout l'emporte
                    avant = set(self.fiche.get(cle, []))
                    ajoutes = [v for v in valeurs if v not in avant]
                    valeurs = [v for v in valeurs if v in neutres] if (ajoutes and ajoutes[0] in neutres) else [v for v in valeurs if v not in neutres]
                self.fiche[cle] = valeurs
            else:
                self.fiche[cle] = select.values[0]
            await inter.response.edit_message(embed=care_embed(self.fiche, self.page), view=CareView(self.owner_id, self.fiche, self.page))
        return callback

    @ui.button(label="Page précédente", emoji="◀️", style=discord.ButtonStyle.secondary, row=4)
    async def precedent(self, inter: discord.Interaction, _):
        await inter.response.edit_message(embed=care_embed(self.fiche, self.page - 1), view=CareView(self.owner_id, self.fiche, self.page - 1))

    @ui.button(label="Page suivante", emoji="▶️", style=discord.ButtonStyle.secondary, row=4)
    async def suivant(self, inter: discord.Interaction, _):
        await inter.response.edit_message(embed=care_embed(self.fiche, self.page + 1), view=CareView(self.owner_id, self.fiche, self.page + 1))

    @ui.button(label="Quoi faire ?", emoji="🧭", style=discord.ButtonStyle.success, row=4)
    async def quoi_faire(self, inter: discord.Interaction, _):
        await inter.response.edit_message(embed=plan_embed(self.fiche), view=PlanView(self.owner_id, self.fiche, self.page))

    @ui.button(label="Tout effacer", emoji="♻️", style=discord.ButtonStyle.danger, row=4)
    async def effacer(self, inter: discord.Interaction, _):
        fiche = medical.nouvelle_fiche()
        fiche["role"] = self.fiche.get("role", medical.SOLDAT)
        await inter.response.edit_message(embed=care_embed(fiche, 0), view=CareView(self.owner_id, fiche, 0))


class PlanView(ui.View):
    """Sous le plan d'action."""

    def __init__(self, owner_id, fiche, page):
        super().__init__(timeout=1800)
        self.owner_id, self.fiche, self.page = owner_id, fiche, page

    async def interaction_check(self, inter: discord.Interaction):
        if inter.user.id != self.owner_id:
            await inter.response.send_message("Cette fiche n'est pas la tienne : ouvre la tienne avec `/soins`.", ephemeral=True)
            return False
        return True

    @ui.button(label="Mettre la fiche à jour", emoji="✏️", style=discord.ButtonStyle.primary)
    async def modifier(self, inter: discord.Interaction, _):
        await inter.response.edit_message(embed=care_embed(self.fiche, self.page), view=CareView(self.owner_id, self.fiche, self.page))

    @ui.button(label="Nouveau blessé", emoji="🆕", style=discord.ButtonStyle.secondary)
    async def nouveau(self, inter: discord.Interaction, _):
        fiche = medical.nouvelle_fiche()
        fiche["role"] = self.fiche.get("role", medical.SOLDAT)
        await inter.response.edit_message(embed=care_embed(fiche, 0), view=CareView(self.owner_id, fiche, 0))

    @ui.button(label="Guide médical", emoji="📖", style=discord.ButtonStyle.secondary)
    async def guide(self, inter: discord.Interaction, _):
        await inter.response.send_message(embed=chapter_embed("medical", 0), view=GuideView(inter.user.id, "medical", 0), ephemeral=inter.guild is not None)


# --- guides -------------------------------------------------------------------------------------
def guides_embed():
    embed = discord.Embed(title="📚 Les guides de la PAG", color=GOLD,
                          description="Choisis un guide dans le menu. Chaque guide se feuillette chapitre par chapitre, ici ou en message privé.")
    for g in medical.GUIDES.values():
        embed.add_field(name=f"{g['emoji']} {g['titre']}", value=f"{g['description']}\n*{len(g['chapitres'])} chapitres*", inline=False)
    embed.set_footer(text=PIED_PAG)
    return embed


def chapter_embed(cle, index):
    g = medical.GUIDES[cle]
    c = g["chapitres"][index]
    embed = discord.Embed(title=f"{c['emoji']} {c['titre']}", description=c["intro"], color=GOLD)
    embed.set_author(name=f"{g['emoji']} {g['titre']}")
    for nom, texte in c["blocs"]:
        embed.add_field(name=nom[:256], value=texte[:1024], inline=False)
    embed.set_footer(text=f"Chapitre {index + 1}/{len(g['chapitres'])} · {PIED_PAG}")
    return embed


class GuidesHomeView(ui.View):
    def __init__(self, owner_id):
        super().__init__(timeout=1800)
        self.owner_id = owner_id
        select = ui.Select(placeholder="Ouvrir un guide…", options=[
            discord.SelectOption(label=g["titre"][:100], value=k, emoji=g["emoji"], description=g["description"][:100]) for k, g in medical.GUIDES.items()])
        select.callback = self._ouvrir(select)
        self.add_item(select)

    def _ouvrir(self, select):
        async def callback(inter: discord.Interaction):
            cle = select.values[0]
            await inter.response.edit_message(embed=chapter_embed(cle, 0), view=GuideView(self.owner_id, cle, 0))
        return callback


class GuideView(ui.View):
    """Un guide ouvert : menu des chapitres et feuilletage."""

    def __init__(self, owner_id, cle, index):
        super().__init__(timeout=1800)
        self.owner_id, self.cle, self.index = owner_id, cle, index
        chapitres = medical.GUIDES[cle]["chapitres"]
        select = ui.Select(placeholder="Aller au chapitre…", row=0, options=[
            discord.SelectOption(label=f"{i + 1}. {c['titre']}"[:100], value=str(i), emoji=c["emoji"], default=i == index) for i, c in enumerate(chapitres)])
        select.callback = self._aller(select)
        self.add_item(select)
        self.precedent.disabled = index == 0
        self.suivant.disabled = index == len(chapitres) - 1
        if cle != "medical":
            self.remove_item(self.fiche_de_soins)

    def _aller(self, select):
        async def callback(inter: discord.Interaction):
            i = int(select.values[0])
            await inter.response.edit_message(embed=chapter_embed(self.cle, i), view=GuideView(self.owner_id, self.cle, i))
        return callback

    @ui.button(label="Précédent", emoji="◀️", style=discord.ButtonStyle.secondary, row=1)
    async def precedent(self, inter: discord.Interaction, _):
        await inter.response.edit_message(embed=chapter_embed(self.cle, self.index - 1), view=GuideView(self.owner_id, self.cle, self.index - 1))

    @ui.button(label="Suivant", emoji="▶️", style=discord.ButtonStyle.secondary, row=1)
    async def suivant(self, inter: discord.Interaction, _):
        await inter.response.edit_message(embed=chapter_embed(self.cle, self.index + 1), view=GuideView(self.owner_id, self.cle, self.index + 1))

    @ui.button(label="Tous les guides", emoji="📚", style=discord.ButtonStyle.secondary, row=1)
    async def accueil(self, inter: discord.Interaction, _):
        await inter.response.edit_message(embed=guides_embed(), view=GuidesHomeView(self.owner_id))

    @ui.button(label="Ouvrir une fiche de soins", emoji="🩺", style=discord.ButtonStyle.success, row=1)
    async def fiche_de_soins(self, inter: discord.Interaction, _):
        fiche = medical.nouvelle_fiche()
        await inter.response.send_message(embed=care_embed(fiche, 0), view=CareView(inter.user.id, fiche, 0), ephemeral=inter.guild is not None)


# ------------------------------------------------------------------------------------------------
# Serveur HTTP : le jeu nous appelle ici
# ------------------------------------------------------------------------------------------------
IGNORED_SOURCES = set()     # serveurs hors front_sources déjà signalés au journal (une ligne chacun, pas une toutes les 5 s)


def front_source_ok(request):
    """Vrai si ce serveur de jeu peut nourrir le front et la mémoire du site : front_sources de config.json (adresses IP)
    vide, ou son adresse dans la liste. Derrière un serveur web local (Caddy), l'adresse est lue dans X-Forwarded-For."""
    sources = CFG.get("front_sources") or []
    if isinstance(sources, str):
        sources = [sources]
    sources = {str(s).strip() for s in sources if str(s).strip()}
    if not sources:
        return True
    remote = str(request.remote or "")
    if remote in ("127.0.0.1", "::1") and request.headers.get("X-Forwarded-For"):
        remote = request.headers["X-Forwarded-For"].split(",")[-1].strip()
    return remote.removeprefix("::ffff:") in sources


def front_actif():
    """Le front gardé dans front.json est-il celui du jeu ? Oui serveur éteint (le site le montre), ou quand le mod en ligne
    annonce la capacité « front ». Un ancien mod remis après le front (retour arrière) : non, le site revient aux
    secteurs au lieu de montrer un front figé comme s'il était actuel."""
    return CAMP.front.a_front() and ("front" in BRIDGE.caps() or not BRIDGE.online())


async def api_sync(request):
    try:
        body = await request.json()
    except Exception:  # noqa: BLE001
        return web.Response(status=400, text="json invalide")
    if body.get("secret") != CFG["secret"]:
        log.warning("sync refusée : mauvais secret depuis %s", request.remote)
        return web.Response(status=403, text="")
    body.pop("secret", None)
    if not BRIDGE.online():
        log.info("contact du serveur de jeu établi depuis %s (%d joueur(s), capacités : %s)", request.remote,
                 len(body.get("players", [])), ", ".join(body.get("caps", [])) or "mod ancien")
    BRIDGE.last_state = body
    BRIDGE.last_sync = time.time()
    remember_static(body)
    # Front (#75) : grille et zones gardées dans front.json AVANT les alertes (l'image de victoire voit la grille finale) ;
    # la ligne « #front <id> <v> » (ou « #front plein ») part en tête de la réponse. Le bloc est ensuite retiré de
    # last_state, qui reste léger. Seuls les serveurs de front_sources nourrissent le front et la mémoire du site : un
    # Workbench d'essai branché sur le même bot n'écrit ni front.json, ni frise, ni archives, ni fiches.
    source_ok = front_source_ok(request)
    front_line = ""
    if source_ok:
        try:
            front_line = CAMP.front_ingest(body, time.time())
        except Exception as ex:         # noqa: BLE001 — le site ne doit jamais gêner le pont du jeu
            log.warning("front : %s", ex)
            front_line = ""
    elif request.remote not in IGNORED_SOURCES:
        IGNORED_SOURCES.add(request.remote)
        log.warning("front et mémoire du site : envois de %s ignorés (absent de front_sources)", request.remote)
    body.pop("front", None)
    if source_ok:
        try:
            CAMP.ingest(body)           # mémoire de la campagne pour le site : soldats, secteurs, trajets (léger, voir campagne.py)
        except Exception as ex:         # noqa: BLE001 — le site ne doit jamais gêner le pont du jeu
            log.warning("campagne : %s", ex)
    BRIDGE.deliver_results(body.get("results"))
    bot.track_stats(body)
    asyncio.create_task(bot.handle_links(body.get("links")))
    try:
        vote_requests_from_game(body.get("votereq"))     # /vote tapé en jeu : rattaché à l'identité du soldat
    except Exception as ex:  # noqa: BLE001 — les votes ne doivent jamais gêner le pont du jeu
        log.warning("votes : %s", ex)
    asyncio.create_task(bot.handle_events(body.get("events")))
    asyncio.create_task(bot.sync_roles(body))
    asyncio.create_task(handle_requests_safe(body))
    answer = BRIDGE.take_pending_lines()
    if front_line:
        answer = front_line + "\n" + answer if answer else front_line
    return web.Response(text=answer, content_type="text/plain", charset="utf-8")


async def api_status(request):
    return web.json_response({"online": BRIDGE.online(), "last_sync": BRIDGE.last_sync, "pending": len(BRIDGE.pending)})


# ------------------------------------------------------------------------------------------------
# Carte de situation web : de l'affichage seulement. Rien de ce qui est servi ici ne permet d'agir sur le jeu, et l'état
# est FILTRÉ : nom, grade, carré de 100 m et code public de la fiche des soldats, missions, front (couleurs, zones,
# dépôts de la PAG ; rien des forces ennemies, I3). Ni identifiants du jeu, ni sanctions, ni secret.
# ------------------------------------------------------------------------------------------------
MAP_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), str(CFG.get("map_dir", "carte")))
MAP_PLACES = []      # les localités, envoyées par le jeu de temps en temps : gardées entre deux envois
MISSION_TYPES = []   # les types de mission qu'un officier peut ordonner (/ordonner), envoyés avec les localités


def remember_static(state):
    """Ce que le jeu n'envoie qu'une fois par minute (localités, types de mission) : gardé dès la réception."""
    global MAP_PLACES, MISSION_TYPES
    try:
        if state.get("places"):
            MAP_PLACES = [{"nom": str(p.get("name", ""))[:40], "x": p.get("x", 0), "z": p.get("z", 0)} for p in state["places"][:200]]
        if state.get("mission_types"):
            MISSION_TYPES = [{"id": int(t.get("id", -1)), "nom": str(t.get("name", ""))[:60]} for t in state["mission_types"][:40]]
    except (TypeError, ValueError, AttributeError) as ex:
        log.warning("localités ou types de mission illisibles : %s", ex)


def map_state():
    """/carte/api/carte, lue toutes les 5 s. Le front lui-même est servi par /carte/api/front, rechargé seulement quand
    front_v change ; ici passent le direct (contestés, attaques) et le gel. « secteurs » n'est rempli que par un ancien
    mod (et reste présent, vide, pour les pages restées en cache). Ancien mod remis après le front : front_v vide, la
    carte revient aux secteurs (front_actif)."""
    state = BRIDGE.last_state or {}
    online = BRIDGE.online()
    actif = front_actif()
    soldiers, missions, sectors = [], [], []
    if online:
        for p in state.get("players", []):
            code = CAMP.code(p["identity"]) if p.get("identity") else ""      # code public de sa fiche (jamais l'identité)
            soldiers.append({"nom": str(p.get("name", ""))[:32], "grade": str(p.get("gradeName", ""))[:24], "carre": str(p.get("grid", ""))[:7], "code": code})
        for m in state.get("missions", []):
            missions.append({"id": str(m.get("id", ""))[:8], "titre": str(m.get("title", ""))[:60], "lieu": str(m.get("site", ""))[:40],
                             "carre": str(m.get("grid", ""))[:7], "difficulte": m.get("difficulty", 0), "chef": str(m.get("claimed", ""))[:32]})
        for t in (state.get("sectors") or [])[:200]:
            if not isinstance(t, dict):
                continue
            sectors.append({"nom": str(t.get("name", ""))[:40], "carre": str(t.get("grid", ""))[:7], "nous": bool(t.get("ours")), "attaque": bool(t.get("attack")),
                            "depot": str(t.get("depot", ""))[:7], "stock": int(t.get("stock", 0) or 0)})
    return {"en_ligne": online, "silence": round(time.time() - BRIDGE.last_sync) if BRIDGE.last_sync else None,
            "heure_jeu": str(state.get("hour", ""))[:12] if online else "", "soldats": soldiers, "missions": missions,
            "secteurs": sectors, "lieux": MAP_PLACES, "front_v": CAMP.front.version_web() if actif else "",
            "contestes": list(CAMP.front.contestes) if online and actif else [], "attaques": CAMP.front.attaques() if online and actif else [],
            "gele": bool(CAMP.front.etat.get("gele")) if actif else False}


async def map_api(request):
    return web.json_response(map_state(), headers={"Cache-Control": "no-store"})


async def front_api(request):
    """/carte/api/front : la grille complète et les zones. La page ne la redemande que quand front_v change ; jamais de
    copie en cache : le décompte kr des zones coupées et « recu » sont ramenés à l'heure de CETTE requête."""
    if not CAMP.front.a_front():
        raise web.HTTPNotFound(text="Front pas encore reçu")
    return web.json_response(CAMP.front.public_front(BRIDGE.online()), headers={"Cache-Control": "no-store"})


async def frieze_api(request):
    """/carte/api/frise : les photos du front de la campagne en cours (I8)."""
    return web.json_response(CAMP.front.public_frise(), headers={"Cache-Control": "no-store"})


FRONT_IMAGE_CACHE = {}      # (révision, titre, sous-titre) -> JPEG : l'image du soir et celle du site, fabriquées une fois
FRONT_IMAGE_LOCK = asyncio.Lock()


async def make_front_image(title, subtitle=""):
    """JPEG du front courant (la même image que sur Discord), ou None sans front. Fabriqué hors de la boucle du bot ;
    ImportError sans Pillow."""
    import trace_image
    data = CAMP.front.pour_image()
    if not data:
        return None
    key = (data.get("rev"), title, subtitle)
    async with FRONT_IMAGE_LOCK:
        if key not in FRONT_IMAGE_CACHE:
            image = await asyncio.get_running_loop().run_in_executor(None, trace_image.fabriquer_front, MAP_DIR, data, title, subtitle)
            if len(FRONT_IMAGE_CACHE) >= 4:
                FRONT_IMAGE_CACHE.clear()
            FRONT_IMAGE_CACHE[key] = image
        return FRONT_IMAGE_CACHE[key]


async def front_image(request):
    """/carte/api/front.jpg : l'image du front, la même que sur Discord ; 503 sans Pillow."""
    import trace_image
    if not CAMP.front.a_front():
        raise web.HTTPNotFound(text="Front pas encore reçu")
    subtitle = trace_image.sous_titre_front(CAMP.front.etat.get("maj") or time.time(), PARIS, "Situation au", True)
    try:
        image = await make_front_image("PAG · Le front d'Everon", subtitle)
    except ImportError:
        raise web.HTTPServiceUnavailable(text="Image indisponible : Pillow n'est pas installé sur le serveur")
    if not image:
        raise web.HTTPNotFound(text="Front pas encore reçu")
    return web.Response(body=image, content_type="image/jpeg", headers={"Content-Disposition": 'inline; filename="front_everon.jpg"', "Cache-Control": "no-cache"})


async def map_index(request):
    return web.FileResponse(os.path.join(MAP_DIR, "index.html"), headers={"Cache-Control": "no-cache"})


async def map_redirect(request):
    raise web.HTTPFound("/carte/")


# ------------------------------------------------------------------------------------------------
# Tableau de suivi du développement : page web en lecture seule (tout se modifie depuis Discord, commandes /suivi)
# ------------------------------------------------------------------------------------------------
BOARD = suivi.Tableau(os.path.join(HERE, "suivi.json"))
BOARD_DIR = os.path.join(HERE, "suivi_web")


async def board_api(request):
    return web.json_response(BOARD.public(), headers={"Cache-Control": "no-store"})


async def board_index(request):
    return web.FileResponse(os.path.join(BOARD_DIR, "index.html"), headers={"Cache-Control": "no-cache"})


async def board_redirect(request):
    raise web.HTTPFound("/suivi/")


# ------------------------------------------------------------------------------------------------
# Accueil du site : la vitrine, avec les portes vers la carte, le suivi et le Discord. Que des totaux, aucun nom.
# ------------------------------------------------------------------------------------------------
SITE_DIR = os.path.join(HERE, "site_web")


def home_state():
    carte = map_state()
    tableau = BOARD.public()
    derniere = None
    if tableau["versions"]:
        v = tableau["versions"][0]
        derniere = {"version": v.get("version", ""), "date": v.get("date", ""), "cartes": len(v.get("cartes", []))}
    invite = str(CFG.get("discord_invite", "")).strip()
    if not invite.startswith(("https://discord.gg/", "https://discord.com/invite/")):
        invite = ""
    etat = {"en_ligne": carte["en_ligne"], "soldats": len(carte["soldats"]), "places": int(CFG.get("max_players", 0) or 0),
            "missions": len(carte["missions"]), "heure_jeu": carte["heure_jeu"],
            "suivi": {"colonnes": [{"cle": c["cle"], "nom": c["nom"], "n": len(c["cartes"])} for c in tableau["colonnes"]], "derniere": derniere},
            "discord": invite, "serveur": str(CFG.get("server_name", ""))[:100]}
    etat.update(front_figures(carte["en_ligne"]))
    return etat


def front_figures(online):
    """Part de l'île tenue, carrés et zones, lus dans front.json : visibles serveur éteint. Sans front.json utilisable
    (pas encore reçu, ou ancien mod remis), le bloc territory du pont en ligne : un mod à front y donne blue et land en
    carrés, ours et total en zones, frozen ; un ancien mod n'y a que ses secteurs (ile_part vaut alors None et les
    pages retombent sur secteurs_nous / secteurs_total)."""
    summary = CAMP.front.resume() if front_actif() else None
    if summary:
        return {"ile_part": summary["part"], "carres_nous": summary["bleus"], "carres_terre": summary["terre"],
                "zones_nous": summary["zones_nous"], "zones_total": summary["zones_total"], "front_maj": summary["maj"], "gele": summary["gele"]}
    terr = ((BRIDGE.last_state or {}).get("territory") or {}) if online else {}
    if not isinstance(terr, dict):
        terr = {}
    blue, land = campagne._entier(terr.get("blue"), 0), campagne._entier(terr.get("land"), 0)
    ours, total = campagne._entier(terr.get("ours"), 0), campagne._entier(terr.get("total"), 0)
    return {"ile_part": round(100.0 * blue / land, 1) if land > 0 else None, "carres_nous": blue, "carres_terre": land,
            "zones_nous": ours, "zones_total": total, "front_maj": 0, "gele": campagne._vrai(terr.get("frozen", False)),
            "secteurs_nous": ours, "secteurs_total": total}


async def home_api(request):
    return web.json_response(home_state(), headers={"Cache-Control": "no-store"})


async def home_index(request):
    return web.FileResponse(os.path.join(SITE_DIR, "index.html"), headers={"Cache-Control": "no-cache"})


def build_web_app(bridge):
    """bridge = True : le pont du jeu (/api/sync) et les pages ; False : les pages seules, pour le port public."""
    app = web.Application(client_max_size=2 * 1024 * 1024)
    if bridge:
        app.add_routes([web.post("/api/sync", api_sync), web.get("/api/status", api_status)])
    if CFG.get("site_enabled", True) and os.path.isfile(os.path.join(SITE_DIR, "index.html")):
        app.add_routes([web.get("/", home_index), web.get("/api/accueil", home_api), web.get("/favicon.ico", redirect("/site/favicon.png"))])
        app.router.add_static("/site", SITE_DIR, append_version=False)
        for nom, fichier in (("campagne", "campagne.html"), ("effectifs", "effectifs.html"), ("dotations", "dotations.html"), ("certifications", "certifications.html"), ("instructeur", "instructeur.html")):
            if os.path.isfile(os.path.join(SITE_DIR, fichier)):
                app.add_routes([web.get("/" + nom, redirect("/" + nom + "/")), web.get("/" + nom + "/", page(os.path.join(SITE_DIR, fichier)))])
        if os.path.isfile(os.path.join(SITE_DIR, "soldat.html")):
            app.add_routes([web.get("/soldat/{code}", page(os.path.join(SITE_DIR, "soldat.html")))])
        app.add_routes([web.get("/campagne/api", campaign_api), web.get("/effectifs/api", roster_api), web.get("/effectifs/api/soldat/{code}", soldier_api)])
        if bridge:
            log.info("accueil du site servi sur /")
    # Campagnes précédentes (K1, B3) : lues par la page Campagne et par la carte (?archive=<nom>)
    app.add_routes([web.get("/campagne/api/archives", archives_api), web.get("/campagne/api/archives/{nom}", archive_api)])
    app.add_routes([web.get("/vote", vote_page), web.get("/vote/", vote_page), web.get("/vote/{code}", vote_redirect)])
    if os.path.isfile(os.path.join(BOARD_DIR, "index.html")):
        app.add_routes([web.get("/suivi", board_redirect), web.get("/suivi/", board_index), web.get("/suivi/api", board_api)])
        if bridge:
            log.info("tableau de suivi servi sur /suivi/")
    if CFG.get("map_enabled", True) and os.path.isfile(os.path.join(MAP_DIR, "index.html")):
        app.add_routes([web.get("/carte", map_redirect), web.get("/carte/", map_index), web.get("/carte/api/carte", map_api),
                        web.get("/carte/api/front", front_api), web.get("/carte/api/frise", frieze_api), web.get("/carte/api/front.jpg", front_image),
                        web.get("/carte/api/historique", history_api), web.get("/carte/api/trajets", tracks_api), web.get("/carte/api/trace.jpg", tracks_image)])
        for folder in ("tuiles", "tuiles_aerien"):
            os.makedirs(os.path.join(MAP_DIR, folder), exist_ok=True)
            app.router.add_static("/carte/" + folder, os.path.join(MAP_DIR, folder), append_version=False)
        if bridge:
            log.info("carte de situation servie sur /carte/ (dossier %s)", MAP_DIR)
    elif CFG.get("map_enabled", True) and bridge:
        log.warning("carte de situation : %s introuvable, page non servie", os.path.join(MAP_DIR, "index.html"))
    return app


# ------------------------------------------------------------------------------------------------
# Campagne, front, archives, trajets, effectifs et fiches : tout est public et FILTRÉ par campagne.py
# (un soldat = un code court, jamais son identité ; ni infraction, ni sanction, ni statut Staff)
# ------------------------------------------------------------------------------------------------
CAMP = campagne.Campagne(os.path.join(HERE, "donnees_web"), CFG["secret"], CFG)     # CFG : réglages front_* de la frise
TRACE_CACHE = {}        # image d'un tracé : gardée 60 s, le site étant public on ne la refabrique pas à chaque clic
TRACE_LOCK = asyncio.Lock()


def campaign_state():
    """/campagne/api : chiffres du front lus dans front.json (visibles serveur éteint), zones, mouvements, archives ;
    le reste (missions, soldats, coffre, dépôts, parc, production) seulement en ligne. Sans front reçu, ou avec un ancien
    mod remis après le front (front_actif), les secteurs passent comme avant."""
    st = BRIDGE.last_state or {}
    online = BRIDGE.online()
    actif = front_actif()
    carte = map_state()
    terr = st.get("territory", {}) if online else {}
    stocks = st.get("stocks", {}) if online else {}
    parc = []
    if online:
        for v in st.get("fleet", [])[:40]:
            parc.append({"nom": str(v.get("name", ""))[:40], "etat": str(v.get("state", ""))[:20], "carre": str(v.get("grid", ""))[:7], "base": bool(v.get("base"))})
    production = [{"zone": str(b.get("sector", ""))[:60], "nombre": b.get("count", 0), "ressource": str(b.get("resource", ""))[:20]}
                  for b in st.get("boxes", [])[:20] if isinstance(b, dict)] if online else []
    etat = {"en_ligne": online, "heure_jeu": carte["heure_jeu"], "soldats": carte["soldats"], "places": int(CFG.get("max_players", 0) or 0),
            "menace": terr.get("threat", 0), "missions": carte["missions"], "missions_terminees": CAMP.missions_recentes(),
            "euros": st.get("euros", 0) if online else None, "stocks": stocks, "parc": parc, "production": production,
            "zones": CAMP.front.zones_campagne(online) if actif else [],
            "mouvements": list(reversed((CAMP.front.etat.get("mouvements") or [])[-8:])) if actif else [],
            "archives": CAMP.archives_liste()}
    etat.update(front_figures(online))
    if not actif:
        hist = CAMP.historique_public()
        etat["secteurs"] = carte["secteurs"]
        etat["derniers_secteurs"] = list(reversed(hist["historique"][-8:]))
    return etat


async def campaign_api(request):
    return web.json_response(campaign_state(), headers={"Cache-Control": "no-store"})


async def archives_api(request):
    """/campagne/api/archives : les campagnes précédentes, la plus récente d'abord."""
    return web.json_response({"archives": CAMP.archives_liste()}, headers={"Cache-Control": "no-cache"})


async def archive_api(request):
    """/campagne/api/archives/{nom} : le fichier d'archive tel quel (nom vérifié : ^[A-Za-z0-9-]{1,60}$)."""
    path = CAMP.archive_chemin(request.match_info.get("nom", ""))
    if not path:
        raise web.HTTPNotFound(text="Archive inconnue")
    return web.FileResponse(path, headers={"Cache-Control": "no-cache"})


async def history_api(request):
    return web.json_response(CAMP.historique_public(), headers={"Cache-Control": "no-store"})


def _periode(request):
    """?heures=3 (24 au plus), ou ?debut=&fin= en secondes Unix (3 jours au plus)."""
    now = time.time()
    try:
        if "debut" in request.query:
            debut = float(request.query["debut"])
            fin = float(request.query.get("fin", now))
        else:
            heures = max(0.25, min(24.0, float(request.query.get("heures", 3))))
            debut, fin = now - heures * 3600, now
    except ValueError:
        debut, fin = now - 3 * 3600, now
    fin = min(fin, now, debut + 3 * 86400)
    return debut, fin


async def tracks_api(request):
    debut, fin = _periode(request)
    return web.json_response({"debut": int(debut), "fin": int(fin), "trajets": CAMP.trajets_entre(debut, fin)}, headers={"Cache-Control": "no-store"})


async def make_track_image(debut, fin, titre):
    """JPEG du tracé entre deux instants, ou None (rien à dessiner, ou Pillow absent). Fabriqué hors de la boucle du bot."""
    import trace_image
    trajets = CAMP.trajets_entre(debut, fin)
    if not trajets:
        return None
    sous_titre = trace_image.sous_titre_periode(debut, fin, PARIS)
    return await asyncio.get_running_loop().run_in_executor(None, trace_image.fabriquer, MAP_DIR, trajets, titre, sous_titre)


async def tracks_image(request):
    debut, fin = _periode(request)
    cle = (int(debut // 60), int(fin // 60))
    async with TRACE_LOCK:
        garde = TRACE_CACHE.get(cle)
        if not garde or time.time() - garde[0] > 60:
            try:
                image = await make_track_image(debut, fin, "PAG · Tracé des déplacements")
            except ImportError:
                raise web.HTTPServiceUnavailable(text="Image indisponible : Pillow n'est pas installé sur le serveur")
            TRACE_CACHE.clear()
            TRACE_CACHE[cle] = garde = (time.time(), image)
    if not garde[1]:
        raise web.HTTPNotFound(text="Aucun déplacement enregistré sur cette période")
    return web.Response(body=garde[1], content_type="image/jpeg", headers={"Content-Disposition": 'inline; filename="trace_pag.jpg"', "Cache-Control": "no-store"})


async def roster_api(request):
    return web.json_response({"soldats": CAMP.effectifs_public()}, headers={"Cache-Control": "no-store"})


async def soldier_api(request):
    fiche = CAMP.fiche_publique(request.match_info.get("code", ""))
    if not fiche:
        raise web.HTTPNotFound(text="Soldat inconnu")
    return web.json_response(fiche, headers={"Cache-Control": "no-store"})


def page(path):
    async def handler(request):
        return web.FileResponse(path, headers={"Cache-Control": "no-cache"})
    return handler


def redirect(target):
    async def handler(request):
        raise web.HTTPFound(target)
    return handler


async def start_web():
    runner = web.AppRunner(build_web_app(True))
    await runner.setup()
    site = web.TCPSite(runner, "0.0.0.0", int(CFG["port"]))
    await site.start()
    log.info("API en écoute sur le port %s", CFG["port"])
    public = int(CFG.get("public_port", 0) or 0)
    if public and public != int(CFG["port"]):
        # Le port public (80) ne sert que les pages. Il faut au service le droit d'ouvrir un port inférieur à 1024
        # (AmbientCapabilities=CAP_NET_BIND_SERVICE) ; sans lui le bot continue sur son seul port habituel.
        try:
            runner = web.AppRunner(build_web_app(False))
            await runner.setup()
            await web.TCPSite(runner, str(CFG.get("public_host", "0.0.0.0")), public).start()
            log.info("site web public en écoute sur le port %s (pages seulement)", public)
        except OSError as e:
            log.warning("site web public : port %s refusé (%s), le site reste sur le port %s", public, e, CFG["port"])


# ------------------------------------------------------------------------------------------------
# Commandes slash
# ------------------------------------------------------------------------------------------------
PARTOUT = app_commands.allowed_contexts(guilds=True, dms=True, private_channels=False)
SERVEUR = app_commands.allowed_contexts(guilds=True, dms=False, private_channels=False)


async def staff_command(inter: discord.Interaction, command: str, sanction=None):
    """Commande slash directe, réservée au Staff."""
    if CFG["staff_role"] not in await roles_of(inter):
        return await inter.response.send_message("Réservé au Staff.", ephemeral=True)
    if not BRIDGE.online():
        return await inter.response.send_message("🔴 Le serveur de jeu ne contacte plus le bot : commande refusée.", ephemeral=True)
    await inter.response.defer(ephemeral=True, thinking=True)
    author = await display_name(inter)
    ok, message = await game_command(author, command)
    icon = "✅" if ok else ("⏳" if ok is None else "❌")
    await inter.followup.send(f"{icon} {message}"[:1900], ephemeral=True)
    await bot.feedback(f"{icon} **{author}** → `{pretty(command)[:200]}` : {message}")
    if sanction:
        await bot.log_sanction(author, sanction[0], sanction[1], sanction[2], ok, message)


@bot.tree.command(name="panel", description="Menu de gestion du serveur de jeu (encadrement)")
@PARTOUT
async def cmd_panel(inter: discord.Interaction):
    if not await can_panel(inter):
        return await inter.response.send_message("Le panel est réservé à l'encadrement : " + ", ".join(CFG["panel_roles"]) + ".", ephemeral=True)
    await inter.response.send_message(embed=home_embed(), view=HomeView(), ephemeral=inter.guild is not None)


@bot.tree.command(name="serveur", description="État du serveur de jeu")
@PARTOUT
async def cmd_serveur(inter: discord.Interaction):
    await inter.response.send_message(embed=status_embed(full=True), ephemeral=inter.guild is not None)


@bot.tree.command(name="joueurs", description="Qui est en ligne sur le serveur")
@PARTOUT
async def cmd_joueurs(inter: discord.Interaction):
    await inter.response.send_message(embed=players_embed().remove_footer(), ephemeral=inter.guild is not None)


@bot.tree.command(name="soins", description="Fiche de soins : coche ce que tu vois sur le blessé, le bot te dit quoi faire et dans quel ordre")
@PARTOUT
async def cmd_soins(inter: discord.Interaction):
    fiche = medical.nouvelle_fiche()
    await inter.response.send_message(embed=care_embed(fiche, 0), view=CareView(inter.user.id, fiche, 0), ephemeral=inter.guild is not None)


@bot.tree.command(name="guide", description="Les guides de la PAG : médical, radio…")
@PARTOUT
async def cmd_guide(inter: discord.Interaction):
    await inter.response.send_message(embed=guides_embed(), view=GuidesHomeView(inter.user.id), ephemeral=inter.guild is not None)


@bot.tree.command(name="fiche", description="Ta fiche de soldat : grade, certifications, missions")
@PARTOUT
async def cmd_fiche(inter: discord.Interaction):
    link = STATE.links.get(str(inter.user.id))
    if not link:
        return await inter.response.send_message("Ton compte Discord n'est pas lié à un soldat. Tape `/lier` ici, puis le code en jeu.", ephemeral=True)
    for p in BRIDGE.players():
        if p.get("identity") == link["identity"]:
            return await inter.response.send_message(embed=player_embed(p), ephemeral=True)
    if not BRIDGE.online():
        return await inter.response.send_message("🔴 Le serveur de jeu est injoignable, ta fiche n'est pas consultable pour l'instant.", ephemeral=True)
    await inter.response.defer(ephemeral=True, thinking=True)
    ok, message = await game_command(await display_name(inter), f"info {target_token(link['name'], link.get('identity', ''))}")
    embed = discord.Embed(title=f"🪖 {link['name']}", description=message[:4000], color=GOLD if ok else RED)
    await inter.followup.send(embed=embed, ephemeral=True)


@bot.tree.command(name="lier", description="Lier ton compte Discord à ton soldat en jeu")
@PARTOUT
async def cmd_lier(inter: discord.Interaction):
    code = BRIDGE.new_link_code(inter.user.id)
    await inter.response.send_message(f"Tape en jeu, dans le chat : `/lier {code}`\nLe code vaut 10 minutes.", ephemeral=True)


@bot.tree.command(name="delier", description="Retirer la liaison entre ton compte Discord et ton soldat")
@PARTOUT
async def cmd_delier(inter: discord.Interaction):
    if STATE.links.pop(str(inter.user.id), None):
        STATE.save()
        await inter.response.send_message("Liaison retirée.", ephemeral=True)
    else:
        await inter.response.send_message("Ton compte n'était pas lié.", ephemeral=True)


# ------------------------------------------------------------------------------------------------
# Votes sur top-serveurs.net. Un vote n'est compté que s'il est rattaché à un soldat, par son IDENTITÉ du jeu :
#  - /vote tapé dans le chat du jeu : le mod envoie « code|identité|pseudo » au contact suivant (liste "votereq") et
#    donne au joueur son lien court penguinassaultgroup.fr/vote/<code>, qui renvoie vers la page de vote, pseudo rempli ;
#  - /vote sur Discord, SEULEMENT avec un compte lié au soldat (/lier) : bouton direct vers la page, pseudo rempli.
# Une seule demande en cours par soldat, quel que soit le chemin. Le bot réclame le vote auprès de l'API (claim-username :
# un vote n'est réclamable qu'une fois ; l'API ignore les majuscules), puis fait créditer le jeu par
# « vote @identité <prime> <horodatage> » : le jeu garde les horodatages déjà comptés, un renvoi ne compte jamais deux fois.
# Un vote fait sans /vote n'est jamais réclamé seul. S'il date de moins de 2 h, sous le pseudo exact du soldat, il est pris
# à la demande suivante de ce soldat. Pseudo porté par plusieurs soldats : aucun vote réclamé (rattachement impossible).
# Le jeton de votes de la fiche va dans config.json ("top_serveurs_token", /vote-reglages), jamais ailleurs.
# ------------------------------------------------------------------------------------------------
VOTE_API = "https://api.top-serveurs.net/v1/votes/claim-username"
VOTE_WINDOW = 7200          # un lien de vote vaut 2 h (l'intervalle de vote de top-serveurs)
VOTE_GRACE = 600            # la demande reste guettée 10 min après la fin du lien : un vote fait juste avant n'est pas perdu
VOTE_MIN_GAP = 3600         # au plus un vote crédité par soldat par heure ; au-delà, mis de côté pour le Staff
VOTE_RECOVER = 7300         # « déjà réclamé » sans aucun vote de ce soldat depuis ce délai : notre réclamation a perdu sa réponse
VOTE_MAX_TRIES = 30         # crédit refusé ou sans réponse du jeu : abandon après 30 essais (une minute d'écart)
VOTE_CODE_RE = re.compile(r"^[A-Z0-9]{4,8}$")
VOTE_PAGE = """<!DOCTYPE html><html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>PAG · Vote</title><link rel="icon" type="image/png" href="/site/favicon.png"><meta name="robots" content="noindex">
<style>body{{margin:0;min-height:100vh;display:grid;place-items:center;background:#0c1116;color:#d7e0e8;font:16px/1.6 "Segoe UI",system-ui,sans-serif}}
main{{max-width:560px;margin:24px 16px;padding:28px;border:1px solid #24313d;border-radius:12px;background:#111820}}
h1{{margin:0 0 12px;font-size:22px;color:#c9a227}}code{{background:#1b2530;padding:2px 6px;border-radius:4px}}a{{color:#7fb8ff}}</style></head>
<body><main><h1>🗳️ Voter pour la PAG</h1>{message}
<p>Un vote n'est compté que s'il est relié à ton soldat. Pour recevoir ton lien personnel, tape <code>/vote</code> dans le chat
du jeu, ou <code>/vote</code> sur le Discord de la PAG si ton compte est lié à ton soldat (<code>/lier</code>).</p>
<p><a href="/">← Accueil du site</a></p></main></body></html>"""


def vote_state():
    d = STATE.data
    d.setdefault("votes", [])            # [{"t", "name", "identity", "discord"}] : votes réclamés et crédités, un par vote
    d.setdefault("vote_pending", {})     # identité -> {"identity", "name", "discord", "since", "until", "next", "codes"}
    d.setdefault("vote_uncredited", [])  # [{"name", "identity", "stamp", "tries"}] : à faire créditer par le jeu
    d.setdefault("vote_manual", [])      # votes réclamés mais NON crédités (garde d'une heure) : le Staff tranche
    return d


def vote_migrate():
    """Première version (22/09) : demandes rangées par pseudo, parfois sans identité. On ne garde que le rattaché."""
    d = vote_state()
    d.pop("vote_checked", None)
    for key, p in list(d["vote_pending"].items()):
        if not p.get("identity") or key != p["identity"]:
            del d["vote_pending"][key]
    now = int(time.time())
    olds = [v for v in d["vote_uncredited"] if v.get("identity")]
    d["vote_uncredited"] = [dict(v, stamp=int(v.get("stamp") or now - len(olds) + i), tries=int(v.get("tries", 0)))
                            for i, v in enumerate(olds)]     # horodatages distincts : deux anciens votes restent deux
    del d["vote_manual"][:-500]
    STATE.save()


def vote_month_count(identity=None):
    start = dt.datetime.now(PARIS).replace(day=1, hour=0, minute=0, second=0, microsecond=0).timestamp()
    return sum(1 for v in vote_state()["votes"] if v.get("t", 0) >= start and (identity is None or v.get("identity") == identity))


def vote_recent(identity, name, now):
    """Un vote a-t-il été réclamé récemment pour ce soldat OU sous ce pseudo (l'API ignore les majuscules) ?"""
    d = vote_state()
    n = str(name).casefold()
    return any(now - v.get("t", 0) < VOTE_RECOVER and (v.get("identity") == identity or str(v.get("name", "")).casefold() == n)
               for v in d["votes"] + d["vote_manual"])


def vote_link(name):
    url = str(CFG["vote_url"])
    return url + ("&" if "?" in url else "?") + "pseudo=" + urllib.parse.quote(name)


def vote_current_name(identity, fallback=""):
    """Le pseudo du soldat tel que le jeu le connaît : en ligne d'abord, puis sa fiche du site, puis la liaison Discord."""
    for p in BRIDGE.players():
        if p.get("identity") == identity and p.get("name"):
            return str(p["name"])
    nom = str(CAMP.soldats.get(CAMP.code(identity), {}).get("nom", ""))
    if nom and len(nom) < 32:            # le site coupe les noms à 32 caractères : un nom de 32 peut être tronqué
        return nom
    return fallback or nom


def vote_homonym(identity, name):
    """Un autre soldat porte-t-il ce pseudo ? top-serveurs ne distingue pas les majuscules : le vote ne pourrait pas être
    rattaché avec certitude, on ne le réclame pas."""
    n = str(name).casefold()
    now = time.time()
    for other_id, other in vote_state()["vote_pending"].items():
        if other_id != identity and other.get("until", 0) + VOTE_GRACE >= now and str(other.get("name", "")).casefold() == n:
            return True
    for p in BRIDGE.players():
        if p.get("identity") and p.get("identity") != identity and str(p.get("name", "")).casefold() == n:
            return True
    mine = CAMP.code(identity)
    return any(code != mine and str(f.get("nom", "")).casefold() == n[:32] for code, f in CAMP.soldats.items())


def vote_register(identity, name, discord_id=0, code=""):
    """Demande de vote de ce soldat, guettée 2 h. Une seule par soldat : le jeu et Discord tombent sur la même."""
    d = vote_state()
    now = time.time()
    p = d["vote_pending"].get(identity) or {"identity": identity, "codes": []}
    p.update({"identity": identity, "name": name, "since": now, "until": now + VOTE_WINDOW, "next": 0})
    p.pop("homonyme_log", None)
    p["discord"] = int(discord_id or p.get("discord") or STATE.discord_for_identity(identity) or 0)
    if code:
        for other_id, other in d["vote_pending"].items():     # code déjà donné à un autre soldat : il passe au plus récent
            if other_id != identity and code in other.get("codes", []):
                other["codes"].remove(code)
        p["codes"] = [c for c in p.get("codes", []) if c != code][-4:] + [code]
    d["vote_pending"][identity] = p
    STATE.save()
    return p


def vote_requests_from_game(requests):
    """Les /vote tapés en jeu, arrivés avec l'état du serveur : « code|identité|pseudo » (le pseudo peut contenir « | »)."""
    for raw in requests or []:
        parts = str(raw).split("|")
        if len(parts) < 3:
            continue
        code, identity, name = parts[0].strip().upper(), parts[1].strip(), "|".join(parts[2:]).strip()
        if not identity or not name or not VOTE_CODE_RE.match(code):
            continue
        known = code in vote_state()["vote_pending"].get(identity, {}).get("codes", [])
        vote_register(identity, name, code=code)
        if known:                        # le jeu renvoie sa file tant que sa réponse ne lui est pas parvenue
            continue
        log.info("vote demandé en jeu par %s (lien %s)", name, code)
        if vote_homonym(identity, name):
            BRIDGE.enqueue("PAG", f"mp @{identity} Un autre soldat porte le pseudo « {name} » : ton vote ne pourrait pas être "
                                  f"relié à ta fiche, il ne sera pas compté. Vois avec l'état-major.")


async def vote_page(request):
    return web.Response(text=VOTE_PAGE.format(message=""), content_type="text/html", charset="utf-8", headers={"Cache-Control": "no-store"})


async def vote_redirect(request):
    """penguinassaultgroup.fr/vote/<code> : le lien court donné en jeu, vers la page de vote avec le pseudo du soldat."""
    code = str(request.match_info.get("code", "")).strip().upper()
    now = time.time()
    message = "<p><b>Ce lien de vote est inconnu ou a expiré</b> (un lien vaut 2 h, et sert pour un seul vote).</p>"
    if VOTE_CODE_RE.match(code):
        for identity, p in vote_state()["vote_pending"].items():
            if code in p.get("codes", []) and p.get("until", 0) > now:
                if vote_homonym(identity, p["name"]):
                    message = (f"<p><b>Un autre soldat porte le pseudo « {html.escape(p['name'])} »</b> : ton vote ne pourrait pas "
                               "être relié à ta fiche. Vois avec l'état-major.</p>")
                    break
                raise web.HTTPFound(vote_link(p["name"]), headers={"Cache-Control": "no-store", "Referrer-Policy": "no-referrer"})
    return web.Response(status=404, text=VOTE_PAGE.format(message=message), content_type="text/html", charset="utf-8",
                        headers={"Cache-Control": "no-store"})


async def vote_claim(session, name):
    """Réclame le vote de ce pseudo. 1 = vote réclamé (à récompenser), 2 = déjà réclamé, 0 = pas de vote, None = erreur."""
    try:
        async with session.get(VOTE_API, params={"server_token": CFG["top_serveurs_token"], "playername": name},
                               timeout=aiohttp.ClientTimeout(total=15)) as resp:
            body = await resp.json(content_type=None)
    except Exception as ex:  # noqa: BLE001
        log.warning("top-serveurs : %s", ex)
        return None
    if not isinstance(body, dict):
        return None
    if body.get("success") and "claimed" in body:
        try:
            return int(body.get("claimed") or 0)
        except (TypeError, ValueError):
            return None
    if body.get("code") == 404:
        return 0
    log.warning("top-serveurs : réponse inattendue %s", str(body)[:200])
    return None


async def vote_credit():
    """Fait créditer par le jeu les votes réclamés. Renvoyer une commande est sans risque : le jeu reconnaît
    l'horodatage d'un vote déjà compté et ne le compte pas deux fois. Pour un même soldat, un vote ne part qu'après
    la confirmation du précédent."""
    d = vote_state()
    if not d["vote_uncredited"] or not BRIDGE.online() or "vote2" not in BRIDGE.caps():
        return                           # « vote2 » : le mod sait écarter un vote renvoyé (l'ancien « vote » le comptait deux fois)
    snapshot = list(d["vote_uncredited"])
    rest, blocked = [], set()
    for v in snapshot:
        if v["identity"] in blocked:
            rest.append(v)
            continue
        ok, message = await game_command("top-serveurs", f"vote @{v['identity']} {int(CFG['vote_reward'])} {int(v['stamp'])}")
        if ok:
            log.info("vote crédité : %s", message)
            continue
        blocked.add(v["identity"])
        v["tries"] = int(v.get("tries", 0)) + 1
        if ok is False:
            log.warning("vote de %s pas encore crédité (%s), essai %d", v.get("name"), message, v["tries"])
        if v["tries"] < VOTE_MAX_TRIES:
            rest.append(v)
        else:
            log.error("vote de %s abandonné après %d essais : à créditer à la main", v.get("name"), VOTE_MAX_TRIES)
    added = [v for v in d["vote_uncredited"] if not any(v is s for s in snapshot)]   # arrivés pendant les envois
    d["vote_uncredited"] = rest + added
    STATE.save()


async def vote_dm(discord_id, text):
    if not discord_id:
        return
    try:
        user = await bot.fetch_user(int(discord_id))
        await user.send(text)
    except discord.HTTPException:
        pass


async def vote_reward(name, identity, discord_id):
    d = vote_state()
    now = int(time.time())
    last = max((int(v.get("t", 0)) for v in d["votes"] if v.get("identity") == identity), default=0)
    if now - last < VOTE_MIN_GAP:
        d["vote_manual"].append({"t": now, "name": name, "identity": identity, "raison": f"vote précédent crédité il y a {(now - last) // 60} min"})
        STATE.save()
        log.error("vote de %s réclamé mais NON crédité : un vote lui a été crédité il y a %d min (à vérifier par le Staff)",
                  name, (now - last) // 60)
        await vote_dm(discord_id, "🗳️ Ton vote est bien arrivé, mais un autre vient de t'être compté il y a moins d'une heure : "
                                  "il est mis de côté pour l'état-major.")
        return
    d["votes"].append({"t": now, "name": name, "identity": identity, "discord": int(discord_id or 0)})
    del d["votes"][:-5000]
    d["vote_uncredited"].append({"name": name, "identity": identity, "stamp": now, "tries": 0})
    STATE.save()
    log.info("vote réclamé pour %s", name)
    total, mine = vote_month_count(), vote_month_count(identity)
    await vote_dm(discord_id, f"🗳️ Merci pour ton vote ! **+{CFG['vote_reward']} €** pour le coffre de la base, versés dès que le "
                              f"serveur de jeu l'enregistre. {mine} vote(s) à ton nom ce mois-ci, {total} pour la PAG. "
                              f"Tu pourras revoter dans 2 h : `/vote` en jeu ou ici.")
    channel = bot.channel_named(str(CFG.get("vote_channel", ""))) if CFG.get("vote_channel") else None
    if channel:
        try:
            await channel.send(f"🗳️ **{name}** a voté pour la PAG sur top-serveurs : +{CFG['vote_reward']} € pour la base "
                               f"({total} vote(s) ce mois-ci). Vote toi aussi : `/vote` en jeu ou ici.")
        except discord.HTTPException:
            pass
    await vote_credit()


async def vote_tick(session):
    d = vote_state()
    await vote_credit()
    if not CFG.get("top_serveurs_token"):
        return
    now = time.time()
    failures = 0
    for identity, p in list(d["vote_pending"].items()):
        until = p.get("until", 0)
        if until + VOTE_GRACE < now:
            d["vote_pending"].pop(identity, None)
            continue
        if p.get("next", 0) > now:
            continue
        if vote_homonym(identity, p["name"]):
            if not p.get("homonyme_log"):
                log.warning("vote : le pseudo %s est porté par plusieurs soldats, demande non réclamée", p["name"])
                p["homonyme_log"] = True
            p["next"] = now + 300
            continue
        claimed = await vote_claim(session, p["name"])
        if claimed is None:
            p["next"] = now + 60
            d["vote_pending"][identity] = d["vote_pending"].pop(identity, p)   # en fin de file : les autres passent d'abord
            failures += 1
            if failures >= 3:
                break                    # l'API elle-même semble injoignable : on reprend à la minute suivante
            continue
        failures = 0
        # chaque minute pendant les 20 premières minutes et autour de la fin du lien, toutes les 5 minutes entre les deux
        p["next"] = now + (60 if now - p.get("since", now) < 1200 or until - now < VOTE_GRACE else 300)
        if claimed == 1:
            d["vote_pending"].pop(identity, None)
            await vote_reward(p["name"], identity, p.get("discord"))
        elif claimed == 2 and not vote_recent(identity, p["name"], now):
            # « déjà réclamé », alors qu'aucun vote de ce soldat n'a été réclamé depuis plus de 2 h : seul ce bot réclame
            # avec notre jeton, c'est donc une de nos réclamations dont la réponse s'est perdue. On le crédite.
            d["vote_pending"].pop(identity, None)
            log.warning("vote de %s : réclamé sans réponse lors d'un essai précédent, crédité maintenant", p["name"])
            await vote_reward(p["name"], identity, p.get("discord"))
    STATE.save()


@bot.tree.command(name="vote", description="Voter pour la PAG sur top-serveurs : ton lien, pseudo de ton soldat déjà rempli")
@PARTOUT
async def cmd_vote(inter: discord.Interaction):
    link = STATE.links.get(str(inter.user.id))
    if not link or not link.get("identity"):
        embed = discord.Embed(title="🗳️ Voter pour la PAG", color=RED, description=(
            "Un vote n'est compté que s'il est relié à ton soldat.\n"
            "• **En jeu** : tape `/vote` dans le chat, tu reçois ton lien personnel.\n"
            "• **Ici** : lie d'abord ton compte Discord à ton soldat avec `/lier`, puis refais `/vote`."))
        return await inter.response.send_message(embed=embed, ephemeral=True)
    identity = link["identity"]
    name = vote_current_name(identity, link.get("name", ""))
    if vote_homonym(identity, name):
        return await inter.response.send_message(
            f"Un autre soldat porte le pseudo **{name}** : ton vote ne pourrait pas être relié à ta fiche. Vois avec l'état-major.",
            ephemeral=True)
    vote_register(identity, name, discord_id=inter.user.id)
    lignes = [f"Soldat : **{name}**. Ce pseudo est déjà rempli sur la page : ne le change pas, c'est lui qui relie le vote à ta fiche.",
              f"Un vote toutes les 2 h. Chaque vote compte sur ta fiche et verse **{CFG['vote_reward']} €** au coffre de la base.",
              "Le bouton vaut 2 h : passé ce délai, refais `/vote`.",
              f"Ce mois-ci : **{vote_month_count()}** vote(s) pour la PAG, dont **{vote_month_count(identity)}** à ton nom."]
    if not CFG.get("top_serveurs_token"):
        lignes.append("La prime automatique n'est pas encore activée sur le bot.")
    embed = discord.Embed(title="🗳️ Voter pour la PAG", description="\n".join(lignes), color=GOLD)
    view = ui.View()
    view.add_item(ui.Button(label="Voter sur top-serveurs", url=vote_link(name), emoji="🗳️"))
    await inter.response.send_message(embed=embed, view=view, ephemeral=True)


@bot.tree.command(name="formation", description="Demander une formation à un instructeur")
@SERVEUR
@app_commands.describe(certification="La certification que tu veux passer", disponibilites="Quand es-tu disponible ?")
@app_commands.choices(certification=[app_commands.Choice(name=label, value=label) for _, label in CERTIFS])
async def cmd_formation(inter: discord.Interaction, certification: app_commands.Choice[str], disponibilites: str = "à convenir"):
    ch = bot.channel_named(CFG["formations_channel"])
    if not ch:
        return await inter.response.send_message("Salon des formations introuvable.", ephemeral=True)
    embed = discord.Embed(title=f"🎓 Demande de formation · {certification.value}", color=GOLD, timestamp=dt.datetime.now(dt.timezone.utc))
    embed.add_field(name="Demandeur", value=inter.user.mention, inline=True)
    link = STATE.links.get(str(inter.user.id))
    embed.add_field(name="Soldat en jeu", value=link["name"] if link else "compte non lié", inline=True)
    embed.add_field(name="Disponibilités", value=disponibilites[:500], inline=False)
    embed.set_footer(text="La PAG · un instructeur habilité prend la demande avec le bouton")
    g = bot.guild()
    role = discord.utils.get(g.roles, name=CFG["instructor_role"]) if g else None
    await ch.send(content=role.mention if role else None, embed=embed, view=FormationView(), allowed_mentions=discord.AllowedMentions(roles=True))
    await inter.response.send_message("Ta demande est transmise aux instructeurs. Tu recevras un message privé quand l'un d'eux la prendra.", ephemeral=True)


CAPTURE_CHOICE = "capture"


async def order_type_autocomplete(inter: discord.Interaction, current: str):
    map_state()         # rafraîchit la mémoire des types et des lieux si le jeu vient de les envoyer
    choix = [app_commands.Choice(name="Capture d'une zone ennemie (au contact du front)", value=CAPTURE_CHOICE)]
    choix += [app_commands.Choice(name=t["nom"][:100], value=str(t["id"])) for t in MISSION_TYPES]
    return [c for c in choix if current.lower() in c.name.lower()][:25]


def capture_places():
    """J4 : les zones qu'une capture peut viser, [(code, libellé)]. Source : capture_targets du jeu (zones au contact, pas
    déjà visées, rien si le front est gelé) ; à défaut les zones au contact de front.json ; ancien mod : les secteurs
    ennemis, par leur nom."""
    state = BRIDGE.last_state or {}
    attacked = {z["c"] for z in CAMP.front.zones_contact() if z["a"]}
    targets = state.get("capture_targets")
    if isinstance(targets, list):
        places = []
        for t in targets[:60]:
            if isinstance(t, dict):
                code, label = str(t.get("c", "")), str(t.get("l", "") or t.get("c", ""))
            else:
                code = label = str(t)
            if code:
                places.append((code, label + (" · attaque en cours" if code in attacked else "")))
        return places
    if "front" in BRIDGE.caps() and CAMP.front.a_front():
        return [(z["c"], z["l"] + (" · attaque en cours" if z["a"] else "")) for z in CAMP.front.zones_contact()]
    names = [str(t.get("name", "")) for t in (state.get("sectors") or []) if isinstance(t, dict) and not t.get("ours")]
    return [(n, n) for n in names]


async def order_place_autocomplete(inter: discord.Interaction, current: str):
    map_state()
    if str(getattr(inter.namespace, "type", "")) == CAPTURE_CHOICE:
        # valeur = code de la zone (« S07 »), libellé = « Régina (S07) » ; le mod reçoit « ordonner capture <chef> S07 »
        seen, choix = set(), []
        for code, label in sorted(capture_places(), key=lambda p: p[1].lower()):
            if code in seen or current.lower() not in (label + " " + code).lower():
                continue
            seen.add(code)
            choix.append(app_commands.Choice(name=label[:100], value=code[:100]))
        return choix[:25]
    noms = [p["nom"] for p in MAP_PLACES]
    noms = sorted({n for n in noms if n and current.lower() in n.lower()})
    return [app_commands.Choice(name=n[:100], value=n[:100]) for n in noms[:25]]


async def order_chief_autocomplete(inter: discord.Interaction, current: str):
    if not await can_take_request(inter):
        return []  # les identités du jeu ne partent qu'aux officiers
    joueurs = [p for p in BRIDGE.players() if p.get("identity") and current.lower() in str(p.get("name", "")).lower()]
    return [app_commands.Choice(name=f"{p.get('gradeName', '')} {p.get('name', '')}".strip()[:100], value=str(p["identity"])[:100]) for p in joueurs[:25]]


@bot.tree.command(name="ordonner", description="Ordonner une mission : type, lieu, chef de mission (officiers)")
@SERVEUR
@app_commands.describe(type="Type de mission, ou capture d'une zone ennemie", lieu="Localité, ou zone ennemie au contact du front pour une capture",
                       chef="Chef de mission parmi les soldats en ligne ; vide = la mission reste au tableau (une capture a toujours un chef)")
@app_commands.autocomplete(type=order_type_autocomplete, lieu=order_place_autocomplete, chef=order_chief_autocomplete)
async def cmd_ordonner(inter: discord.Interaction, type: str, lieu: str, chef: str = ""):
    if not await can_take_request(inter):
        return await inter.response.send_message("Réservé aux officiers.", ephemeral=True)
    if "ordonner" not in BRIDGE.caps():
        return await inter.response.send_message("🔴 Le serveur de jeu est hors ligne, ou son mod ne sait pas encore ordonner une mission.", ephemeral=True)
    type, lieu, chef = type.strip(), " ".join(lieu.split()), chef.strip()
    if type != CAPTURE_CHOICE and not type.isdigit():
        return await inter.response.send_message("Choisis le type dans la liste proposée.", ephemeral=True)
    if type == CAPTURE_CHOICE and not chef:
        return await inter.response.send_message("Une capture a toujours un chef de mission : choisis un soldat en ligne.", ephemeral=True)
    if not lieu:
        return await inter.response.send_message("Choisis un lieu dans la liste proposée.", ephemeral=True)
    if chef and chef not in {str(p.get("identity")) for p in BRIDGE.players()}:
        return await inter.response.send_message("Ce soldat n'est plus en ligne : choisis le chef dans la liste proposée.", ephemeral=True)
    await inter.response.defer(ephemeral=True, thinking=True)
    auteur = await display_name(inter)
    ok, message = await game_command(auteur, f"ordonner {type} {chef or '-'} {lieu}")
    if ok:
        ch = bot.channel_named(CFG["command_channel"])
        if ch:
            try:
                await ch.send(embed=discord.Embed(description=f"🎯 {message}", color=GOLD), allowed_mentions=discord.AllowedMentions.none())
            except discord.HTTPException:
                pass
        return await inter.followup.send(f"✅ {message}", ephemeral=True)
    await inter.followup.send(f"⚠️ {message}", ephemeral=True)


@bot.tree.command(name="operation", description="Programmer une opération avec inscriptions (encadrement)")
@SERVEUR
@app_commands.describe(titre="Nom de l'opération", date="JJ/MM/AAAA", heure="HH:MM, heure de Paris", description="Briefing court")
async def cmd_operation(inter: discord.Interaction, titre: str, date: str, heure: str, description: str = ""):
    if not await can_panel(inter):
        return await inter.response.send_message("Réservé à l'encadrement.", ephemeral=True)
    try:
        when = dt.datetime.strptime(f"{date.strip()} {heure.strip().replace('h', ':')}", "%d/%m/%Y %H:%M").replace(tzinfo=PARIS)
    except ValueError:
        return await inter.response.send_message("Date ou heure illisible. Exemple : date `25/09/2026`, heure `21:00`.", ephemeral=True)
    if when.timestamp() < time.time():
        return await inter.response.send_message("Cette date est déjà passée.", ephemeral=True)
    ch = bot.channel_named(CFG["operations_channel"])
    if not ch:
        return await inter.response.send_message("Salon des opérations introuvable.", ephemeral=True)
    op = {"title": titre[:200], "ts": when.timestamp(), "desc": description[:1500], "author": inter.user.display_name, "signups": {}, "reminded": False, "closed": False}
    msg = await ch.send(content="@everyone", embed=operation_embed(op), view=OperationView(), allowed_mentions=discord.AllowedMentions(everyone=True))
    STATE.ops[str(msg.id)] = op
    STATE.save()
    await inter.response.send_message(f"Opération publiée dans {ch.mention}. Rappel automatique 30 minutes avant.", ephemeral=True)


# --- commandes Staff directes (équivalents rapides du panel) ---------------------------------------
@bot.tree.command(name="info", description="Fiche d'un soldat (Staff)")
@SERVEUR
@app_commands.describe(pseudo="Pseudo en jeu, exact")
async def cmd_info(inter: discord.Interaction, pseudo: str):
    await staff_command(inter, f"info {target_token(pseudo)}")


@bot.tree.command(name="ban", description="Bannir un joueur du serveur de jeu (Staff)")
@SERVEUR
@app_commands.describe(pseudo="Pseudo en jeu", jours="Durée en jours, 0 = définitif", motif="Motif")
async def cmd_ban(inter: discord.Interaction, pseudo: str, jours: int = 0, motif: str = "aucun motif donné"):
    await staff_command(inter, f"ban {target_token(pseudo)} {jours} {motif}", ("ban", pseudo, f"{'définitif' if jours == 0 else str(jours) + ' jour(s)'} · {motif}"))


@bot.tree.command(name="unban", description="Lever un bannissement (Staff)")
@SERVEUR
async def cmd_unban(inter: discord.Interaction, pseudo: str):
    await staff_command(inter, f"unban {target_token(pseudo)}", ("unban", pseudo, ""))


@bot.tree.command(name="kick", description="Expulser un joueur connecté (Staff)")
@SERVEUR
async def cmd_kick(inter: discord.Interaction, pseudo: str, motif: str = "aucun motif donné"):
    await staff_command(inter, f"kick {target_token(pseudo)} {motif}", ("kick", pseudo, motif))


@bot.tree.command(name="avertir", description="Avertissement officiel, compté sur la fiche (Staff)")
@SERVEUR
async def cmd_avertir(inter: discord.Interaction, pseudo: str, motif: str):
    await staff_command(inter, f"avertir {target_token(pseudo)} {motif}", ("avertir", pseudo, motif))


@bot.tree.command(name="grade", description="Changer le grade d'un soldat (Staff)")
@SERVEUR
@app_commands.choices(grade=[app_commands.Choice(name=GRADE_LABELS[i], value=GRADES[i]) for i in range(len(GRADES))])
async def cmd_grade(inter: discord.Interaction, pseudo: str, grade: app_commands.Choice[str]):
    await staff_command(inter, f"grade {target_token(pseudo)} {grade.value}")


@bot.tree.command(name="certif", description="Accorder ou retirer une certification (Staff)")
@SERVEUR
@app_commands.choices(certif=[app_commands.Choice(name=label, value=key) for key, label in CERTIFS],
                      action=[app_commands.Choice(name="accorder", value="on"), app_commands.Choice(name="retirer", value="off")])
async def cmd_certif(inter: discord.Interaction, pseudo: str, certif: app_commands.Choice[str], action: app_commands.Choice[str]):
    await staff_command(inter, f"certif {target_token(pseudo)} {certif.value} {action.value}")


@bot.tree.command(name="instructeur", description="Habiliter ou retirer un instructeur pour une certification (Staff)")
@SERVEUR
@app_commands.choices(certif=[app_commands.Choice(name=label, value=key) for key, label in CERTIFS],
                      action=[app_commands.Choice(name="habiliter", value="on"), app_commands.Choice(name="retirer", value="off")])
async def cmd_instructeur(inter: discord.Interaction, pseudo: str, certif: app_commands.Choice[str], action: app_commands.Choice[str]):
    await staff_command(inter, f"instructeur {target_token(pseudo)} {certif.value} {action.value}")


@bot.tree.command(name="staff", description="Donner ou retirer le statut Staff en jeu (Staff)")
@SERVEUR
@app_commands.choices(action=[app_commands.Choice(name="donner", value="on"), app_commands.Choice(name="retirer", value="off")])
async def cmd_staff(inter: discord.Interaction, pseudo: str, action: app_commands.Choice[str]):
    await staff_command(inter, f"staff {target_token(pseudo)} {action.value}")


@bot.tree.command(name="dire", description="Message à tous les joueurs en ligne (Staff)")
@SERVEUR
async def cmd_dire(inter: discord.Interaction, texte: str):
    await staff_command(inter, f"dire {texte}")


@bot.tree.command(name="mp", description="Message privé à un joueur en ligne (Staff)")
@SERVEUR
async def cmd_mp(inter: discord.Interaction, pseudo: str, texte: str):
    await staff_command(inter, f"mp {target_token(pseudo)} {texte}")


@bot.tree.command(name="sauver", description="Sauvegarder toutes les fiches et l'état du serveur (Staff)")
@SERVEUR
async def cmd_sauver(inter: discord.Interaction):
    await staff_command(inter, "sauver")


# ------------------------------------------------------------------------------------------------
# /suivi : le tableau de développement. Réservé au propriétaire du bot (et aux identifiants de board_users).
# ------------------------------------------------------------------------------------------------
BOARD_OWNERS = set()


async def board_allowed(inter):
    if not BOARD_OWNERS:
        try:
            info = await bot.application_info()
            if info.team:
                BOARD_OWNERS.add(info.team.owner_id)
            elif info.owner:
                BOARD_OWNERS.add(info.owner.id)
        except discord.HTTPException:
            pass
    return inter.user.id in BOARD_OWNERS or inter.user.id in [int(i) for i in CFG.get("board_users", [])]


@bot.tree.command(name="vote-reglages", description="Réglages des votes top-serveurs : jeton, salon d'annonce, prime (propriétaire du bot)")
@app_commands.describe(jeton="Jeton de votes de la fiche top-serveurs (page d'administration). Il n'est écrit que dans config.json.",
                       salon="Salon où annoncer chaque vote (« - » pour aucun)", prime="Euros versés au coffre de la base par vote")
@SERVEUR
async def cmd_vote_reglages(inter: discord.Interaction, jeton: str = None, salon: str = None, prime: app_commands.Range[int, 0, 10000] = None):
    if not await board_allowed(inter):
        return await inter.response.send_message("Réservé au propriétaire du bot.", ephemeral=True)
    changes = {}
    if jeton is not None:
        changes["top_serveurs_token"] = jeton.strip()
    if salon is not None:
        changes["vote_channel"] = "" if salon.strip() in ("-", "") else salon.strip()
    if prime is not None:
        changes["vote_reward"] = int(prime)
    if changes:
        try:
            with open(CONFIG_PATH, encoding="utf-8") as f:
                raw = json.load(f)
            raw.update(changes)
            tmp = CONFIG_PATH + ".tmp"
            with open(tmp, "w", encoding="utf-8") as f:
                json.dump(raw, f, indent=2, ensure_ascii=False)
            os.chmod(tmp, 0o640)
            os.replace(tmp, CONFIG_PATH)
        except OSError as ex:
            return await inter.response.send_message(f"Impossible d'écrire config.json : {ex}", ephemeral=True)
        CFG.update(changes)
        log.info("réglages des votes modifiés par %s : %s", inter.user.id, ", ".join(sorted(changes)))
    token = str(CFG.get("top_serveurs_token", ""))
    etat = [f"Jeton : {'renseigné (' + str(len(token)) + ' caractères)' if token else 'absent → pas de prime automatique'}",
            f"Salon d'annonce : {CFG.get('vote_channel') or 'aucun'}",
            f"Prime : {CFG.get('vote_reward', 150)} € par vote",
            f"Page de vote : {CFG.get('vote_url')}"]
    await inter.response.send_message(("Réglages enregistrés." + chr(10) if changes else "") + chr(10).join(etat), ephemeral=True)


def board_link():
    base = str(CFG.get("public_url", "")).rstrip("/")
    return base + "/suivi/" if base else ""


def board_embed(selected=None, note=""):
    embed = discord.Embed(title="🗂️ Suivi du développement", color=GOLD)
    if board_link():
        embed.url = board_link()
    for cle, nom in suivi.COLONNES:
        cartes = BOARD.cartes(cle)
        lignes = []
        for c in cartes[:8]:
            marque = "▶ " if selected and c["id"] == selected else ""
            lignes.append(f"{marque}`#{c['id']}` {c['titre']}"[:90])
        if len(cartes) > 8:
            lignes.append(f"… et {len(cartes) - 8} autre(s)")
        embed.add_field(name=f"{nom} · {len(cartes)}", value="\n".join(lignes) or "—", inline=True)
    carte = BOARD.trouver(selected) if selected else None
    if carte:
        detail = carte.get("detail") or "pas de détail"
        embed.add_field(name=f"Carte choisie : #{carte['id']} · {carte['titre']}"[:256], inline=False,
                        value=f"{suivi.NOMS[carte['colonne']]} · modifiée le {carte.get('modifie', '?')}\n{detail}"[:1000])
    versions = BOARD.data["versions"]
    pied = f"{len(versions)} mise(s) à jour dans l'historique"
    if versions:
        pied += f" · dernière : {versions[0]['version']} ({versions[0]['date'][:10]})"
    embed.set_footer(text=(note + " · " if note else "") + pied)
    return embed


class BoardCardModal(ui.Modal):
    def __init__(self, view, carte=None):
        super().__init__(title="Modifier la carte" if carte else "Nouvelle carte")
        self.board_view, self.carte = view, carte
        self.titre = ui.TextInput(label="Titre", max_length=suivi.TITRE_MAX, default=carte["titre"] if carte else None)
        self.detail = ui.TextInput(label="Détail (facultatif)", style=discord.TextStyle.paragraph, required=False,
                                   max_length=suivi.DETAIL_MAX, default=(carte.get("detail") or None) if carte else None)
        self.add_item(self.titre)
        self.add_item(self.detail)

    async def on_submit(self, inter):
        if self.carte:
            BOARD.modifier(self.carte["id"], self.titre.value, self.detail.value)
            note = f"#{self.carte['id']} modifiée"
        else:
            carte = BOARD.ajouter(self.titre.value, self.board_view.column, self.detail.value)
            self.board_view.selected = carte["id"]
            note = f"#{carte['id']} ajoutée dans {suivi.NOMS[carte['colonne']]}"
        await self.board_view.refresh(inter, note)


class BoardPublishModal(ui.Modal):
    def __init__(self, view):
        super().__init__(title="Publier une mise à jour")
        self.board_view = view
        self.version = ui.TextInput(label="Version (ex. 1.4.0)", max_length=suivi.VERSION_MAX)
        self.notes = ui.TextInput(label="Notes (facultatif)", style=discord.TextStyle.paragraph, required=False, max_length=suivi.NOTES_MAX)
        self.add_item(self.version)
        self.add_item(self.notes)

    async def on_submit(self, inter):
        note = await board_publish(self.version.value, self.notes.value)
        await self.board_view.refresh(inter, note)


async def board_publish(version, notes):
    """Archive les cartes terminées sous ce numéro de version et annonce la mise à jour. Retourne le compte rendu."""
    entree = BOARD.publier(version, notes)
    if not entree:
        return "Rien à publier : aucune carte dans « Terminé »"
    channel = bot.channel_named(CFG["updates_channel"])
    if not channel:
        return f"Version {entree['version']} archivée ({len(entree['cartes'])} carte(s)) — salon {CFG['updates_channel']} introuvable, pas d'annonce"
    embed = discord.Embed(title=f"🚀 Mise à jour {entree['version']}", color=GOLD, description=entree["notes"] or None)
    lignes, champs = [], []
    for c in entree["cartes"]:
        ligne = f"• {c['titre']}"
        if sum(len(x) + 1 for x in lignes) + len(ligne) > 1000:
            champs.append(lignes)
            lignes = []
        lignes.append(ligne)
    champs.append(lignes)
    for i, bloc in enumerate(champs[:5]):
        embed.add_field(name="Au programme" if i == 0 else "\u200b", value="\n".join(bloc), inline=False)
    if board_link():
        embed.add_field(name="Suivi du développement", value=board_link(), inline=False)
    embed.set_footer(text="La PAG · milsim semi-RP armée française")
    try:
        await channel.send(embed=embed)
    except discord.HTTPException as ex:
        return f"Version {entree['version']} archivée, mais l'annonce a échoué : {ex}"
    return f"Version {entree['version']} publiée : {len(entree['cartes'])} carte(s) archivée(s), annonce postée"


class BoardView(ui.View):
    def __init__(self, owner_id, selected=None, column="idee"):
        super().__init__(timeout=900)
        self.owner_id, self.selected, self.column, self.confirm_delete = owner_id, selected, column, False
        self.build()

    def build(self):
        self.clear_items()
        cartes = sorted(BOARD.cartes(), key=lambda c: c.get("modifie", ""), reverse=True)[:25]
        if cartes:
            pick = ui.Select(placeholder="Choisir une carte…", row=0,
                             options=[discord.SelectOption(label=f"#{c['id']} · {c['titre']}"[:100], value=str(c["id"]),
                                                           description=suivi.NOMS[c["colonne"]][:100], default=c["id"] == self.selected) for c in cartes])
            pick.callback = self.on_pick
            self.add_item(pick)
        move = ui.Select(placeholder="Déplacer la carte choisie vers… (ou colonne d'une nouvelle carte)", row=1,
                         options=[discord.SelectOption(label=nom, value=cle, default=cle == self.column) for cle, nom in suivi.COLONNES])
        move.callback = self.on_move
        self.add_item(move)
        for label, emoji, style, handler in (("Nouvelle carte", "➕", discord.ButtonStyle.success, self.on_new),
                                             ("Reculer", "◀", discord.ButtonStyle.secondary, self.on_back),
                                             ("Avancer", "▶", discord.ButtonStyle.primary, self.on_forward),
                                             ("Modifier", "✏️", discord.ButtonStyle.secondary, self.on_edit),
                                             ("Confirmer la suppression" if self.confirm_delete else "Supprimer", "🗑️", discord.ButtonStyle.danger, self.on_delete)):
            button = ui.Button(label=label, emoji=emoji, style=style, row=2)
            button.callback = handler
            self.add_item(button)
        publish = ui.Button(label="Publier une mise à jour", emoji="🚀", style=discord.ButtonStyle.success, row=3)
        publish.callback = self.on_publish
        self.add_item(publish)

    async def interaction_check(self, inter):
        if inter.user.id != self.owner_id:
            await inter.response.send_message("Ce panneau n'est pas le tien.", ephemeral=True)
            return False
        return True

    async def refresh(self, inter, note=""):
        if self.selected and not BOARD.trouver(self.selected):
            self.selected = None
        self.build()
        await inter.response.edit_message(embed=board_embed(self.selected, note), view=self)

    async def on_pick(self, inter):
        self.selected, self.confirm_delete = int(inter.data["values"][0]), False
        carte = BOARD.trouver(self.selected)
        if carte:
            self.column = carte["colonne"]
        await self.refresh(inter)

    async def on_move(self, inter):
        self.column, self.confirm_delete = inter.data["values"][0], False
        note = "Colonne choisie pour la prochaine carte : " + suivi.NOMS[self.column]
        if self.selected and BOARD.deplacer(self.selected, self.column):
            note = f"#{self.selected} → {suivi.NOMS[self.column]}"
        await self.refresh(inter, note)

    async def on_new(self, inter):
        await inter.response.send_modal(BoardCardModal(self))

    async def step(self, inter, pas):
        self.confirm_delete = False
        carte = BOARD.decaler(self.selected, pas) if self.selected else None
        if carte:
            self.column = carte["colonne"]
        await self.refresh(inter, f"#{carte['id']} → {suivi.NOMS[carte['colonne']]}" if carte else "Choisis d'abord une carte")

    async def on_back(self, inter):
        await self.step(inter, -1)

    async def on_forward(self, inter):
        await self.step(inter, 1)

    async def on_edit(self, inter):
        carte = BOARD.trouver(self.selected) if self.selected else None
        if not carte:
            return await self.refresh(inter, "Choisis d'abord une carte")
        await inter.response.send_modal(BoardCardModal(self, carte))

    async def on_delete(self, inter):
        if not self.selected:
            return await self.refresh(inter, "Choisis d'abord une carte")
        if not self.confirm_delete:
            self.confirm_delete = True
            return await self.refresh(inter, f"Supprimer #{self.selected} ? Clique encore pour confirmer")
        BOARD.supprimer(self.selected)
        note, self.selected, self.confirm_delete = f"#{self.selected} supprimée", None, False
        await self.refresh(inter, note)

    async def on_publish(self, inter):
        await inter.response.send_modal(BoardPublishModal(self))


suivi_group = app_commands.Group(name="suivi", description="Tableau de suivi du développement (réservé au développeur)",
                                 allowed_contexts=app_commands.AppCommandContext(guild=True, dm_channel=True, private_channel=False))
COLUMN_CHOICES = [app_commands.Choice(name=nom, value=cle) for cle, nom in suivi.COLONNES]


@suivi_group.command(name="tableau", description="Ouvrir le tableau : ajouter, déplacer, modifier, publier")
async def cmd_suivi_tableau(inter: discord.Interaction):
    if not await board_allowed(inter):
        link = board_link()
        return await inter.response.send_message("Le tableau se gère par son développeur." + (f" Tu peux le consulter ici : {link}" if link else ""), ephemeral=True)
    await inter.response.send_message(embed=board_embed(), view=BoardView(inter.user.id), ephemeral=True)


@suivi_group.command(name="ajouter", description="Ajouter une carte d'un coup")
@app_commands.describe(titre="Le titre de la carte", colonne="Colonne (Idée par défaut)", detail="Détail, facultatif")
@app_commands.choices(colonne=COLUMN_CHOICES)
async def cmd_suivi_ajouter(inter: discord.Interaction, titre: str, colonne: app_commands.Choice[str] = None, detail: str = ""):
    if not await board_allowed(inter):
        return await inter.response.send_message("Réservé au développeur.", ephemeral=True)
    carte = BOARD.ajouter(titre, colonne.value if colonne else "idee", detail)
    await inter.response.send_message(embed=board_embed(carte["id"], f"#{carte['id']} ajoutée dans {suivi.NOMS[carte['colonne']]}"),
                                      view=BoardView(inter.user.id, carte["id"], carte["colonne"]), ephemeral=True)


@suivi_group.command(name="publier", description="Publier une mise à jour : archive les cartes terminées et l'annonce")
@app_commands.describe(version="Numéro de version, par exemple 1.4.0", notes="Quelques mots sur cette mise à jour, facultatif")
async def cmd_suivi_publier(inter: discord.Interaction, version: str, notes: str = ""):
    if not await board_allowed(inter):
        return await inter.response.send_message("Réservé au développeur.", ephemeral=True)
    await inter.response.defer(ephemeral=True)
    await inter.followup.send(await board_publish(version, notes), ephemeral=True)


bot.tree.add_command(suivi_group)


if __name__ == "__main__":
    bot.run(TOKEN, log_handler=None)
