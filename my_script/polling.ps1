# polling.ps1 - StackChan <-> Claude Code polling script

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

$ip          = "192.168.1.114"
$ms          = 500
$claudeDir   = "C:\StackChan_Claude"  # StackChan 専用環境（CLAUDE.md・メモリ）
$pendingUrl  = "http://" + $ip + "/pending_command"
$resultUrl   = "http://" + $ip + "/command_result"
$ct          = "application/json"
$tag         = ([char]91) + "polling" + ([char]93)
$sessionFile = Join-Path $claudeDir "stackchan_session.txt"

# セッションID管理
if (Test-Path $sessionFile) {
    $sessionId      = (Get-Content $sessionFile).Trim()
    $sessionCreated = $true
    Write-Host ($tag + " セッション再開: " + $sessionId)
} else {
    $sessionId      = [System.Guid]::NewGuid().ToString()
    $sessionCreated = $false
    # 初回起動時に即座にセッションIDを保存（応答の有無に関わらず）
    Set-Content -Path $sessionFile -Value $sessionId
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

            # デバッグ：system_prompt が実際に渡されてるか確認
            if ($sysPrompt -and $sysPrompt.Length -gt 100) {
                Write-Host ($tag + " [DEBUG] sysPrompt length: " + $sysPrompt.Length + " chars (first 80: " + $sysPrompt.Substring(0, 80) + ")")
            } elseif (-not $sysPrompt) {
                Write-Host ($tag + " [DEBUG] sysPrompt is empty or null")
            }

            # claude -p でレスポンス生成（セッション管理付き）
            if (-not $sessionCreated) {
                # 初回：--session-id で新規セッションを作成
                if ($sysPrompt) {
                    Write-Host ($tag + " [DEBUG] Calling claude with --system-prompt (length: " + $sysPrompt.Length + ")")
                    $voice = (claude -p $prompt --system-prompt $sysPrompt --session-id $sessionId --add-dir $claudeDir --model haiku 2>&1 | Out-String).Trim()
                } else {
                    Write-Host ($tag + " [DEBUG] Calling claude WITHOUT --system-prompt")
                    $voice = (claude -p $prompt --session-id $sessionId --add-dir $claudeDir --model haiku 2>&1 | Out-String).Trim()
                }
                if ($voice -match '\[META\]') {
                    Write-Host ($tag + " [DEBUG] [META] tag found in response")
                } else {
                    Write-Host ($tag + " [DEBUG] NO [META] tag in response (first 80 chars: " + $voice.Substring(0, [Math]::Min(80, $voice.Length)) + ")")
                }
                $sessionCreated = $true
                if (-not $voice) {
                    Write-Host ($tag + " [DEBUG] Claude returned empty on first query.")
                }
            } else {
                # 2回目以降：--resume でセッションを引き継ぐ
                if ($sysPrompt) {
                    Write-Host ($tag + " [DEBUG] Calling claude --resume with --system-prompt (length: " + $sysPrompt.Length + ")")
                    Write-Host "=== SYSTEM PROMPT ==="
                    Write-Host $sysPrompt
                    Write-Host "=== USER PROMPT ==="
                    Write-Host $prompt
                    Write-Host "=== CLAUDE RESPONSE ==="
                    $voice = (claude -p $prompt --system-prompt $sysPrompt --resume $sessionId --add-dir $claudeDir --model haiku 2>&1 | Out-String).Trim()
                } else {
                    Write-Host ($tag + " [DEBUG] Calling claude --resume WITHOUT --system-prompt")
                    Write-Host "=== USER PROMPT ==="
                    Write-Host $prompt
                    Write-Host "=== CLAUDE RESPONSE ==="
                    $voice = (claude -p $prompt --resume $sessionId --add-dir $claudeDir --model haiku 2>&1 | Out-String).Trim()
                }
                Write-Host $voice
                Write-Host "=== END ==="

                if ($voice -match '\[META\]') {
                    Write-Host ($tag + " [DEBUG] [META] tag found in response")
                } else {
                    Write-Host ($tag + " [DEBUG] NO [META] tag in response (first 80 chars: " + $voice.Substring(0, [Math]::Min(80, $voice.Length)) + ")")
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