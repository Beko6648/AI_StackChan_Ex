#include <ESP32WebServer.h>
#include <nvs.h>
#include <SD.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ArduinoYaml.h>
#include "WebAPI.h"
#include "Avatar.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include "Robot.h"
#include "mod/ModManager.h"
#include "mod/AiStackChan/AiStackChanMod.h"

using namespace m5avatar;
extern Avatar avatar;
extern uint8_t m5spk_virtual_channel;
extern String STT_API_KEY;
extern AiStackChanMod* g_ai_stackchan_mod;

ESP32WebServer server(80);

// C++11 multiline string constants are neato...
static const char HEAD[] PROGMEM = R"KEWL(
<!DOCTYPE html>
<html lang="ja">
<head>
  <meta charset="UTF-8">
  <title>AIｽﾀｯｸﾁｬﾝ</title>
</head>)KEWL";

static const char APIKEY_HTML[] PROGMEM = R"KEWL(
<!DOCTYPE html>
<html>
  <head>
    <meta charset="UTF-8">
    <title>APIキー設定</title>
  </head>
  <body>
    <h1>APIキー設定</h1>
    <form>
      <label for="role1">OpenAI API Key</label>
      <input type="text" id="openai" name="openai" oninput="adjustSize(this)"><br>
      <label for="role2">VoiceVox API Key</label>
      <input type="text" id="voicevox" name="voicevox" oninput="adjustSize(this)"><br>
      <label for="role3">Speech to Text API Key</label>
      <input type="text" id="sttapikey" name="sttapikey" oninput="adjustSize(this)"><br>
      <button type="button" onclick="sendData()">送信する</button>
    </form>
    <script>
      function adjustSize(input) {
        input.style.width = ((input.value.length + 1) * 8) + 'px';
      }
      function sendData() {
        // FormDataオブジェクトを作成
        const formData = new FormData();

        // 各ロールの値をFormDataオブジェクトに追加
        const openaiValue = document.getElementById("openai").value;
        if (openaiValue !== "") formData.append("openai", openaiValue);

        const voicevoxValue = document.getElementById("voicevox").value;
        if (voicevoxValue !== "") formData.append("voicevox", voicevoxValue);

        const sttapikeyValue = document.getElementById("sttapikey").value;
        if (sttapikeyValue !== "") formData.append("sttapikey", sttapikeyValue);

	    // POSTリクエストを送信
	    const xhr = new XMLHttpRequest();
	    xhr.open("POST", "/apikey_set");
	    xhr.onload = function() {
	      if (xhr.status === 200) {
	        alert("データを送信しました！");
	      } else {
	        alert("送信に失敗しました。");
	      }
	    };
	    xhr.send(formData);
	  }
	</script>
  </body>
</html>)KEWL";

#if 0
static const char ROLE_HTML[] PROGMEM = R"KEWL(
<!DOCTYPE html>
<html>
<head>
	<title>ロール設定</title>
	<meta charset="UTF-8">
	<meta name="viewport" content="width=device-width, initial-scale=1.0">
	<style>
		textarea {
			width: 80%;
			height: 200px;
			resize: both;
		}
	</style>
</head>
<body>
	<h1>ロール設定</h1>
	<form onsubmit="postData(event)">
		<label for="textarea">ここにロールを記述してください。:</label><br>
		<textarea id="textarea" name="textarea"></textarea><br><br>
		<input type="submit" value="Submit">
	</form>
	<script>
		function postData(event) {
			event.preventDefault();
			const textAreaContent = document.getElementById("textarea").value.trim();
//			if (textAreaContent.length > 0) {
				const xhr = new XMLHttpRequest();
				xhr.open("POST", "/role_set", true);
				xhr.setRequestHeader("Content-Type", "text/plain;charset=UTF-8");
			// xhr.onload = () => {
			// 	location.reload(); // 送信後にページをリロード
			// };
			xhr.onload = () => {
				document.open();
				document.write(xhr.responseText);
				document.close();
			};
				xhr.send(textAreaContent);
//        document.getElementById("textarea").value = "";
				alert("Data sent successfully!");
//			} else {
//				alert("Please enter some text before submitting.");
//			}
		}
	</script>
