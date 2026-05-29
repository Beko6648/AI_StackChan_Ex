# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AI Stack-chan Ex is a fork of ronron-gh/AI_StackChan_Ex that extends the AI Stack-chan mascot robot with emotion management, natural facial expressions, and acknowledgment sounds. The project targets M5Stack devices (Core2, CoreS3, AtomS3R) running Arduino firmware with multiple AI service integrations.

**Version**: 0.20.1

**Key Features**:
- Multi-backend AI conversation (ChatGPT, Module LLM, Realtime API, Gemini)
- Emotion-driven avatar expressions (happy, neutral, sad, angry, doubt, sleepy, sleeping)
- Mood system (joy/trust/sleepiness/wantToTalk) with LLM-driven dynamic updates
- Spontaneous self-talk based on mood parameters
- Sleep/wake cycle with yawn, sleep, and wake-up sounds
- Character switching system via SD card YAML files
- Long-term memory via SPIFFS
- Browser-based settings UI (personalize/settings pages)
- Automatic acknowledgment sounds during AI response wait times
- Pluggable modular application system
- Local-only and cloud-based AI service options

## Build & Development

### Prerequisites
- **IDE**: VSCode with PlatformIO extension
- **Target Devices**: M5Stack Core2, CoreS3, AtomS3R + Atomic Echo Base
- **Workspace Path**: Keep workspace as close to C:\ as possible (short path) to avoid library include path issues

### Claude Code によるビルド・書き込み

Claude Code から直接ビルド・書き込みが可能。`pio` は PATH に含まれないためフルパスで実行する。

```powershell
# ビルドのみ
& "C:\Users\beko6\.platformio\penv\Scripts\pio.exe" run -e m5stack-cores3

# ビルド＆書き込み（デバイスを USB 接続し、VS Code のシリアルモニターを閉じてから実行）
& "C:\Users\beko6\.platformio\penv\Scripts\pio.exe" run -e m5stack-cores3 -t upload
```

**注意**: 書き込み時に VS Code のシリアルモニターが開いていると COM ポートが占有されて失敗する。事前に閉じること。

### Claude Code による動作確認フロー

シリアルモニターはツールのタイムアウト制限があるため、監視時間内に確認対象の処理が起きるようテスト用設定を事前に行う。

**標準的な手順:**
1. テスト用パラメータに変更（例: `WANT_TO_TALK_RATE` を短くして30秒で自発発話させる）
2. ビルド＆書き込み
3. シリアル監視（例: 2分間）でログを取得・確認
4. 問題があればコード修正→ 2 に戻る
5. 確認完了後、テスト用パラメータを本番値に戻す

```powershell
# シリアル監視（指定秒数だけログを取得・UTF-8指定）
$port = New-Object System.IO.Ports.SerialPort "COM4", 115200
$port.Encoding = [System.Text.Encoding]::UTF8
$port.Open()
$end = (Get-Date).AddSeconds(90)
$log = @()
while ((Get-Date) -lt $end) {
    if ($port.BytesToRead -gt 0) {
        $line = $port.ReadLine()
        $log += $line
    }
    Start-Sleep -Milliseconds 10
}
$port.Close()
# 必要なキーワードだけ表示（生ログにAPIキー・WiFiパスワードが含まれるため直接表示しない）
$log | Select-String -Pattern "selfTalk|自発発話|\[META\]|\[Character\]|choices|Smart Config|DNS Failed"
```

**⚠️ セキュリティ注意**: シリアルログには WiFi パスワード・API キーが含まれる。生ログを直接出力せず、必ず `Select-String` でフィルタリングしてから確認すること。ログファイルもツール完了後に削除が望ましい。

**知見:**
- `.Encoding = UTF8` を指定しないと日本語が `?` になる
- WiFi 接続失敗時（`DNS Failed`）は再起動で解決することが多い
- 監視時間は「起動時間（約10秒）＋テスト用自発発話待機時間＋余裕」で設定する（例: 30秒自発発話なら90秒監視）
- 自発発話の確認では LLM の `choices[].content` と `[META]` ログを見ればよい

### WebAPI を使った動作確認フロー

