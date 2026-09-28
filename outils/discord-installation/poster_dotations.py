# -*- coding: utf-8 -*-
# Publie (ou met à jour) l'embed « Dotations » dans le salon 🎒┃dotations, avec le logo et un bouton vers le document.
import os, sys, io, json, uuid, urllib.request, urllib.error, winreg
from PIL import Image
sys.stdout.reconfigure(encoding='utf-8')

SALON = '1550200956098969722'
LIEN = 'https://penguinassaultgroup.fr/dotations/'
LOGO = r'E:\Projets\PAG-Tenues\logo.png'
MARQUE = 'guide des dotations'   # sert à retrouver le message pour le mettre à jour


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
 'embeds': [{
  'title': '🎒 Dotations de la PAG',
  'url': LIEN,
  'color': 0xC9A227,
  'description': ("Tout ce qu'un soldat de la **PAG** doit porter et emporter, réuni dans un seul document illustré.\n\n"
                  "Avant ta prochaine mission, ouvre-le et vérifie ta tenue et ton sac : **ce qui n'y figure pas ne se porte pas.**"),
  'fields': [
   {'name': '🪖 Habillement', 'value': "La tenue Centre Europe, la même pour tous, de la recrue à l'officier. Casques des équipages et des pilotes.", 'inline': False},
   {'name': '🎖️ Bérets', 'value': "Un béret par classe : recrue, soldat, logisticien, instructeur, officier, vétéran, invité.", 'inline': False},
   {'name': '🔫 Arsenal par certification', 'value': "La dotation commune, puis une fiche par rôle : fusilier, mule, grenadier, mitrailleur, tireur de précision, infirmier, chef de groupe, opérateur radio, logisticien, équipages.", 'inline': False},
   {'name': '🎨 Trois couleurs à retenir', 'value': "🟨 **Imposé** · 🟦 **Au choix** · 🟩 **Personnalisation libre**", 'inline': False}
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
print('embeds :', len(m['embeds']), '· bouton :', bool(m.get('components')), '· logo :', bool(m['embeds'][0].get('thumbnail')))