</body>
</html>)KEWL";
#endif

#define IMPORT_FILE(section, filename, symbol) \
static constexpr const char* filename_##symbol = filename; \
extern const uint8_t symbol[], sizeof_##symbol[]; \
asm(\
  ".section " #section "\n"\
  ".balign 4\n"\
  ".global " #symbol "\n"\
  #symbol ":\n"\
  ".incbin \"incbin/" filename "\"\n"\
  ".global sizeof_" #symbol "\n"\
  ".set sizeof_" #symbol ", . - " #symbol "\n"\
  ".balign 4\n"\
  ".section \".text\"\n")

//IMPORT_FILE(.rodata, "index.html", index_html);
IMPORT_FILE(.rodata, "personalize.html", personalize_html);
IMPORT_FILE(.rodata, "personalize.js", personalize_js);
IMPORT_FILE(.rodata, "settings.html", settings_html);
IMPORT_FILE(.rodata, "settings.js", settings_js);


void handleRoot() {
  //Serial.println("handleRoot");
  //server.send(200, "text/plain", "hello from m5stack!");
  server.send_P(200, "text/html", (const char*)personalize_html, (size_t)sizeof_personalize_html);
}

void handle_personalize_html() {
  server.send_P(200, "text/html", (const char*)personalize_html, (size_t)sizeof_personalize_html);
}

void handle_personalize_js() {
  server.send_P(200, "application/javascript", (const char*)personalize_js, (size_t)sizeof_personalize_js);
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
//  server.send(404, "text/plain", message);
  server.send(404, "text/html", String(HEAD) + String("<body>") + message + String("</body>"));
}

void handle_speech() {
  String message = server.arg("say");
  String speaker = server.arg("voice");
  //if(speaker != "") {
  //  TTS_PARMS = TTS_SPEAKER + speaker;
  //}
  Serial.println(message);
  ////////////////////////////////////////
  // 音声の発声
  ////////////////////////////////////////
  //avatar.setExpression(Expression::Happy);
  robot->speech(message);
  server.send(200, "text/plain", String("OK"));
}

void handle_chat() {
  static String response = "";
  // tts_parms_no = 1;
  String text = server.arg("text");
  String speaker = server.arg("voice");
  //if(speaker != "") {
  //  TTS_PARMS = TTS_SPEAKER + speaker;
  //}

  robot->chat(text);

  server.send(200, "text/html", String(HEAD)+String("<body>")+response+String("</body>"));
}

void handle_apikey() {
  // ファイルを読み込み、クライアントに送信する
  server.send(200, "text/html", APIKEY_HTML);
}

#if 0
void handle_apikey_set() {
  // POST以外は拒否
  if (server.method() != HTTP_POST) {
    return;
  }
  // openai
  String openai = server.arg("openai");
  // voicetxt
  String voicevox = server.arg("voicevox");
  // voicetxt
  String sttapikey = server.arg("sttapikey");
 
  OPENAI_API_KEY = openai;
  VOICEVOX_API_KEY = voicevox;
  STT_API_KEY = sttapikey;
  Serial.println(openai);
  Serial.println(voicevox);
  Serial.println(sttapikey);

  uint32_t nvs_handle;
  if (ESP_OK == nvs_open("apikey", NVS_READWRITE, &nvs_handle)) {
    nvs_set_str(nvs_handle, "openai", openai.c_str());
    nvs_set_str(nvs_handle, "voicevox", voicevox.c_str());
    nvs_set_str(nvs_handle, "sttapikey", sttapikey.c_str());
    nvs_close(nvs_handle);
  }
  server.send(200, "text/plain", String("OK"));
}
#endif

