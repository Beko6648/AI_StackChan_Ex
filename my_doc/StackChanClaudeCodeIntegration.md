# StackChan ←→ Claude Code 連携設計書

## 概要
StackChan を UI として Claude Code を操作する機能。ユーザーが StackChan に音声で指示を出すと、Claude Code が実行して結果を音声で返す。

**ステータス：** 設計確定（2026-05-29）  
**セキュリティ：** ローカルネットワーク内のみ  

---

## アーキテクチャ

```
boot.ps1
  ├── メインセッション（Beko ↔ Claude Code）  ← 通常の対話
  └── ポーリングスクリプト（常駐・別プロセス）
        ↓ StackChan からコマンド検出
        ↓
        claude -p "コマンド" --resume stackchan-session
        （StackChan 専用セッションで処理）
        ↓
        結果を StackChan に POST
```

**ポイント：**
- メインセッションと StackChan 専用セッションは完全に独立
- StackChan 専用セッションは `--session-id` / `--resume` でコンテキストを維持
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
6. StackChan 専用の Claude Code セッションで処理
   claude -p "{コマンド}" --resume stackchan-session
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

### 処理フロー
```powershell
# polling.ps1（常駐スクリプト）
while ($true) {
    # 1. StackChan に確認
    $res = Invoke-RestMethod -Uri "http://192.168.1.114/pending_command" -Method GET

    if ($res.command_id -ne $null) {
        # 2. StackChan 専用セッションで処理
        $result = claude -p $res.text --resume "stackchan-session"

        # 3. 結果を返送
        $body = @{
            command_id  = $res.command_id
            voice_text  = $result  # Claude が voice_text / detail_text を含む JSON を出力
            detail_text = ""
        } | ConvertTo-Json
        Invoke-RestMethod -Uri "http://192.168.1.114/command_result" -Method POST -Body $body -ContentType "application/json"
    }

    Start-Sleep -Milliseconds 500
}
```

### StackChan 専用セッションの管理
- `--session-id` で固定 UUID を指定（初回）
- 以降は `--resume` で同じセッションを継続
- コンテキストが維持されるため、会話の連続性がある

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
- [ ] ビルド・動作確認（実機 CoreS3）

### Windows 側
- [ ] ポーリングスクリプト（polling.ps1）実装
- [ ] boot.ps1 にポーリングスクリプト起動を追加

---

## 検討経緯メモ

| 案 | 結果 |
|---|---|
| Claude Code がサーバー | IP 公開のセキュリティリスク → 却下 |
| ポーリング（メインセッション内） | Claude Code の処理を遮る → 却下 |
| Named Pipe で stdin 注入 | TTY 問題・実装複雑・不安定 → 却下 |
| MCP サーバー | Claude Code 側がポーリング必要 → 解決にならない → 却下 |
| **ポーリングスクリプト + 専用セッション** | **採用** |
