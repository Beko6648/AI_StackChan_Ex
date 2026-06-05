# CLAUDE.md — AI Stack-chan Ex (委譲用最小版)

M5Stack CoreS3 AIロボット。PlatformIO + Arduino。
フォーク: github.com/Beko6648/AI_StackChan_Ex

## Build
```powershell
& "C:\Users\beko6\.platformio\penv\Scripts\pio.exe" run -e m5stack-cores3
```
Flash前はVS Codeのシリアルモニターを閉じること。

## WebAPI (IP: 192.168.1.114)
- `/status` GET — WiFi確認
- `/reboot` POST — 再起動
- `/mode` GET/POST — モード切替
- `/command_result` POST — 応答送信

## Key files
- `firmware/src/driver/HeadTouch.cpp` — タッチセンサー(Si12T, I2C 0x68)
- `firmware/src/mood/MoodManager.h` — 感情パラメータ
- `firmware/src/llm/ChatGPT/ChatGPT.cpp` — LLM連携
- `firmware/src/Robot.cpp` — ロボット制御
- `firmware/src/mod/AiStackChan/AiStackChanMod.cpp` — メインモジュール

## Pitfalls
- シリアルログにWiFiパスワード・APIキー含む → フィルタ必須
- `heap_caps_malloc` → `heap_caps_free` (delete不可)
- .Encoding = UTF8 指定必須 (日本語文字化け防止)
- M5Unified CoreS3: 0.1.17固定

## 編集ルール
- コミット確認は必須。ひとりでにコミットしない
- `lib/` 配下は原則変更禁止
- テスト用パラメータは必ず本番値に戻す