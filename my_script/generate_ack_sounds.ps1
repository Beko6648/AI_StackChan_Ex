# VoiceVox を使って相槌音声ファイルを一括生成するスクリプト
# 話者IDとセリフの追加・変更は ack_phrases.json を編集する

# ====== 設定 ======
$outputDir    = "$PSScriptRoot\sounds"
$baseUrl      = "http://localhost:50021"
$phrasesFile  = "$PSScriptRoot\json\ack_phrases.json"
$speakersFile = "$PSScriptRoot\json\voicevox_speakers.json"
# ==================

# ack_phrases.json から話者IDとセリフ設定を読み込む
$config     = Get-Content $phrasesFile -Raw -Encoding UTF8 | ConvertFrom-Json
$speakerIds = $config.speakerIds

# 感情ごとのセリフをフラットなリストに展開する
$phrases = @()
foreach ($exp in $config.expressions.PSObject.Properties.Name) {
    foreach ($phrase in $config.expressions.$exp) {
        $phrases += @{ file = "${exp}_$($phrase.romaji).wav"; text = $phrase.text }
    }
}

# voicevox_speakers.json から 話者ID → "キャラクター名_スタイル名" のマップを作成する
$speakerNameMap = @{}
if (Test-Path $speakersFile) {
    $speakersData  = Get-Content $speakersFile -Raw -Encoding UTF8 | ConvertFrom-Json
    # PS5.1 で生成した場合は配列が value プロパティにラップされているため展開する
    $speakersArray = if ($speakersData.PSObject.Properties.Name -contains 'value') { $speakersData.value } else { $speakersData }
    foreach ($speaker in $speakersArray) {
        foreach ($style in $speaker.styles) {
            $speakerNameMap["$($style.id)"] = "$($speaker.name)_$($style.name)"
        }
    }
} else {
    Write-Host "警告: $speakersFile が見つかりません。話者名を取得できませんでした。"
}

$client = New-Object System.Net.WebClient
$client.Encoding = [System.Text.Encoding]::UTF8

foreach ($speakerId in $speakerIds) {
    # 話者ごとの出力フォルダを作成する（ID + 話者名）
    $speakerLabel = if ($speakerNameMap.ContainsKey("$speakerId")) { $speakerNameMap["$speakerId"] } else { "unknown" }
    $speakerDir   = "$outputDir\${speakerId}_${speakerLabel}"
    if (-not (Test-Path $speakerDir)) {
        New-Item -ItemType Directory -Path $speakerDir | Out-Null
    }

    Write-Host "===== Speaker ID: $speakerId ($speakerLabel) ====="

    foreach ($phrase in $phrases) {
        $text    = $phrase.text
        $outPath = "$speakerDir\$($phrase.file)"

        Write-Host "  Generating: $($phrase.file) ($text) ..."

        try {
            # Step1: audio_query を取得する（空ボディで POST）
            $queryUrl   = "$baseUrl/audio_query?text=$([Uri]::EscapeDataString($text))&speaker=$speakerId"
            $queryBytes = $client.UploadData($queryUrl, "POST", [System.Text.Encoding]::UTF8.GetBytes(""))
            $queryJson  = [System.Text.Encoding]::UTF8.GetString($queryBytes)

            # Step2: 音声合成して WAV ファイルを保存する
            $synthUrl = "$baseUrl/synthesis?speaker=$speakerId"
            $client.Headers["Content-Type"] = "application/json"
            $wavBytes = $client.UploadData($synthUrl, "POST", [System.Text.Encoding]::UTF8.GetBytes($queryJson))
            $client.Headers.Remove("Content-Type")
            [System.IO.File]::WriteAllBytes($outPath, $wavBytes)

            Write-Host "    -> Saved: $outPath"
        } catch {
            Write-Host "    -> Error: $($_.Exception.Message)"
        }
    }

    Write-Host ""
}

Write-Host "Done."
