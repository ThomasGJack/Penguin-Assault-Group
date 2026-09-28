# PAG-Bot : installation complete sur Windows, en une fois.
#  1. Python (via winget si absent)   2. environnement virtuel + dependances   3. config.json avec un secret aleatoire
#  4. jeton du bot (saisie masquee, enregistre dans les variables d'environnement de ton compte)
#  5. regle de pare-feu pour le port   6. demarrage automatique a l'ouverture de session (tache planifiee)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot
$env:PYTHONUTF8 = "1"

function Etape($t) { Write-Host ""; Write-Host "=== $t" -ForegroundColor Cyan }

# ---------------------------------------------------------------- 1. Python
Etape "Python"
$py = $null
foreach ($c in @("python", "py")) {
    try { $v = & $c --version 2>&1; if ($LASTEXITCODE -eq 0 -and "$v" -match "Python 3\.(1[0-9]|[89])") { $py = $c; break } } catch {}
}
if (-not $py) {
    Write-Host "Python absent : installation via winget (accepte la demande Windows si elle apparait)..."
    winget install --id Python.Python.3.12 -e --accept-source-agreements --accept-package-agreements
    if ($LASTEXITCODE -ne 0) { Write-Host "winget a echoue. Installe Python 3.12 depuis https://www.python.org/downloads/ (coche 'Add python.exe to PATH') puis relance." -ForegroundColor Red; exit 1 }
    # le PATH du processus courant ne connait pas encore le nouveau Python
    $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
    $py = "python"
}
Write-Host "Python : $(& $py --version)"

# ---------------------------------------------------------------- 2. venv + dependances
Etape "Dependances"
if (-not (Test-Path "venv")) { & $py -m venv venv }
& "venv\Scripts\python.exe" -m pip install --quiet --upgrade pip
& "venv\Scripts\python.exe" -m pip install --quiet -r requirements.txt
Write-Host "discord.py et aiohttp installes dans venv\"

# ---------------------------------------------------------------- 3. config.json
Etape "Configuration"
if (-not (Test-Path "config.json")) {
    $cfg = Get-Content "config.example.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    $guild = Read-Host "Identifiant du serveur Discord (Entree = $($cfg.guild_id))"
    if ($guild) { $cfg.guild_id = [int64]$guild.Trim() }
    $bytes = New-Object byte[] 24; [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
    $cfg.secret = ($bytes | ForEach-Object { $_.ToString("x2") }) -join ""
    $cfg | ConvertTo-Json -Depth 5 | Out-File "config.json" -Encoding utf8
    Write-Host "config.json cree avec un secret aleatoire."
} else {
    $cfg = Get-Content "config.json" -Raw -Encoding UTF8 | ConvertFrom-Json
    Write-Host "config.json existant conserve."
}

# ---------------------------------------------------------------- 4. jeton
Etape "Jeton du bot"
$existing = [Environment]::GetEnvironmentVariable("DISCORD_TOKEN", "User")
if ($existing) {
    $r = Read-Host "Un jeton est deja enregistre pour ton compte. Le remplacer ? (o/N)"
    if ($r -ne "o" -and $r -ne "O") { $existing = $existing } else { $existing = $null }
}
if (-not $existing) {
    $secure = Read-Host "Colle le jeton du bot (Developer Portal > Bot > Reset Token) puis Entree ; rien ne s'affiche, c'est normal" -AsSecureString
    $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure)
    $token = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
    [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
    if (-not $token) { Write-Host "Jeton vide, abandon." -ForegroundColor Red; exit 1 }
    [Environment]::SetEnvironmentVariable("DISCORD_TOKEN", $token.Trim(), "User")
    Write-Host "Jeton enregistre dans les variables d'environnement de ton compte Windows (jamais dans un fichier du dossier)."
}

# ---------------------------------------------------------------- 5. pare-feu
Etape "Pare-feu"
$port = [int]$cfg.port
try {
    $rule = Get-NetFirewallRule -DisplayName "PAG-Bot" -ErrorAction SilentlyContinue
    if (-not $rule) {
        New-NetFirewallRule -DisplayName "PAG-Bot" -Direction Inbound -Protocol TCP -LocalPort $port -Action Allow | Out-Null
        Write-Host "Regle de pare-feu ajoutee pour le port $port."
    } else { Write-Host "Regle de pare-feu deja presente." }
} catch {
    Write-Host "Impossible d'ajouter la regle de pare-feu (lance installer.bat en administrateur pour l'avoir). Le bot fonctionne quand meme sur le PC." -ForegroundColor Yellow
}

# ---------------------------------------------------------------- 6. demarrage automatique
Etape "Demarrage automatique"
$r = Read-Host "Lancer le bot automatiquement a chaque ouverture de session Windows ? (O/n)"
if ($r -ne "n" -and $r -ne "N") {
    $action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$PSScriptRoot\lancer.ps1`" -Silencieux"
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
    $settings = New-ScheduledTaskSettingsSet -RestartCount 999 -RestartInterval (New-TimeSpan -Minutes 1) -ExecutionTimeLimit ([TimeSpan]::Zero) -StartWhenAvailable
    Register-ScheduledTask -TaskName "PAG-Bot" -Action $action -Trigger $trigger -Settings $settings -Force | Out-Null
    Write-Host "Tache planifiee 'PAG-Bot' creee : le bot demarre avec ta session, en arriere-plan (journal dans pag-bot.log)."
}

# ---------------------------------------------------------------- resume
Etape "A reporter sur le serveur de jeu"
$ip = "TON-IP-PUBLIQUE"
try { $ip = (Invoke-RestMethod -Uri "https://api.ipify.org" -TimeoutSec 5) } catch {}
Write-Host "Dans profile/SimpleRP/discord.json du serveur LobbyHost, ajoute :"
Write-Host "  `"bridge_url`": `"http://$ip`:$port/`","
Write-Host "  `"bridge_secret`": `"$($cfg.secret)`""
Write-Host ""
Write-Host "Sur ta box Internet : redirige le port $port TCP vers ce PC, sinon le serveur de jeu ne pourra pas joindre le bot." -ForegroundColor Yellow
Write-Host "Installation terminee. Lance lancer.bat pour demarrer le bot maintenant." -ForegroundColor Green
