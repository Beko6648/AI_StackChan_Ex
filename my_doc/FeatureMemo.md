# AI スタックちゃん Ex 機能メモ

---

## 実装済み機能

---

### 感情・気分システム

`MoodManager`（`firmware/src/mood/`）が4パラメータで内面状態を管理する。

| パラメータ | 範囲 | 更新ルール |
|---|---|---|
| joy（楽しい⇔悲しい） | -1.0〜1.0 | 会話の [META] タグで変動・約10分で中立に減衰・スワイプで +0.1 |
| trust（信頼⇔怒り） | -1.0〜1.0 | 会話の [META] タグで変動・時間減衰なし・スワイプで +0.05 |
| sleepiness（眠気） | 0.0〜1.0 | 放置で増加（0→0.8 約10分）・会話で -0.3 |
| wantToTalk（話したい度） | 0.0〜1.0 | 放置で増加（0→1.0 約5分）・発話/会話でゼロリセット |

**更新周期**: 30秒（deltaSeconds 補正のため増加量は周期に依存しない）

**表情マッピング**（最も強いパラメータが表情を決定）

| 条件 | 表情 | エフェクト |
|---|---|---|
| sleepiness >= 1.0 | Sleeping | Zzz（右上→左下・大中小・呼吸同期） |
| sleepiness が最大かつ >= 0.8 | Sleepy | 泡マーク |
| joy が最大かつ >= 0.6 | Happy | ハート |
| joy が最大かつ <= -0.6 | Sad | 寒さマーク |
| trust が最大かつ >= 0.6 | Happy | ハート |
| trust が最大かつ <= -0.6 | Angry | 怒りマーク |
| それ以外 | Neutral | なし |

**LLM との連動**: LLM レスポンス末尾の `[META]{"emotion":"happy","joy":0.3,"trust":0.1}[/META]` を解析して表情・気分を更新。

---

### 自発発話（selfTalk）

`wantToTalk >= 1.0` で `selfTalk()` を起動し、LLM に話しかけさせる。

- 会話履歴がある場合は過去の話題を踏まえる
- キャラクターファイルの `self_talk_prompt` でプロンプトを定義（気分はトーンの参考として末尾付加）
- 自発発話後は `wantToTalk` をゼロリセット

---

### 就寝・起床システム

| タイミング | 動作 |
|---|---|
| sleepiness >= 0.8（初回） | あくび音声を再生（`yawn_fuwa.mp3`） |
| sleepiness >= 1.0（初回） | 就寝音声をランダム再生・Sleeping 表情・頭を正面固定・きょろきょろ停止 |
| 起床（HeadTouch いずれか） | 起床音声をランダム再生・sleepiness/wantToTalk リセット・負の joy/trust を半分回復 |

音声ファイルは SD カード `/ack/{speakerId}/` に配置（相槌と共通の仕組み）。

---

### 相槌システム（AckSound）

STT 完了直後に感情対応の相槌 MP3 を再生。

- 表情（StackChanMind）に応じた重み付き抽選で選択
- 音声は VoiceVox で生成（`my_script/generate_ack_sounds.ps1`）

| 表情 | 相槌例 |
|---|---|
| Happy | はい |
| Neutral | はい |
| Sad | ううん |
| Doubt | ううん / えっとお |
| Sleepy | ううん |
| Angry | えーー |

---

### 頭部タッチセンサー（HeadTouch）

Si12T（I2C 0x68）3ゾーン静電容量式センサー。

| ジェスチャー | 動作 |
|---|---|
| ロングプレス（800ms） | 音声認識開始 |
| スワイプ | joy +0.1 / trust +0.05・表情を2秒変化（Sad/Angry→Neutral、それ以外→Happy） |

---

### アイドル頭部動作（IdleLookAround）

- 水平 ±70度・垂直 ±30度でランダムにきょろきょろ（1〜7秒キープ）
- 会話中は停止し、会話終了後に再開

---

### キャラクターシステム

SD カードの `/characters/{name}.yaml` からキャラクターを定義・切り替え。`SC_ExConfig.yaml` の `character.name` で指定し起動時に自動適用。

**キャラクターファイルの設定項目**

```yaml
name: "名前"
voice: "47"             # TTS 話者ID（空なら SC_ExConfig の tts.voice を使用）
memory: true            # 長期記憶の有効・無効
system_prompt: |        # 人格・口調の基本定義
talk_prompt: |          # 毎回の会話時に system role へ追加する指示
self_talk_prompt: |     # 自発発話時のユーザーメッセージ
```

**収録キャラクター**

