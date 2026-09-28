# -*- coding: utf-8 -*-
"""Savoir médical du PAG-Bot : formulaire de /soins, moteur « Quoi faire ? », chapitres du guide médical.

Aucune dépendance à Discord : tout se teste hors ligne. Les chiffres viennent du code d'ACE Dev (modules
Medical Core, Circulation, Breathing) et des réglages de la PAG :
  - réserve de sang 3000 ml ; classe II sous 70 %, III sous 40 %, IV sous 20 %, mort à 0 (10 min au pire) ;
  - cerveau : 15 min d'arrêt cardiaque sans massage ; le massage suspend le compteur ;
  - massage : réussite testée toutes les 15 s, 20 % en classe IV, 75 % à partir de la classe II ;
    l'épinéphrine rapproche ces tests (jusqu'à +50 %) et accélère le cœur ;
  - une solution saline rend 750 ml ; la phényléphrine remonte la tension et freine le saignement, mais
    ralentit aussi la perfusion ; la morphine baisse pouls et tension, la naloxone l'annule ;
  - seuils d'ACE : pouls < 40 instable, < 30 critique, < 20 arrêt, > 220 arrêt ; SpO2 < 85 / 75 / 65.
"""

SOLDAT, INFIRMIER, MEDECIN = "soldat", "infirmier", "medecin"
QUI = {SOLDAT: "🪖", INFIRMIER: "⛑️", MEDECIN: "🩺"}
QUI_NOM = {SOLDAT: "tout soldat", INFIRMIER: "infirmier", MEDECIN: "médecin"}
RANG = {SOLDAT: 0, INFIRMIER: 1, MEDECIN: 2}

# ------------------------------------------------------------------------------------------------
# Formulaire : (clé, titre, multiple, [(valeur, libellé, émoji)])
# ------------------------------------------------------------------------------------------------
PAGES = [
    ("Premier regard", [
        ("role", "Ton rôle", False, [(SOLDAT, "Soldat (dotation commune)", "🪖"), (INFIRMIER, "Infirmier", "⛑️"), (MEDECIN, "Médecin", "🩺")]),
        ("conscience", "Conscience", False, [("conscient", "Conscient, il répond", "🙂"), ("inconscient", "Inconscient", "😵")]),
        ("saignements", "Où saigne-t-il ? (plusieurs choix)", True, [
            ("aucun", "Aucun saignement", "✅"), ("membre", "Bras ou jambe", "💪"), ("tete_cou", "Tête ou cou", "🗣️"),
            ("thorax", "Thorax", "🫁"), ("abdomen", "Abdomen", "🩹"), ("massif", "Saignement massif, plusieurs plaies", "🩸")]),
        ("sang", "Sang perdu (menu médical : Examiner)", False, [
            ("inconnu", "Je ne sais pas", "❔"), ("normal", "Normal, pas d'hémorragie", "✅"), ("c1", "Hémorragie de classe I", "🟢"),
            ("c2", "Hémorragie de classe II", "🟡"), ("c3", "Hémorragie de classe III", "🟠"), ("c4", "Hémorragie de classe IV", "🔴")]),
    ]),
    ("Signes vitaux", [
        ("pouls", "Pouls (Prendre le pouls)", False, [
            ("inconnu", "Pas encore pris", "❔"), ("absent", "Aucun pouls (0)", "💔"), ("lent", "Très lent : moins de 40", "🔻"),
            ("bas", "Lent : 40 à 59", "↘️"), ("normal", "Normal : 60 à 100", "💚"), ("rapide", "Rapide : 101 à 150", "↗️"),
            ("tres_rapide", "Très rapide : plus de 150", "🔺")]),
        ("tension", "Tension (Mesurer la tension)", False, [
            ("inconnu", "Pas encore mesurée", "❔"), ("imprenable", "Imprenable (0/0)", "💔"), ("tres_basse", "Très basse : moins de 70", "🔻"),
            ("basse", "Basse : 70 à 99", "↘️"), ("normale", "Normale : 100 à 160", "💚"), ("haute", "Haute : plus de 160", "🔺")]),
        ("respiration", "Respiration (Mesurer la respiration)", False, [
            ("inconnu", "Pas encore mesurée", "❔"), ("absente", "Ne respire pas", "🚫"), ("lente", "Lente : moins de 10 par minute", "↘️"),
            ("normale", "Normale : 10 à 24", "💚"), ("rapide", "Rapide : plus de 24", "↗️")]),
        ("spo2", "Saturation SpO2 (Mesurer la SpO2)", False, [
            ("inconnu", "Pas encore mesurée", "❔"), ("ok", "90 % et plus", "💚"), ("basse", "85 à 89 %", "🟡"), ("critique", "Moins de 85 %", "🔴")]),
    ]),
    ("Voies aériennes, douleur, soins déjà faits", [
        ("voies", "Voies aériennes et thorax (plusieurs choix)", True, [
            ("rien", "Rien à signaler", "✅"), ("sur_le_dos", "Inconscient couché sur le dos", "🛌"), ("vomi", "Il a vomi", "🤮"),
            ("obstruction", "Voies aériennes obstruées", "😮‍💨"), ("pneumo_ouvert", "Pneumothorax ouvert", "🫁"),
            ("pneumo_tension", "Pneumothorax sous tension", "🎈")]),
        ("douleur", "Douleur", False, [("aucune", "Pas de douleur", "🙂"), ("moderee", "Douleur supportable", "😣"), ("forte", "Forte douleur", "😖")]),
        ("deja", "Déjà fait (plusieurs choix)", True, [
            ("rien", "Rien encore", "⏳"), ("garrot", "Garrot posé", "🪢"), ("bande", "Plaies bandées", "🩹"), ("seal", "Pansement thoracique posé", "🫁"),
            ("morphine", "Morphine donnée", "💉"), ("saline", "Perfusion en cours", "💧"), ("epinephrine", "Épinéphrine donnée", "⚡"),
            ("massage", "Massage cardiaque en cours", "🫀")]),
    ]),
]
DEFAUTS = {"role": SOLDAT, "conscience": "", "saignements": [], "sang": "inconnu", "pouls": "inconnu", "tension": "inconnu",
           "respiration": "inconnu", "spo2": "inconnu", "voies": [], "douleur": "aucune", "deja": []}
