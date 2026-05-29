# StackChan ←→ Claude Code 連携設計書

## 概要
StackChan を UI として Claude Code を操作する機能。ユーザーが StackChan に音声で指示を出すと、Claude Code が実行して結果を音声で返す。

**ステータス：** 実装完了・動作確認済み（2026-05-29）  
**セキュリティ：** ローカルネットワーク内のみ  

---

## アーキテクチャ

```
claude_boot.ps1（pwsh で手動起動）
  ├── メインセッション（Beko ↔ Claude Code）  ← 通常の対話
  └── polling.ps1（常駐・別プロセス・最小化ウィンドウ）
        ↓ 500ms ごとに StackChan をポーリング
        ↓ コマンド検出
        ↓ claude -p でレスポンス生成
        ↓
        結果を StackChan に POST
```

**ポイント：**
- polling.ps1 は claude_boot.ps1 起動時に自動で別プロセス起動
- `claude -p` はステートレス（セッション引き継ぎなし）
- メインセッションへの干渉なし

---

## 基本フロー

```
1. ユーザー → StackChan に音声で指示
   ↓
2. StackChan が音声認識 → テキスト化
   ↓
3. StackChan がコマンドをキューに保持（/pending_command で提供）
   ↓
4. ポーリングスクリプト（Windows 側・常駐）が定期的に確認
   GET http://192.168.1.114/pending_command
   ↓
5. 新しいコマンドを検出
   ↓
6. polling.ps1 が claude -p でレスポンスを生成
   ↓
7. 処理結果を StackChan に返送
   POST http://192.168.1.114/command_result
   { "voice_text": "読み上げる要点", "detail_text": "詳細情報" }
   ↓
8. StackChan が VOICEVOX で voice_text を音声化・再生
```

---

## エンドポイント仕様

### StackChan が提供するエンドポイント

#### `/pending_command` （ポーリングスクリプトが確認）
- **方式：** GET
- **レスポンス（コマンドあり）：**
  ```json
  {
    "command_id": "unique_id",
    "text": "ユーザーが話した内容"
  }
  ```
- **レスポンス（コマンドなし）：**
  ```json
  {
    "command_id": null
  }
  ```

#### `/command_result` （ポーリングスクリプトが結果を送信）
- **方式：** POST
- **リクエスト：**
  ```json
  {
    "command_id": "unique_id",
    "voice_text": "読み上げる要点（短い。VOICEVOX で音声化）",
    "detail_text": "詳細情報（長くても OK。画面表示など）"
  }
  ```
- **レスポンス：**
  ```json
  {
    "status": "OK"
  }
  ```

---

## Windows 側の実装（ポーリングスクリプト）

### 実装ファイル
- `C:\ClaudeCode_work\claude_boot.ps1` — Claude Code 常駐起動 + polling.ps1 自動起動
- `C:\ClaudeCode_work\polling.ps1` — StackChan ←→ Claude Code 橋渡し

### 実行環境
- **pwsh（PowerShell 7）で実行すること**
- Windows PowerShell（5.x）だと UTF-8 エンコーディング問題で日本語が文字化けする
- claude_boot.ps1 を pwsh で手動起動すれば polling.ps1 も自動で起動する

### 処理フロー（実装済み）
```powershell
# polling.ps1（抜粋）
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

while ($true) {
    $res = Invoke-RestMethod -Uri "http://192.168.1.114/pending_command" -Method GET -TimeoutSec 2

    if ($null -ne $res.command_id) {
        $voice = (claude -p $prompt 2>$null)

        $body = (@{ command_id = $res.command_id; voice_text = $voice } | ConvertTo-Json -Compress)
        Invoke-RestMethod -Uri "http://192.168.1.114/command_result" -Method POST -Body $body -ContentType "application/json"
    }

    Start-Sleep -Milliseconds 500
}
```

