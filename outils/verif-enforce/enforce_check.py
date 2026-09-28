"""Contrôleur statique Enforce Script pour SimpleRP (remplace en partie la compilation).

Usage : python enforce_check.py <dossier_du_code_du_mod> [--json sortie.json] [--seulement Fichier1.c,Fichier2.c]
Indexe les scripts du jeu (vanilla complet) et des mods (deps, acedev, bacon, crx), puis vérifie le code du mod.
Calibré sur le code validé du mod (sauvegarde avant_front_76) : ce qui y compile ne doit pas être signalé.
"""
import os, re, sys, io, json, collections, pickle

SP = r"C:\Users\goule\AppData\Local\Temp\claude\C--Users-goule-Documents\e1f1bd8a-58e8-4a5d-8f58-bc003292c2d8\scratchpad"
SOURCES = [SP + r"\vanilla\scripts", SP + r"\deps", SP + r"\acedev", SP + r"\bacon_full", SP + r"\crx"]
CACHE = SP + r"\front\code\index_api.pkl"
SEP = os.sep
PRE = SEP + SEP + '?' + SEP

KEYWORDS = set("""if else for foreach while switch case default return break continue new delete class modded enum typedef
static override protected private proto native external event sealed volatile notnull ref const owned autoptr out inout
local void int float bool string vector auto typename this super null true false thread func array set map Class
ResourceName Managed""".split())
RESERVED = ("reference local external event volatile owned inout thread func typename sealed native proto autoptr notnull "
            "out null delete new super this case switch class enum typedef modded static const private protected "
            "override auto").split()
RESERVED_RE = '|'.join(RESERVED)
CLASH_NAMES = set()
PRIMS = set("int float bool string vector void auto typename ResourceName func".split())
UNIVERSAL = set("Cast Type ClassName StaticGetType IsInherited ToString Insert Get Set Count Find Contains Remove Clear Resize "
                "IsEmpty Sort InsertAt RemoveOrdered Copy InsertAll GetKey GetElement ReplaceKey GetKeyByValue Length Substring "
                "IndexOf Replace ToLower ToUpper Trim TrimInPlace Split ToInt ToFloat ToVector Format Join LastIndexOf Normalized "
                "LengthSq IsValidIndex Debug GetRandomElement GetRandomIndex SwapItems Invert Distance DistanceSq".split())


def lp(path):
    a = os.path.abspath(path).replace('/', SEP)
    return a if a.startswith(PRE) else PRE + a


def strip(src):
    """Retire commentaires et contenu des chaînes (garde les guillemets vides et les sauts de ligne)."""
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            j = src.find('\n', i)
            i = n if j < 0 else j
            continue
        if c == '/' and i + 1 < n and src[i + 1] == '*':
            j = src.find('*/', i + 2)
            seg = src[i:(n if j < 0 else j + 2)]
            out.append('\n' * seg.count('\n'))
            i = n if j < 0 else j + 2
            continue
        if c == '"':
            j = i + 1
            while j < n and src[j] != '"':
                if src[j] == '\\':
                    j += 1
                j += 1
            seg = src[i:j + 1]
            out.append('""' + '\n' * seg.count('\n'))
            i = j + 1
            continue
        out.append(c)
        i += 1
    return ''.join(out)


def match_brace(s, i):
    d = 0
    for j in range(i, len(s)):
        if s[j] == '{':
            d += 1
        elif s[j] == '}':
            d -= 1
            if d == 0:
                return j
    return len(s) - 1


