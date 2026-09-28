# -*- coding: utf-8 -*-
# Lance setup_discord.py avec le jeton lu dans les variables d'environnement utilisateur (jamais affiché), puis
# met à jour sur place l'annonce d'ouverture (post_once ne la retouche pas).
import os, sys, json, runpy, winreg, urllib.request
sys.stdout.reconfigure(encoding='utf-8')
if not os.environ.get('DISCORD_TOKEN'):
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, 'Environment') as k:
        os.environ['DISCORD_TOKEN'] = winreg.QueryValueEx(k, 'DISCORD_TOKEN')[0]
os.environ.setdefault('DISCORD_GUILD', '1548785514080108574')
mod = runpy.run_path('setup_discord.py', run_name='setup')
mod['main']()

H = {'Authorization': 'Bot ' + os.environ['DISCORD_TOKEN'], 'User-Agent': 'PAG-Setup (local, 1.0)', 'Content-Type': 'application/json'}
def api(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    return json.load(urllib.request.urlopen(urllib.request.Request('https://discord.com/api/v10' + path, data=data, headers=H, method=method)))
ANNONCES = '1548799060511817739'
for m in api('GET', '/channels/%s/messages?limit=50' % ANNONCES):
    for e in m.get('embeds', []):
        if e.get('title', '').startswith('🪖 Ouverture'):
            api('PATCH', '/channels/%s/messages/%s' % (ANNONCES, m['id']), {'embeds': [{'title': e['title'], 'description': mod['ANNOUNCE_TEXT'], 'color': mod['COLOR_EMBED']}]})
            print("annonce d'ouverture mise à jour sur place :", m['id'])