書き込み後、シリアル監視の前に WebAPI で WiFi 接続を確認・再起動できる。デバイスのIPアドレスは通常 `192.168.1.114`（環境依存。BtnC を押すと画面にQRコードとIPが表示される）。

```powershell
# WiFi接続確認
Invoke-WebRequest -Uri "http://192.168.1.114/status" -Method GET -UseBasicParsing

# 未接続または接続不安定な場合は再起動
Invoke-WebRequest -Uri "http://192.168.1.114/reboot" -Method POST -UseBasicParsing

# 再起動後15秒待って再確認
Start-Sleep -Seconds 15
Invoke-WebRequest -Uri "http://192.168.1.114/status" -Method GET -UseBasicParsing
```

**推奨フロー:**
1. 書き込み完了後に `/status` で WiFi 接続を確認
2. `"wifi":false` なら `/reboot` で再起動し15秒後に再確認
3. WiFi 接続確認後にシリアル監視を開始

### バックグラウンドでのシリアル監視（サブエージェント）

シリアル監視をバックグラウンドのサブエージェントに任せることで、監視中に別の作業を並行して進められる。

**事前準備（各自の環境で一度だけ設定）:**

`.claude/settings.json`（gitignore対象）に以下を追加する。これがないとサブエージェントが PowerShell を実行できず、確認プロンプトも出ずに失敗する。

```json
{
  "permissions": {
    "allow": [
      "PowerShell(*)"
    ]
  }
}
```

**サブエージェントへの依頼例:**
```
Agent ツールで run_in_background: true を指定し、以下を実行させる:
- COM4 を 90 秒間監視
- selfTalk / 自発発話 / META / choices などのキーワードでフィルタリング
- APIキー・パスワードを含む行は除外して結果を返す
```

**注意:**
- シリアル監視中は COM4 を占有するため、監視完了まで書き込みはできない
- シリアルポートのバッファに古いログが溜まっている場合、監視開始直後に過去のログが一気に流れることがある（値の急変は誤検知の可能性あり）

### Build Targets

PlatformIO build targets in `platformio.ini`:

```
# Basic environment (requires Web APIs for LLM, STT, TTS)
pio run -e m5stack-core2

# With Realtime API (near-zero latency voice chat)
pio run -e m5stack-core2-realtime

# With Module LLM (fully local AI)
pio run -e m5stack-core2-llm

# With local TTS (AquesTalk)
pio run -e m5stack-core2-realtime-aquestalk

# CoreS3 variants
pio run -e m5stack-cores3-realtime
pio run -e m5stack-cores3-llm

# AtomS3R with Realtime API
pio run -e m5stack-atoms3r-realtime
```

### Build Flags

Key preprocessor flags in `platformio.ini`:
- `REALTIME_API`: OpenAI Realtime API for low-latency conversations
- `USE_LLM_MODULE`: M5Stack Module LLM (fully local inference)
- `ENABLE_WAKEWORD`: Wake word detection (SimpleVox)
- `USE_AQUESTALK`: AquesTalk TTS for local speech synthesis
- `ENABLE_SD_UPDATER`: SD card firmware update capability
- `ENABLE_CAMERA`: CoreS3 camera for face detection
- `ENABLE_TAP_DETECT`: Accelerometer double-tap detection

### Configuration Files

Device configuration is YAML-based on SD card:

1. **`/yaml/SC_SecConfig.yaml`** - WiFi credentials, API keys (ChatGPT, Google STT, TTS services)
2. **`/yaml/SC_BasicConfig.yaml`** - Servo pins, servo center/offset, Bluetooth settings
3. **`/app/AiStackChanEx/SC_ExConfig.yaml`** - LLM type, TTS type, STT type, wake word type, acknowledgment speaker ID, active character name
4. **`/characters/{name}.yaml`** - Character definitions (system_prompt, talk_prompt, self_talk_prompt, voice, memory)

See `Copy-to-SD/` for templates and example character files.

### Acknowledgment Sound Generation

```powershell
cd my_script
pwsh ./generate_ack_sounds.ps1
# Generates MP3 files in sounds/ by speaker ID
# Copy to SD card: /ack/{speakerId}/
```

