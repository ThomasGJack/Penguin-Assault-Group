# -*- coding: utf-8 -*-
# Traduction française d'ACE Dev pour la PAG.
# 1. extraire_textes.py relit les mods ACE Dev installés et écrit textes_ace.json ;
# 2. ce script réécrit, dans le mod PAG, chaque table « fr_fr » d'ACE (même chemin, même GUID : surcharge) en gardant
#    exactement la structure du fichier d'origine et en ne remplaçant que les textes.
# Un identifiant absent du dictionnaire garde le texte du mod (anglais le plus souvent) : après une mise à jour d'ACE,
# relancer les deux scripts suffit, rien ne s'affiche jamais sous forme de code.
import io, os, sys, json, subprocess
sys.stdout.reconfigure(encoding='utf-8')
ICI = os.path.dirname(os.path.abspath(__file__))
MOD = r'C:\Users\goule\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP'
META = '''MetaFileClass {
 Name "{%s}%s"
 Configurations {
  CONFResourceClass PC {
  }
  CONFResourceClass XBOX_ONE : PC {
  }
  CONFResourceClass XBOX_SERIES : PC {
  }
  CONFResourceClass PS4 : PC {
  }
  CONFResourceClass PS5 : PC {
  }
  CONFResourceClass HEADLESS : PC {
  }
 }
}
'''

