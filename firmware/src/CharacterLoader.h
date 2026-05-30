#ifndef __CHARACTER_LOADER_H__
#define __CHARACTER_LOADER_H__

#include <Arduino.h>

struct CharacterData {
    String systemPrompt;
    String talkPrompt;          // 毎回の会話時に system role に追加する指示
    String selfTalkPrompt;      // 自発発話時のプロンプト
    String claudeErrorText;     // Claude Code が応答できなかったときの読み上げ文言
    String claudeTimeoutText;   // Claude Code 待機タイムアウト時の読み上げ文言
    String voice;               // TTS 話者ID。空の場合は SC_ExConfig の tts.voice を使用
    bool   memory = false;      // 長期記憶の有効・無効
};

// SD カードの /characters/{name}.yaml からキャラクター設定を読み込む
// 成功時は true を返し、data に内容を格納する
// ファイルが存在しない場合や name が空の場合は false を返す
bool loadCharacter(const String& name, CharacterData& data);

#endif
