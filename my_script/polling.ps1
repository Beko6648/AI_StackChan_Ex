# polling.ps1 - StackChan <-> Claude Code polling script

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$ip  = "192.168.1.114"
$ms  = 500
$pendingUrl  = "http://" + $ip + "/pending_command"
$resultUrl   = "http://" + $ip + "/command_result"
$ct          = "application/json"
$tag         = ([char]91) + "polling" + ([char]93)
$sessionFile = Join-Path $PSScriptRoot "stackchan_session.txt"

# セッションID管理
if (Test-Path $sessionFile) {
    $sessionId      = (Get-Content $sessionFile).Trim()
    $sessionCreated = $true
    Write-Host ($tag + " セッション再開: " + $sessionId)
} else {
    $sessionId      = [System.Guid]::NewGuid().ToString()
    $sessionCreated = $false
    Write-Host ($tag + " 新規セッション: " + $sessionId)
}

Write-Host ($tag + " 起動")

while ($true) {
    try {
        $res = Invoke-RestMethod -Uri $pendingUrl -Method GET -TimeoutSec 2 -ErrorAction Stop
        if ($null -ne $res.command_id) {
            $type = if ($res.type) { $res.type } else { "user" }
            Write-Host ($tag + " コマンド受信 [" + $type + "]: " + $res.text)

            # type に応じてプロンプトを組み立てる
            # user: StackChan のシステムプロンプトをそのまま使い、text をユーザー質問として渡す
            # self_talk: text がすでに完全なプロンプトなのでそのまま渡す
            if ($type -eq "self_talk") {
                $prompt    = $res.text
                $sysPrompt = $null
            } else {
                $prompt    = $res.text
                $sysPrompt = $res.system_prompt
            }

            # claude -p でレスポンス生成（セッション管理付き）
            if (-not $sessionCreated) {
                # 初回：--session-id で新規セッションを作成
                # --add-dir でクローディアの CLAUDE.md・メモリを読み込む（ミニクローディア構成）
                if ($sysPrompt) {
                    $voice = (claude -p $prompt --system-prompt $sysPrompt --session-id $sessionId --add-dir "C:\ClaudeCode_work" 2>$null)
                } else {
                    $voice = (claude -p $prompt --session-id $sessionId --add-dir "C:\ClaudeCode_work" 2>$null)
                }
                if ($voice) {
                    Set-Content -Path $sessionFile -Value $sessionId
                    $sessionCreated = $true
                    Write-Host ($tag + " セッション保存: " + $sessionId)
                }
            } else {
                # 2回目以降：--resume でセッションを引き継ぐ
                if ($sysPrompt) {
                    $voice = (claude -p $prompt --system-prompt $sysPrompt --resume $sessionId --add-dir "C:\ClaudeCode_work" 2>$null)
                } else {
                    $voice = (claude -p $prompt --resume $sessionId --add-dir "C:\ClaudeCode_work" 2>$null)
                }
            }

            $errorText = if ($res.error_text) { $res.error_text } else { "ごめん、うまく考えられなかった。もう一回聞いてみて" }
            if (-not $voice) { $voice = $errorText }

            Write-Host ($tag + " 応答: " + $voice)
            $body = (@{ command_id = $res.command_id; voice_text = $voice } | ConvertTo-Json -Compress)
            Invoke-RestMethod -Uri $resultUrl -Method POST -Body $body -ContentType $ct -ErrorAction Stop
            Write-Host ($tag + " 送信完了")
        }
    }
    catch {
        if ($_.Exception.Message -notlike "*Timeout*" -and $_.Exception.Message -notlike "*canceled*") {
            Write-Host ($tag + " エラー: " + $_)
            Start-Sleep -Seconds 3
        }
    }
    Start-Sleep -Milliseconds $ms
}