FR = {
    # --- Ballistics
    "ACE_Ballistics-Item_BallisticTables_Mrads_Description": "Recueil de tables balistiques en milliradians pour les munitions compatibles avec votre fusil.",
    "ACE_Ballistics-Item_BallisticTables_Mrads_Name": "Tables balistiques (mrad)",
    "ACE_Ballistics-Item_BallisticTables_WpMils_Description": "Recueil de tables balistiques en millièmes du pacte de Varsovie pour les munitions compatibles avec votre fusil.",
    "ACE_Ballistics-Item_BallisticTables_WpMils_Name": "Tables balistiques (millièmes PV)",
    # --- Captives
    "ACE_Captives-Editor_ContextAction_ToggleCaptive_Name": "Faire ou libérer prisonnier",
    "ACE_Captives-Editor_ContextAction_ToggleSurrender_Name": "Faire se rendre ou relever",
    "ACE_Captives-Emotes_Emote_Surrender_Name": "Se rendre",
    "ACE_Captives-UserAction_EscortCaptive": "Escorter le prisonnier",
    "ACE_Captives-UserAction_ReleasePrisoner": "Libérer le prisonnier",
    "ACE_Captives-UserAction_TakeAsPrisoner": "Faire prisonnier",
    "ACE_Captives-UserAction_UnloadCaptive": "Débarquer le prisonnier :",
    # --- Carrying, Chopping
    "ACE_Carrying-UserAction_Drag": "Traîner",
    "ACE_Chopping_RemoveBush": "Couper le buisson",
    "ACE_Chopping_RemoveTree": "Abattre l'arbre",
    # --- Core
    "ACE-Acronym": "ACE",
    "ACE-ControlsHint_InspectTurret": "Inspecter la tourelle",
    "ACE-Emotes_Emote_MetalClanging_Name": "Frapper le métal",
    "ACE-Item_AnvilPatch_Name": "Écusson ACE Anvil",
    "ACE-Item_Banana_Description": "Du potassium, c'est bon pour la santé !",
    "ACE-Item_Banana_Name": "Banane",
    "ACE-RadialMenu_ChangeMagazine": "Changer de chargeur",
    "ACE-Item_CbrnPatch_Name": "Écusson ACE NRBC",
    "ACE-Item_EngineerPatch_Name": "Écusson ACE Génie",
    "ACE-Item_MedicPatch_Name": "Écusson ACE Santé",
    "ACE-UserAction_Carry": "Porter",
    "ACE-UserAction_Carrying": "Porte un blessé",
    "ACE-UserAction_LoadCarried": "Embarquer le blessé :",
    "ACE-UserAction_Release": "Déposer",
    "ACE-Weapon_FragBanana_Description": "Du potassium explosif !",
    "ACE-Weapon_FragBanana_Name": "Banane à fragmentation",
    "ACE_Core-AddonName": "Socle",
    # --- Explosives
    "ACE-Explosives_FailReason_Buried": "Enterrée",
    "ACE-Explosives_FailReason_HardSurface": "Sol trop dur",
    "ACE-Explosives_Item_DeadManSwitch_Name": "Interrupteur homme mort",
    "ACE-Explosives_Item_VMM3_Description": "Détecteur de métaux et de mines, pour le déminage et la recherche de munitions non explosées.",
    "ACE-Explosives_Item_VMM3_Name": "Détecteur de mines VMM3",
    "ACE-Explosives_UserAction_Bury_Name": "Enterrer",
    "ACE-Explosives_UserAction_ConnectToInventoryExplosives_Name": "Relier aux explosifs de l'inventaire",
    "ACE-Explosives_UserAction_Disarm_Name": "Désamorcer",
    "ACE-Explosives_UserAction_Unbury_Name": "Déterrer",
    # --- Facepaint
    "ace-item-facepaint-uk-description": "Stick de crème de camouflage pour le visage, de fabrication britannique. Une couleur différente à chaque extrémité.",
    "ace-item-facepaint-uk-name": "Crème de camouflage",
    "ace-item-facepaint-us-description": "Stick de crème de camouflage pour le visage, de fabrication américaine. Une couleur différente à chaque extrémité.",
    "ace-item-facepaint-us-name": "Crème de camouflage",
    "ace-item-facepaint-ussr-description": "Stick de crème de camouflage pour le visage, de fabrication soviétique.",
    "ace-item-facepaint-ussr-name": "Crème de camouflage",
    # --- Finger
    "ACE_Finger-AddonName": "Pointer du doigt",
    "ACE_Finger-Keybind_PointOnMap": "Pointer sur la carte",
    "ACE_Finger-Notification_PlayerPointsAtEntity": "%1 désigne %2",
    "ACE_Finger-Notification_PlayerPointsAtPosition": "%1 désigne une position",
    # --- Medical Breathing
    "ACE_Medical-InfoWidget_ChestSeal": "Empêche le pneumothorax sous tension",
    "ACE_Medical-InfoWidget_KingLT": "Garde les voies aériennes libres",
    "ACE_Medical-InfoWidget_NCDKit": "Traite le pneumothorax sous tension",
    "ACE_Medical-InfoWidget_OxygenMask": "Apporte de l'oxygène",
    "ACE_Medical-Item_ChestSeal_Description": "Pansement spécial pour les plaies ouvertes du thorax.",
    "ACE_Medical-Item_ChestSeal_Name": "Pansement thoracique",
    "ACE_Medical-Item_KingLT_Description": "Tube laryngé introduit dans la gorge pour garder les voies aériennes libres.",
    "ACE_Medical-Item_KingLT_Name": "Tube laryngé King LT",
    "ACE_Medical-Item_NCDKit_Description": "Kit pour l'exsufflation à l'aiguille d'un pneumothorax sous tension.",
    "ACE_Medical-Item_NCDKit_Name": "Kit d'exsufflation (NCD)",
    "ACE_Medical-Item_OxygenMask_Description": "Masque pour l'oxygénothérapie.",
    "ACE_Medical-Item_OxygenMask_Name": "Masque à oxygène",
    "ACE_Medical-Notification_BREATHING_RESULT": "Fréquence respiratoire : %1 par minute",
    "ACE_Medical-Notification_SPO2_RESULT": "Saturation en oxygène (SpO2) : %1%",
    "ACE_Medical-OpenPneumothorax": "Pneumothorax ouvert",
    "ACE_Medical-RadialMenu_AirwayThoraxManagement": "Voies aériennes et thorax",
    "ACE_Medical-TensionPneumothorax": "Pneumothorax sous tension",
    "ACE_Medical-UserAction_CheckBreathing": "Mesurer la respiration",
    "ACE_Medical-UserAction_CheckSpO2": "Mesurer la SpO2",
    "ACE_Medical-UserAction_ChestSeal": "Poser un pansement thoracique",
    "ACE_Medical-UserAction_ClearVomit": "Dégager les vomissures",
    "ACE_Medical-UserAction_KingLT": "Poser le tube King LT",
    "ACE_Medical-UserAction_NeedleDecompression": "Exsuffler à l'aiguille",
    "ACE_Medical-UserAction_OxygenMask": "Mettre le masque à oxygène",
    "ACE_Medical-UserAction_TiltHead": "Basculer la tête en arrière",
    # --- Medical Circulation
    "ACE_Medical-AmmoniumCarbonatePackageAction": "Faire respirer les sels",
    "ACE_Medical-BloodState_Class1": "Hémorragie de classe I",
    "ACE_Medical-BloodState_Class2": "Hémorragie de classe II",
    "ACE_Medical-BloodState_Class3": "Hémorragie de classe III",
    "ACE_Medical-BloodState_Class4": "Hémorragie de classe IV",
    "ACE_Medical-CheckBloodPressureAction": "Mesurer la tension",
    "ACE_Medical-CheckPulseAction": "Prendre le pouls",
    "ACE_Medical-CPRAction": "Faire un massage cardiaque",
    "ACE_Medical-Editor_TooltipDetail_Brain_Name": "Cerveau",
    "ACE_Medical-Editor_TooltipDetail_VitalSigns_Name": "Signes vitaux",
    "ACE_Medical-FailReason_NotOnBack": "Le patient n'est pas sur le dos",
    "ACE_Medical-InfoWidget_AmmoniumCarbonate": "Aide à reprendre connaissance",
    "ACE_Medical-InfoWidget_Epinephrine": "Accélère le cœur",
    "ACE_Medical-InfoWidget_Metoprolol": "Ralentit le cœur",
    "ACE_Medical-InfoWidget_Naloxone": "Antidote des opioïdes comme la morphine",
    "ACE_Medical-InfoWidget_Phenylephrine": "Remonte la tension, ralentit le saignement et la perfusion",
    "ACE_Medical-Item_AmmoniumCarbonate_Description": "Sels à faire respirer à un patient inconscient.",
    "ACE_Medical-Item_AmmoniumCarbonate_Name": "Carbonate d'ammonium (sels)",
    "ACE_Medical-Item_Epinephrine_Description": "À injecter à un patient en arrêt cardiaque.",
    "ACE_Medical-Item_Epinephrine_Name": "Épinéphrine",
    "ACE_Medical-Item_Metoprolol_Description": "À injecter à un patient dont le cœur bat trop vite.",
    "ACE_Medical-Item_Metoprolol_Name": "Métoprolol",
    "ACE_Medical-Item_Naloxone_Description": "À injecter en cas de surdose de morphine.",
    "ACE_Medical-Item_Naloxone_Name": "Naloxone",
    "ACE_Medical-Item_Phenylephrine_Description": "À injecter pour remonter la tension d'un patient.",
    "ACE_Medical-Item_Phenylephrine_Name": "Phényléphrine",
    "ACE_Medical-MetoprololInjectionAction": "Injecter le métoprolol",
    "ACE_Medical-NaloxoneInjectionAction": "Injecter la naloxone",
    "ACE_Medical-Notification_BLOOD_PRESSURE_RESULT": "Tension artérielle : %1/%2 mmHg",
    "ACE_Medical-Notification_PULSE_RESULT": "Pouls : %1 par minute",
    "ACE_Medical-PhenylephrineInjectionAction": "Injecter la phényléphrine",
    # --- Medical Core
    "ACE_Medical-AddonName": "Médical",
    "ACE_Medical-Editor_TooltipDetail_Resilience_Name": "Résistance",
    "ACE_Medical-EpinephrineInjectionAction": "Injecter l'épinéphrine",
    "ACE_Medical-FailReason_NoPain": "Le patient ne souffre pas",
    "ACE_Medical-FailReason_NotUnconscious": "Le patient est conscient",
    "ACE_Medical-FailReason_TooInjured": "Le patient est trop gravement blessé",
    "ACE_Medical-Item_Epinephrine_Description": "À injecter à un patient inconscient.",
    "ACE_Medical-InfoWidget_Morphine": "Soulage les fortes douleurs",
    "ACE_Medical-Item_Morphine_Description": "À injecter à un patient qui souffre beaucoup. Une seule dose.",
    "ACE_Medical-KeyBind_OpenRadialMenu": "Ouvrir le menu médical",
    "ACE_Medical-RadialMenu_ExaminePatient": "Examiner le patient",
    "ACE_Medical-RadialMenu_FluidReplacement": "Perfusion",
    "ACE_Medical-RadialMenu_FractureManagement": "Fractures",
    "ACE_Medical-RadialMenu_HemorrhageControl": "Arrêter les hémorragies",
    "ACE_Medical-RadialMenu_Medication": "Médicaments",
    "ACE_Medical-UserAction_BackPosition": "Mettre sur le dos",
    "ACE_Medical-UserAction_LeftRecoveryPosition": "Mettre en PLS, côté gauche",
    "ACE_Medical-UserAction_RightRecoveryPosition": "Mettre en PLS, côté droit",
    # --- Overheating
    "ACE_Overheating-AddonName": "Surchauffe",
    "ACE_Overheating-CheckBarrelTemperaturAction": "Vérifier la température du canon",
    "ACE_Overheating-CoolBarrelInWaterAction": "Refroidir le canon dans l'eau",
    "ACE_Overheating-Notification_BARREL_TEMPERATURE_RESULT": "Température du canon : %1 °C",
    "ACE_Overheating-SwapBarrelAction": "Changer de canon",
    "ACE_Overheating_TryClearWeaponJam": "Tenter de désenrayer l'arme",
    # --- Radio
    "ACE_Radio-FailReason_NotLoaded": "Aucune clé chargée",
    "ACE_Radio-Item_KYK13_Description": "Boîtier qui transfère des clés de chiffrement vers les radios.",
    "ACE_Radio-Item_KYK13_Name": "Chargeur de clés KYK-13",
    "ACE_Radio-Settings-Radio": "Radio",
    "ACE_Radio-Settings-Radio_Beep_Ch1": "Bip d'émission du canal 1",
    "ACE_Radio-Settings-Radio_Beep_Ch2": "Bip d'émission du canal 2",
    "ACE_Radio-Settings-Radio_Beep_High": "Aigu",
    "ACE_Radio-Settings-Radio_Beep_Low": "Grave",
    "ACE_Radio-Settings-Radio_Beep_Off": "Aucun",
    "ACE_Radio-Settings_Beep_Cycle": "Bip au changement d'émetteur",
    "ACE_Radio-Settings_Click_Off": "Clic de fin d'émission",
    "ACE_Radio-UserAction_Address": "Adresse : %1",
    "ACE_Radio-UserAction_ClearAll": "Tout effacer",
    "ACE_Radio-UserAction_CopyKey": "Copier %1 vers %2",
    "ACE_Radio-UserAction_LoadKey": "Charger %1",
    # --- Scopes
    "ACE_Scopes-AddonName": "Lunettes",
    "ACE_Scopes-KeyBind_ACE_HorizontalZeroingLeft_Name": "Dérive : vers la gauche",
    "ACE_Scopes-KeyBind_ACE_HorizontalZeroingRight_Name": "Dérive : vers la droite",
    "ACE_Scopes-KeyBind_VerticalZeroingDown_Name": "Hausse : baisser",
    "ACE_Scopes-KeyBind_VerticalZeroingUp_Name": "Hausse : monter",
    # --- Tactical Ladder, Periscope
    "ACE_TacticalLadder-Item_Description": "Échelle portable pour franchir un obstacle ou entrer par le haut.",
    "ACE_TacticalLadder-Item_Name": "Échelle télescopique",
    "ACE_TacticalLadder-UserAction_Extend": "Déployer",
    "ACE_TacticalLadder-UserAction_TooShort": "Trop courte",
    "ACE_TacticalLadder-UserAction_TopExitObstructed": "Sortie du haut encombrée",
    "ACE_TacticalPeriscope-Item_Periscope_Description": "Périscope à main pour observer sans quitter son abri.",
    "ACE_TacticalPeriscope-Item_Periscope_Name": "Périscope",
    # --- Trenches
    "ACE_Trenches-EditableEntity_PersonalTrench_01_big_Name": "Tranchée individuelle (grande)",
    "ACE_Trenches-EditableEntity_PersonalTrench_01_giant_Name": "Tranchée individuelle (très grande)",
    "ACE_Trenches-EditableEntity_PersonalTrench_01_small_Name": "Tranchée individuelle (petite)",
    "ACE_Trenches-EditableEntity_TrenchSegment_01_short_Name": "Segment de tranchée (court)",
    "ACE_Trenches-EditableEntity_VehicleTrench_01_Name": "Tranchée pour véhicule",
    "ACE_Trenches-Item_SandbagContainer_Description": "Conteneur pour transporter des sacs de sable vides.",
    "ACE_Trenches-Item_SandbagContainer_Name": "Conteneur de sacs de sable",
    # --- Weather
    "ACE_Weather-AddonName": "Météo ACE",
    "ACE_Weather-Item_Kacestrel4500_Description": "Station météo de poche : vent, température.",
    "ACE_Weather-Item_Kacestrel4500_Name": "Anémomètre Kacetrel 4500",
    "ACE_Weather-Keybind_WindInfoToggle": "Afficher le vent",
}
# Dans Medical Core, deux identifiants existent aussi dans Circulation avec un autre sens : ceux du socle médical
FR_PAR_TABLE = {
    "Language/ACE_Medical_Core_localization.fr_fr.conf": {
        "ACE_Medical-InfoWidget_Epinephrine": "Aide à reprendre connaissance",
        "ACE_Medical-Item_Epinephrine_Name": "Épinéphrine",
    },
}