| ファイル | voice | memory | 特徴 |
|---|---|---|---|
| `butler.yaml` | 47（ナースロボ） | true | 執事口調・マスターへの献身 |
| `friendly.yaml` | 89（Voidoll） | true | フレンドリー・元気なトーン |

---

### 長期記憶

`memory: true` のキャラクターで有効。LLM が `update_memory` ツールでユーザー属性を SPIFFS に保存し、再起動後も `User Info:` として引き継がれる。LLM が要約・更新するため容量上限に達しにくい。

---

### ブラウザ設定 UI

スタックちゃんの IP アドレスにブラウザからアクセスして設定変更できる。

| 画面 | 機能 |
|---|---|
| `/personalize.html` | システムプロンプト編集・長期記憶確認/消去・キャラクター YAML 編集・アクティブキャラクター切り替え |
| `/settings.html` | サーボオフセット（X・Y）変更・保存 |

設定変更後は再起動で反映。

---

### エルメス Webhook 連携（新方式）

**古い方式（廃止予定）:** webhook_channel.ts (Bun MCP) + クローディア (Claude Code) — 上記「クローディア連携」参照。

**新しい方式:** `my_script/hermes_webhook_server.py` + エルメス (Hermes Agent) に置き換え済み。

フロー:
```
StackChan（音声入力）→ STT → HTTP POST :8788
→ hermes_webhook_server.py → hermes chat -q → 返答
→ POST /command_result → VOICEVOX → 再生
```

起動（WSL）。
```bash
bash my_script/hermes_webhook_start.sh start    # 起動
bash my_script/hermes_webhook_start.sh stop     # 停止
bash my_script/hermes_webhook_start.sh status   # 状態確認
```

環境変数:
- `STACKCHAN_IP` — StackChan の IP (デフォルト: 192.168.1.114)
- `WEBHOOK_PORT` — 待受ポート (デフォルト: 8788)
- `HERMES_MODEL` — 使用モデル（空=デフォルト）
- `RESPONSE_TIMEOUT` — タイムアウト秒 (デフォルト: 60)

`my_script/webhook_channel.ts`（Bun/TypeScript 製 MCP Channel サーバー）を介して、クローディア本体セッションと双方向通信する。

**フロー:**
```
StackChan（音声入力）→ STT → HTTP POST http://PC-IP:8788/
→ webhook_channel.ts → MCP 通知 → クローディア本体セッション
→ reply ツール → HTTP POST /command_result → VOICEVOX → 再生
```

**AI モード切り替え:** Settings UI または `/mode` エンドポイントで `chatgpt` / `claude_code` を切り替え。NVS 保存で再起動後も維持。

**Webhook URL 変更:** `/webhook_url` エンドポイントで PC の IP が変わっても再書き込み不要。

---

### 手動スリープ操作

`/sleep` エンドポイントまたは Settings UI の Sleep/Wakeup ボタンで手動で眠らせる・起こす操作が可能。

---

### VAD 実装の技術的背景と不具合経緯

> **注意:** 以下は現在 `feature/vad-cache-invalidate` ブランチで開発中の機能。`main` には未マージ。現在の `main` は固定3.75秒録音（VADなし）。

**問題1: PSRAM キャッシュ不整合**
`M5.Mic.record()` は DMA で PSRAM に書き込むが、CPU キャッシュが更新されないため振幅の読み取りが常に 0 になった。Whisper API は PSRAM を直接読むため音声認識は正常動作していた。

**解決策:** DMA 対応内部 RAM（300バイト）をダブルバッファとして確保し、録音 → 振幅計算 → PSRAM へコピー の順で処理。

**問題2: mic_task スタックオーバーフロー**
DMA RAM への録音で mic_task のスタックが不足してクラッシュした。

M5Unified の Mic_Class.cpp を調査した結果、mic_task 内で `alloca(dma_buf_len × 2)` によりスタック上に DMA バッファを確保している。スタックサイズの計算式は `2048 + (dma_buf_len × 2)` だが、`i2s_read` の内部処理やシグナル処理でまれにこれを超えることが判明。DMA RAM か PSRAM かは mic_task のスタック使用量に影響しない（ユーザーバッファへの書き込みは別処理）。

**解決策（暫定）:** `dma_buf_len=1024` に設定して mic_task のスタックを 4096バイトに増やした。副作用として1チャンクあたりの録音時間が 9.375ms → 16ms に変化したため `SILENCE_CHUNKS=59`・`record_number=600` に再調整した。4〜5回の会話で依然クラッシュが発生する。