Configuration: `my_script/json/ack_phrases.json`

## Architecture

### High-Level Structure

```
firmware/src/
├── main.cpp                 # Entry, mod initialization
├── Robot.h/cpp              # Core: LLM, TTS, STT management
├── StackchanExConfig.h      # Configuration structs
├── StackChanMind.h/cpp      # Emotion state machine
├── AckSound.h/cpp           # Acknowledgment sound playback
├── CharacterLoader.h/cpp    # SD card character YAML loader
├── mood/
│   └── MoodManager.h/cpp    # joy/trust/sleepiness/wantToTalk management
├── head/
│   ├── HeadMotionController.h/cpp  # Head motion strategy controller
│   └── IdleLookAround.h/cpp        # Idle look-around motion
├── driver/
│   └── HeadTouch.h/cpp      # Si12T capacitive touch sensor (3-zone)
├── mod/                     # Pluggable application modules
│   ├── ModBase.h            # Abstract base
│   ├── ModManager.h/cpp     # Mod registry & switching
│   ├── AiStackChan/         # Main AI conversation mod
│   ├── Pomodoro/            # Pomodoro timer
│   ├── PhotoFrame/          # Photo gallery
│   ├── StatusMonitor/       # System info
│   └── QRdisplay/           # QR code display
├── llm/                     # LLM backends
│   ├── LLMBase.h/cpp        # Abstract interface
│   ├── ChatHistory.h/cpp    # Conversation history
│   ├── RealtimeLLMBase.h/cpp # WebSocket realtime
│   ├── ChatGPT/             # OpenAI ChatGPT
│   │   ├── FunctionCall.h/cpp
│   │   ├── MCPClient.h/cpp  # Model Context Protocol
│   │   └── RealtimeChatGPT.h/cpp
│   ├── Gemini/              # Google Gemini
│   ├── ModuleLLM/           # M5Stack Module LLM
│   └── ModuleLLMFncl/       # Module LLM + function calling
├── tts/                     # TTS backends
│   ├── TTSBase.h            # Abstract interface
│   ├── WebVoiceVoxTTS.h/cpp
│   ├── ElevenLabsTTS.h/cpp
│   ├── OpenAITTS.h/cpp
│   ├── UAquesTalkTTS.h/cpp
│   └── ModuleLLMTTS.h/cpp
├── stt/                     # STT backends
│   ├── STTBase.h
│   ├── CloudSpeechClient.h/cpp
│   ├── Whisper.h/cpp
│   └── ModuleLLMASR/Whisper
├── driver/                  # Hardware drivers
│   ├── Audio.h/cpp          # Audio recording
│   ├── Camera.h/cpp         # ESP32 camera
│   ├── WakeWord.h/cpp       # Wake word detection
│   ├── ModuleLLM.h/cpp      # Serial communication
│   ├── PlayMP3.h/cpp        # MP3/WAV playback
│   ├── TapDetect.h/cpp      # Accelerometer
│   ├── WatchDog.h/cpp
│   └── M5AudioModule.h/cpp
└── share/                   # Utilities
    ├── Version.h
    ├── DefaultParams.h
    ├── SDUtil.h
    └── Mutex.h
```

### Module System

All user applications inherit from `ModBase`:

```cpp
class ModBase {
  virtual void init();                           // Setup
  virtual void update(int page);                 // Main loop
  virtual void btnA_pressed(), btnC_pressed();   // Input handlers
  virtual void display_touched(int16_t x, int16_t y);
  virtual bool isBusy();                         // Is processing?
};
```

Register multiple mods in `main.cpp:init_mod()`. Users swipe LCD left/right to switch mods. `ModManager` routes input to active mod.

### Emotion System

**StackChanMind** manages avatar emotional state:

```cpp
class StackChanMind {
  Expression getEmotion();
  String emotionToString();                   // For LLM prompts
  void setEmotion(const String& name);        // Update + apply
  void applyExpression();                     // Restore without change
};
```

**Emotions**: neutral, happy, sleepy, doubt, sad, angry (map to `m5avatar::Expression`).

