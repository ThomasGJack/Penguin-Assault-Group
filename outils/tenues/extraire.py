# -*- coding: utf-8 -*-
# Récupère les captures collées dans la conversation (transcript) vers captures/, sans doublon.
import json, base64, os, hashlib, glob, io
from PIL import Image as PILImage
T = r"C:\Users\goule\.claude\projects\C--Users-goule-Documents\e1f1bd8a-58e8-4a5d-8f58-bc003292c2d8.jsonl"
out = 'captures'
seen = {hashlib.md5(open(f, 'rb').read()).hexdigest() for f in glob.glob(out + '/**/*.png', recursive=True)}
n = len(glob.glob(out + '/cap_*.png')); new = []
def walk(o):
    global n
    if isinstance(o, dict):
        s = o.get('source')
        if o.get('type') == 'image' and isinstance(s, dict) and s.get('type') == 'base64':
            raw = base64.b64decode(s['data']); h = hashlib.md5(raw).hexdigest()
            if h not in seen and PILImage.open(io.BytesIO(raw)).width in list(range(495, 550)) + list(range(1600, 1760)):  # les planches de contrôle sont plus larges
                seen.add(h); n += 1
                name = 'cap_%02d.png' % n
                open(os.path.join(out, name), 'wb').write(raw); new.append(name)
        for v in o.values(): walk(v)
    elif isinstance(o, list):
        for v in o: walk(v)
for l in open(T, encoding='utf-8'):
    if '"base64"' in l:
        try: walk(json.loads(l))
        except Exception: pass
print('nouvelles :', new)