**根本解決の選択肢（未実施）:**
- `dma_buf_len` をさらに増やす（2048以上）→ チャンク時間が 128ms以上になり VAD の精度が著しく低下
- M5Unified の `.pio/libdeps/` 内でスタック基底値を `2048` → `6144` 以上に変更 → `dma_buf_len=128`（デフォルト）のまま十分なスタックを確保できるが、PlatformIO のライブラリ更新で変更が消えるリスクがある

**問題3: `M5.Mic.begin()` の二重呼び出し**
TTS 再生後に `PlayMP3.cpp` が `M5.Mic.begin()` を呼ぶが、録音開始時にも `AudioWhisper::Record()` が `M5.Mic.begin()` を呼ぶため二重になり、2回目の会話でクラッシュした。

**解決策:** `AudioWhisper::Record()` の冒頭に `M5.Mic.end()` を追加して前の状態をリセット。また `PlayMP3.cpp` から不要な `M5.Mic.end()` / `M5.Mic.begin()` を削除し、マイク管理を `AudioWhisper::Record()` に一元化した。

**残課題（未解決）:**
- `I2S: register I2S object to platform failed` エラーが録音・TTS 開始時に毎回発生。M5Unified 0.1.17 の CoreS3 固有の既知問題で外部からは修正不可。このエラー自体でクラッシュはしないが、mic_task スタックオーバーフローと組み合わさると lwIP TCP スタックを破壊してクラッシュに至る。
- キャッシュ無効化（`Cache_Invalidate_Addr` / `esp_cache_msync`）で PSRAM 直接録音に戻す根本解決は、使用 ESP-IDF バージョン（5.1）では適切な API が存在しないため未実施。ESP-IDF 5.2+ では `esp_cache_msync` が利用可能。

---

## 残タスク

### 調査・修正待ち

- [ ] **就寝中の首振り動作**: `_headCtrl.stop()` 後も縦軸の動作だけ残っている可能性がある
- [ ] **不明エラー**: 音声認識時に時折発生する SD カードエラー（`sd_diskio.cpp:199`）
- [ ] **MP3:ERROR_BUFLEN 0**: TTS 再生のたびに発生。VoiceVox ストリーミング URL からの MP3 読み込み時にネットワーク遅延でバッファが空になる。AudioGeneratorMP3 の3回リトライで再生は続くが毎回出力される

### 未着手（細かい改善・修正）

- [ ] **LLM完了前にTTS生成を開始する（非同期TTS）**: `invokeAsyncTtsStreamTask()` を標準ビルドでも有効化する。OpenAI API のストリーミングレスポンス対応が必要（現在は非ストリーミング）。

- [ ] **VAD 実装**: 現状は固定3.75秒録音。`feature/vad-cache-invalidate` で post-processing VAD を開発中（末尾無音トリム方式）。真のリアルタイムVADは i2s_read() 直接使用が必要。
- [ ] **画面タッチ操作**: M5Stack 版は画面下部の物理ボタンがないため BtnA/BtnC を画面タッチで代替する
- [ ] **電源オフ時のサーボ正面復帰**: 電源が切れるタイミングでサーボが正面に戻っていない
- [ ] **相槌と MoodManager の連動**: 現在は StackChanMind（表情）ベースの選択。joy パラメータと連動させる
- [ ] **表情変化のランダム演出**: アイドル時に気分に応じた表情をランダムにふらっと切り替える

### 未着手（大きな追加機能）

- [ ] **音源定位・顔追従**: ウェイクワード後にマイクで方向推定、カメラで顔追従
- [ ] **夢の発話**: 就寝時に会話ログを LLM に渡して夢を生成し、起床時に発話する
- [ ] **頭部 LED による感情可視化**: joy（左 LED）・trust（右 LED）をリアルタイム表示
- [ ] **ベクトル検索による長期記憶（RAG）**: 現在の SPIFFS 要約テキストと共存させる形で、会話エピソードをベクトル化して SD カードに蓄積し意味的に近い記憶を検索・注入する。ESP32 上での先行事例はほぼなくフロンティア的実装。
  - モデル: `text-embedding-3-small`（$0.02/1Mトークン、年間約5円）
  - 次元数: 512（精度とサイズのバランス）
  - ストレージ: SD カード（1ベクトル 2KB × 100会話 = 200KB）
  - 計算: ESP32 の PSRAM 上でコサイン類似度線形スキャン
  - レイテンシ影響: クエリ Embedding に +0.5〜1秒（STT と並列化で軽減可能）
  - 取得件数: 上位3件をプロンプトに注入
- [ ] **Vision API による画像認識**: CoreS3 のカメラで撮影した画像を gpt-4o-mini の Vision 機能で解析し、会話に活用する（顔認識・表情分析・周囲の状況把握など）。コストは通常会話と同価格（$0.15/1Mトークン）。