LIBELLES = {cle: {v: (emoji + " " + lib) for v, lib, emoji in opts} for _, champs in PAGES for cle, _, _, opts in champs}
TITRES = {cle: titre for _, champs in PAGES for cle, titre, _, _ in champs}
POCHES = {"c2": 1, "c3": 2, "c4": 3}


def nouvelle_fiche():
    return {k: (list(v) if isinstance(v, list) else v) for k, v in DEFAUTS.items()}


def resume(fiche):
    """Lignes « titre : valeur » de ce qui a été renseigné, pour l'embed de la fiche."""
    lignes = []
    for _, champs in PAGES:
        for cle, titre, multiple, _ in champs:
            val = fiche.get(cle)
            if multiple:
                txt = ", ".join(LIBELLES[cle][v] for v in val) if val else "—"
            else:
                txt = LIBELLES[cle].get(val, "—") if val else "—"
            lignes.append((titre.split(" (")[0], txt))
    return lignes


# ------------------------------------------------------------------------------------------------
# Moteur : de la fiche au plan d'action, dans l'ordre
# ------------------------------------------------------------------------------------------------
def plan(fiche):
    """Rend {"gravite", "couleur", "titre", "constat": [...], "etapes": [(qui, texte)], "rappels": [...], "manque": [...]}."""
    f = nouvelle_fiche()
    f.update(fiche or {})
    role = f["role"] if f["role"] in RANG else SOLDAT
    saigne = [s for s in f["saignements"] if s != "aucun"]
    voies = [v for v in f["voies"] if v != "rien"]
    deja = set(f["deja"]) - {"rien"}
    inconscient = f["conscience"] == "inconscient"
    arret = f["pouls"] == "absent" or f["tension"] == "imprenable"
    ne_respire_pas = f["respiration"] == "absente"
    sang = f["sang"]
    etapes, constat, rappels, manque = [], [], [], []

    def etape(qui, texte):
        etapes.append((qui, texte))

    # --- ce qu'il faudrait encore regarder
    if not f["conscience"]:
        manque.append("la conscience : parle-lui, secoue-le")
    if not f["saignements"]:
        manque.append("les saignements : menu médical, « Examiner le patient »")
    if f["pouls"] == "inconnu":
        manque.append("le pouls : « Prendre le pouls »")
    if inconscient and f["respiration"] == "inconnu":
        manque.append("la respiration : « Mesurer la respiration »")

    # --- gravité
    rouge = arret or ne_respire_pas or "massif" in saigne or sang == "c4" or "pneumo_tension" in voies or f["spo2"] == "critique" or f["pouls"] == "lent"
    orange = bool(saigne) or sang == "c3" or inconscient or voies or f["tension"] in ("tres_basse", "basse") or f["pouls"] in ("bas", "tres_rapide") or f["spo2"] == "basse"
    if rouge:
        gravite, couleur, titre = "rouge", 0xC0392B, "🔴 URGENCE ABSOLUE"
    elif orange:
        gravite, couleur, titre = "orange", 0xE67E22, "🟠 URGENT"
    else:
        gravite, couleur, titre = "vert", 0x3E8E5A, "🟢 STABLE"

    # --- constat
    if arret:
        constat.append("💔 **Arrêt cardiaque** : le cerveau tient 15 minutes sans massage. Tant qu'on masse, le compteur est suspendu.")
    if saigne:
        constat.append("🩸 **Hémorragie active** : sans soins, la mort arrive en 10 minutes au pire. Le sang passe avant tout le reste.")
    if sang in ("c3", "c4"):
        constat.append(f"🅾️ **{LIBELLES['sang'][sang][2:]}** : la réserve de sang est entamée, il faudra perfuser ({POCHES[sang]} poches).")
    if "pneumo_tension" in voies:
        constat.append("🎈 **Pneumothorax sous tension** : l'air écrase le poumon et le cœur. Il faut exsuffler, c'est un geste de médecin.")
    if not constat and gravite == "vert":
        constat.append("Rien d'alarmant dans ce que tu as relevé. On surveille et on rend compte.")

    # --- 0. sécurité
    etape(SOLDAT, "**À couvert d'abord.** Traîne ou porte le blessé derrière un abri, puis annonce-le à la radio : qui, où (le carré), quoi.")

    # --- 1. hémorragies
    if saigne:
        if arret and "massage" not in deja:
            etape(SOLDAT, "**À deux, on se partage le travail** : l'un arrête le sang, l'autre masse. Seul : le sang d'abord, masser un blessé qui se vide ne sert à rien.")
        if "membre" in saigne or "massif" in saigne:
            if "garrot" in deja:
                etape(SOLDAT, "Le garrot est posé : **bande la plaie** du membre, puis **retire le garrot**.")
            else:
                etape(SOLDAT, "**Garrot** sur le bras ou la jambe qui saigne, le plus haut possible. Il arrête le sang tout de suite. Puis **bandage**, et on retire le garrot une fois la plaie bandée.")
        autres = [z for z in ("tete_cou", "thorax", "abdomen") if z in saigne]
        if autres or "massif" in saigne:
            zones = {"tete_cou": "la tête ou le cou", "thorax": "le thorax", "abdomen": "l'abdomen"}
            noms = ", ".join(zones[z] for z in autres) or "les autres plaies"
            etape(SOLDAT, f"**Bandage** sur {noms} : pas de garrot possible ici, on bande plaie après plaie en commençant par celle qui saigne le plus.")
        if "thorax" in saigne and "seal" not in deja:
            etape(SOLDAT, "Plaie au thorax : **pansement thoracique** par-dessus. Il évite que le pneumothorax passe sous tension.")
        etape(SOLDAT, "Rouvre « Examiner le patient » : tant qu'une zone saigne encore, on recommence. C'est ton matériel qui sert, il est fait pour ton binôme.")
    elif f["saignements"]:
        constat.append("✅ Aucun saignement actif.")

    # --- 2. arrêt cardiaque
    if arret:
        if "sur_le_dos" not in voies:
            etape(SOLDAT, "**Mets-le sur le dos** (« Mettre sur le dos ») : le massage est impossible autrement.")
        if "massage" in deja:
            etape(SOLDAT, "**Continue le massage cardiaque sans t'arrêter.** La réussite est testée toutes les 15 secondes : il faut tenir dans la durée.")
        else:
            etape(SOLDAT, "**Fais un massage cardiaque**, sans t'arrêter, jusqu'à la reprise du pouls ou l'arrivée d'un soignant. Relayez-vous.")
        if "epinephrine" not in deja:
            etape(INFIRMIER, "**Épinéphrine** : elle rapproche les chances de reprise du cœur. Une dose, pendant le massage.")
        if sang in ("c3", "c4", "inconnu"):
            chance = {"c4": "20 %", "c3": "40 % environ", "inconnu": "20 à 75 % selon le sang perdu"}[sang]
            etape(INFIRMIER, f"**Perfuse pendant le massage** : avec ce niveau de sang, chaque test de reprise n'a que {chance} de chances. À partir de la classe II on remonte à 75 %.")
        rappels.append("Ne donne **pas de phényléphrine** pendant une perfusion d'urgence : elle ralentit la perfusion de moitié.")
        rappels.append("Pas de morphine à un patient en arrêt : elle fait encore baisser le pouls et la tension.")

    # --- 3. voies aériennes et respiration
    if "pneumo_tension" in voies:
        etape(MEDECIN, "**Exsuffle à l'aiguille** (kit NCD), puis pansement thoracique s'il n'y en a pas. Sans médecin : pansement thoracique, et évacuation immédiate vers un médecin.")
    if "pneumo_ouvert" in voies and "seal" not in deja and "thorax" not in saigne:
        etape(SOLDAT, "**Pansement thoracique** sur la plaie : un pneumothorax ouvert s'aggrave chaque minute et peut passer sous tension.")
    if inconscient and not arret:
        if "vomi" in voies:
            etape(SOLDAT, "**Dégage les vomissures** tout de suite : il s'étouffe.")
        if "obstruction" in voies or "sur_le_dos" in voies:
            etape(SOLDAT, "**Bascule la tête en arrière** pour libérer la langue, puis **mets-le en PLS** (côté gauche ou droit) : un inconscient sur le dos peut s'étouffer.")
        else:
            etape(SOLDAT, "Inconscient avec un pouls : **mets-le en PLS** et bascule la tête en arrière. On ne le laisse jamais sur le dos.")
        if "obstruction" in voies:
            etape(MEDECIN, "**Tube laryngé King LT** si l'obstruction revient : il garde les voies aériennes libres pour de bon.")
    if ne_respire_pas and not arret:
        etape(SOLDAT, "Il ne respire pas mais a un pouls : **bascule la tête en arrière**, dégage la bouche, et reprends le pouls toutes les 30 secondes. L'arrêt cardiaque n'est pas loin.")
    if f["spo2"] in ("basse", "critique") or f["respiration"] in ("lente", "rapide") or "pneumo_ouvert" in voies or "pneumo_tension" in voies:
        if not arret:
            etape(INFIRMIER, "**Masque à oxygène** : la saturation est la troisième cause d'arrêt cardiaque après le sang et le cœur. Sous 85 % l'état devient instable, sous 65 % c'est l'arrêt.")

    # --- 4. sang perdu
    if sang in POCHES and not arret:
        n = POCHES[sang]
        suite = "" if "saline" not in deja else " Une perfusion est déjà en cours : enchaîne les suivantes sans attendre."
        etape(INFIRMIER, f"**Perfuse {n} solution{'s' if n > 1 else ''} saline{'s' if n > 1 else ''}** (une poche rend 750 ml, un quart de la réserve).{suite} Le blessé porte la sienne : prends-la sur lui avant d'entamer les tiennes.")
    elif sang == "inconnu" and (saigne or f["tension"] in ("basse", "tres_basse")) and not arret:
        etape(SOLDAT, "Regarde la **classe d'hémorragie** dans « Examiner le patient » : c'est elle qui dit combien de poches il faudra.")

    # --- 5. cœur et tension
    if not arret:
        if f["pouls"] == "lent":
            if "morphine" in deja:
                etape(INFIRMIER, "Pouls très lent après une morphine : **naloxone**, c'est l'antidote. Puis surveille le pouls.")
            etape(INFIRMIER, "Pouls sous 40 : l'état est instable, sous 20 c'est l'arrêt. **Épinéphrine** pour relancer le cœur, et prépare-toi à masser.")
        elif f["pouls"] == "bas" and "morphine" in deja:
            etape(INFIRMIER, "Pouls lent après une morphine : surveille. S'il passe sous 40, **naloxone**.")
        if f["pouls"] in ("rapide", "tres_rapide"):
            if saigne or sang in ("c2", "c3", "c4", "inconnu"):
                etape(SOLDAT, "Le pouls rapide est une **réaction à la perte de sang** : on ne le ralentit pas, on arrête l'hémorragie et on perfuse. Il redescendra seul.")
            elif f["pouls"] == "tres_rapide":
                etape(MEDECIN, "Pouls au-dessus de 150 sans hémorragie : **métoprolol** pour le ralentir. Au-delà de 220, c'est l'arrêt cardiaque.")
        if f["tension"] == "tres_basse":
            if saigne:
                etape(MEDECIN, "Tension très basse et hémorragie non maîtrisée : **phényléphrine**, elle remonte la tension et freine le saignement. Attention, elle ralentit aussi la perfusion : à donner avant de perfuser, pas pendant.")
            else:
                etape(INFIRMIER, "Tension très basse : **perfuse** d'abord. Si elle ne remonte pas une fois le sang revenu, le médecin donnera de la phényléphrine.")
        elif f["tension"] == "basse" and sang in ("normal", "c1") and not saigne:
            etape(INFIRMIER, "Tension un peu basse sans perte de sang : surveille. Si une morphine a été donnée, c'est son effet.")
        if f["tension"] == "haute":
            rappels.append("Tension haute : plus d'épinéphrine ni de phényléphrine. Au-delà de 220 de maxima, ACE refuse toute reprise du cœur.")

    # --- 6. douleur
    if f["douleur"] == "forte" and not arret:
        if "morphine" in deja:
            etape(SOLDAT, "Il a encore mal mais a **déjà eu sa morphine** : on n'en donne pas une seconde. Elle agit lentement et dure longtemps.")
        elif inconscient:
            pass
        elif f["tension"] in ("tres_basse",) or f["pouls"] in ("lent", "bas"):
            etape(SOLDAT, "Forte douleur mais pouls ou tension trop bas : **pas de morphine pour l'instant**, elle les ferait encore baisser. On stabilise d'abord.")
        else:
            etape(SOLDAT, "**Une morphine, une seule** : elle calme la douleur mais fait baisser le pouls et la tension.")

    # --- 7. réveil
    if inconscient and not arret and not saigne and f["pouls"] in ("normal", "rapide", "bas") and sang in ("normal", "c1", "c2", "inconnu"):
        etape(INFIRMIER, "Stabilisé mais toujours inconscient : **carbonate d'ammonium** sous le nez (« Faire respirer les sels »). De 50 à 100 % de chances, plus le cerveau a souffert moins ça marche : on peut recommencer.")

    # --- 8. suite
    if arret or gravite == "rouge":
        etape(SOLDAT, "**Reprends le pouls et la tension toutes les deux minutes** et tiens la radio au courant. Dès que le pouls revient : arrête le massage, PLS, et on continue la perfusion.")
    else:
        etape(SOLDAT, "**Surveille** : pouls et saignements toutes les cinq minutes, et compte rendu à ton chef de groupe.")

    # --- ce que ce rôle ne peut pas faire lui-même
    attendus = sorted({q for q, _ in etapes if RANG[q] > RANG[role]}, key=lambda q: RANG[q])
    if attendus:
        noms = " et ".join("un " + QUI_NOM[q] for q in attendus)
        rappels.insert(0, f"Les étapes marquées {' '.join(QUI[q] for q in attendus)} demandent {noms} : appelle-le dès maintenant à la radio, avec le carré.")
    if role == SOLDAT:
        rappels.append("La solution saline que tu portes se donne à l'infirmier : c'est lui qui perfuse.")
    return {"gravite": gravite, "couleur": couleur, "titre": titre, "constat": constat, "etapes": etapes, "rappels": rappels, "manque": manque, "role": role}


