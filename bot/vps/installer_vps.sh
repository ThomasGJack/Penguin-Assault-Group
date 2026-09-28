#!/usr/bin/env bash
# PAG-Bot : installation (ou mise à jour) sur un VPS Debian 12 / Ubuntu. À lancer en root, depuis le dossier envoyé par scp.
#   bash installer_vps.sh            installe ou met à jour le bot, demande le jeton s'il n'est pas encore enregistré
#   bash installer_vps.sh --jeton    redemande le jeton (après un « Reset Token » dans le portail Discord)
# Le jeton n'est jamais affiché ni passé en argument : il est saisi en masqué et rangé dans /etc/pag-bot.env (root, 600).
set -euo pipefail

ICI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST=/opt/pag-bot
ENVF=/etc/pag-bot.env
UNIT=/etc/systemd/system/pag-bot.service
JEU_IP="${JEU_IP:-IP_DU_SERVEUR_DE_JEU}"   # serveur de jeu LobbyHost : seul autorisé à joindre l'API si ufw est actif

[ "$(id -u)" -eq 0 ] || { echo "Lance ce script en root (ou avec sudo)."; exit 1; }
for f in bot.py medical.py suivi.py campagne.py trace_image.py requirements.txt config.json; do
  [ -f "$ICI/$f" ] || { echo "Fichier manquant à côté du script : $f"; exit 1; }
done

echo "== 1/6 Paquets système"
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq python3 python3-venv python3-pip ca-certificates curl >/dev/null

echo "== 2/6 Utilisateur et dossier"
id pagbot >/dev/null 2>&1 || useradd --system --home-dir "$DEST" --shell /usr/sbin/nologin pagbot
install -d -o pagbot -g pagbot -m 750 "$DEST"
install -o pagbot -g pagbot -m 640 "$ICI/bot.py" "$DEST/bot.py"
install -o pagbot -g pagbot -m 640 "$ICI/medical.py" "$DEST/medical.py"
install -o pagbot -g pagbot -m 640 "$ICI/suivi.py" "$DEST/suivi.py"
install -o pagbot -g pagbot -m 640 "$ICI/campagne.py" "$DEST/campagne.py"
install -o pagbot -g pagbot -m 640 "$ICI/trace_image.py" "$DEST/trace_image.py"
install -o pagbot -g pagbot -m 640 "$ICI/requirements.txt" "$DEST/requirements.txt"
if [ -f "$DEST/config.json" ]; then
  echo "   config.json déjà présent sur le VPS : conservé (supprime-le avant de relancer pour le remplacer)"
else
  install -o pagbot -g pagbot -m 640 "$ICI/config.json" "$DEST/config.json"
fi
# Les comptes liés (/lier) : repris du PC s'ils existent et qu'il n'y en a pas encore ici
if [ -f "$ICI/state.json" ] && [ ! -f "$DEST/state.json" ]; then
  install -o pagbot -g pagbot -m 640 "$ICI/state.json" "$DEST/state.json"
fi

echo "== 3/6 Environnement Python"
[ -x "$DEST/venv/bin/python" ] || runuser -u pagbot -- python3 -m venv "$DEST/venv"
runuser -u pagbot -- "$DEST/venv/bin/pip" install --quiet --upgrade pip
runuser -u pagbot -- "$DEST/venv/bin/pip" install --quiet -r "$DEST/requirements.txt"

echo "== 4/6 Jeton du bot"
# La saisie du jeton est un outil à part, réutilisable : pag-bot-jeton (root). Jamais d'argument, jamais d'affichage.
cat > /usr/local/sbin/pag-bot-jeton <<'OUTIL'
#!/usr/bin/env bash
set -euo pipefail
[ "$(id -u)" -eq 0 ] || { echo "Lance : sudo pag-bot-jeton"; exit 1; }
[ -t 0 ] || { echo "Il faut un terminal (ssh -t) pour saisir le jeton."; exit 1; }
echo "Colle le jeton du bot (portail Discord > Bot > Reset Token). La saisie est masquée."
read -rs -p "Jeton : " JETON; echo
JETON="$(printf '%s' "$JETON" | tr -d '[:space:]')"
[ "${#JETON}" -ge 50 ] || { echo "Jeton trop court, rien n'a été enregistré."; exit 1; }
umask 077
printf 'DISCORD_TOKEN=%s
PYTHONUTF8=1
' "$JETON" > /etc/pag-bot.env
unset JETON
chown root:root /etc/pag-bot.env; chmod 600 /etc/pag-bot.env
echo "Jeton enregistré dans /etc/pag-bot.env (lisible par root seulement)."
systemctl restart pag-bot
sleep 5
if systemctl is-active --quiet pag-bot; then
  echo "PAG-Bot tourne. Dernières lignes du journal :"
  journalctl -u pag-bot -n 8 --no-pager | sed 's/^/   /'