LLM responses embed emotion metadata:
```
"That's wonderful! [META]{"emotion":"happy","joy":0.3,"trust":0.1}[/META]"
```

Firmware extracts emotion, joy, and trust values. Emotion is applied to Avatar via `StackChanMind`, joy/trust update `MoodManager` parameters.

### AI Conversation Flows

**Standard (ChatGPT/Web API)**:
1. User speaks → STT (text)
2. Text → LLM (response text)
3. Response → TTS (audio)
4. Between 2-3: Play acknowledgment sound

**Realtime API**:
1. Audio stream → WebSocket → RealtimeLLMBase → Audio output
   - Direct audio-to-audio, bypasses STT/TTS latency

**Module LLM**:
1. All services (LLM, STT, TTS) via serial UART to M5Stack expansion module
   - Fully offline, no cloud API calls

### Critical Classes

**Robot** (hub managing AI services):
- `initLLM()`, `initRtLLM()`, `initSTT()`, `initTTS()` instantiate services from config
- Detects if all services offline, sets `isOffline` flag
- `asyncPlaying` + `invokeAsyncTtsStreamTask()` enable concurrent TTS during LLM response

**LLMBase** (all LLM backends):
- `chat(text, base64_buf)` initiates conversation
- Manages `ChatHistory` (message pairs for context)
- Supports system prompts and optional memory (SPIFFS)
- `outputTextQueue` for async TTS

**RealtimeLLMBase** (Realtime API):
- WebSocket client to OpenAI Realtime API
- Direct audio streaming, no intermediate text
- `getAudioLevel()` for lip-sync

**AiStackChanMod** (main application):
- Routes user input (buttons, touch, wake word)
- UI layout via `box_t` touch regions
- Coordinates Robot with Avatar animations
- Expression changes via `StackChanMind`

### Data Flow During Conversation

1. Wake word detected → activate STT
2. User speaks → `Audio::Record()` captures WAV, base64 encode
3. STT complete → `playAckSound()` for feedback
4. LLM processes → `Robot::chat()` to backend
5. TTS streams → `Robot::speech()` playback (async)
6. Lip sync → `lipSync()` task reads TTS audio level, drives Avatar mouth
7. Parse response → Extract `[META]` tags, apply emotion via `StackChanMind::setEmotion()`

## Key Implementation Details

### Memory Management

- **PSRAM**: All targets use `BOARD_HAS_PSRAM` for extended memory (critical for JSON, audio)
- **JSON**: `SpiRamJsonDocument` allocates from PSRAM to avoid heap fragmentation
- **Audio**: `Audio::wavData` dynamically allocated (16-bit mono PCM, ~1.9s max)

### Configuration Loading

- **Stackchan-arduino library** provides hardware-agnostic YAML parsing
- **Three-tier config load**:
  1. SC_SecConfig.yaml (WiFi, API keys)
  2. SC_BasicConfig.yaml (servo pins, hardware)
  3. SC_ExConfig.yaml (LLM/TTS/STT choices)

### Serial Communication

- **Module LLM**: UART (configurable in SC_ExConfig.yaml; Core2 default: Rx:13, Tx:14)
- **FTP Server**: File access via ESP8266FtpServer during development

### Known Constraints

- **Path length**: Workspace near C:\ root to avoid Arduino library include path failures
- **SD card**: FAT32 has issues with Japanese chars; use ASCII-only paths
- **CoreS3 M5Unified**: Version 0.1.17 (not latest) for compatibility; Core2 uses 0.2.7
- **Wake word**: Not supported on AtomS3R (SimpleVox disabled)

### HeadTouch Sensor

- **デバイス**: Si12T 静電容量式 3ゾーンタッチセンサー
- **I2C**: アドレス 0x68、SDA=G12、SCL=G11
- **ジェスチャー**:
  - `LongPress`（800ms）→ 音声認識開始
  - `SwipeForward` / `SwipeBackward` → joy +0.1 / trust +0.05・表情2秒変化

### SD カードのファイル構造