### 注意事項
- polling.ps1 の編集は **VS Code でコピペ**すること（Claude Code のツール直書きだとスマートクォートが混入してエラーになる）
- claude_boot.ps1 を複数回起動すると polling.ps1 が多重起動する。タスクマネージャーで pwsh プロセス数を確認すること

---

## ファームウェア側の実装範囲

### 設計決定事項
- **音声入力の振り分け**：全ての音声入力を Claude Code に転送（既存 LLM は使用しない）
- **voice_text**：`claude -p` の出力をそのまま渡す（全て読み上げ前提のプロンプト設計）
- **TTS**：既存の `WebVoiceVoxTTS` を使用
- **キュー**：サイズ1。処理中（pending_command あり）は新しい音声入力をビジーとして無視

### 1. `/pending_command` エンドポイント追加（WebAPI.cpp）
- StackChan のコマンドキューから次のコマンドを返す
- なければ `command_id: null` を返す

### 2. `/command_result` エンドポイント追加（WebAPI.cpp）
- ポーリングスクリプトからの結果を受け取る
- `voice_text` を既存 WebVoiceVoxTTS で読み上げ
- 受信後、キューをクリアして次の音声入力を受け付ける

### 3. コマンドキュー管理（AiStackChanMod）
- ユーザーの音声入力をテキスト化した後、キューに追加
- 処理中（キューにコマンドあり）は新しい音声入力を無視
- `/pending_command` で提供
- `/command_result` 受信後にキューをクリア

### 4. AI モード切り替え（WebAPI.cpp + Settings UI）
- `GET /mode` — 現在のモードを返す（`chatgpt` or `claude_code`）
- `POST /mode` — モードを切り替える（`{"mode": "chatgpt"}` or `{"mode": "claude_code"}`）
- モードは NVS に保存され、再起動後も維持される（デフォルト：`chatgpt`）
- `/settings.html` の AI Mode セクションから GUI で切り替え可能

---

## セキュリティ考慮

- **ローカルネットワーク内限定**
- StackChan の IP は固定（192.168.1.114）
- 認証は不要（ローカル限定のため）
- 将来的には簡単なトークン認証を追加できる

---

## 対象デバイス
- **CoreS3** のみ（当初）
- 他デバイス対応は後日

---

## 将来の拡張

### ウェイクワード検出（タッチ不要の音声起動）
- タッチ操作なしで話しかけたことを検知し、自動的に音声認識を開始する
- 方式：ESP-SR ライブラリによるウェイクワード検出
- VAD（音量しきい値）方式は実装が簡単だが環境音誤作動のリスクあり → ウェイクワード方式を採用
- **実装タイミング：** ポーリング連携（基本機能）完了後に追加

---

## 実装タスク

### ファームウェア側（ブランチ：feature/stackchan-claude-api）
- [x] `/pending_command` エンドポイント実装（WebAPI.cpp）
- [x] `/command_result` エンドポイント実装（WebAPI.cpp）
- [x] コマンドキュー管理機能実装（AiStackChanMod）
- [x] AI モード切り替え（GET/POST /mode、NVS保存、Settings UI）
- [x] ビルド・動作確認（実機 CoreS3）

### Windows 側
- [x] ポーリングスクリプト（polling.ps1）実装
- [x] claude_boot.ps1 にポーリングスクリプト起動を追加（pwsh で自動起動）
- [x] 実機動作確認（StackChan が日本語で応答）

---

## 検討経緯メモ

| 案 | 結果 |
|---|---|
| Claude Code がサーバー | IP 公開のセキュリティリスク → 却下 |
| ポーリング（メインセッション内） | Claude Code の処理を遮る → 却下 |
| Named Pipe で stdin 注入 | TTY 問題・実装複雑・不安定 → 却下 |
| MCP サーバー | Claude Code 側がポーリング必要 → 解決にならない → 却下 |
| **ポーリングスクリプト + 専用セッション** | **採用** |