else
  echo "Le service n'a pas démarré. Journal :"; journalctl -u pag-bot -n 20 --no-pager; exit 1
fi
OUTIL
chmod 750 /usr/local/sbin/pag-bot-jeton

JETON_OK=0
grep -q '^DISCORD_TOKEN=.\{20,\}' "$ENVF" 2>/dev/null && JETON_OK=1
if [ "${1:-}" = "--jeton" ]; then JETON_OK=0; fi
if [ "$JETON_OK" -eq 1 ]; then
  echo "   Jeton déjà enregistré : conservé (sudo pag-bot-jeton pour le changer)."
elif [ ! -t 0 ]; then
  echo "   Pas de terminal : le jeton sera saisi plus tard avec  sudo pag-bot-jeton"
  [ -f "$ENVF" ] || { umask 077; printf 'PYTHONUTF8=1
' > "$ENVF"; chmod 600 "$ENVF"; }
fi

echo "== 5/6 Service systemd"
cat > "$UNIT" <<'UNITE'
[Unit]
Description=PAG-Bot (Discord <-> Arma Reforger)
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=pagbot
Group=pagbot
WorkingDirectory=/opt/pag-bot
EnvironmentFile=/etc/pag-bot.env
ExecStart=/opt/pag-bot/venv/bin/python /opt/pag-bot/bot.py
Restart=always
RestartSec=10
# Durcissement : le bot n'écrit que dans son dossier
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
PrivateTmp=true
ReadWritePaths=/opt/pag-bot

[Install]
WantedBy=multi-user.target
UNITE
chmod 644 "$UNIT"
systemctl daemon-reload
systemctl enable pag-bot >/dev/null 2>&1
if [ "$JETON_OK" -eq 1 ]; then
  systemctl restart pag-bot
elif [ -t 0 ]; then
  /usr/local/sbin/pag-bot-jeton && JETON_OK=1
fi

echo "== 6/6 Pare-feu"
PORT="$(python3 -c "import json;print(json.load(open('$DEST/config.json'))['port'])" 2>/dev/null || echo 8787)"
if command -v ufw >/dev/null 2>&1 && ufw status | grep -q "Status: active"; then
  ufw allow from "$JEU_IP" to any port "$PORT" proto tcp >/dev/null
  echo "   ufw actif : port $PORT ouvert pour le serveur de jeu ($JEU_IP) uniquement."
else
  echo "   ufw n'est pas actif : le port $PORT est ouvert à tous (l'API reste protégée par le secret)."
  echo "   Si ton hébergeur a un pare-feu dans son panneau, ouvre-y le port TCP $PORT."
fi

sleep 4
echo
IP="$(curl -4 -s --max-time 5 https://api.ipify.org || hostname -I | awk '{print $1}')"
if [ "$JETON_OK" -ne 1 ]; then
  echo "Installation terminée, service activé mais PAS démarré : il manque le jeton."
  echo "À faire :  sudo pag-bot-jeton"
  echo "Ensuite, dans discord.json sur LobbyHost :   \"bridge_url\": \"http://$IP:$PORT/\""
  exit 0
fi
if systemctl is-active --quiet pag-bot; then
  echo "PAG-Bot tourne. Démarrage automatique au reboot : activé."
  echo "Dernières lignes du journal :"
  journalctl -u pag-bot -n 8 --no-pager | sed 's/^/   /'
  echo
  echo "À mettre dans discord.json sur LobbyHost :   \"bridge_url\": \"http://$IP:$PORT/\""
  echo "Test depuis ton PC :                        http://$IP:$PORT/api/status"
else
  echo "Le service n'a pas démarré. Journal :"
  journalctl -u pag-bot -n 25 --no-pager
  exit 1
fi
echo
echo "Commandes utiles :  journalctl -u pag-bot -f   |   systemctl restart pag-bot   |   systemctl status pag-bot"