void handle_role_set() {
  String html = "";

  // POST以外は拒否
  if (server.method() != HTTP_POST) {
    return;
  }
  String role = server.arg("plain");

  // JSONデータをSPIFFSに保存
  if(robot->llm->save_userRole(role)){
#if 0
    // 整形したJSONデータを出力するHTMLデータを作成する
    serializeJsonPretty(robot->llm->get_chat_doc(), html);
    html = "<html><body><pre>" + html + "</pre></body></html>";
    //Serial.println(html);
#endif
    server.send(200, "text/plain", String("Role set successful"));
  }
  else{
    //html = "Failed to save role to SPIFFS.";
    server.send(500, "text/plain", String("Role set failed"));
  }

  // HTMLデータをシリアルに出力する
  //server.send(200, "text/html", html);
};

void handle_role_get() {
#if 0
  String html = "";
  serializeJsonPretty(robot->llm->get_chat_doc(), html);
  html = "<html><body><pre>" + html + "</pre></body></html>";

  // HTMLデータをシリアルに出力する
  //Serial.println(html);
  server.send(200, "text/html", String(HEAD) + html);
#endif
  Serial.println("http request: handle_role_get");
  Serial.println(robot->llm->get_userRole());
  server.send(200, "text/plain", robot->llm->get_userRole());
};

void handle_memory_get() {
  Serial.println("http request: handle_memory_get");
  Serial.println(robot->llm->get_userInfo());
  server.send(200, "text/plain", robot->llm->get_userInfo());
};

void handle_memory_clear() {
  Serial.println("http request: handle_memory_clear");
  bool result = robot->llm->clear_userInfo();
  if(result){
    server.send(200, "text/plain", String("Memory clear successful"));
  }else{
    server.send(500, "text/plain", String("Memory clear failed"));
  }
};

static const char* BASIC_CONFIG_PATH  = "/yaml/SC_BasicConfig.yaml";
static const char* EX_CONFIG_PATH     = "/app/AiStackChanEx/SC_ExConfig.yaml";

void handle_settings_html() {
    server.send_P(200, "text/html", (const char*)settings_html, (size_t)sizeof_settings_html);
}

void handle_settings_js() {
    server.send_P(200, "application/javascript", (const char*)settings_js, (size_t)sizeof_settings_js);
}

// サーボオフセット取得
void handle_servo_offset_get() {
    File file = SD.open(BASIC_CONFIG_PATH);
    if (!file) {
        server.send(500, "application/json", "{\"x\":0,\"y\":0}");
        return;
    }
    DynamicJsonDocument doc(4096);
    auto err = deserializeYml(doc, file);
    file.close();
    if (err) {
        server.send(500, "application/json", "{\"x\":0,\"y\":0}");
        return;
    }
    int x = doc["servo"]["offset"]["x"] | 0;
    int y = doc["servo"]["offset"]["y"] | 0;
    server.send(200, "application/json",
                String("{\"x\":") + x + ",\"y\":" + y + "}");
}

// サーボオフセット保存
void handle_servo_offset_set() {
    String body = server.arg("plain");
    DynamicJsonDocument req(128);
    if (deserializeJson(req, body)) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    int newX = req["x"] | 0;
    int newY = req["y"] | 0;

    // YAML を読み込んで offset だけ書き換えて保存
    File file = SD.open(BASIC_CONFIG_PATH);
    if (!file) {
        server.send(500, "text/plain", "Failed to open config file");
        return;
    }
    DynamicJsonDocument doc(4096);
    auto err = deserializeYml(doc, file);
    file.close();
    if (err) {
        server.send(500, "text/plain", "YAML parse error");
        return;
    }
    doc["servo"]["offset"]["x"] = newX;
    doc["servo"]["offset"]["y"] = newY;

    File out = SD.open(BASIC_CONFIG_PATH, FILE_WRITE);
    if (!out) {
        server.send(500, "text/plain", "Failed to write config file");
        return;
    }
    serializeJsonPretty(doc, out);
    out.close();
    Serial.printf("[WebAPI] Servo offset saved: x=%d y=%d\n", newX, newY);
    server.send(200, "text/plain", "OK");
}

