# Consignes communes aux codeurs — refonte du front (#75) + Commandeur ennemi (#76)

Jack compilera TOUT d'un seul coup à la fin, sans compilation intermédiaire. Chaque ligne doit compiler du premier coup,
et le comportement doit être exactement celui décidé. Tu n'as pas de compilateur : le contrôleur statique et ta rigueur le remplacent.

## Chemins
- SP = %USERPROFILE%\AppData\Local\Temp\claude\C--Users-goule-Documents\e1f1bd8a-58e8-4a5d-8f58-bc003292c2d8\scratchpad
- Code de travail (TU ÉCRIS ICI) : SP\front\code\SimpleRP  (copie de Scripts/Game/SimpleRP du mod, avec les 20 squelettes neufs)
- Prefabs neufs : SP\front\code\Prefabs\<sous-chemin du mod> ; prefab du mode de jeu : SP\front\code\Prefabs\SRP_GameMode.et
- Squelettes d'origine (référence, NE PAS MODIFIER) : SP\front\code\_squelettes
- Vrai mod : %USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP — N'Y ÉCRIS JAMAIS, n'y fais JAMAIS « cd ». Lecture seule si besoin (Prefabs, Configs).
- Bot et site (copie de travail) : SP\front\code\bot

## Documents qui font foi (dans cet ordre de priorité)
1. SP\front\plan\arbitrages.txt — arbitrages de Jack (Q1-Q10 front, C1-C8 Commandeur) : priment sur tout.
2. SP\front\code\CONTRATS_FRONT.md et SP\front\code\CONTRATS_CMD.md — contrats figés (classes, méthodes publiques, ordre des appels,
   front.json, évènements, réglages, et §8 : méthodes NOUVELLES attendues dans les fichiers EXISTANTS avec leur signature exacte).
3. Les squelettes dans SP\front\code\SimpleRP : signatures, champs, commentaires //! qui disent quoi faire.
4. SP\front\code\par_fichier\<NomFichier>.md — consignes détaillées de conception pour chaque fichier (certaines sont déclarées
   caduques dans les contrats : les contrats priment).
5. Conception complète si besoin : SP\front\plan\mod_*.json (front), SP\front\cmd\plan\sous_*.json (Commandeur), et les deux
   coherence.json. Réponses de Jack : SP\front\reponses_lisibles.txt, SP\front\cmd\reponses_lisibles.txt.
6. SP\front\code\GUIDS.md — identifiants des nouveaux prefabs et composants : à utiliser tels quels.

## Règles de travail
- Tu es le SEUL à modifier tes fichiers ; ne modifie AUCUN autre fichier. Si tu as besoin d'un changement ailleurs (méthode
  manquante, signature à changer), ne le fais pas : décris-le précisément dans « demandes_autres_fichiers » de ta réponse.
- Ne change PAS les signatures publiques du contrat (nom, paramètres, type de retour). Tu peux ajouter des méthodes et champs
  protected/private utiles. Si une signature du contrat est vraiment impossible, garde-la et signale-le dans « ecarts_contrat ».
- Remplis TOUS les corps des méthodes de tes fichiers : aucun « TODO », aucun corps vide laissé par paresse, aucun « pas encore
  branché » dans la version finale (sauf si le contrat dit explicitement qu'une méthode reste neutre).
- Fichiers existants : modifie-les avec des remplacements ciblés (outil Edit) ; ne réécris pas un gros fichier en entier ; garde
  intactes toutes les fonctions non concernées ; retire ce que le contrat dit de retirer.
- Style du projet : commentaires en français, sobres, comme le code existant (//! en tête de méthode, // pour expliquer un choix,
  en-tête de fichier qui dit le rôle et qui l'appelle). Textes affichés aux joueurs : français simple, jamais le vrai prénom de Jack, jamais
  les mots « IA », « automatique », « garnison », « groupe », « patrouille » dans les textes radio/Discord qui parlent du Commandeur.
- Tous les textes radio passent par SRP_FrontRadio (seul auteur) ; tous les chiffres réglables passent par SRP_CmdSettings (seul
  lecteur de réglages) avec la valeur par défaut de Jack.

## Règles Enforce (vérifiées sur le code qui compile, 26/09)
- string.Format : 9 paramètres au plus (%1 à %9). Au-delà, découpe en plusieurs Format concaténés.
- Pas de variable nommée map, set ou array (base est permis), ni reference (mot réservé : « Broken expression »).
- Un seul case par valeur (pas de « case A: case B: » empilés) : répète le code ou utilise if/else.
- override seulement si la méthode existe dans une classe parente (vérifie par Grep dans le jeu), même signature.
- Pas de paramètre out avec valeur par défaut. Pas de |= (écris a = a | b).
- Une variable ne peut pas être redéclarée dans le même bloc ni dans un bloc imbriqué (deux blocs frères if/else : permis ;
  deux boucles sœurs for (int i…) : permis). En cas de doute, prends un autre nom.
- Pas de vector + float (vector + vector, vector * float sont permis). Construis Vector(x, y, z).
- Dans une modded class, tout champ ajouté contient SRP_ (ex. m_bSRP_Actif) ; ne redéclare pas un champ qui existe déjà.
- Un composant a sa classe ...Class (class SRP_XComponentClass : ScriptComponentClass {}), comme les composants existants.
- Membres : ref array<ref T> pour les tableaux d'objets possédés ; ref T pour un objet possédé ; attention aux cycles de ref.
- RPC : [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)] ; un Broadcast ne s'exécute pas sur l'hôte : applique aussi en local.
- Délais sauvegardés : en heure Unix (System.GetUnixTime() — vérifie le nom exact dans le jeu), jamais GetTickCount pour ce qui
  est sauvé. GetGame().GetCallqueue().CallLater(fonction, delaiMs, repeter, params…) comme le code existant.
- Vérifie par Grep CHAQUE classe/méthode du jeu que tu appelles : scripts complets du jeu dans SP\vanilla\scripts (Core, GameLib,
  Game…) et des mods dans SP\deps (RHS, ACE, CRX, AMF). Une méthode qui n'existe pas = erreur de compilation qui bloque tout.
- Identifiants de prefabs du jeu : python SP\rdb_find.py "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\addons\data\resourceDatabase.rdb" <nom_de_fichier.et>
  (donne GUID + chemin ; une référence s'écrit "{GUID}Chemin/Du/Fichier.et").

## Contrôleur statique (OBLIGATOIRE avant de rendre)
python SP\front\code\enforce_check.py SP\front\code\SimpleRP --seulement TesFichiers.c,Autre.c
- Il repère : méthodes/types inexistants (jeu + mods + mod), override sans parent, redéclarations, nombre d'arguments faux
  (méthodes du mod au nom unique), règles Enforce ci-dessus, doublons de classe/enum. Zéro fausse alerte sur le code validé.
- Corrige tout ce qui concerne TES fichiers jusqu'à 0 alerte. Une alerte qui vient d'un fichier d'un autre (méthode pas encore
  écrite par lui) n'est pas pour toi : mets-la dans « demandes_autres_fichiers » si la méthode manque vraiment au contrat.
- Il ne vérifie pas les types des arguments ni les conversions : relis toi-même chaque appel.
