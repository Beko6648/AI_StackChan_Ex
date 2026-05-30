# StackChan ←→ Claude Code 連携設計書

## 概要
StackChan を UI として Claude Code を操作する機能。ユーザーが StackChan に音声で指示を出すと、Claude Code が実行して結果を音声で返す。

**ステータス：** 実装完了・動作確認済み（2026-05-30）  
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
- `claude -p --resume` でセッションを引き継ぐ（会話の文脈が維持される）
- メインセッションへの干渉なし
- ユーザー発話・自発発話どちらも `dispatchChat()` 経由で一元管理

---

## 基本フロー

### ユーザー発話（type: "user"）
```
1. ユーザー → StackChan に音声で指示
   ↓
2. StackChan が音声認識 → テキスト化
   ↓
3. dispatchChat(text, "user") → コマンドキューに積む
   ↓
4. polling.ps1 が 500ms ごとに確認
   GET http://192.168.1.114/pending_command
   → { command_id, text, type:"user", system_prompt }
   ↓
5. system_prompt をシステムプロンプトとして、text をユーザー質問として Claude に投げる
   claude -p $text --system-prompt $systemPrompt --resume $sessionId
   ↓
6. 結果を StackChan に返送
   POST http://192.168.1.114/command_result
   { "command_id": "...", "voice_text": "..." }
   ↓
7. StackChan が VOICEVOX で voice_text を音声化・再生
```

### 自発発話（type: "self_talk"）
```
1. MoodManager が閾値を超える → selfTalk() が起動
   ↓
2. self_talk_prompt（キャラ YAML）＋気分説明をプロンプトに組み立て
   ↓
3. dispatchChat(prompt, "self_talk") → コマンドキューに積む
   ↓
4. polling.ps1 が検出
   → { command_id, text: 完全プロンプト, type:"self_talk", system_prompt }
   ↓
5. text をそのまま Claude に投げる（system_prompt 不要）
   claude -p $text --resume $sessionId
   ↓
6. 結果を StackChan に返送 → VOICEVOX で読み上げ
```

---

## エンドポイント仕様

### StackChan が提供するエンドポイント

#### `/pending_command` （ポーリングスクリプトが確認）
- **方式：** GET
- **レスポンス（コマンドあり）：**
  ```json
  {
    "command_id": "12",
    "text": "ユーザーが話した内容（type=user）/ 完全プロンプト（type=self_talk）",
    "type": "user | self_talk",
    "system_prompt": "キャラクター YAML の system_prompt（ChatGPT モードと同一ソース）",
    "error_text": "Claude が応答できなかった場合のフォールバック文言"
  }
  ```
- **レスポンス（コマンドなし）：**
  ```json
  {
    "command_id": null
  }
  ```
- **type の使い分け：**
  - `user` — polling.ps1 が `system_prompt` をシステムプロンプトとして設定し、`text` をユーザー質問として渡す
  - `self_talk` — `text` に完全プロンプトが入っているので、そのまま Claude に渡す