// アクティブキャラクター取得
void handle_active_character_get() {
    File file = SD.open(EX_CONFIG_PATH);
    if (!file) {
        server.send(500, "text/plain", "Failed to open ExConfig");
        return;
    }
    DynamicJsonDocument doc(8192);
    auto err = deserializeYml(doc, file);
    file.close();
    if (err) {
        server.send(500, "text/plain", "YAML parse error");
        return;
    }
    String name = doc["character"]["name"] | "";
    server.send(200, "text/plain", name);
}

// アクティブキャラクター変更
void handle_active_character_set() {
    String name = server.arg("plain");
    name.trim();
    if (name.isEmpty()) {
        server.send(400, "text/plain", "name required");
        return;
    }
    File file = SD.open(EX_CONFIG_PATH);
    if (!file) {
        server.send(500, "text/plain", "Failed to open ExConfig");
        return;
    }
    DynamicJsonDocument doc(8192);
    auto err = deserializeYml(doc, file);
    file.close();
    if (err) {
        server.send(500, "text/plain", "YAML parse error");
        return;
    }
    doc["character"]["name"] = name;
    File out = SD.open(EX_CONFIG_PATH, FILE_WRITE);
    if (!out) {
        server.send(500, "text/plain", "Failed to write ExConfig");
        return;
    }
    serializeJsonPretty(doc, out);
    out.close();
    Serial.printf("[WebAPI] Active character set: %s\n", name.c_str());
    server.send(200, "text/plain", "OK");
}

// キャラクターファイル一覧を返す（JSON配列）
void handle_characters_list() {
    String json = "[";
    File dir = SD.open("/characters");
    if (dir) {
        bool first = true;
        File entry = dir.openNextFile();
        while (entry) {
            if (!entry.isDirectory()) {
                // フルパスからファイル名だけを取り出す
                String fullPath = entry.name();
                int slashIdx = fullPath.lastIndexOf('/');
                String name = (slashIdx >= 0) ? fullPath.substring(slashIdx + 1) : fullPath;
                if (name.endsWith(".yaml")) {
                    name = name.substring(0, name.length() - 5);
                    if (!first) json += ",";
                    json += "\"" + name + "\"";
                    first = false;
                }
            }
            entry.close();
            entry = dir.openNextFile();
        }
        dir.close();
    }
    json += "]";
    server.send(200, "application/json", json);
}

// キャラクターファイルの内容を返す
void handle_character_get() {
    String name = server.arg("name");
    if (name.isEmpty()) {
        server.send(400, "text/plain", "name parameter required");
        return;
    }
    String path = "/characters/" + name + ".yaml";
    File file = SD.open(path.c_str());
    if (!file) {
        server.send(404, "text/plain", "Character not found: " + name);
        return;
    }
    String content = "";
    while (file.available()) content += (char)file.read();
    file.close();
    server.send(200, "text/plain; charset=utf-8", content);
}

// キャラクターファイルを保存する
void handle_character_set() {
    String name = server.arg("name");
    if (name.isEmpty()) {
        server.send(400, "text/plain", "name parameter required");
        return;
    }
    String path = "/characters/" + name + ".yaml";
    String content = server.arg("plain");
    File file = SD.open(path.c_str(), FILE_WRITE);
    if (!file) {
        server.send(500, "text/plain", "Failed to open file for writing");
        return;
    }
    file.print(content);
    file.close();
    Serial.printf("[WebAPI] Character saved: %s\n", path.c_str());
    server.send(200, "text/plain", "OK");
}

void handle_sleep() {
    String action = server.arg("action");

    if (!g_ai_stackchan_mod) {
        server.send(503, "application/json", "{\"error\":\"AiStackChanMod not initialized\"}");
        return;
    }

    if (action == "sleep") {
        g_ai_stackchan_mod->requestManualSleep();
        server.send(200, "application/json", "{\"status\":\"OK\",\"action\":\"sleep\"}");
    }
    else if (action == "wakeup") {
        g_ai_stackchan_mod->requestManualWakeup();
        server.send(200, "application/json", "{\"status\":\"OK\",\"action\":\"wakeup\"}");
    }
    else {
        server.send(400, "application/json", "{\"error\":\"invalid action\"}");
    }
}

