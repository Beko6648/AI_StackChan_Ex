#ifndef __CHARACTER_LOADER_H__
#define __CHARACTER_LOADER_H__

#include <Arduino.h>

struct CharacterData {
    String systemPrompt;
    String voice;    // TTS 話者ID。空の場合は SC_ExConfig の tts.voice を使用
    bool   memory = false;  // 長期記憶の有効・無効
};

// SD カードの /characters/{name}.yaml からキャラクター設定を読み込む
// 成功時は true を返し、data に内容を格納する
// ファイルが存在しない場合や name が空の場合は false を返す
bool loadCharacter(const String& name, CharacterData& data);

#endif