#### `/command_result` （ポーリングスクリプトが結果を送信）
- **方式：** POST
- **リクエスト：**
  ```json
  {
    "command_id": "12",
    "voice_text": "読み上げるテキスト（VOICEVOX で音声化）"
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
- polling.ps1 の編集は Claude Code の **Edit ツール**で直接可能（Write/Bash はスマートクォートを混入させるため使用不可）
- claude_boot.ps1 を複数回起動すると polling.ps1 が多重起動する。タスクマネージャーで pwsh プロセス数を確認すること
- セッションをリセットしたい場合は `stackchan_session.txt` を削除する

---

## ファームウェア側の実装範囲

### 設計決定事項
- **発話の一元管理**：ユーザー発話・自発発話ともに `dispatchChat()` を経由してモードに応じた送信先に振り分ける
- **システムプロンプトの一元管理**：`/pending_command` にキャラクター YAML の `system_prompt` を含めて渡す。ChatGPT モードと同一ソースを使うため設定が分散しない
- **自発発話**：Claude Code モードでも `selfTalk()` が動作する。`type: "self_talk"` で通知し、polling.ps1 側でプロンプトをそのまま渡す
- **TTS**：既存の `WebVoiceVoxTTS` を使用
- **キュー**：サイズ1。処理中（pending_command あり）は新しい音声入力・自発発話をビジーとして無視
- **ビジータイムアウト**：30秒以内に `/command_result` が来なければ自動でビジー解除。`claude_timeout_text` を読み上げる
- **エラー・タイムアウト文言**：キャラクター YAML の `claude_error_text` / `claude_timeout_text` で設定可能。未設定時はデフォルト文言を使用

### 1. `dispatchChat()`（AiStackChanMod.cpp）
- ユーザー発話・自発発話の送信先を一元管理する static 関数
- ChatGPT モード → `robot->chat()` を直接呼ぶ
- Claude Code モード → コマンドキューに積んで polling.ps1 経由で処理

### 2. `/pending_command` エンドポイント（WebAPI.cpp）
- コマンドキューから次のコマンドを JSON で返す
- `command_id` / `text` / `type` / `system_prompt` / `error_text` を含む
- コマンドがなければ `{"command_id": null}` を返す

### 3. `/command_result` エンドポイント（WebAPI.cpp）
- polling.ps1 からの返答テキストを受け取る
- `voice_text` を VOICEVOX で読み上げ
- 受信後にビジーフラグを解除し、次のコマンドを受け付け可能にする

### 4. AI モード切り替え（WebAPI.cpp + Settings UI）
- `GET /mode` — 現在のモードを返す（`chatgpt` or `claude_code`）
- `POST /mode` — モードを切り替える（NVS 保存、再起動後も維持、デフォルト：`chatgpt`）
- `/settings.html` の AI Mode セクションから GUI で切り替え可能

---

## キャラクター YAML の Claude Code 用フィールド

既存の `system_prompt` / `talk_prompt` / `self_talk_prompt` に加え、以下を設定可能。未設定時はデフォルト文言を使用。

```yaml
claude_error_text: "ごめん、うまく考えられなかった。もう一回聞いてみて"
claude_timeout_text: "頭がぼーっとしちゃった"
```

| フィールド | 用途 | デフォルト |
|---|---|---|
| `claude_error_text` | Claude が空の応答を返したとき（polling.ps1 側） | ごめん、うまく考えられなかった。もう一回聞いてみて |
| `claude_timeout_text` | 30秒以内に返答が来なかったとき（firmware 側） | 頭がぼーっとしちゃった |

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
- [x] `dispatchChat()` によるユーザー発話・自発発話の一元管理
- [x] Claude Code モードでの自発発話対応（type: "self_talk"）
- [x] `/pending_command` に `type` / `system_prompt` / `error_text` フィールド追加
- [x] 30秒ビジータイムアウト（自動解除 + `claude_timeout_text` 読み上げ）
- [x] エラー・タイムアウト文言をキャラクター YAML で設定可能
- [x] ビルド・動作確認（実機 CoreS3）

### Windows 側
- [x] ポーリングスクリプト（polling.ps1）実装
- [x] claude_boot.ps1 にポーリングスクリプト起動を追加（pwsh で自動起動）
- [x] セッション引き継ぎ（`--resume`、セッション ID を stackchan_session.txt に保存）
- [x] `type` フィールドに応じた処理分岐（user / self_talk）
- [x] `system_prompt` フィールドを `--system-prompt` で Claude に渡す
- [x] `error_text` フィールドをフォールバック文言に使用
- [x] 実機動作確認（StackChan が日本語で応答・自発発話・タイムアウト復帰）

---

## 検討経緯メモ

| 案 | 結果 |
|---|---|
| Claude Code がサーバー | IP 公開のセキュリティリスク → 却下 |
| ポーリング（メインセッション内） | Claude Code の処理を遮る → 却下 |
| Named Pipe で stdin 注入 | TTY 問題・実装複雑・不安定 → 却下 |
| MCP サーバー | Claude Code 側がポーリング必要 → 解決にならない → 却下 |
| **ポーリングスクリプト + 専用セッション** | **採用** |
