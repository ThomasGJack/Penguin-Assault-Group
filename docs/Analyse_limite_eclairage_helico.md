# Pourquoi une vraie lumière sur l'hélico n'éclaire pas le sol

Recherche du 24/09/2026 : 4 enquêtes (historique de nos essais et journaux, fichiers du jeu et du moteur, documentation et forums en ligne, les 539 mods installés), une synthèse, puis deux relectures critiques.

## En bref

- **Ce qui est prouvé.** Toutes les lumières dont la source était à 80 m ou moins de la tache ont éclairé le sol. Toutes celles posées sur l'hélico ou juste dessous (entre 146 et 280 m de la tache) n'ont jamais éclairé le sol vu depuis le soldat. Ce n'est pas un bug de création : les journaux montrent les lampes créées et accrochées à l'hélico. Leur orientation n'est pas écrite dans les journaux, mais le ruban, qui suit exactement le même calcul, tombe au bon endroit.
- **Nos réglages du 24/09 étaient mauvais.** Ils étaient 10 à 15 fois trop faibles, mais ça n'explique pas tout.
- **Le moteur a très probablement une limite à lui.** Le 20/09, une lumière très forte, libre et placée juste sous l'hélico, se voyait depuis Zeus près de l'hélico, mais n'éclairait pas le sol vu depuis le soldat. Nos réglages faibles du 24/09 auraient dû donner au moins une tache pâle, et ils n'ont rien donné.
- **Le moteur traite à part les « lumières lointaines ».** Son menu de diagnostic le montre. Aucune documentation publique ne donne la règle ni la distance exacte.
- **Le jeu ne le fait pas non plus.** Aucun projecteur d'hélicoptère, ni du jeu ni des 539 mods, n'éclaire à plus de 100 m. Bohemia a même baissé la puissance des projecteurs en juin 2025. Les seules lumières qui éclairent le sol d'en haut sont les fusées éclairantes, suspendues à 32-64 m.
- **Notre solution actuelle est la bonne.** Une vraie lumière près de la tache (l'appoint), plus le faisceau dessiné. C'est d'ailleurs aussi le principe de la lumière de visée de la caméra de débogage du jeu, posée par un script à 150 m au-dessus du point visé, sans rapport avec la position de la caméra.

## 1. Ce qui est prouvé par nos essais

| Lumière | Source → tache | Réglages | Résultat |
|---|---|---|---|
| Projecteur du stand de tir | 3 à 6 m | LV 2, affaiblissement 0,05, cône 70° | éclaire |
| Appoint au sol (24/09) | 45 m puis 70 m | LV 7,5 → 14, affaiblissement 2, cône 33-54° | éclaire |
| Version publiée du 20/09 | 80 m (placée sur l'axe) | LV 10,5, affaiblissement 0,05, cône 23° | éclaire (trop fort, « lumière divine ») |
| Lumière libre sous l'hélico (20/09) | ~277 m | LV 11, affaiblissement 0,05, cône 16° | vue dans Zeus, rien depuis le soldat |
| Lampe du nez (24/09, toutes versions) | 150 à 280 m | LV 9-15, affaiblissement 2, cône 10° | rien, jamais |
| Phare du Mi-8 repris par script (24/09) | 150 à 280 m | LV 9-15, affaiblissement 2 (au lieu de 0,7), ombres | rien (bascule ON/OFF sans effet) |

Seule la distance sépare nettement ce qui marche de ce qui ne marche pas. L'angle du cône est aussi un indice : tout ce qui échoue a un cône de 16° ou moins, tout ce qui marche a 23° ou plus.

## 2. Ce que nos réglages avaient de faux

D'après le texte du moteur et la documentation officielle de LightEntity :
- **LV** est une échelle « à doubler » : +1 donne une lumière deux fois plus forte.
- **L'affaiblissement** (DistanceAttenuation) est la puissance de la chute avec la distance. 2 correspond à une ampoule nue (chute au carré) ; 0,05 à presque aucune chute.

Lumière reçue au sol, en ordre de grandeur (calcul déduit, pas donné tel quel par Bohemia) : **LV − affaiblissement × log2(distance)**.

| Lumière | Au sol |
|---|---|
| Lampe du nez (LV = 6 + log2 d, affaiblissement 2) | −1,6 à −3,3 |
| Premier appoint, faible mais visible | −3,5 |
| Appoint actuel | +0,5 à +1,1 |
| Lumière libre du 20/09 | +9,4 |
| Phares de voiture, fusées éclairantes, caméra de débogage | +1,4 à +2,5 |

- Notre formule compense la distance **une fois**, alors que l'affaiblissement 2 la fait chuter **deux fois**. Plus l'hélico monte, plus la tache s'assombrit. Le plafond à LV 15 empêche en plus de compenser.
- Pour le phare du Mi-8, on a remplacé son affaiblissement d'origine (0,7) par 2. Ses ombres étaient aussi allumées (valeur par défaut du moteur), alors qu'il est collé à la carlingue.

Mais cette erreur seule prédit une tache pâle, du niveau de ton premier appoint, pas « rien ». Et le 20/09, une lumière environ 1 000 fois plus forte au sol n'a pas marché non plus depuis le soldat. Nos réglages n'étaient donc pas le seul blocage.

## 3. La limite du moteur (probable, pas prouvée)

Le menu de diagnostic du moteur (Workbench, touches Win + Alt, Render > Analytic lights) contient :
- « Disable far lights » ;
- « Show far lights » avec trois choix : disabled, show, no clip ;
- « Show log » (journal des lumières lointaines) ;
- « Farlight intensity ».

Le jeu contient aussi des shaders à part pour ces lumières lointaines (FarLights, FarLightSphere, FarLightCone).

Le moteur a donc un traitement spécial pour les lumières éloignées, avec sa propre « coupure » (clip). La lumière du 20/09 vue dans Zeus près de l'hélico, mais pas depuis le soldat, colle bien avec une règle basée sur la distance entre la caméra et la source. Aucune page publique ne donne cette règle ni sa distance.

**Pistes encore ouvertes** (nos essais ne permettent pas de les départager, parce que la source était toujours à la fois loin de la tache et loin de la caméra) :
1. une coupure selon la distance entre la caméra et la source (circuit « far lights ») ;
2. une portée maximale réelle, quelle que soit la valeur de Radius (aucune lumière n'a jamais éclairé à plus de 80 m de sa source dans nos essais) ;
3. un cône étroit (16° ou moins) jugé « peu important » et écarté ;
4. pour les lampes accrochées à l'hélico : la façon de les accrocher (sans PROXY ni RECALC, contrairement au jeu).

Ce qui n'est pas en cause : le seuil ClipEV (identique sur l'appoint qui marche), un bug d'orientation, l'hélico hors de l'écran.

## 4. Ce que font le jeu et les mods

- **Jeu de base.** Tous les projecteurs et phares d'atterrissage d'hélico (Mi-8, UH-1H) ont une portée de 100 m au plus, orientés vers le bas pour l'atterrissage. Mise à jour 1.4.0.38 (juin 2025) : « projecteurs ajustés pour ne pas créer de points trop brillants la nuit ».
- **Mods installés.** 685 lumières passées en revue. Aucun projecteur d'hélico au-delà de 100 m (MH-60, NH90, MH-6, UH-1H, Mi-8 RHS). Le MH-60 oriente son projecteur en tournant un os du modèle, pas en déplaçant la lumière. Les seules lumières faites pour éclairer d'en haut sont des fusées éclairantes, suspendues à 32-64 m (portée 100-300 m, affaiblissement 0,8 à 1).
- **Caméra de débogage du jeu.** Sa lumière de visée est posée par script à 150 m au-dessus du point visé, pas sur la caméra.
- **Workshop.** Des mods « Mi8 Searchlight » et « Huey Searchlight » existent, mais sans aucun réglage publié, et ils ne sont pas installés.

## 5. Les essais pour trancher (du moins cher au plus cher)

Attends au moins 15 secondes après chaque bascule : l'œil du jeu s'adapte à la lumière. Les essais du 24/09 ne duraient que 1 à 5 secondes.

1. **Menu de diagnostic, sans compiler.** En jeu dans Workbench, la nuit, l'hélico fixé sur toi, appoint coupé au menu Staff. Ouvre le menu de diagnostic (Win + Alt), puis Render > Analytic lights. Mets « Show log » sur true et « Show far lights » sur « no clip », en regardant le sol sous l'hélico. Essaie aussi « Disable far lights » true puis false, avec l'appoint allumé.
   - Si la tache apparaît en « no clip », ou si « Disable far lights » éteint l'appoint : **c'est la coupure « lumières lointaines » du moteur. C'est prouvé.**
   - Si rien ne change : ce n'est pas elle, on passe à l'essai 2.
   - Bonus : GameCode > Lights > « Light positions » montre où sont les lampes et où elles visent.
2. **Éditeur de monde, sans compiler.** Sur un calque d'essai que tu n'enregistres pas, la nuit :
   - pose `SRP_ProjecteurAppoint.et` à 200 m au-dessus d'un champ, tourné vers le bas ;
   - règle LV 17,3, Radius 300, SpotAngle 30 (pour que le cône étroit ne fausse pas l'essai), DistanceAttenuation 2 ;
   - mets la caméra au sol dans la tache, puis rapproche-la lentement de la lampe.
   - Tache visible du sol : **nos réglages étaient le seul problème.**
   - Tache qui apparaît seulement quand la caméra se rapproche : **coupure selon la distance à la caméra.** La distance où elle apparaît donne le seuil.
   - Jamais de tache : **portée maximale réelle.**
3. **Avec compilation.** Seulement si les deux premiers essais ne suffisent pas : éloigner l'appoint (100, 120, 150 m) sans changer son cône, pour trouver la distance à partir de laquelle il s'éteint.

## 6. Ce que ça change pour l'hélico

- **Aujourd'hui :** on garde l'appoint près de la tache, le ruban dessiné et les ombres de l'appoint. C'est la seule façon qui a toujours marché.
- **Phare du Mi-8 :** à retirer, sauf surprise à l'essai 1. Il ne peut rien apporter au sol (portée 100 m dans le jeu, et la même limite).
- **Si l'essai 1 ou 2 montre que c'était seulement nos réglages :** la lampe du nez peut devenir le vrai projecteur, avec l'affaiblissement à 1 et la puissance à 1,7 + log2(distance). L'appoint deviendrait alors facultatif.
- **Si c'est la coupure du moteur :** l'appoint peut être éloigné de la tache jusqu'à la distance limite trouvée, pour que ses ombres partent presque de l'hélico.

Fichiers de travail (tableaux des lumières, calculs, extraits) : dossier `scratchpad\lumiere_limite\` de la session.