CLASS_RE = re.compile(r'\b(modded\s+)?class\s+(\w+)(?:\s*<[^>{]*>)?\s*(?:(?::|extends)\s*(\w+))?[^;{]*\{')
ENUM_RE = re.compile(r'\benum\s+(\w+)(?:\s*:\s*\w+)?\s*\{([^}]*)\}')
TYPEDEF_RE = re.compile(r'\btypedef\s+[^;]*?\s(\w+)\s*;')
MODS = r'(?:static|override|protected|private|proto|native|external|event|sealed|volatile|notnull|owned|ref|const|local)'
METH_RE = re.compile(r'^[ \t]*((?:' + MODS + r'[ \t]+)*)[\w\[\]]+(?:[ \t]*<[^<>\n]*(?:<[^<>\n]*>[^<>\n]*)?>)?[ \t]+(\w+)[ \t]*\(', re.M)
FUNC_RE = re.compile(r'\b(\w+)\s*\(([^(){};]*(?:\([^(){};]*\)[^(){};]*)*)\)\s*(?:const\s*)?\{')
FIELD_RE = re.compile(r'(?:^|[;{}\n])[ \t]*((?:(?:static|protected|private|ref|const|autoptr|notnull)\s+)*)((?:ref\s+)?\w+(?:\s*<[^;(){}=]*>)?(?:\[\s*\w*\s*\])?)\s+(\w+)\s*(?:=[^;]*)?;', re.M)
DECL_RE = re.compile(r'(?:^|[;{}(\n])\s*(?:foreach\s*\(\s*)?(?:(?:ref|auto|const|autoptr|notnull)\s+)?(int|float|bool|string|vector|auto|[A-Z]\w*(?:<[^;(){}=]*>)?(?:\[\s*\d*\s*\])?)\s+([a-zA-Z_]\w*)\s*(?=[=;,:)\[])')
FMT_RE = re.compile(r'string\.Format\s*\(\s*"((?:[^"\\\n]|\\.)*)"')


def depth1_text(body):
    out = []
    d = 0
    for ch in body:
        if ch == '{':
            d += 1
            if d == 1:
                out.append('{')
            continue
        if ch == '}':
            d -= 1
            if d == 0:
                out.append('}')
            continue
        if d == 0:
            out.append(ch)
    return ''.join(out)


def new_index():
    return {'classes': {}, 'enums': {}, 'typedefs': set(), 'globals': set(), 'all_methods': set(), 'consts': set()}


def index_text(s, fname, idx):
    for m in CLASS_RE.finditer(s):
        name, parent = m.group(2), m.group(3)
        b0 = m.end() - 1
        b1 = match_brace(s, b0)
        top = depth1_text(s[b0 + 1:b1])
        c = idx['classes'].setdefault(name, {'parents': set(), 'methods': set(), 'fields': set(), 'files': set()})
        if parent and not m.group(1):
            c['parents'].add(parent)
        c['files'].add(fname)
        for mm in METH_RE.finditer(top):
            if mm.group(2) not in KEYWORDS:
                c['methods'].add(mm.group(2))
                idx['all_methods'].add(mm.group(2))
        for fm in FIELD_RE.finditer(top):
            c['fields'].add(fm.group(3))
    for m in ENUM_RE.finditer(s):
        vals = set(v.strip().split('=')[0].strip() for v in m.group(2).split(',') if v.strip())
        idx['enums'].setdefault(m.group(1), set()).update(vals)
        idx['consts'].update(vals)
    for m in TYPEDEF_RE.finditer(s):
        idx['typedefs'].add(m.group(1))
    idx['consts'].update(re.findall(r'\bconst\s+\w+\s+(\w+)\s*=', s))
    top = depth1_text(s)
    for mm in METH_RE.finditer(top):
        idx['globals'].add(mm.group(2))
        idx['all_methods'].add(mm.group(2))


def index_file(path, idx):
    try:
        src = io.open(lp(path), encoding='utf-8', errors='replace').read()
    except Exception:
        return
    index_text(strip(src), os.path.basename(path), idx)


def build_index():
    if os.path.exists(CACHE):
        return pickle.load(open(CACHE, 'rb'))
    idx = new_index()
    for d in SOURCES:
        for root, _, files in os.walk(lp(d)):
            for f in files:
                if f.endswith('.c'):
                    index_file(os.path.join(root, f), idx)
    pickle.dump(idx, open(CACHE, 'wb'))
    return idx


def split_args(argstr):
    d = 0; parts = []; cur = []
    for ch in argstr:
        if ch in '([{<':
            d += 1
        elif ch in ')]}>':
            d -= 1
        if ch == ',' and d == 0:
            parts.append(''.join(cur)); cur = []
        else:
            cur.append(ch)
    if ''.join(cur).strip():
        parts.append(''.join(cur))
    return parts


