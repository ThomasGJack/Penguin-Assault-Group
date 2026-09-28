# -*- coding: utf-8 -*-
# Démonstration locale de la carte de situation : sert la page et un faux état qui bouge. Aucun lien avec le bot ni le serveur.
# Usage : python demo.py  puis ouvrir http://localhost:8790/
import json, math, os, time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

ICI = os.path.dirname(os.path.abspath(__file__))
T0 = time.time()
LIEUX = [("Levie", 7464, 4738), ("Laruns", 7558, 5541), ("La Valette", 6737, 5650), ("Camurac", 6589, 3114), ("Chotain", 7080, 6030),
         ("Régina", 7180, 2330), ("Durras", 8690, 2690), ("Morton", 5120, 4020), ("Figari", 5240, 5330), ("Provins", 5560, 6050),
         ("Saint-Pierre", 9690, 1560), ("Montignac", 4780, 7060), ("Entre-Deux", 5830, 7120), ("Saint-Philippe", 4520, 10760)]


def carre(x, z):
    return "%03d %03d" % (int(x // 100), int(z // 100))


def etat():
    t = time.time() - T0
    # une escouade qui roule de la base vers Laruns, un binôme d'éclaireurs, un soldat resté à la base
    k = (math.sin(t / 40) + 1) / 2
    ex, ez = 7414 + (7558 - 7414) * k, 4333 + (5541 - 4333) * k
    soldats = [
        {"nom": "Jack Lania", "grade": "Cne", "carre": carre(ex, ez)},
        {"nom": "Alpha", "grade": "Sgt", "carre": carre(ex + 20, ez - 30)},
        {"nom": "Bravo", "grade": "Cpl", "carre": carre(ex - 40, ez + 60)},
        {"nom": "Charlie", "grade": "1cl", "carre": carre(ex + 110, ez + 10)},
        {"nom": "Delta", "grade": "Cpl", "carre": carre(6737 + 300 * math.cos(t / 25), 5650 + 300 * math.sin(t / 25))},
        {"nom": "Echo", "grade": "Sdt", "carre": carre(6737 + 300 * math.cos(t / 25) + 35, 5650 + 300 * math.sin(t / 25))},
        {"nom": "Foxtrot", "grade": "Rec", "carre": carre(7420, 4340)},
    ]
    missions = [
        {"id": "M07", "titre": "Cache à saboter", "lieu": "Laruns", "carre": carre(7558, 5541), "difficulte": 2, "chef": "Jack Lania"},
        {"id": "M08", "titre": "Documents à récupérer", "lieu": "Durras", "carre": carre(8690, 2690), "difficulte": 3, "chef": ""},
    ]
    secteurs = [{"nom": n, "carre": carre(x, z), "nous": n in ("La Valette", "Camurac"), "attaque": n == "Camurac" and int(t) % 40 < 25} for n, x, z in LIEUX if n != "Levie"]
    return {"en_ligne": True, "silence": 2, "heure_jeu": "21 h 40", "soldats": soldats, "missions": missions, "secteurs": secteurs,
            "lieux": [{"nom": n, "x": x, "z": z} for n, x, z in LIEUX]}


class Page(SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=ICI, **k)

    def do_GET(self):
        if self.path.split('?')[0].endswith('/api/carte'):
            corps = json.dumps(etat()).encode()
            self.send_response(200)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Content-Length', str(len(corps)))
            self.end_headers()
            self.wfile.write(corps)
            return
        super().do_GET()

    def log_message(self, *a):
        pass


if __name__ == '__main__':
    print('Carte de démonstration : http://localhost:8790/')
    ThreadingHTTPServer(('127.0.0.1', 8790), Page).serve_forever()
