# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

AI Stack-chan Ex is a fork of ronron-gh/AI_StackChan_Ex that extends the AI Stack-chan mascot robot with emotion management, natural facial expressions, and acknowledgment sounds. The project targets M5Stack devices (Core2, CoreS3, AtomS3R) running Arduino firmware with multiple AI service integrations.

**Version**: 0.20.0

**Key Features**:
- Multi-backend AI conversation (ChatGPT, Module LLM, Realtime API, Gemini)
- Emotion-driven avatar expressions (happy, neutral, sad, angry, doubt, sleepy)
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
6. **ユーザーに確認結果を報告し、コミットの承諾を得てからコミットする**（確認OKでも自動でコミットしない）

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

書き込み後、シリアル監視の前に WebAPI で WiFi 接続を確認・再起動できる。デバイスのIPアドレスは通常 `192.168.1.114`。

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

Device configuration is YAML-based on SD card in `/yaml/`:

1. **SC_SecConfig.yaml** - WiFi credentials, API keys (ChatGPT, Google STT, TTS services)
2. **SC_BasicConfig.yaml** - Servo pins (varies by device), servo speeds, Bluetooth settings
3. **SC_ExConfig.yaml** - LLM type (0: ChatGPT, 1: ModuleLLM, 2: ModuleLLM+FuncCall, 3: Gemini), TTS type, STT type, wake word type, acknowledgment speaker ID

See `Copy-to-SD/yaml/` for templates.

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
"That's wonderful! [META]{"emotion":"happy"}[/META]"
```

Firmware extracts and applies emotion after speech, making expressions reactive to conversation content.

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

No automated test framework. Manual validation:
1. Compile all target environments (no warnings)
2. Flash to device, verify UI and conversation
3. Test offline mode (isOffline flag)
4. Test API failover (invalid keys, no network)

## ESP32 Platform-Specific Pitfalls

Known behaviors that differ from standard Arduino or desktop environments:

- **SD `entry.name()` returns full path**: `SD.openNextFile()` で取得した `File` の `.name()` は `/directory/filename.yaml` のようなフルパスを返す。ファイル名だけ必要な場合は `fullPath.substring(fullPath.lastIndexOf('/') + 1)` で取り出すこと。
- **`FILE_WRITE` は上書き（`"w"` モード）**: 既存ファイルを開くと内容が切り捨てられる。追記したい場合は `FILE_APPEND`（`"a"` モード）を使うこと。
- **SD はマウント済み前提**: `WebAPI.cpp` などから SD を使う場合、`main.cpp` で `SD.begin()` が先に呼ばれていることを前提とする。
- **新しい Web API エンドポイントはコミット前に必ず実機確認**: ビルドが通ってもランタイムの挙動が想定と異なることがある。curl やブラウザで実際のレスポンスを確認してからコミットすること。

## Recent & Planned

**Implemented (v0.20.0)**:
- Emotion management (StackChanMind)
- Acknowledgment sounds with emotion weighting
- LLM response emotion metadata parsing
- Realtime API support
- MCP (Model Context Protocol) in ChatGPT

**Planned** (see `my_doc/FeatureMemo.md`):
- Autonomous thinking & spontaneous speech
- Sound localization & face tracking (dual-mic TDOA)
- Sleep/wake cycles with dream narration
- Idle animation choreography (emotion-driven)
- Vector-based long-term memory (embeddings)

## External Resources

- **Original Stack-chan**: https://github.com/stack-chan/stack-chan
- **Stackchan-arduino library**: https://github.com/mongonta0716/stackchan-arduino
- **M5Stack docs**: https://docs.m5stack.com
- **PlatformIO docs**: https://docs.platformio.org
- **Avatar library**: firmware/lib/m5stack-avatar/
