# AI_StackChan_Ex ファームウェア 設計ドキュメント

リポジトリ: https://github.com/ronron-gh/AI_StackChan_Ex.git  
調査日: 2026-05-09

---

## 全体アーキテクチャ

```
main.cpp (エントリーポイント)
 ├─ 設定読み込み (YAML → StackchanExConfig)
 ├─ WiFi 接続 / オフライン判定
 ├─ Robot (中央ハブ) 生成
 │   ├─ LLMBase* llm
 │   ├─ TTSBase* tts
 │   ├─ STTBase* stt
 │   └─ ServoCustom* servo
 ├─ Avatar タスク群を起動 (FreeRTOS)
 └─ ModManager (UI モード管理)
```

中核は **Robot クラス**。LLM・TTS・STT の各インスタンスをポインタで保持し、
設定 YAML の `type` 番号に応じた実装を生成するファクトリ的な役割を持つ。

---

## サービス抽象化 (ストラテジーパターン)

YAML の `type` 番号を変えるだけで実装が切り替わる。

### LLMBase — `firmware/src/llm/LLMBase.h`

| type | クラス | 通信 |
|------|--------|------|
| 0 | ChatGPT | HTTPS REST + MCP |
| 1 | ChatModuleLLM | シリアル UART |
| 2 | ChatModuleLLMFncl | シリアル UART + 関数呼び出し |
| 3 | GeminiLive | WebSocket |

Realtime API ビルド (`REALTIME_API` フラグ) 時は `RealtimeChatGPT` または `GeminiLive` が使われる。

### TTSBase — `firmware/src/tts/TTSBase.h`

| type | クラス | 通信 |
|------|--------|------|
| 0 | WebVoiceVoxTTS | HTTP (ローカルサーバー) |
| 1 | ElevenLabsTTS | HTTPS |
| 2 | OpenAITTS | HTTPS |
| 3 | UAquesTalkTTS | オフライン (Cライブラリ) |
| 4 | ModuleLLMTTS | シリアル UART |

### STTBase — `firmware/src/stt/STTBase.h`

| type | クラス | 通信 |
|------|--------|------|
| 0 | CloudSpeechClient | HTTPS (Google Cloud) |
| 1 | Whisper | HTTPS (OpenAI) |
| 2 | ModuleLLMASR | シリアル UART |
| 3 | ModuleLLMWhisper | シリアル UART |

---

## UI モード管理 — ModManager

`ModBase` を継承した画面モードを deque で管理。画面の左右フリックで切替。

```
ModBase (抽象) — firmware/src/mod/ModBase.h
 ├─ AiStackChanMod   通常会話 (タッチ → STT → LLM → TTS)
 ├─ RealtimeAiMod    WebSocket ストリーミング会話
 ├─ StatusMonitorMod 状態表示
 ├─ VolumeSettingMod 音量設定
 ├─ PomodoroMod
 └─ PhotoFrameMod
```

各モードは `update()` / `btnA_pressed()` / `display_touched()` 等のコールバックをオーバーライドする。

---

## データフロー

### 通常会話モード (AiStackChanMod)

```
マイク録音 (16bit PCM 16kHz)
  ↓
STT モジュール → テキスト
  ↓
LLM モジュール → 応答テキスト (Function Calling / MCP 対応)
  ↓
TTS モジュール → 音声データ
  ↓
スピーカー再生
  ↓ (並行)
Avatar 口パク (lipSync タスク)
サーボ視線制御 (servo タスク)
```

### リアルタイム API モード (RealtimeAiMod)

```
マイク (PCM 16kHz) ──WebSocket──▶ ChatGPT Realtime API / GeminiLive
                                        ↓
                          音声ストリーム (base64 デコード)
                                        ↓
                          スピーカー再生 + 口パク + サーボ
```

STT・TTS のラウンドトリップがなく低レイテンシ。

---

## マルチタスク構成 (FreeRTOS)

| タスク | 起動元 | 役割 |
|--------|--------|------|
| `loop()` | main | ボタン/タッチ/HTTP/FTP 処理 |
| `lipSync` | avatar.addTask | Speaker 音量レベル → Avatar 口パク |
| `servo` | avatar.addTask | Avatar 視線座標 → サーボ角度 (5秒周期) |
| `battery_check` | avatar.addTask | バッテリー表示更新 (60秒周期) |
| `asyncTtsStreamTask` | setup | 非同期 TTS 再生キュー処理 |

オーディオリソース競合は `enterMutexAudio()` / `exitMutexAudio()` で保護。
(`firmware/src/share/Mutex.h`)

---

## 初期化シーケンス (setup())

```
1. ハードウェア初期化
   M5.begin() / Mutex / Mic・Speaker I2S 設定

2. YAML 設定読み込み
   SD → SPIFFS → 旧形式 .txt の順でフォールバック
   StackchanExConfig::loadConfig() で LLM/TTS/STT type を確定

3. WiFi 接続
   失敗時は SmartConfig (30秒タイムアウト) → 全失敗でオフラインモード

4. Robot 生成
   initServo() / initLLM() or initRtLLM() / initSTT() / initTTS()

5. オンラインサービス起動 (WiFi 接続時のみ)
   ESP32WebServer (port 80) / FTP サーバー / NTP 時刻同期

6. Avatar 初期化
   avatar.init() + lipSync / servo / battery_check タスク登録

7. ModManager 初期化
   AiStackChanMod or RealtimeAiMod + StatusMonitorMod + VolumeSettingMod
```