void handle_face() {
  String expression = server.arg("expression");
  expression = expression + "\n";
  Serial.println(expression);
  switch (expression.toInt())
  {
    case 0: avatar.setExpression(Expression::Neutral); break;
    case 1: avatar.setExpression(Expression::Happy); break;
    case 2: avatar.setExpression(Expression::Sleepy); break;
    case 3: avatar.setExpression(Expression::Doubt); break;
    case 4: avatar.setExpression(Expression::Sad); break;
    case 5: avatar.setExpression(Expression::Angry); break;  
  } 
  server.send(200, "text/plain", String("OK"));
}

// デバイス状態を返す（WiFi接続状態・IP・ヒープ）
void handle_status() {
    String json = "{";
    json += "\"wifi\":" + String(WiFi.isConnected() ? "true" : "false") + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"minHeap\":" + String(ESP.getMinFreeHeap());
    json += "}";
    server.send(200, "application/json", json);
}

// AI モード取得（GET /mode）
// 現在の AI モードを JSON で返す。settings.html の初期表示に使用
void handle_mode_get() {
    if (!g_ai_stackchan_mod) {
        server.send(503, "application/json", "{\"error\":\"not initialized\"}");
        return;
    }
    String mode = g_ai_stackchan_mod->getClaudeCodeMode() ? "claude_code" : "chatgpt";
    server.send(200, "application/json", "{\"mode\":\"" + mode + "\"}");
}

// AI モード切り替え（POST /mode）
// body: {"mode":"claude_code"} または {"mode":"chatgpt"}
// 設定は NVS に保存されるため再起動後も維持される
void handle_mode_set() {
    if (server.method() != HTTP_POST) return;
    if (!g_ai_stackchan_mod) {
        server.send(503, "application/json", "{\"error\":\"not initialized\"}");
        return;
    }
    String body = server.arg("plain");
    DynamicJsonDocument doc(128);
    if (deserializeJson(doc, body)) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    String mode = doc["mode"] | "";
    if (mode == "claude_code") {
        g_ai_stackchan_mod->setClaudeCodeMode(true);
        server.send(200, "application/json", "{\"mode\":\"claude_code\"}");
    } else if (mode == "chatgpt") {
        g_ai_stackchan_mod->setClaudeCodeMode(false);
        server.send(200, "application/json", "{\"mode\":\"chatgpt\"}");
    } else {
        server.send(400, "application/json", "{\"error\":\"invalid mode\"}");
    }
}

// Claude Code 連携：保留コマンドを返す（GET /pending_command）
// polling.ps1 が 500ms ごとにポーリングするエンドポイント。
// コマンドがあれば command_id / text / type / system_prompt を返す。
// コマンドがなければ {"command_id":null} を返す
void handle_pending_command() {
    if (!g_ai_stackchan_mod) {
        server.send(503, "application/json", "{\"error\":\"not initialized\"}");
        return;
    }
    server.send(200, "application/json", g_ai_stackchan_mod->getPendingCommandJson());
}

// Claude Code 連携：Claude の返答を受け取り TTS で読み上げる（POST /command_result）
// body: {"command_id":"...", "voice_text":"読み上げるテキスト"}
// 読み上げ後にビジーフラグを解除し、次のコマンドを受け付け可能にする
void handle_command_result() {
    if (server.method() != HTTP_POST) return;
    if (!g_ai_stackchan_mod) {
        server.send(503, "application/json", "{\"error\":\"not initialized\"}");
        return;
    }
    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, body)) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    String voice_text = doc["voice_text"] | "";
    g_ai_stackchan_mod->receiveCommandResult(voice_text);
    server.send(200, "application/json", "{\"status\":\"OK\"}");
}

// デバイスを再起動する
void handle_reboot() {
    server.send(200, "text/plain", "Rebooting...");
    delay(200);
    ESP.restart();
}