# ------------------------------------------------------------------------------------------------
# Guides : {clé: {"titre", "emoji", "description", "chapitres": [{"titre", "emoji", "intro", "blocs": [(nom, texte)]}]}}
# ------------------------------------------------------------------------------------------------
GUIDE_MEDICAL = {
    "titre": "Guide médical de la PAG", "emoji": "🩺",
    "description": "Tout le système de soins, du premier garrot à l'exsufflation : quoi regarder, quoi faire, dans quel ordre, et qui a le droit de le faire.",
    "chapitres": [
        {"titre": "Les principes", "emoji": "📜", "intro": "Quatre idées à connaître par cœur avant de toucher un blessé.", "blocs": [
            ("🤝 La règle du binôme", "Le matériel de soin que tu portes **n'est pas pour toi** : il est pour ton binôme, et c'est le sien qui te sauvera. Tu ne pars jamais sans binôme, et tu sais toujours où il est. Un soldat conscient et seul se soigne lui-même, mais c'est l'exception."),
            ("⏱️ Les deux horloges", "Un coup grave ne tue pas : il met à terre, inconscient ou le cœur arrêté.\n• **10 minutes** pour arrêter le sang, au pire.\n• **15 minutes** pour relancer un cœur arrêté. Tant qu'on masse, ce compteur est suspendu.\nAprès, c'est fini. Un blessé abandonné est un mort."),
            ("🔢 L'ordre des gestes", "**1.** À couvert, le blessé aussi.\n**2.** La radio : qui, où, quoi.\n**3.** Le sang : garrot, bandage, pansement thoracique.\n**4.** Le pouls : pas de pouls, massage.\n**5.** La respiration : PLS, tête en arrière.\n**6.** L'infirmier, puis le médecin."),
            ("☠️ Ce qui reste mortel", "Un second coup mortel sur un soldat déjà à terre : la seconde chance ne joue qu'une fois. On sécurise le blessé avant de le soigner."),
        ]},
        {"titre": "Examiner un blessé", "emoji": "🔎", "intro": "Tout passe par le menu médical (touche à régler dans Contrôles) et l'action « Examiner le patient ».", "blocs": [
            ("Ce que tu lis", "• **Les zones qui saignent**, en rouge sur la silhouette.\n• **La classe d'hémorragie** : I, II, III ou IV.\n• **Pneumothorax** ouvert ou sous tension, quand il y en a un.\n• Garrots posés, perfusion en cours."),
            ("Ce que tu mesures", "• « **Prendre le pouls** » : battements par minute.\n• « **Mesurer la tension** » : deux chiffres, comme 120/80.\n• « **Mesurer la respiration** » : respirations par minute.\n• « **Mesurer la SpO2** » : la saturation en oxygène, en %."),
            ("Dans quel ordre", "Saignements d'abord, pouls ensuite, respiration enfin. Le reste (tension, SpO2) est le travail de l'infirmier une fois le blessé hors de danger immédiat."),
            ("💡 Astuce", "La commande **/soins** du bot fait le tri à ta place : tu coches ce que tu vois, il te rend les gestes dans l'ordre."),
        ]},
        {"titre": "Les saignements, zone par zone", "emoji": "🩸", "intro": "Le sang passe avant tout le reste : masser un blessé qui se vide ne sert à rien.", "blocs": [
            ("💪 Bras et jambes", "**Garrot** le plus haut possible : il arrête le sang tout de suite. Puis **bandage** de la plaie, et on **retire le garrot** une fois bandé. Un garrot oublié continue de faire mal."),
            ("🗣️ Tête et cou", "Pas de garrot possible : **bandage** direct, et vite. Une plaie au cou est une hémorragie massive."),
            ("🫁 Thorax", "**Bandage**, puis **pansement thoracique** par-dessus. Toute plaie grave au thorax a 1 chance sur 4 de donner un pneumothorax."),
            ("🩹 Abdomen", "**Bandage**. Pas de garrot, pas de pansement thoracique."),
            ("🦵 Artère fémorale et cou", "Ce sont les saignements les plus rapides du jeu. Garrot en haut de la cuisse pour la fémorale, bandage appuyé pour le cou."),
            ("Plusieurs plaies", "On commence par celle qui saigne le plus. Après chaque geste, on rouvre « Examiner le patient » : tant qu'une zone est rouge, on recommence."),
        ]},
        {"titre": "Le sang perdu et la perfusion", "emoji": "💧", "intro": "Arrêter le sang ne le rend pas. C'est le travail de l'infirmier.", "blocs": [
            ("Les classes d'hémorragie", "La réserve est de 3000 ml.\n• **Classe I** : plus de 70 % restant. Rien à faire.\n• **Classe II** : moins de 70 %. État instable.\n• **Classe III** : moins de 40 %. État critique.\n• **Classe IV** : moins de 20 %. Arrêt cardiaque.\n• **0 %** : mort."),
            ("La solution saline", "Une poche rend **750 ml**, un quart de la réserve.\n• Classe II : **1 poche**.\n• Classe III : **2 poches**.\n• Classe IV : **3 poches**.\nChaque soldat porte la sienne : l'infirmier la prend sur le blessé avant d'entamer les siennes. La Mule en garde 4 en réserve."),
            ("Pourquoi c'est vital pour le cœur", "Le massage cardiaque a **20 %** de chances de réussir en classe IV, et **75 %** à partir de la classe II. Perfuser pendant qu'on masse multiplie les chances par presque quatre."),
        ]},
        {"titre": "L'inconscient qui respire", "emoji": "😵", "intro": "Il a un pouls, il respire, mais il ne répond plus.", "blocs": [
            ("Le danger", "Sur le dos, la langue peut boucher la gorge, et il peut vomir sans pouvoir se dégager. Un inconscient ne reste **jamais sur le dos**, sauf pour le masser."),
            ("Les gestes", "• « **Mettre en PLS** », côté gauche ou droit.\n• « **Basculer la tête en arrière** » pour libérer la langue.\n• « **Dégager les vomissures** » s'il a vomi.\nCe sont des gestes de tout soldat."),
            ("Le réveil", "Il ne se réveille pas tout seul. L'infirmier fait respirer le **carbonate d'ammonium** : de 50 à 100 % de chances, selon ce que le cerveau a subi. On peut recommencer."),
            ("Le médecin", "Si l'obstruction revient sans cesse : **tube laryngé King LT**, qui garde les voies aériennes libres."),
        ]},
        {"titre": "L'arrêt cardiaque et le massage", "emoji": "🫀", "intro": "Pas de pouls, tension imprenable : le cœur est arrêté. C'est l'issue normale d'un coup mortel.", "blocs": [
            ("Avant de masser", "• Le sang d'abord, ou à deux en même temps.\n• Le patient **sur le dos** : « Mettre sur le dos ». Sinon le jeu refuse."),
            ("Masser", "« **Faire un massage cardiaque** », sans s'arrêter. La reprise du cœur est testée **toutes les 15 secondes** : il faut tenir dans la durée, et se relayer. Tant qu'on masse, l'horloge des 15 minutes est suspendue."),
            ("Ce qui aide", "• **Épinéphrine** (infirmier) : elle rapproche les tests de reprise, jusqu'à une fois et demie plus souvent.\n• **Perfusion** (infirmier) : de 20 % à 75 % de chances par test."),
            ("Ce qui nuit", "• La **morphine** : elle fait baisser le pouls et la tension.\n• La **phényléphrine** pendant la perfusion : elle la ralentit de moitié.\n• Une **tension au-dessus de 220** : ACE refuse alors toute reprise. Attention aux épinéphrines en série."),
            ("Quand le pouls revient", "On arrête le massage, PLS, et on continue la perfusion. Le cerveau récupère seul en dix minutes."),
        ]},
        {"titre": "Le thorax : pneumothorax", "emoji": "🫁", "intro": "Une plaie grave au thorax laisse entrer l'air autour du poumon.", "blocs": [
            ("Pneumothorax ouvert", "Le poumon perd du volume, la saturation baisse, et ça **s'aggrave chaque minute**. Geste de tout soldat : « **Poser un pansement thoracique** ». Il empêche le passage sous tension."),
            ("Pneumothorax sous tension", "L'air ne sort plus et écrase le poumon et le cœur. Geste de **médecin** : « **Exsuffler à l'aiguille** » avec le kit NCD. Sans médecin : pansement thoracique et évacuation immédiate."),
            ("L'oxygène", "L'infirmier pose le **masque à oxygène** dès que la SpO2 passe sous 90 %. Sous 85 % l'état devient instable, sous 75 % critique, sous 65 % c'est l'arrêt cardiaque."),
            ("À la PAG", "Un pneumothorax ne tue pas directement, mais il fait chuter la saturation. C'est la cause cachée de beaucoup d'arrêts cardiaques « sans raison »."),
        ]},
        {"titre": "Les médicaments, un par un", "emoji": "💉", "intro": "Chacun a un effet, un moment, et un danger.", "blocs": [
            ("🪖 Morphine — tout soldat", "Calme une forte douleur. **Une seule dose.** Elle fait baisser le pouls et la tension : jamais à un patient en arrêt ou à la tension très basse."),
            ("⛑️ Épinéphrine — infirmier", "Accélère le cœur et rapproche les chances de reprise pendant un massage. Pour l'arrêt cardiaque et le pouls sous 40. Pas en série : la tension monte."),
            ("⛑️ Carbonate d'ammonium — infirmier", "Les sels, sous le nez d'un inconscient stabilisé. De 50 à 100 % de chances de réveil."),
            ("⛑️ Naloxone — infirmier", "L'antidote de la morphine. Pour un pouls ou une respiration qui s'effondrent après une morphine, ou après deux doses données par erreur."),
            ("🩺 Métoprolol — médecin", "Ralentit un cœur au-dessus de 150 **quand ce n'est pas la perte de sang qui l'emballe**. Au-delà de 220, c'est l'arrêt."),
            ("🩺 Phényléphrine — médecin", "Remonte la tension et freine le saignement. Mais elle **ralentit la perfusion de moitié** : avant de perfuser, jamais pendant."),
        ]},
        {"titre": "Les valeurs repères", "emoji": "📊", "intro": "Les chiffres du jeu, à garder sous les yeux.", "blocs": [
            ("💚 Normal", "Pouls **80** · Tension **120/80** · Respiration **15** par minute · SpO2 **97 %**."),
            ("🟡 Instable", "Pouls sous **40** · Tension sous **70** avec un pouls lent · Sang sous **70 %** (classe II) · SpO2 sous **85 %**."),
            ("🟠 Critique", "Pouls sous **30** · Sang sous **40 %** (classe III) · SpO2 sous **75 %**."),
            ("🔴 Arrêt cardiaque", "Pouls sous **20** ou au-dessus de **220** · Sang sous **20 %** (classe IV) · SpO2 sous **65 %**."),
            ("Un pouls rapide", "De 100 à 150, c'est presque toujours une réaction à la perte de sang. On ne le ralentit pas : on arrête l'hémorragie et on perfuse."),
        ]},
        {"titre": "Qui fait quoi, avec quoi", "emoji": "🎒", "intro": "Le matériel pris à l'arsenal sans la certification est une infraction.", "blocs": [
            ("🪖 Tout soldat", "4 bandages, 1 garrot, 1 morphine, 1 pansement thoracique, 1 solution saline (pour l'infirmier).\nGestes : garrot, bandage, pansement thoracique, prendre le pouls, massage cardiaque, PLS, tête en arrière, vomissures, traîner et porter."),
            ("⛑️ Infirmier", "12 bandages, 4 garrots, 4 pansements thoraciques, 8 solutions salines, 4 morphines, 4 épinéphrines, 3 carbonates d'ammonium, 1 naloxone, 1 masque à oxygène.\nGestes en plus : perfusion, épinéphrine, sels, naloxone, oxygène, tension et SpO2."),
            ("🩺 Médecin", "Tout le matériel de l'infirmier, plus 1 kit médical, 2 tubes King LT, 2 kits d'exsufflation, 2 métoprolols, 2 phényléphrines.\nGestes en plus : exsufflation, King LT, métoprolol, phényléphrine."),
            ("🎒 La Mule", "Une réserve pour l'infirmier : 4 solutions salines et 8 bandages."),
        ]},
        {"titre": "Les erreurs qui tuent", "emoji": "⚠️", "intro": "Celles qu'on voit à chaque mission.", "blocs": [
            ("Garder son matériel « pour soi »", "Le binôme meurt avec quatre bandages intacts dans ta poche."),
            ("Se jeter sur le blessé sous le feu", "Deux blessés au lieu d'un. À couvert d'abord."),
            ("Masser un blessé qui saigne", "Le sang s'en va plus vite qu'on ne relance le cœur. Le sang d'abord, ou à deux."),
            ("Laisser un inconscient sur le dos", "Il s'étouffe en silence pendant qu'on regarde ailleurs."),
            ("La seconde morphine", "« Il a encore mal » : elle agit lentement. La seconde fait s'effondrer le pouls."),
            ("Ralentir un pouls rapide", "Il est rapide parce qu'il manque de sang. Le métoprolol l'achève."),
            ("Oublier la radio", "L'infirmier ne devine pas. Qui, où, quoi, dès la première seconde."),
        ]},
    ],
}