class Checker:
    def __init__(self, idx, mod):
        self.idx, self.mod = idx, mod

    def cls(self, name):
        a = self.idx['classes'].get(name)
        b = self.mod['classes'].get(name)
        if not a and not b:
            return None
        return {'parents': (a['parents'] if a else set()) | (b['parents'] if b else set()),
                'methods': (a['methods'] if a else set()) | (b['methods'] if b else set())}

    def chain(self, name, seen=None):
        seen = seen if seen is not None else set()
        c = self.cls(name)
        if name in seen or not c:
            return
        seen.add(name)
        yield c
        for p in c['parents']:
            yield from self.chain(p, seen)

    def has_method(self, name, meth):
        return any(meth in c['methods'] for c in self.chain(name))

    def known(self, t):
        for ix in (self.idx, self.mod):
            if t in ix['classes'] or t in ix['enums'] or t in ix['typedefs'] or t in ix['globals'] or t in ix['all_methods'] or t in ix['consts']:
                return True
        return t in PRIMS

    def check(self, path):
        issues = []
        src = io.open(lp(path), encoding='utf-8', errors='replace').read()
        s = strip(src)
        name = os.path.basename(path)

        def add(pos, kind, msg):
            ln = s.count('\n', 0, pos) + 1 if pos >= 0 else 0
            issues.append({'fichier': name, 'ligne': ln, 'type': kind, 'msg': msg})

        for a, b in (('{', '}'), ('(', ')'), ('[', ']')):
            if s.count(a) != s.count(b):
                add(-1, 'equilibre', '%s %d contre %s %d' % (a, s.count(a), b, s.count(b)))
        for m in re.finditer(r'\|=', s):
            add(m.start(), 'regle', '|= interdit')
        for m in re.finditer(r'\bcase\s+[^:;]+:\s*case\b', s):
            add(m.start(), 'regle', 'case empilés')
        for m in re.finditer(r'\bout\s+[\w<>]+\s+\w+\s*=', s):
            add(m.start(), 'regle', 'paramètre out avec valeur par défaut')
        for m in re.finditer(r'\b(?:ref\s+)?[A-Za-z_]\w*(?:<[^;(){}]*>)?\s+(map|set|array)\s*[=;,)]', s):
            add(m.start(), 'regle', 'variable nommée %s' % m.group(1))
        # Mots réservés d'Enforce employés comme nom (variable, paramètre) : « reference » a cassé la compilation (27/09)
        for m in re.finditer(r'\b(?:int|float|bool|string|vector|auto|typename|ResourceName|[A-Z]\w*(?:<[^;(){}]*>)?)\s+(' + RESERVED_RE + r')\s*(?=[=;,)\[]|\bin\b|:)', s):
            add(m.start(), 'regle', 'nom réservé par Enforce : %s' % m.group(1))
        # Modulo dans un calcul à virgule : Enforce passe tout le calcul en float, « Unknown operator '%' » (27/09)
        for st in re.finditer(r'[^;{}]+', s):
            stmt = st.group(0)
            if not re.search(r'[\w)\]]\s*%\s*[\w(]', stmt):
                continue
            if re.match(r'\s*(?:float|vector)\s+\w+\s*=', stmt) or re.search(r'\b\d+\.\d+\b|\bm_f[A-Z]\w*', stmt):
                add(st.start(), 'regle', 'modulo dans un calcul à virgule (calculer l\'entier à part)')
        # Méthode appelée sans « this. » alors qu'une classe du jeu porte son nom (Room -> « Method 'Room' is private »)
        for m in re.finditer(r'(?<![\w.~])(' + '|'.join(sorted(CLASH_NAMES)) + r')\s*\(', s) if CLASH_NAMES else []:
            before = s[max(0, m.start() - 40):m.start()]
            if re.search(r'(?:new|class|void|int|float|bool|string|vector|protected|static|override)\s+$', before):
                continue
            add(m.start(), 'regle', 'appel de %s sans « this. » : une classe du jeu porte ce nom' % m.group(1))
        for m in re.finditer(r'\bstring\.Format\s*\(', s):
            j = m.end(); d = 1
            while j < len(s) and d:
                if s[j] == '(':
                    d += 1
                elif s[j] == ')':
                    d -= 1
                j += 1
            args = split_args(s[m.end():j - 1])
            if len(args) - 1 > 9:
                add(m.start(), 'regle', 'string.Format avec %d paramètres (max 9)' % (len(args) - 1))
        nocom = re.sub(r'//[^\n]*', '', src)
        for m in FMT_RE.finditer(nocom):
            if re.search(r'%(?![1-9])', m.group(1).replace('%%', '')):
                add(-1, 'avertissement', 'signe pour cent seul dans un string.Format : "%s"' % m.group(1)[:60])
        # classes : override et champs des classes modded
        for m in CLASS_RE.finditer(s):
            modded, cname, parent = m.group(1), m.group(2), m.group(3)
            b0 = m.end() - 1; b1 = match_brace(s, b0)
            top = depth1_text(s[b0 + 1:b1])
            if modded:
                for fm in FIELD_RE.finditer(top):
                    if 'SRP' not in fm.group(3) and fm.group(2) not in ('return',):
                        add(b0 + 1 + fm.start(3), 'regle', 'champ « %s » ajouté à la classe modded %s sans SRP_' % (fm.group(3), cname))
            for mm in METH_RE.finditer(top):
                if 'override' not in mm.group(1):
                    continue
                meth = mm.group(2)
                if modded:
                    base_cls = self.idx['classes'].get(cname)
                    ok = bool(base_cls) and (meth in base_cls['methods'] or any(self.has_method(p, meth) for p in base_cls['parents']))
                    ok = ok or not base_cls
                elif parent is None or self.cls(parent) is None:
                    ok = True
                else:
                    ok = self.has_method(parent, meth)
                if not ok:
                    add(b0 + 1 + mm.start(2), 'override', 'override %s.%s : aucune méthode de ce nom dans les parents (%s)' % (cname, meth, parent or cname))
        # redéclarations dans un même bloc ou un bloc englobant
        for mm in FUNC_RE.finditer(s):
            if mm.group(1) in ('if', 'for', 'foreach', 'while', 'switch', 'catch', 'return', 'else'):
                continue
            b0 = mm.end() - 1; b1 = match_brace(s, b0)
            body = s[b0 + 1:b1]
            stack = []; scope_at = []
            for k, ch in enumerate(body):
                if ch == '{':
                    stack.append(k)
                elif ch == '}' and stack:
                    stack.pop()
                scope_at.append(tuple(stack))
            scope_at.append(tuple(stack))
            decls = {}
            for p in split_args(mm.group(2)):
                parts = p.split('=')[0].split()
                if len(parts) >= 2:
                    decls[parts[-1]] = [()]
            for dm in DECL_RE.finditer(body):
                t, v = dm.group(1), dm.group(2)
                if t in ('return', 'else', 'new', 'delete', 'case') or v in KEYWORDS:
                    continue
                here = scope_at[dm.start(2)]
                # variable d'en-tête de for / foreach : elle n'appartient qu'à sa boucle (vérifié sur le code qui compile)
                head = body[max(0, dm.start() - 16):dm.start(1)]
                if re.search(r'\bfor(?:each)?\s*\(\s*$', head):
                    here = here + (-(dm.start() + 1),)
                prev = decls.setdefault(v, [])
                if any(here[:len(ps)] == ps for ps in prev):
                    add(b0 + 1 + dm.start(2), 'redeclaration', 'variable « %s » déjà déclarée dans le même bloc ou un bloc englobant (%s)' % (v, mm.group(1)))
                prev.append(here)
        # types et appels
        for m in re.finditer(r'\b((?:SRP|SCR)_\w+)\b', s):
            if not self.known(m.group(1)):
                add(m.start(), 'type_inconnu', m.group(1))
        for m in re.finditer(r'\b([A-Z]\w+)\.(\w+)\s*\(', s):
            c, meth = m.group(1), m.group(2)
            if meth in UNIVERSAL or self.cls(c) is None:
                continue
            if not self.has_method(c, meth):
                add(m.start(), 'methode_statique', '%s.%s() introuvable' % (c, meth))
        for m in re.finditer(r'\b([A-Z]\w+)\.([A-Z_][A-Z0-9_]+)\b(?!\s*\()', s):
            vals = self.idx['enums'].get(m.group(1)) or self.mod['enums'].get(m.group(1))
            if vals is not None and m.group(2) not in vals:
                add(m.start(), 'enum', '%s.%s inconnu' % (m.group(1), m.group(2)))
        for m in re.finditer(r'[\w\)\]]\s*\.\s*(\w+)\s*\(', s):
            meth = m.group(1)
            if meth not in UNIVERSAL and meth not in self.idx['all_methods'] and meth not in self.mod['all_methods']:
                add(m.start(1), 'methode_inconnue', '.%s() introuvable dans le jeu, les mods et le code du mod' % meth)
        for m in re.finditer(r'\bnew\s+(\w+)', s):
            c = m.group(1)
            if c not in ('array', 'map', 'set', 'ref') and not self.known(c):
                add(m.start(), 'type_inconnu', 'new %s' % c)
        # champs inconnus : m_xxx / s_xxx qui n'existent comme champ nulle part (jeu, mods, mod)
        known_fields = self.mod.get('all_fields', set())
        for m in re.finditer(r'(?<![\w.])([ms]_\w+)\b', s):
            if m.group(1) not in known_fields:
                add(m.start(), 'champ_inconnu', '%s : aucun champ de ce nom (faute de frappe ?)' % m.group(1))
        # accès : une méthode protected/private du mod (nom propre au mod) appelée depuis une autre classe
        acc = self.mod.get('access', {})
        ranges = []
        for cm in CLASS_RE.finditer(s):
            cb0 = cm.end() - 1
            ranges.append((cb0, match_brace(s, cb0), cm.group(2)))
        def encl(pos):
            best = None
            for a0, a1, nm in ranges:
                if a0 <= pos <= a1 and (best is None or a0 > best[0]):
                    best = (a0, nm)
            return best[1] if best else None
        for m in re.finditer(r'(?:\b([A-Z]\w+)|[\w\)\]])\s*\.\s*(\w+)\s*\(', s):
            meth = m.group(2)
            decl = acc.get(meth)
            if not decl or meth in self.idx['all_methods'] or meth in UNIVERSAL:
                continue
            if any(a == 'public' for _, a in decl):
                continue
            here = encl(m.start())
            owners = set(c for c, _ in decl)
            ok = False
            if here:
                chain_names = set()
                todo = [here]
                while todo:
                    x = todo.pop()
                    if x in chain_names:
                        continue
                    chain_names.add(x)
                    c = self.cls(x)
                    if c:
                        todo.extend(c['parents'])
                ok = bool(chain_names & owners)
            if not ok:
                add(m.start(2), 'acces', '%s() est %s dans %s : appel depuis %s refusé par le compilateur' % (meth, '/'.join(sorted(set(a for _, a in decl))), '/'.join(sorted(owners)), here or 'hors classe'))
        # nombre d'arguments : seulement pour les méthodes du mod dont le nom est unique (une seule signature connue)
        sigs = self.mod.get('sigs', {})
        for m in re.finditer(r'(?<![\w.~])(?:[\w\)\]]\s*\.\s*)?(\w+)\s*\(', s):
            meth = m.group(1)
            if meth not in sigs or len(sigs[meth]) != 1 or meth in self.idx['all_methods'] or meth in KEYWORDS:
                continue
            j = m.end(); d = 1
            while j < len(s) and d:
                if s[j] == '(':
                    d += 1
                elif s[j] == ')':
                    d -= 1
                j += 1
            inner = s[m.end():j - 1]
            if re.match(r'\s*\)?\s*[{;]', s[j - 1:j + 3]) and re.search(r'\b(?:void|int|float|bool|string|vector|[A-Z]\w*)\s+$', s[max(0, m.start() - 40):m.start()]):
                continue  # c'est la déclaration elle-même
            n = len(split_args(inner)) if inner.strip() else 0
            lo, hi = next(iter(sigs[meth]))
            if n < lo or n > hi:
                add(m.start(), 'arguments', '%s() appelée avec %d argument(s), attend %s' % (meth, n, str(lo) if lo == hi else '%d à %d' % (lo, hi)))
        return issues


