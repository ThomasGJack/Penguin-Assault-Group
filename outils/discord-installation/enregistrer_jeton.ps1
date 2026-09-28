# Enregistre le jeton du bot dans les variables d'environnement de ton compte Windows (saisie masquee).
$s = Read-Host "Colle le jeton du bot puis Entree (rien ne s'affiche, c'est normal)" -AsSecureString
$p = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($s)
$t = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($p)
[Runtime.InteropServices.Marshal]::ZeroFreeBSTR($p)
if (-not $t) { Write-Host "Jeton vide, rien d'enregistre."; exit 1 }
[Environment]::SetEnvironmentVariable("DISCORD_TOKEN", $t, "User")
Write-Host "Jeton enregistre (longueur $($t.Length)). Tu peux fermer cette fenetre et dire ok a Claude."
