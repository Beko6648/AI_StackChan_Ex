#include "CharacterLoader.h"
#include <SD.h>
#include <ArduinoJson.h>
#include <ArduinoYaml.h>

bool loadCharacter(const String& name, CharacterData& data) {
    if (name.isEmpty()) return false;

    String path = String("/characters/") + name + ".yaml";
    File file = SD.open(path.c_str());
    if (!file) {
        Serial.printf("[Character] File not found: %s\n", path.c_str());
        return false;
    }

    DynamicJsonDocument doc(4096);
    auto err = deserializeYml(doc, file);
    file.close();

    if (err) {
        Serial.printf("[Character] YAML parse error: %s\n", err.c_str());
        return false;
    }

    if (!doc["system_prompt"].is<const char*>()) {
        Serial.println("[Character] system_prompt not found");
        return false;
    }

    data.systemPrompt        = doc["system_prompt"].as<String>();
    data.conversationPrompt  = doc["conversation_prompt"].as<String>();
    data.voice               = doc["voice"].as<String>();
    data.memory              = doc["memory"] | false;

    Serial.printf("[Character] Loaded: %s (voice=%s memory=%s)\n",
                  name.c_str(),
                  data.voice.isEmpty() ? "default" : data.voice.c_str(),
                  data.memory ? "true" : "false");
    return true;
}
