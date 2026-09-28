# -*- coding: utf-8 -*-
import os, sys, json, urllib.request, winreg
sys.stdout.reconfigure(encoding='utf-8')
def token():
    t = os.environ.get('DISCORD_TOKEN')
    if t: return t
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, 'Environment') as k:
        return winreg.QueryValueEx(k, 'DISCORD_TOKEN')[0]
G = '1548785514080108574'
H = {'Authorization': 'Bot ' + token(), 'User-Agent': 'PAG-Setup (local, 1.0)', 'Content-Type': 'application/json'}
def api(method, path, body=None):
    data = json.dumps(body).encode() if body is not None else None
    return json.load(urllib.request.urlopen(urllib.request.Request('https://discord.com/api/v10' + path, data=data, headers=H, method=method)))
ch = api('GET', '/guilds/%s/channels' % G)
if any('dotations' in c['name'] for c in ch):
    print('existe déjà'); raise SystemExit
regles = [c for c in ch if c['id'] == '1548799050084782211'][0]   # 📜┃règles : lecture seule, on reprend ses permissions
new = api('POST', '/guilds/%s/channels' % G, {
    'name': '🎒┃dotations', 'type': 0, 'parent_id': '1548799128450891836',
    'topic': "Guide d'habillement et d'arsenal de la PAG : la tenue, le béret de sa classe et la dotation de chaque certification.",
    'permission_overwrites': regles['permission_overwrites']})
print('créé :', new['name'], new['id'])