```
/ack/{speakerId}/
  {expression}_{romaji}.mp3   # 相槌・あくび・就寝・起床音声
  例: happy_hai.mp3 / yawn_fuwa.mp3 / sleep_oyasumi.mp3

/characters/{name}.yaml       # キャラクター定義
/yaml/SC_BasicConfig.yaml
/yaml/SC_SecConfig.yaml
/app/AiStackChanEx/SC_ExConfig.yaml
```

### SPIFFS のシステムプロンプト構造

```
messages[0] → キャラクターロール（character YAML の system_prompt）
messages[1] → システムロール（memory指示 + talk_prompt + 感情指示）
messages[2] → User Info（長期記憶の要約テキスト）
```

### 会話履歴

- `MAX_HISTORY = 10`（5往復）で古いものから自動削除

### MoodManager パラメータ

| パラメータ | 値 | 意味 |
|---|---|---|
| UPDATE_INTERVAL_MS | 30000 | 更新周期（ms） |
| WANT_TO_TALK_THRESHOLD | 1.0 | 自発発話トリガー閾値 |
| WANT_TO_TALK_RATE | 1.0/300.0 | 増加レート（約5分で閾値到達） |
| SLEEPINESS_THRESHOLD | 0.8 | Sleepy表情閾値 |
| SLEEPINESS_RATE | 0.8/600.0 | 増加レート（約10分で閾値到達） |
| JOY_DECAY_RATE | 1.0/600.0 | joy減衰（約10分で±1.0→0.0） |
| TRUST_DECAY_RATE | 0.0 | trust減衰なし（信頼・怒りは持続） |
| MOOD_THRESHOLD | 0.6 | 表情切り替え閾値 |

テスト時は `WANT_TO_TALK_RATE` や `SLEEPINESS_RATE` の分母を小さくして加速する。

### WebAPI エンドポイント一覧

| エンドポイント | メソッド | 機能 |
|---|---|---|
| `/` | GET | Personalize画面 |
| `/personalize.html` | GET | Personalize画面 |
| `/settings.html` | GET | Settings画面 |
| `/status` | GET | WiFi接続状態・IP・ヒープを返す |
| `/reboot` | POST | デバイスを再起動する |
| `/role_set` | POST | システムプロンプトをSPIFFSに保存 |
| `/role_get` | POST | 現在のシステムプロンプトを取得 |
| `/memory_get` | POST | 長期記憶（User Info）を取得 |
| `/memory_clear` | POST | 長期記憶を消去 |
| `/characters` | GET | キャラクター一覧（JSON配列） |
| `/character_get?name=xxx` | GET | キャラクターYAML取得 |
| `/character_set?name=xxx` | POST | キャラクターYAML保存 |
| `/active_character` | GET | アクティブキャラクター名取得 |
| `/active_character` | POST | アクティブキャラクター変更 |
| `/servo_offset` | GET | サーボオフセット取得 |
| `/servo_offset` | POST | サーボオフセット保存 |
| `/sleep` | POST | 睡眠状態を操作する（`action=sleep` で就寝、`action=wakeup` で起床） |
| `/pending_command` | GET | Claude Code 連携用：次のコマンドを返す（なければ `command_id: null`） |
| `/command_result` | POST | Claude Code 連携用：レスポンスを受け取り VOICEVOX で読み上げ |
| `/mode` | GET | 現在の AI モードを返す（`chatgpt` or `claude_code`） |
| `/mode` | POST | AI モードを切り替える（NVS 保存、再起動後も維持） |

## Development Workflows

### Adding a New Mod

1. Create `firmware/src/mod/{ModName}/{ModName}Mod.h` and `.cpp`
2. Inherit from `ModBase`, implement virtual methods
3. Register in `main.cpp:init_mod()` via `add_mod(new YourMod())`
4. Test mod switching via LCD swipe

### Adding LLM Support

1. Create class in `firmware/src/llm/{ServiceName}/`, inherit `LLMBase`
2. Implement `chat()` to call API, populate `outputTextQueue`
3. Implement `init_chat_doc()` for service-specific JSON
4. Set `isOfflineService` if no network needed
5. Update `Robot::initLLM()` switch statement
6. Add config option to SC_ExConfig.yaml

### Debugging

