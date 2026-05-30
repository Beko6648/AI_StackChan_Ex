# アバター処理フロー図

## 1. 初期化フロー（setup）

```mermaid
flowchart TD
    A[setup 開始] --> B[M5.begin]
    B --> C[init_mic_spk]
    C --> D[SD/SPIFFS から設定読み込み]
    D --> E[WiFi 接続]
    E --> F[Robot 生成\nLLM/TTS/STT 初期化]
    F --> G[init_mod\nAiStackChanMod 生成]
    G --> H[avatar.init colorDepth=16\n→ drawLoop タスク起動\n→ facialLoop タスク起動]
    H --> I[avatar.addTask lipSync\n優先度2 / 2048byte]
    I --> J[avatar.addTask servo\n優先度1 / 2048byte]
    J --> K[avatar.addTask battery_check\n優先度1 / 2048byte]
    K --> L[loop 開始]
```

---

## 2. ランタイム タスク構成

```mermaid
flowchart LR
    subgraph RTOS["FreeRTOS タスク（並行実行）"]
        direction TB

        subgraph DL["drawLoop　優先度2 / 66ms周期\nAvatar.cpp:18"]
            DL1["avatar->draw()\n  └ DrawContext 生成\n  └ face->draw(ctx)\n  └ LCD へ描画"]
            DL2["fadeoutProcess()\n  （mod切替アニメ）"]
        end

        subgraph FL["facialLoop　優先度2 / 66ms周期\nAvatar.cpp:31"]
            FL1["サッカード\nmillis() 基準\n500〜2500ms ごと\n→ setGaze()"]
            FL2["まばたき\nmillis() 基準\n開:2500〜4500ms\n閉:300〜500ms\n→ setEyeOpenRatio()"]
            FL3["呼吸\nループカウンタ基準 ※\nsin(c×2π/100)\n→ setBreath()"]
        end

        subgraph LS["lipSync　優先度2 / 100ms周期\nmain.cpp:82"]
            LS1["TTS音量取得\ntts->getLevel()\n または getAudioLevel()"]
            LS2["口開度計算\nlevel / 15000.0\n→ setMouthOpenRatio()"]
            LS3["head rotation\ngetGaze() → gazeX × 5\n→ setRotation()"]
        end

        subgraph SV["servo　優先度1 / 5000ms周期\nmain.cpp:113"]
            SV1["getGaze() で視線取得"]
            SV2["moveToGaze()\n水平: gazeX×15度\n垂直: gazeY×10度"]
        end

        subgraph BC["battery_check　優先度1 / 60秒周期\nmain.cpp:137"]
            BC1["getBatteryLevel()"]
            BC2["setBatteryIcon()\nsetBatteryStatus()"]
        end

        subgraph SPK["M5.Speaker タスク\nCore1固定 APP_CPU_NUM\n64kHz サンプルレート"]
            SPK1["音声データ出力\nTTS ストリーム再生"]
        end
    end

    subgraph ML["メインループ loop()"]
        ML1["M5.update()"]
        ML2["mod->idle()\n AiStackChanMod"]
        ML3["ボタン/タッチ処理"]
        ML4["Web/FTP サーバ処理"]
        ML1 --> ML2 --> ML3 --> ML4
    end
```

> ※ 呼吸の `c` は毎 66ms でインクリメントされるためループ速度に依存する（vTaskDelay 変更で速度が変わる）

---

## 3. Avatar 状態変数と読み書き関係

```mermaid
flowchart TD
    subgraph AvatarState["Avatar 状態変数（共有メモリ）"]
        S_EXP["expression\n表情種別"]
        S_GZ["gazeV / gazeH\n視線方向"]
        S_EOR["eyeOpenRatio\n目の開き具合"]
        S_BR["breath\n呼吸量"]
        S_MOR["mouthOpenRatio\n口の開き具合"]
        S_ROT["rotation\n頭の傾き"]
        S_BAT["batteryIconStatus\nbatteryLevel"]
    end

    FL_W["facialLoop"] -->|setGaze| S_GZ
    FL_W -->|setEyeOpenRatio| S_EOR
    FL_W -->|setBreath| S_BR

    LS_W["lipSync"] -->|getGaze → setRotation| S_ROT
    LS_W -->|setMouthOpenRatio| S_MOR
    LS_W -.->|getGaze 参照| S_GZ

    BC_W["battery_check"] -->|setBatteryStatus| S_BAT

    MOD["AiStackChanMod\n→ StackChanMind\n→ setEmotion\n→ setExpression"] -->|setExpression| S_EXP

    S_EXP -->|DrawContext| DRAW["drawLoop\nface->draw()\n→ LCD 描画"]
    S_GZ --> DRAW
    S_EOR --> DRAW
    S_BR --> DRAW
    S_MOR --> DRAW
    S_ROT --> DRAW
    S_BAT --> DRAW

    S_GZ -->|getGaze| SV_R["servo\n→ moveToGaze\n→ 物理サーボ"]
```

---

## 4. 会話中の処理フロー（AiStackChanMod::idle）

```mermaid
sequenceDiagram
    participant Mod as AiStackChanMod::idle
    participant Robot as Robot
    participant STT as STT
    participant LLM as LLM / ChatGPT
    participant TTS as TTS
    participant Mind as StackChanMind
    participant Avatar as Avatar（状態変数）
    participant Draw as drawLoop（並行）

    Note over Draw: 常時 66ms ごとに描画中

    Mod->>Robot: listen()
    Robot->>STT: 音声録音 → テキスト変換
    STT-->>Robot: テキスト
    Robot->>Mod: playAckSound()（相槌音）

    Robot->>LLM: chat(テキスト)
    LLM-->>Robot: 応答テキスト\n[META]{"emotion":"happy"}[/META] 含む

    Robot->>TTS: speech(テキスト)
    Note over TTS: asyncTtsStreamTask\nで非同期再生

    par TTS 再生中
        TTS-->>Avatar: getLevel() → setMouthOpenRatio()
        Note over Avatar: lipSync が 100ms ごとに口を更新
    and
        Draw-->>Draw: 66ms ごとに LCD 更新
    end

    Robot->>Mind: setEmotion("happy")
    Mind->>Avatar: setExpression(Happy)
    Note over Draw: 次の draw() で Happy 表情を描画
```

---

## 備考

| タスク | 周期 | 実効 fps | 依存タイミング |
|---|---|---|---|
| drawLoop | 66ms | 約15fps | 固定（ハードコード） |
| facialLoop | 66ms | 約15fps | サッカード/まばたき: millis()基準、呼吸: カウンタ基準 |
| lipSync | 100ms | 約10fps | 固定（ハードコード） |
| servo | 5000ms | 0.2Hz | IdleLookAround が直接制御するときは無効 |
| battery_check | 60000ms | 毎分 | — |
