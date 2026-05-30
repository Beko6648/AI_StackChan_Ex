#include "MetaTagParser.h"
#include <ArduinoJson.h>
#include "StackChanMind.h"
#include "mood/MoodManager.h"

extern StackChanMind stackChanMind;
extern MoodManager*  g_moodManager;

void applyMetaTag(String& text) {
  // LLM が全角タグを出力する場合があるため半角に正規化する
  text.replace("【META】",  "[META]");
  text.replace("【/META】", "[/META]");

  int metaStart = text.indexOf("[META]");
  int metaEnd   = text.indexOf("[/META]");

  if (metaStart >= 0 && metaEnd > metaStart) {
    // 正常形式: [META]{...}[/META] → 感情・気分を適用してタグを除去
    String metaContent = text.substring(metaStart + 6, metaEnd);
    DynamicJsonDocument metaDoc(128);
    if (!deserializeJson(metaDoc, metaContent)) {
      const char* emotion = metaDoc["emotion"];
      if (emotion) stackChanMind.setEmotion(String(emotion));
      if (g_moodManager) {
        if (metaDoc.containsKey("joy"))   g_moodManager->addJoy(metaDoc["joy"].as<float>());
        if (metaDoc.containsKey("trust")) g_moodManager->addTrust(metaDoc["trust"].as<float>());
        Serial.printf("[META] emotion=%s joy=%.2f trust=%.2f\n",
                      emotion ? emotion : "none",
                      g_moodManager->getJoy(), g_moodManager->getTrust());
      }
    }
    text = text.substring(0, metaStart) + text.substring(metaEnd + 7);
    text.trim();
  } else if (metaEnd >= 0) {
    // 不正形式: [/META] だけ出現した場合はそれ以降を除去
    text = text.substring(0, metaEnd);
    text.trim();
  }

  // 残存タグを念のため除去
  text.replace("[META]",  "");
  text.replace("[/META]", "");
}
