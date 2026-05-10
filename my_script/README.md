# my_script

スタックちゃんのファームウェア開発用スクリプト集です。

---

## generate_ack_sounds.ps1 — 相槌音声ファイル生成

VoiceVox を使って、感情別の相槌 MP3 ファイルを一括生成します。

### 前提条件

- VoiceVox エンジンがローカルで起動していること（デフォルト: `http://localhost:50021`）
- `ffmpeg` にパスが通っていること（WAV → MP3 変換に使用）

### 使い方

```powershell
pwsh ./generate_ack_sounds.ps1
```

生成対象の話者 ID とセリフは `json/ack_phrases.json` で管理しています。

### 出力フォルダ構造

```
sounds/
  {speakerId}_{キャラクター名}_{スタイル名}/
    happy_un.mp3
    happy_hai.mp3
    neutral_fumu.mp3
    ...
```

例: `sounds/3_ずんだもん_ノーマル/`

### SD カードへのコピー手順

ファームウェアは SD カード上の以下のパスを参照します。

```
/ack/{speakerId}/{ファイル名}.mp3
```

そのため、`sounds/` 以下のフォルダをコピーする際は **フォルダ名を speakerId の数字だけにリネーム**してください。

```
（コピー元）sounds/3_ずんだもん_ノーマル/  →  （SD カード）/ack/3/
（コピー元）sounds/47_ナースロボ＿タイプＴ_ノーマル/  →  （SD カード）/ack/47/
```

> 注意: SD カード上のパスに日本語を含めると ESP32 の FAT32 ドライバで
> 文字化けやファイルオープン失敗が起きる場合があります。パスは ASCII のみにしてください。

### SC_ExConfig.yaml の設定

使用する話者 ID を `ack.speakerId` に指定します。
`-1` を指定した場合は `tts.voice` の値を自動使用します（WebVoiceVox 設定時のみ）。

```yaml
ack:
  speakerId: 3   # VoiceVox の話者 ID（例: 3 = ずんだもん ノーマル）
```

---

## get_voicevox_speakers.ps1 — 話者一覧取得

VoiceVox エンジンから話者一覧を取得して `json/voicevox_speakers.json` に保存します。
`generate_ack_sounds.ps1` の実行前に一度だけ実行してください。

```powershell
pwsh ./get_voicevox_speakers.ps1
```
