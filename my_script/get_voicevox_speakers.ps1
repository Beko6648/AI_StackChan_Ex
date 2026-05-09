# VoiceVox の話者一覧を取得して JSON ファイルに保存するスクリプト
# 事前に VoiceVox を起動しておく必要がある

$url        = "http://localhost:50021/speakers"
$outputFile = "$PSScriptRoot\json\voicevox_speakers.json"

Write-Host "VoiceVox から話者一覧を取得中..."

try {
    $client = New-Object System.Net.WebClient
    $client.Encoding = [System.Text.Encoding]::UTF8
    # 話者一覧を取得して整形した上で JSON ファイルに保存する
    $json = $client.DownloadString($url) | ConvertFrom-Json | ConvertTo-Json -Depth 10
    [System.IO.File]::WriteAllText($outputFile, $json, [System.Text.Encoding]::UTF8)
    Write-Host "保存完了: $outputFile"
} catch {
    Write-Host "エラー: VoiceVox が起動していない可能性があります。"
    Write-Host $_.Exception.Message
}
