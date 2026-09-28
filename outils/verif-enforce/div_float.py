# -*- coding: utf-8 -*-
# Cherche les divisions / modulos ENTIERS placés dans une expression à virgule (Enforce convertit alors tout le calcul
# en float : « Unknown operator '%' », ou division à virgule silencieuse). Heuristique : rapport à relire à la main.
import re, io, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import enforce_check as ec

root = sys.argv[1]
only = set(sys.argv[2].split(',')) if len(sys.argv) > 2 else None

DECL = re.compile(r'\b(int|float|bool|string|vector)\s+(\w+)\s*(?=[=;,)])')
INT_CALLS = r'(?:\.Count\(\)|\.Length\(\)|\.ToInt\(\)|Math\.(?:AbsInt|ClampInt|MaxInt|MinInt|RandomInt|RandomIntInclusive)\([^()]*\)|GetCellCount\(\)|GetZoneCount\(\))'


def types_of(src):
    t = {}
    for m in DECL.finditer(src):
        t.setdefault(m.group(2), set()).add(m.group(1))
    return t


def is_int_operand(tok, types):
    tok = tok.strip()
    if re.fullmatch(r'\d+', tok):
        return True
    if re.fullmatch(r'\d+\.\d*', tok):
        return False
    if re.search(INT_CALLS + r'$', tok):
        return True
    name = re.sub(r'\[.*\]$', '', tok).split('.')[-1]
    if re.fullmatch(r'm_i[A-Z]\w*|s_i[A-Z]\w*|[A-Z_]{3,}', name):
        return True
    ts = types.get(name)
    return ts == {'int'}


def is_float_context(stmt, types):
    if re.match(r'\s*(?:float|vector)\s+\w+\s*=', stmt):
        return True
    if re.search(r'\b\d+\.\d+\b', stmt):
        return True
    for name in re.findall(r'\b([a-zA-Z_]\w*)\b', stmt):
        if re.fullmatch(r'm_f[A-Z]\w*|s_f[A-Z]\w*', name):
            return True
        if types.get(name) == {'float'} or types.get(name) == {'vector'}:
            return True
    if re.search(r'Math\.(?:Floor|Ceil|Round|Sqrt|Pow|Sin|Cos|Atan2|Clamp|Lerp|Min|Max)\s*\(|Vector\s*\(|\bvector\.', stmt):
        return True
    return False


OPERAND = r'(\((?:[^()]|\([^()]*\))*\)|[\w.]+(?:\([^()]*\))?(?:\[[^\]]*\])?)'
BIN = re.compile(OPERAND + r'\s*([/%])\s*' + OPERAND)

for f in sorted(os.listdir(root)):
    if not f.endswith('.c') or (only and f not in only):
        continue
    raw = io.open(os.path.join(root, f), encoding='utf-8', errors='replace').read()
    s = ec.strip(raw)
    types = types_of(s)
    pos = 0
    for stmt_m in re.finditer(r'[^;{}]+', s):
        stmt = stmt_m.group(0)
        if '/' not in stmt and '%' not in stmt:
            continue
        for m in BIN.finditer(stmt):
            a, op, b = m.group(1), m.group(2), m.group(3)
            ai = is_int_operand(a[1:-1] if a.startswith('(') else a, types) if not a.startswith('(') else all(
                is_int_operand(x, types) for x in re.split(r'[-+*/%]', a[1:-1]) if x.strip())
            bi = is_int_operand(b, types) if not b.startswith('(') else all(
                is_int_operand(x, types) for x in re.split(r'[-+*/%]', b[1:-1]) if x.strip())
            if ai and bi and is_float_context(stmt, types):
                ln = s.count('\n', 0, stmt_m.start() + m.start()) + 1
                print('%s:%d  %s  ||  %s' % (f, ln, m.group(0), ' '.join(stmt.split())[:170]))
