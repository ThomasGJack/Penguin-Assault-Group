# Identifiants fixés (26/09) — à utiliser tels quels, vérifiés libres dans le mod

Nouveaux prefabs (fichier .et + fichier .et.meta, même modèle que Prefabs/Props/SRP_Sirene.et et .meta du mod) :
| Prefab | GUID | Chemin dans le mod |
|---|---|---|
| Repère point clé (centre de localité) | {6A5B0C0D0E0F7A01} | Prefabs/Props/SRP_PointCle_Centre.et |
| Repère point clé (QG de ville ou bourg) | {6A5B0C0D0E0F7A02} | Prefabs/Props/SRP_PointCle_QG.et |
| Poste de commandement ennemi (table, radio, cartes) | {6A5B0C0D0E0F7A03} | Prefabs/Props/SRP_PosteCommandement.et |
| Mât à nos couleurs | {6A5B0C0D0E0F7A04} | Prefabs/Props/SRP_MatCouleurs.et |
| Pièce de mortier ennemie | {6A5B0C0D0E0F7A05} | Prefabs/Missions/SRP_Piece_Mortier.et |

Dans le code, une référence s'écrit "{GUID}Chemin", par exemple "{6A5B0C0D0E0F7A03}Prefabs/Props/SRP_PosteCommandement.et".

Composants ajoutés au prefab du mode de jeu (Prefabs/MP/Modes/Plain/SRP_GameMode.et) :
| Composant | Identifiant dans le prefab |
|---|---|
| SRP_FrontComponent | {6A5B0C0D0E0FF010} |
| SRP_FrontEnemyComponent | {6A5B0C0D0E0FF004} |

Les prefabs neufs s'écrivent dans le dossier de travail : ...\scratchpad\front\code\Prefabs\<même sous-chemin que dans le mod> (ex. code\Prefabs\Props\SRP_PosteCommandement.et). Ils seront copiés dans le mod à l'installation.
Ne JAMAIS utiliser la plage 6A5B0C0D0E0FC0xx (déjà prise dans le mod).
