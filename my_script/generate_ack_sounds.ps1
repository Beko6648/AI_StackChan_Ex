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
        $phrases += @{
            file             = "${exp}_$($phrase.romaji).wav"
            text             = $phrase.text
            params           = $phrase.params
            kana             = $phrase.kana
            is_interrogative = $phrase.is_interrogative
        }
    }
}

# voicevox_speakers.json から 話者ID → "キャラクター名_スタイル名" のマップを作成する
$speakerNameMap = @{}
if (Test-Path $speakersFile) {
    $speakersData  = Get-Content $speakersFile -Raw -Encoding UTF8 | ConvertFrom-Json
    # PS5.1 の ConvertFrom-Json は JSON 配列を value プロパティにラップするため展開する
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
            $queryUrl    = "$baseUrl/audio_query?text=$([Uri]::EscapeDataString($text))&speaker=$speakerId"
            $queryBytes  = $client.UploadData($queryUrl, "POST", [System.Text.Encoding]::UTF8.GetBytes(""))
            $queryObject = [System.Text.Encoding]::UTF8.GetString($queryBytes) | ConvertFrom-Json

            # params が定義されている場合は audio_query のトップレベルパラメータを上書きする
            if ($null -ne $phrase.params) {
                foreach ($param in $phrase.params.PSObject.Properties) {
                    $queryObject.$($param.Name) = $param.Value
                }
            }

            # kana が定義されている場合は AquesTalk 記法でアクセントを再計算して差し替える
            # 記法例: フ'ム（'の直後から音が下がる頭高型）、フム（音が下がらない平板型）
            if ($null -ne $phrase.kana -and $phrase.kana -ne "") {
                $accentUrl = "$baseUrl/accent_phrases?text=$([Uri]::EscapeDataString($phrase.kana))&speaker=$speakerId&is_kana=true"
                $accentResponse = Invoke-WebRequest -Uri $accentUrl -Method POST -UseBasicParsing -SkipHttpErrorCheck
                if ($accentResponse.StatusCode -ne 200) { throw "accent_phrases API error: $($accentResponse.StatusCode)" }
                $queryObject.accent_phrases = @($accentResponse.Content | ConvertFrom-Json)
            }

            # is_interrogative が定義されている場合は最後のアクセント句に適用する（語尾の上がり下がり）
            if ($null -ne $phrase.is_interrogative) {
                $queryObject.accent_phrases[-1].is_interrogative = $phrase.is_interrogative
            }

            $queryJson = $queryObject | ConvertTo-Json -Depth 10

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