subprocess.run([sys.executable, os.path.join(ICI, 'extraire_textes.py')], check=True)
tables = json.load(io.open(os.path.join(ICI, 'textes_ace.json'), encoding='utf-8'))
traduits = manquants = 0
for t in tables:
    local = FR_PAR_TABLE.get(t['chemin'], {})
    textes = []
    for l in t['lignes']:
        fr = local.get(l['id'], FR.get(l['id']))
        if fr is None:
            fr = l['fr_mod']
            manquants += 1
            print('  non traduit :', l['id'], '->', fr)
        else:
            traduits += 1
        assert '"' not in fr and '\n' not in fr, l['id']
        textes.append(fr)
    # On reprend le fichier du mod ligne à ligne : seules les chaînes du second bloc changent
    sortie, bloc, n = [], 0, 0
    for ligne in t['source'].split('\n'):
        brut = ligne.strip()
        if brut.endswith('{') and not brut.startswith('"'):
            bloc += 1            # 1 = StringTableRuntime, 2 = Ids, 3 = textes
        if bloc == 3 and brut.startswith('"'):
            sortie.append(ligne[:len(ligne) - len(ligne.lstrip())] + '"' + textes[n] + '"')
            n += 1
        else:
            sortie.append(ligne)
    assert n == len(textes), (t['chemin'], n, len(textes))
    assert t['guid'], t['chemin']
    dest = os.path.join(MOD, t['chemin'].replace('/', os.sep))
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    io.open(dest, 'w', encoding='utf-8', newline='\n').write('\n'.join(sortie))
    io.open(dest + '.meta', 'w', encoding='utf-8', newline='\n').write(META % (t['guid'], t['chemin']))
print('%d tables écrites dans le mod, %d textes traduits, %d laissés tels quels' % (len(tables), traduits, manquants))