def main():
    root = sys.argv[1]
    only = None
    if '--seulement' in sys.argv:
        only = set(sys.argv[sys.argv.index('--seulement') + 1].split(','))
    idx = build_index()
    mod = new_index()
    files = [os.path.join(dp, f) for dp, _, fs in os.walk(lp(root)) for f in fs if f.endswith('.c')]
    decl_files = collections.defaultdict(list)
    enum_files = collections.Counter()
    for p in files:
        s = strip(io.open(p, encoding='utf-8', errors='replace').read())
        index_text(s, os.path.basename(p), mod)
        for m in CLASS_RE.finditer(s):
            if not m.group(1):
                decl_files[m.group(2)].append(os.path.basename(p))
        for m in ENUM_RE.finditer(s):
            enum_files[m.group(1)] += 1
    issues = []
    for k, fl in decl_files.items():
        if len(fl) > 1:
            issues.append({'fichier': ', '.join(fl), 'ligne': 0, 'type': 'doublon_classe', 'msg': 'classe %s déclarée %d fois' % (k, len(fl))})
        if k in idx['classes'] and not k.startswith('SRP_'):
            issues.append({'fichier': ', '.join(fl), 'ligne': 0, 'type': 'doublon_classe', 'msg': 'classe %s existe déjà dans le jeu (utiliser modded)' % k})
    for k, n in enum_files.items():
        if n > 1:
            issues.append({'fichier': '', 'ligne': 0, 'type': 'doublon_enum', 'msg': 'enum %s déclarée %d fois' % (k, n)})
    # signatures des méthodes du mod : nom -> ensemble de (min, max) arguments
    sigs = collections.defaultdict(set)
    decl_re = re.compile(r'^[ \t]*(?:(?:' + MODS + r')[ \t]+)*[\w\[\]]+(?:[ \t]*<[^<>\n]*(?:<[^<>\n]*>[^<>\n]*)?>)?[ \t]+(\w+)[ \t]*\(([^()]*(?:\([^()]*\)[^()]*)*)\)\s*(?:const\s*)?[{;]', re.M)
    for p in files:
        s = strip(io.open(p, encoding='utf-8', errors='replace').read())
        for m in decl_re.finditer(s):
            if m.group(1) in KEYWORDS:
                continue
            params = [x for x in split_args(m.group(2)) if x.strip() and x.strip() != 'void']
            hi = len(params)
            lo = sum(1 for x in params if '=' not in x)
            sigs[m.group(1)].add((lo, hi))
    mod['sigs'] = sigs
    # noms de méthodes du mod qui sont aussi des classes du jeu ou des mods (appel sans « this. » = la classe)
    CLASH_NAMES.clear()
    CLASH_NAMES.update(n for n in sigs if n in idx['classes'] and not n.startswith('SRP_'))
    # accès des méthodes du mod : nom -> [(classe, public|protected|private)]
    access = collections.defaultdict(list)
    for p in files:
        s = strip(io.open(p, encoding='utf-8', errors='replace').read())
        for cm in CLASS_RE.finditer(s):
            cb0 = cm.end() - 1
            top = depth1_text(s[cb0 + 1:match_brace(s, cb0)])
            for mm in METH_RE.finditer(top):
                if mm.group(2) in KEYWORDS or mm.group(2) == cm.group(2):
                    continue
                mods = mm.group(1)
                a = 'private' if 'private' in mods else ('protected' if 'protected' in mods else 'public')
                access[mm.group(2)].append((cm.group(2), a))
    mod['access'] = access
    # tous les noms de champs connus (jeu + mods + mod), plus les variables locales ou paramètres nommés m_/s_ (rares)
    af = set()
    for ix in (idx, mod):
        for c in ix['classes'].values():
            af |= c['fields']
    for p in files:
        s = strip(io.open(p, encoding='utf-8', errors='replace').read())
        af.update(re.findall(r'\b(?:int|float|bool|string|vector|auto|[A-Z]\w*(?:<[^;(){}=]*>)?)\s+([ms]_\w+)\s*[=;,)\[]', s))
    for d in SOURCES:
        pass
    mod['all_fields'] = af | set(idx.get('all_fields_extra', set()))
    ck = Checker(idx, mod)
    for p in files:
        if only and os.path.basename(p) not in only:
            continue
        issues += ck.check(p)
    cnt = collections.Counter(i['type'] for i in issues)
    for i in sorted(issues, key=lambda x: (x['type'], x['fichier'], x['ligne'])):
        print('%-18s %-32s %5s  %s' % (i['type'], i['fichier'][:32], i['ligne'], i['msg']))
    print('TOTAL', len(issues), dict(cnt))
    if '--json' in sys.argv:
        json.dump(issues, io.open(sys.argv[sys.argv.index('--json') + 1], 'w', encoding='utf-8'), ensure_ascii=False, indent=1)


if __name__ == '__main__':
    main()
