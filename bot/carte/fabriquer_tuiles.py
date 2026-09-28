# -*- coding: utf-8 -*-
# Fabrique la carte satellite d'Everon pour la carte de situation, à partir des données du jeu installé :
#   1. assemble les 2 500 tuiles « supertexture » du terrain (1 m par pixel, 12 800 x 12 800) ;
#   2. met de l'eau sur la mer (le terrain ne contient que le fond marin), d'après la carte en relief du jeu ;
#   3. découpe le tout en pyramide de tuiles de 256 px (zooms 0 à 6, 1 m par pixel au zoom 6).
# Usage : fabriquer_tuiles.py <dossier des tuiles .edds du terrain> <carte en relief .png (4096)> <dossier de sortie>
# Dépendances : pip install pillow lz4 numpy
import io, os, struct, sys
import lz4.block
import numpy as np
from PIL import Image, ImageFilter

Image.MAX_IMAGE_PIXELS = None
terrain, relief, sortie = sys.argv[1], sys.argv[2], sys.argv[3]
COTE, PAS, TAILLE, MONDE, ZMAX = 50, 256, 12800, 16384, 6


def lire(chemin):
    """Le plus grand mip d'une texture .edds (DDS + blocs COPY / LZ4 chaînés par morceaux de 64 Ko)."""
    d = open(chemin, 'rb').read()
    mips = max(1, struct.unpack('<I', d[28:32])[0])
    header_size = 4 + 124 + (20 if d[84:88] == b'DX10' else 0)
    header = bytearray(d[:header_size])
    pos = header_size
    table = []
    for _ in range(mips):
        table.append((d[pos:pos + 4], struct.unpack('<i', d[pos + 4:pos + 8])[0]))
        pos += 8
    for index, (tag, size) in enumerate(table):
        block = d[pos:pos + size]
        pos += size
        if index != mips - 1:
            continue
        if tag == b'COPY':
            data = block
        else:
            target = struct.unpack('<I', block[:4])[0]
            out, p, dictionary = bytearray(), 4, b''
            while p < len(block) and len(out) < target:
                csize = struct.unpack('<I', block[p:p + 4])[0] & 0x7FFFFFFF
                p += 4
                out += lz4.block.decompress(block[p:p + csize], uncompressed_size=min(65536, target - len(out)), dict=dictionary)
                p += csize
                dictionary = bytes(out[-65536:])
            data = bytes(out)
    struct.pack_into('<I', header, 28, 1)
    image = Image.open(io.BytesIO(bytes(header) + data))
    image.load()
    return image.convert('RGB')


# 1. assemblage : tuile 0 en bas à gauche, rangées vers le nord ; chaque tuile est stockée le sud en haut, avec une
#    bordure répliquée de 4 px qu'on retire (sinon une couture tous les 256 m)
print('assemblage…')
image = Image.new('RGB', (TAILLE, TAILLE))
for n in range(COTE * COTE):
    tuile = lire(os.path.join(terrain, 'Eden_%d_supertexture.edds' % n)).transpose(Image.FLIP_TOP_BOTTOM)
    tuile = tuile.crop((4, 4, PAS - 4, PAS - 4)).resize((PAS, PAS), Image.LANCZOS)
    image.paste(tuile, ((n % COTE) * PAS, (COTE - 1 - n // COTE) * PAS))

# 2. la mer : masque tiré de la carte en relief du jeu (mer turquoise), eau sombre au large, plus claire près des côtes
print('mer…')
carte = np.asarray(Image.open(relief).convert('RGB')).astype(np.int16)
mer = ((carte[:, :, 2] - carte[:, :, 0]) > 40).astype(np.uint8) * 255
masque = Image.fromarray(mer).filter(ImageFilter.GaussianBlur(1.2))
large = Image.fromarray(mer).filter(ImageFilter.GaussianBlur(40))          # ~125 m : 0 à la côte, 255 au large
COTE_EAU = np.array([38, 78, 92], np.float32)
LARGE_EAU = np.array([12, 20, 36], np.float32)
BANDE = 800
for y in range(0, TAILLE, BANDE):
    boite = (0, y, TAILLE, y + BANDE)
    e = 4096 / TAILLE
    source = (0, y * e, 4096, (y + BANDE) * e)
    m = np.asarray(masque.resize((TAILLE, BANDE), Image.BILINEAR, box=source)).astype(np.float32)[:, :, None] / 255
    g = np.asarray(large.resize((TAILLE, BANDE), Image.BILINEAR, box=source)).astype(np.float32)[:, :, None] / 255
    sol = np.asarray(image.crop(boite)).astype(np.float32)
    profondeur = np.clip((g - 0.5) * 2, 0, 1)                                # 0 au rivage, 1 au large
    eau = COTE_EAU * (1 - profondeur) + LARGE_EAU * profondeur
    opacite = m * (0.72 + 0.28 * profondeur)                                 # le fond se devine près du bord
    image.paste(Image.fromarray((sol * (1 - opacite) + eau * opacite).clip(0, 255).astype(np.uint8)), (0, y))

image.resize((2048, 2048), Image.LANCZOS).save(os.path.join(os.path.dirname(sortie.rstrip('/\\')), 'everon_apercu.jpg'), quality=88)

# 3. pyramide de tuiles : le monde fait 16 384 m (256 px x 2^6), l'île est calée en bas à gauche (origine du jeu)
print('tuiles…')
total = 0
for z in range(ZMAX, -1, -1):
    echelle = 2 ** (z - ZMAX)
    cote = int(TAILLE * echelle)
    niveau = image if z == ZMAX else image.resize((cote, cote), Image.LANCZOS)
    monde = int(MONDE * echelle)
    haut = monde - cote                                                       # l'image commence à cette rangée du monde
    for ty in range(haut // PAS, monde // PAS):
        for tx in range(0, (cote + PAS - 1) // PAS):
            boite = (tx * PAS, ty * PAS - haut, tx * PAS + PAS, ty * PAS - haut + PAS)
            morceau = Image.new('RGB', (PAS, PAS), tuple(int(v) for v in LARGE_EAU))
            morceau.paste(niveau.crop((max(boite[0], 0), max(boite[1], 0), min(boite[2], cote), min(boite[3], cote))), (max(-boite[0], 0), max(-boite[1], 0)))
            dossier = os.path.join(sortie, str(z), str(tx))
            os.makedirs(dossier, exist_ok=True)
            morceau.save(os.path.join(dossier, '%d.webp' % ty), quality=82, method=4)
            total += 1
    print('zoom', z, ':', cote, 'px')
print(total, 'tuiles dans', sortie)
