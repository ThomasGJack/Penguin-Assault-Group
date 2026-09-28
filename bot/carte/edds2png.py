# -*- coding: utf-8 -*-
# Convertit une texture .edds d'Enfusion (DDS + blocs de mips « COPY » / « LZ4 ») en PNG : on ne garde que le plus grand mip.
# Usage : edds2png.py <entree.edds> <sortie.png>
import io, struct, sys
import lz4.block
from PIL import Image

src, dst = sys.argv[1], sys.argv[2]
d = open(src, 'rb').read()
assert d[:4] == b'DDS '
height, width, mips = struct.unpack('<I', d[12:16])[0], struct.unpack('<I', d[16:20])[0], max(1, struct.unpack('<I', d[28:32])[0])
dx10 = d[84:88] == b'DX10'
header_size = 4 + 124 + (20 if dx10 else 0)
header = bytearray(d[:header_size])

# Table des blocs : un par mip, du plus petit au plus grand
pos = header_size
table = []
for _ in range(mips):
    tag = d[pos:pos + 4]
    size = struct.unpack('<i', d[pos + 4:pos + 8])[0]
    table.append((tag, size))
    pos += 8


def mip_size(level):
    w, h = max(1, width >> level), max(1, height >> level)
    return ((w + 3) // 4) * ((h + 3) // 4) * 16      # BC7 : 16 octets par bloc de 4 x 4


data = None
for index, (tag, size) in enumerate(table):
    level = mips - 1 - index
    block = d[pos:pos + size]
    pos += size
    if level != 0:
        continue
    if tag == b'COPY':
        data = block
    elif tag == b'LZ4 ':
        # Flux de morceaux de 64 Ko chaînés : [taille cible du tout][(taille compressée, drapeau de fin dans l'octet haut) + bloc]…
        target = struct.unpack('<I', block[:4])[0]
        out = bytearray()
        p = 4
        dictionary = b''
        while p < len(block) and len(out) < target:
            raw = struct.unpack('<I', block[p:p + 4])[0]
            csize = raw & 0x7FFFFFFF
            p += 4
            chunk = block[p:p + csize]
            p += csize
            want = min(65536, target - len(out))
            piece = lz4.block.decompress(chunk, uncompressed_size=want, dict=dictionary)
            out += piece
            dictionary = bytes(out[-65536:])
        data = bytes(out)
    else:
        raise SystemExit('bloc inconnu : %r' % tag)

assert data is not None and len(data) == mip_size(0), (len(data) if data else None, mip_size(0))
struct.pack_into('<I', header, 28, 1)                  # un seul mip
image = Image.open(io.BytesIO(bytes(header) + data))
image.load()
image.convert('RGB').save(dst)
print(dst, image.size)
