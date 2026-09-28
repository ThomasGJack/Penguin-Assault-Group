# -*- coding: utf-8 -*-
# Publie (ou met à jour) l'embed « Dotations » dans le salon 🎒┃dotations, avec le logo et un bouton vers le document.
import os, sys, io, json, uuid, urllib.request, urllib.error, winreg
from PIL import Image
sys.stdout.reconfigure(encoding='utf-8')

SALON = '1548799060511817739'   # 📢┃annonces
DOTATIONS = '1550200956098969722'
LIEN = 'https://penguinassaultgroup.fr/dotations/'
LOGO = r'E:\Projets\PAG-Tenues\logo.png'
MARQUE = 'annonce des dotations'   # sert à retrouver le message pour le mettre à jour


def token():
    t = os.environ.get('DISCORD_TOKEN')
    if t:
        return t
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, 'Environment') as k:
        return winreg.QueryValueEx(k, 'DISCORD_TOKEN')[0]


AUTH = {'Authorization': 'Bot ' + token(), 'User-Agent': 'PAG-Setup (local, 1.0)'}


def api(method, path, body=None, headers=None):
    h = dict(AUTH)
    h.update(headers or {'Content-Type': 'application/json'})
    data = body if isinstance(body, (bytes, type(None))) else json.dumps(body).encode()
    try:
        return json.load(urllib.request.urlopen(urllib.request.Request('https://discord.com/api/v10' + path, data=data, headers=h, method=method)))
    except urllib.error.HTTPError as e:
        print('ERREUR', e.code, e.read().decode('utf-8', 'replace')[:400])
        raise SystemExit(1)


im = Image.open(LOGO)
im.thumbnail((320, 320), Image.LANCZOS)
buf = io.BytesIO()
im.save(buf, 'PNG', optimize=True)

charge = {
 'content': '@everyone',
 'allowed_mentions': {'parse': ['everyone']},
 'embeds': [{
  'title': '🎒 Les dotations de la PAG sont disponibles',
  'url': LIEN,
  'color': 0xC9A227,
  'description': ("Soldats, le **guide des dotations** est publié. À partir de maintenant, il est **obligatoire** pour toute sortie en mission."),
  'fields': [
   {'name': '📍 Où le trouver', 'value': "Salon <#" + DOTATIONS + ">, dans la catégorie 🎓 FORMATIONS.", 'inline': False},
   {'name': '📖 Ce qu\'il contient', 'value': "• La tenue Centre Europe, la même pour tous, de la recrue à l'officier.\n• Le béret de chaque classe.\n• La dotation commune, puis une fiche par certification : fusilier, mule, grenadier, mitrailleur, tireur de précision, infirmier, chef de groupe, opérateur radio, logisticien, équipages.", 'inline': False},
   {'name': '⚠️ La règle à retenir', 'value': "Ce qui ne figure pas dans le guide ne se porte pas. Avant chaque départ, vérifie ta tenue, ton béret et ton sac.", 'inline': False},
   {'name': '🕐 Un doute ?', 'value': "Demande à ton chef de groupe ou à un instructeur avant de partir, pas sur le terrain.\n\n*Serrés, on tient.*", 'inline': False}
  ],
  'thumbnail': {'url': 'attachment://logo.png'},
  'footer': {'text': 'La PAG · ' + MARQUE + ' · milsim semi-RP armée française'}
 }],
 'components': [{'type': 1, 'components': [{'type': 2, 'style': 5, 'label': 'Ouvrir le guide des dotations', 'emoji': {'name': '📖'}, 'url': LIEN}]}],
 'attachments': [{'id': 0, 'filename': 'logo.png'}]
}

sep = uuid.uuid4().hex
corps = b''.join([
 ('--%s\r\nContent-Disposition: form-data; name="payload_json"\r\nContent-Type: application/json\r\n\r\n' % sep).encode(),
 json.dumps(charge).encode('utf-8'),
 ('\r\n--%s\r\nContent-Disposition: form-data; name="files[0]"; filename="logo.png"\r\nContent-Type: image/png\r\n\r\n' % sep).encode(),
 buf.getvalue(),
 ('\r\n--%s--\r\n' % sep).encode()])
entetes = {'Content-Type': 'multipart/form-data; boundary=' + sep}

moi = api('GET', '/users/@me')['id']
anciens = [m for m in api('GET', '/channels/%s/messages?limit=50' % SALON)
           if m['author']['id'] == moi and any(MARQUE in (e.get('footer') or {}).get('text', '') for e in m.get('embeds', []))]
if anciens:
    m = api('PATCH', '/channels/%s/messages/%s' % (SALON, anciens[0]['id']), corps, entetes)
    print('mis à jour :', m['id'])
else:
    m = api('POST', '/channels/%s/messages' % SALON, corps, entetes)
    print('publié :', m['id'])
print('ping everyone :', m.get('mention_everyone'))
print('embeds :', len(m['embeds']), '· bouton :', bool(m.get('components')), '· logo :', bool(m['embeds'][0].get('thumbnail')))