#if 0
void handle_setting() {
  String value = server.arg("volume");
  String led = server.arg("led");
  String speaker = server.arg("speaker");
//  volume = volume + "\n";
  Serial.println(speaker);
  Serial.println(value);
  size_t speaker_no;

  if(speaker != ""){
    speaker_no = speaker.toInt();
    if(speaker_no > 60) {
      speaker_no = 60;
    }
    TTS_SPEAKER_NO = String(speaker_no);
    TTS_PARMS = TTS_SPEAKER + TTS_SPEAKER_NO;
  }

  if(value == "") value = "180";
  size_t volume = value.toInt();
  uint8_t led_onoff = 0;
  uint32_t nvs_handle;
  if (ESP_OK == nvs_open("setting", NVS_READWRITE, &nvs_handle)) {
    if(volume > 255) volume = 255;
    nvs_set_u32(nvs_handle, "volume", volume);
    if(led != "") {
      if(led == "on") led_onoff = 1;
      else  led_onoff = 0;
      nvs_set_u8(nvs_handle, "led", led_onoff);
    }
    nvs_set_u8(nvs_handle, "speaker", speaker_no);

    nvs_close(nvs_handle);
  }
  M5.Speaker.setVolume(volume);
  M5.Speaker.setChannelVolume(m5spk_virtual_channel, volume);
  server.send(200, "text/plain", String("OK"));
}
#endif


void init_web_server(void)
{
  // Files
  //
  server.on("/", handleRoot);
  server.on("/personalize.html", handle_personalize_html);
  server.on("/personalize.js", handle_personalize_js);
  server.on("/settings.html", handle_settings_html);
  server.on("/status", handle_status);
  server.on("/reboot", HTTP_POST, handle_reboot);
  server.on("/settings.js", handle_settings_js);


  // APIs
  //
  server.on("/speech", handle_speech);
  server.on("/face", handle_face);
  server.on("/chat", handle_chat);
  server.on("/apikey", handle_apikey);
  //server.on("/setting", handle_setting);
  //server.on("/apikey_set", HTTP_POST, handle_apikey_set);
  server.on("/role_set", HTTP_POST, handle_role_set);
  server.on("/role_get", handle_role_get);
  server.on("/memory_get", handle_memory_get);
  server.on("/memory_clear", handle_memory_clear);
  server.on("/servo_offset", HTTP_GET, handle_servo_offset_get);
  server.on("/servo_offset", HTTP_POST, handle_servo_offset_set);
  server.on("/active_character", HTTP_GET, handle_active_character_get);
  server.on("/active_character", HTTP_POST, handle_active_character_set);
  server.on("/characters", handle_characters_list);
  server.on("/character_get", handle_character_get);
  server.on("/character_set", HTTP_POST, handle_character_set);
  server.on("/sleep", HTTP_POST, handle_sleep);
  server.on("/mode", HTTP_GET, handle_mode_get);
  server.on("/mode", HTTP_POST, handle_mode_set);
  server.on("/pending_command", HTTP_GET, handle_pending_command);  // 後方互換（polling.ps1向け）
  server.on("/command_result", HTTP_POST, handle_command_result);
  server.on("/webhook_url", HTTP_GET,  []() {
    if (!g_ai_stackchan_mod) { server.send(503, "application/json", "{\"error\":\"not initialized\"}"); return; }
    server.send(200, "application/json", "{\"url\":\"" + g_ai_stackchan_mod->getWebhookUrl() + "\"}");
  });
  server.on("/webhook_url", HTTP_POST, []() {
    if (!g_ai_stackchan_mod) { server.send(503, "application/json", "{\"error\":\"not initialized\"}"); return; }
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, server.arg("plain"))) { server.send(400, "text/plain", "Invalid JSON"); return; }
    String url = doc["url"] | "";
    if (url.isEmpty()) { server.send(400, "text/plain", "url required"); return; }
    g_ai_stackchan_mod->saveWebhookUrl(url);
    server.send(200, "application/json", "{\"status\":\"OK\"}");
  });

  // Other
  //
  server.onNotFound(handleNotFound);
  server.on("/inline", [](){
    server.send(200, "text/plain", "this works as well");
  });

  server.begin();
  Serial.println("HTTP server started");
  M5.Lcd.println("HTTP server started");  
}

void web_server_handle_client(void)
{
  server.handleClient();
}
