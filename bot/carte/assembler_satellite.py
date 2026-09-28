# -*- coding: utf-8 -*-
# Assemble la texture satellite d'Everon à partir des 2 500 tuiles « supertexture » du terrain du jeu
# (worlds/Eden/Eden/.Data/Eden_<n>_supertexture.edds, 50 x 50 tuiles de 256 m, 256 px chacune = 1 m par pixel).
# Usage : assembler_satellite.py <dossier des tuiles> <sortie.png> [ordre]
#   ordre = « bas » (tuile 0 en bas à gauche, rangées vers le nord) ou « haut » (tuile 0 en haut à gauche)
import io, os, struct, sys
import lz4.block
from PIL import Image

dossier, sortie = sys.argv[1], sys.argv[2]
ordre = 'bas'
if len(sys.argv) > 3:
    ordre = sys.argv[3]
COTE = 50


def lire(chemin):
    """Le plus grand mip d'une tuile .edds (DDS + blocs COPY / LZ4 chaînés par morceaux de 64 Ko)."""
    d = open(chemin, 'rb').read()
    height, width = struct.unpack('<I', d[12:16])[0], struct.unpack('<I', d[16:20])[0]
    mips = max(1, struct.unpack('<I', d[28:32])[0])
    dx10 = d[84:88] == b'DX10'
    header_size = 4 + 124 + (20 if dx10 else 0)
    header = bytearray(d[:header_size])
    pos = header_size
    table = []
    for _ in range(mips):
        table.append((d[pos:pos + 4], struct.unpack('<i', d[pos + 4:pos + 8])[0]))
        pos += 8
    data = None
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


premiere = lire(os.path.join(dossier, 'Eden_0_supertexture.edds'))
pas = premiere.size[0]
print('tuile :', premiere.size, '-> image de', pas * COTE, 'px')
image = Image.new('RGB', (pas * COTE, pas * COTE))
for n in range(COTE * COTE):
    tuile = lire(os.path.join(dossier, 'Eden_%d_supertexture.edds' % n))
    colonne, rangee = n % COTE, n // COTE
    if ordre == 'bas':
        rangee = COTE - 1 - rangee
    # Les tuiles du terrain sont stockées le sud en haut : on les retourne (vérifié par la continuité des bords)
    image.paste(tuile.transpose(Image.FLIP_TOP_BOTTOM), (colonne * pas, rangee * pas))
image.save(sortie)
print(sortie, image.size)