- **Serial**: 115200 baud; stack traces via `esp32_exception_decoder`
- **SPIFFS Web UI**: WebAPI.h for file/config inspection
- **FTP**: Browse M5Stack files during development
- **Debug flags**: Uncomment in platformio.ini (e.g., `CORE_DEBUG_LEVEL=5`)

## Testing

自動テストフレームワークなし。動作確認は実機（CoreS3）で行う。

1. `m5stack-cores3` ターゲットでビルドしてエラー・警告がないことを確認
2. デバイスに書き込み、`/status` エンドポイントで WiFi 接続を確認
3. 対象機能をテスト（詳細は「Claude Code による動作確認フロー」参照）
4. シリアルログでエラーがないことを確認
5. テスト用パラメータを使った場合は本番値に戻す

## ESP32 Platform-Specific Pitfalls

Known behaviors that differ from standard Arduino or desktop environments:

- **SD `entry.name()` returns full path**: `SD.openNextFile()` で取得した `File` の `.name()` は `/directory/filename.yaml` のようなフルパスを返す。ファイル名だけ必要な場合は `fullPath.substring(fullPath.lastIndexOf('/') + 1)` で取り出すこと。
- **`FILE_WRITE` は上書き（`"w"` モード）**: 既存ファイルを開くと内容が切り捨てられる。追記したい場合は `FILE_APPEND`（`"a"` モード）を使うこと。
- **SD はマウント済み前提**: `WebAPI.cpp` などから SD を使う場合、`main.cpp` で `SD.begin()` が先に呼ばれていることを前提とする。
- **新しい Web API エンドポイントはコミット前に必ず実機確認**: ビルドが通ってもランタイムの挙動が想定と異なることがある。curl やブラウザで実際のレスポンスを確認してからコミットすること。

## Recent & Planned

**Implemented (v0.20.1)**:
- Emotion management (StackChanMind + MoodManager)
- Acknowledgment sounds with emotion weighting
- LLM response emotion/joy/trust metadata parsing
- Spontaneous self-talk (selfTalk) based on mood
- Sleep/wake cycle with Sleeping expression and sounds
- Head touch sensor (Si12T) with swipe/long-press gestures
- Idle look-around head motion (HeadMotionController)
- Character switching system via SD card YAML
- Long-term memory (SPIFFS-based)
- Browser settings UI (/personalize.html, /settings.html)
- WebAPI device control (/status, /reboot)
- Realtime API support
- MCP (Model Context Protocol) in ChatGPT
- Claude Code 連携（polling.ps1 + /pending_command・/command_result・/mode エンドポイント、Settings UI、AIモード切り替え）

**Planned** (see `my_doc/FeatureMemo.md`):
- Sound localization & face tracking (dual-mic TDOA)
- Dream narration on wake-up
- Idle expression animation (random mood-based)
- Vector-based long-term memory (embeddings)
- Head LED emotion visualization

## CLAUDE.md の最新化手順

実装が変わったときは以下のタイミングで CLAUDE.md を更新する。

**更新が必要なとき:**
- 新機能を実装してマージしたとき
- パラメータ値（MoodManager のレート・閾値など）を変更したとき
- 新しい WebAPI エンドポイントを追加したとき
- SD カードのファイル構造が変わったとき
- ビルド・動作確認フローが変わったとき
- ESP32 固有の挙動で新しい知見を得たとき

**更新の観点:**
1. `## Recent & Planned` の Implemented / Planned リストを見直す
2. `### WebAPI エンドポイント一覧` に追加エンドポイントを記載する
3. `### MoodManager パラメータ` の値が実装と一致しているか確認する
4. `### SD カードのファイル構造` に新しいファイル種別を追記する
5. `## ESP32 Platform-Specific Pitfalls` に新しい知見を追記する
6. バージョン番号（`**Version**`）が `Version.h` と一致しているか確認する

## External Resources

- **Original Stack-chan**: https://github.com/stack-chan/stack-chan
- **Stackchan-arduino library**: https://github.com/mongonta0716/stackchan-arduino
- **M5Stack docs**: https://docs.m5stack.com
- **PlatformIO docs**: https://docs.platformio.org
- **Avatar library**: firmware/lib/m5stack-avatar/