---

## 設定ファイル構成

SD カード配置:

```
/yaml/SC_BasicConfig.yaml   サーボ・Bluetooth・ディスプレイ設定
/yaml/SC_SecConfig.yaml     WiFi・API キー
/app/AiStackChanEx/SC_ExConfig.yaml   LLM/TTS/STT サービス選択
```

SC_ExConfig.yaml のスキーマ:

```yaml
llm:
  type: 0          # 0=ChatGPT, 1=ModuleLLM, 2=ModuleLLMFncl, 3=Gemini
  model: "gpt-4o"
  enableMemory: true
  mcpServers:
    - name: "server1"
      url: "http://localhost"
      port: 3000

tts:
  type: 2          # 0=WebVoiceVox, 1=ElevenLabs, 2=OpenAI, 3=AquesTalk, 4=ModuleLLM
  voice: "1"

stt:
  type: 1          # 0=Google, 1=Whisper, 2=ModuleLLM ASR, 3=ModuleLLM Whisper

wakeword:
  type: 0          # 0=SimpleVox, 1=ModuleLLM KWS
  keyword: "ok google"

audio:
  speaker_volume: 60

moduleLLM:
  rxPin: 2
  txPin: 1
```

---

## ビルド環境 (platformio.ini)

| 環境名 | 用途 | 主なフラグ |
|--------|------|-----------|
| `m5stack-cores3` | CoreS3 基本構成 | — |
| `m5stack-cores3-realtime` | CoreS3 + Realtime API | `REALTIME_API` |
| `m5stack-core2` | Core2 基本構成 | — |
| `m5stack-core2-realtime` | Core2 + Realtime API (デフォルト) | `REALTIME_API` |
| `m5stack-core2-llm` | Core2 + M5 LLM Module | `USE_LLM_MODULE` |
| `m5stack-atoms3r` | AtomS3R (小型) | — |

主要コンパイルフラグ:

| フラグ | 効果 |
|--------|------|
| `REALTIME_API` | WebSocket ストリーミング LLM を有効化 |
| `USE_LLM_MODULE` | M5 LLM Module (シリアル) を有効化 |
| `USE_AQUESTALK` | オフライン TTS ライブラリを有効化 |
| `ENABLE_CAMERA` | カメラ入力 → LLM を有効化 |
| `ENABLE_TAP_DETECT` | IMU ダブルタップ検出を有効化 |
| `ENABLE_SD_UPDATER` | SD カード経由 OTA を有効化 |

---

## 主要ファイルマップ

| ファイル | 役割 |
|----------|------|
| `firmware/src/main.cpp` | 初期化・メインループ |
| `firmware/src/Robot.h/cpp` | LLM/TTS/STT のファクトリ & 中央ハブ |
| `firmware/src/StackchanExConfig.h/cpp` | YAML 設定の読み込み |
| `firmware/src/llm/LLMBase.h/cpp` | LLM 抽象基底クラス・会話履歴・SPIFFS 記憶 |
| `firmware/src/llm/ChatGPT/` | ChatGPT / RealtimeChatGPT / FunctionCall / MCPClient |
| `firmware/src/llm/Gemini/` | GeminiLive |
| `firmware/src/llm/RealtimeLLMBase.h` | WebSocket ストリーミング共通基底 |
| `firmware/src/tts/TTSBase.h` | TTS 抽象基底クラス |
| `firmware/src/stt/STTBase.h` | STT 抽象基底クラス |
| `firmware/src/mod/ModBase.h` | UI モード抽象基底クラス |
| `firmware/src/mod/ModManager.h/cpp` | UI モード切替・フェードアニメーション |
| `firmware/src/mod/AiStackChan/AiStackChanMod.h` | 通常会話モード |
| `firmware/src/mod/AiStackChan/RealtimeAiMod.h` | リアルタイム会話モード |
| `firmware/src/WebAPI.h/cpp` | HTTP Web UI サーバー (設定・会話 REST API) |
| `firmware/src/ServoCustom.h/cpp` | サーボ制御 (StackchanSERVO ラッパー) |
| `firmware/src/share/Mutex.h/cpp` | オーディオリソース排他制御 |
| `firmware/src/driver/Audio.h` | WAV 録音ヘルパー |
| `firmware/src/driver/ModuleLLM.h` | M5 LLM Module シリアル制御 |
| `firmware/lib/m5stack-avatar/` | Avatar 描画ライブラリ (バンドル) |

---

## 改造ポイントガイド

| やりたいこと | 対象 |
|-------------|------|
| 新しい LLM サービスを追加 | `LLMBase` を継承 → `Robot::initLLM()` に分岐追加 |
| 新しい TTS サービスを追加 | `TTSBase` を継承 → `Robot::initTTS()` に分岐追加 |
| 新しい STT サービスを追加 | `STTBase` を継承 → `Robot::initSTT()` に分岐追加 |
| 新しい UI 画面を追加 | `ModBase` を継承 → `init_mod()` に `add_mod()` で登録 |
| ボタン/タッチの挙動を変える | `AiStackChanMod` のコールバックを変更 |
| ソース無改変で設定のみ変える | SD カードの YAML を編集 |