GUIDE_RADIO = {
    "titre": "Procédure radio", "emoji": "📻",
    "description": "Comment s'annoncer, rendre compte et clore un échange sur le réseau de la PAG.",
    "chapitres": [
        {"titre": "S'annoncer", "emoji": "🗣️", "intro": "On nomme d'abord celui qu'on appelle, puis soi-même.", "blocs": [
            ("L'appel", "« **Mister, ici Jack, parlez.** »\nCelui qu'on appelle entend son nom en premier et tend l'oreille."),
            ("La réponse", "« **Jack, ici Mister, parlez.** » ou plus court : « Ici Mister, parlez. »"),
            ("Tout le groupe", "« **À tous, ici Jack…** »"),
            ("Indicatifs", "En mission, on préfère les indicatifs : « Alpha 1, ici Alpha 2 ». À la base, les pseudos passent très bien."),
        ]},
        {"titre": "Les mots du réseau", "emoji": "📖", "intro": "Peu de mots, toujours les mêmes.", "blocs": [
            ("Parlez", "J'ai fini ma phrase, j'attends ta réponse."),
            ("Terminé", "La conversation est finie. Jamais « parlez » et « terminé » ensemble."),
            ("Reçu", "J'ai compris ton message."),
            ("Répétez · Collationnez", "« Répétez » : redis ton message. « Collationnez » : répète-moi ce que je viens de dire, pour vérifier des coordonnées ou un ordre."),
            ("Attendez · Affirmatif · Négatif", "« Attendez » : je te rappelle. Oui et non se disent « affirmatif » et « négatif »."),
        ]},
        {"titre": "Le compte rendu", "emoji": "📝", "intro": "Trois mots : QUI, OÙ, QUOI.", "blocs": [
            ("Contact", "« Alpha 1, ici Alpha 2. Carré 087 028. Trois fantassins ennemis, à l'arrêt, direction nord. Parlez. »"),
            ("Blessé", "« Alpha 1, ici Alpha 2. Alpha 3 à terre, sans pouls, massage en cours, demande infirmier, carré 087 028. Parlez. »"),
            ("Le carré", "Deux groupes de trois chiffres : le premier de gauche à droite, le second de bas en haut. Un carré fait 100 m de côté."),
            ("La discipline", "On écoute avant de parler, on est bref, et pas de bavardage sur le réseau en mission."),
        ]},
    ],
}

GUIDES = {"medical": GUIDE_MEDICAL, "radio": GUIDE_RADIO}
