#include <Arduino.h>
#include <deque>
#include <SD.h>
#include <SPIFFS.h>
#include "mod/ModManager.h"
#include "AiStackChanMod.h"
#include <Avatar.h>
#include "Robot.h"
#include "llm/ChatGPT/ChatGPT.h"
#include "llm/ChatGPT/FunctionCall.h"
#include "driver/PlayMP3.h"
#include "driver/WakeWord.h"
#include "driver/ModuleLLM.h"
#include <WiFiClientSecure.h>
#include "Scheduler.h"
#include "MySchedule.h"
#include "share/SDUtil.h"
#include "AckSound.h"
#include "StackChanMind.h"
#include "mood/MoodManager.h"
#include "llm/ChatHistory.h"
#include "head/IdleLookAround.h"

extern StackchanExConfig system_config;
#if defined( ENABLE_CAMERA )
#include "driver/Camera.h"
#endif
#include "driver/AudioWhisper.h"       //speechToText
#include "stt/Whisper.h"               //speechToText
#include "driver/Audio.h"              //speechToText
#include "stt/CloudSpeechClient.h"     //speechToText
#include "rootCA/rootCACertificate.h"  //speechToText
#include "rootCA/rootCAgoogle.h"       //speechToText
#include "driver/Audio.h"              //speechToText

using namespace m5avatar;

#if defined(ENABLE_WAKEWORD)
bool wakeword_is_enable = false;
#endif

/// 外部参照 ///
extern Avatar avatar;
extern bool servo_home;
//extern bool wakeword_is_enable;
extern void sw_tone();
extern void alarm_tone();
///////////////

// static 関数からコントローラ・マネージャにアクセスするためのファイルスコープポインタ
static HeadMotionController* s_headCtrl    = nullptr;
static MoodManager*          s_moodManager = nullptr;


static void report_batt_level(){
  char buff[100];
  int level = M5.Power.getBatteryLevel();
#if defined(ENABLE_WAKEWORD)
  mode = 0;
#endif
  if(M5.Power.isCharging())
    sprintf(buff,"充電中、バッテリーのレベルは%d％です。",level);
  else
    sprintf(buff,"バッテリーのレベルは%d％です。",level);
  avatar.setExpression(Expression::Happy);
#if defined(ENABLE_WAKEWORD)
  mode = 0; 
#endif
  robot->speech(String(buff));
  delay(1000);
  avatar.setExpression(Expression::Neutral);
}


static void STT_ChatGPT(const char *base64_buf = NULL) {
  bool prev_servo_home = servo_home;
#ifdef USE_SERVO
  servo_home = true;
#endif

  // 会話中は頭のきょろきょろを停止して正面を向く
  if (s_headCtrl)    s_headCtrl->stop();
  if (s_moodManager) s_moodManager->onConversation();

  avatar.setExpression(s_moodManager ? s_moodManager->getDominantExpression() : Expression::Neutral);
  avatar.setSpeechText("御用でしょうか？");

  String ret = robot->listen();
  avatar.setSpeechText("");

#ifdef USE_SERVO
  //servo_home = prev_servo_home;
  servo_home = false;
#endif
  Serial.println("音声認識終了");
  Serial.println("音声認識結果");
  if(ret != "") {
    Serial.println(ret);
    playAckSound(system_config.getExConfig());  // 現在の感情に応じた相槌を再生
    robot->chat(ret, base64_buf);
    avatar.setSpeechText("");
    servo_home = true;
  } else {
    Serial.println("音声認識失敗");
    avatar.setExpression(Expression::Sad);
    avatar.setSpeechText("聞き取れませんでした");
    delay(2000);
    avatar.setSpeechText("");
    stackChanMind.applyExpression();  // Sad 表示後、現在の感情表情に戻す
    servo_home = true;
  }

  // 会話終了後にきょろきょろを再開
  if (s_headCtrl) s_headCtrl->setMotion(new IdleLookAround());
}

// 自発発話（ユーザー入力なしで気分をもとに LLM → TTS）
static void selfTalk() {
  if (s_headCtrl) s_headCtrl->stop();

  String moodDesc = s_moodManager ? s_moodManager->getMoodDescription() : "普通の気分";
  String selfTalkPrompt = robot->llm->getSelfTalkPrompt();
  String prompt;
  if (!selfTalkPrompt.isEmpty()) {
    // キャラクターファイルの self_talk_prompt を使用
    prompt = selfTalkPrompt;
    if (chatHistory.get_size() > 0) {
      prompt += "（現在の気分: " + moodDesc + "。過去の会話の話題を自然に引用してもよい）";
    } else {
      prompt += "（現在の気分: " + moodDesc + "）";
    }
  } else {
    // フォールバック
    if (chatHistory.get_size() > 0) {
      prompt = "あなたはかわいいロボットのスタックちゃんです。気分は「" + moodDesc + "」です。過去の会話の話題を踏まえつつ、自然に話しかけてください。";
    } else {
      prompt = "あなたはかわいいロボットのスタックちゃんです。気分は「" + moodDesc + "」です。この前提で自由に話しかけてください。";
    }
  }
  Serial.println("自発発話: " + prompt);

  robot->chat(prompt);
  avatar.setSpeechText("");

  if (s_moodManager) s_moodManager->onSelfTalk();
  if (s_headCtrl)    s_headCtrl->setMotion(new IdleLookAround());
}


AiStackChanMod::AiStackChanMod(bool _isOffline)
  : isOffline{_isOffline}
{
  box_servo.setupBox(80, 120, 80, 80);
#if defined(ENABLE_CAMERA)
  box_stt.setupBox(107, 0, M5.Display.width()-107, 80);
  box_subWindow.setupBox(0, 0, 107, 80);
#else
  box_stt.setupBox(0, 0, M5.Display.width(), 60);
#endif
  box_BtnA.setupBox(0, 100, 40, 60);
  box_BtnC.setupBox(280, 100, 40, 60);

  //SDカードのMP3ファイル（アラーム用）をSPIFFSにコピーする（SDカードだと音が途切れ途切れになるため）。
  //すでにSPIFFSにファイルがあればコピーはしない。強制的にコピー（上書き）したい場合は第2引数をtrueにする。
  //String fname = String(APP_DATA_PATH) + String(FNAME_ALARM_MP3);
  //copySDFileToSPIFFS(fname.c_str(), false);

  if(!isOffline){
    //スケジューラ設定
    init_schedule();
  }


  if(robot->m_config.getExConfig().wakeword.type == WAKEWORD_TYPE_MODULE_LLM_KWS){
#if defined(USE_LLM_MODULE)
    // Nothing to initialize here
#endif
  }
  else{
#if defined(ENABLE_WAKEWORD)
    wakeword_init();
#endif
  }

  // アイドル時の頭の動きを設定
  s_headCtrl    = &_headCtrl;
  s_moodManager = &_moodManager;
  g_moodManager = &_moodManager;

  // 頭部タッチセンサー初期化
  _headTouch.begin();
  _headCtrl.setMotion(new IdleLookAround());

}


void AiStackChanMod::init(void)
{
  avatar.setSpeechText("");
#if defined(ENABLE_CAMERA)
  if(isSubWindowON){
    avatar.set_isSubWindowEnable(true);
  }
#endif
}

void AiStackChanMod::pause(void)
{
#if defined(ENABLE_CAMERA)
  if(isSubWindowON){
    avatar.set_isSubWindowEnable(false);
  }
#endif
}


void AiStackChanMod::update(int page_no)
{

}

void AiStackChanMod::btnA_pressed(void)
{
#if defined(ARDUINO_M5STACK_ATOMS3R)
  sw_tone();
  STT_ChatGPT();
#else

#if defined(ENABLE_WAKEWORD)
  if(mode >= 0){
    sw_tone();
    if(mode == 0){
      avatar.setSpeechText("ウェイクワード有効");
      mode = 1;
      wakeword_is_enable = true;
    } else {
      avatar.setSpeechText("ウェイクワード無効");
      mode = 0;
      wakeword_is_enable = false;
    }
    delay(1000);
    avatar.setSpeechText("");
  }
#endif  //ENABLE_WAKEWORD

#endif  //ARDUINO_M5STACK_ATOMS3R
}


void AiStackChanMod::btnB_longPressed(void)
{
#if defined(ENABLE_WAKEWORD)
  M5.Mic.end();
  M5.Speaker.tone(1000, 100);
  delay(500);
  M5.Speaker.tone(600, 100);
  delay(1000);
  M5.Speaker.end();
  M5.Mic.begin();
  wakeword_is_enable = false; //wakeword 無効
  mode = -1;
#ifdef USE_SERVO
    servo_home = true;
    delay(500);
#endif
  avatar.setSpeechText("ウェイクワード登録開始");
#endif
}

void AiStackChanMod::btnC_pressed(void)
{
  static bool isQrDrawing = false;
  if(!isQrDrawing){
    avatar.setSpeechText("");
    String url = String("http://") + WiFi.localIP().toString();
    avatar.updateSubWindowQrcode(url);
    avatar.set_isSubWindowEnable(true);
    isQrDrawing = true;
  }else{
    avatar.set_isSubWindowEnable(false);
    isQrDrawing = false;
  }
}

void AiStackChanMod::display_touched(int16_t x, int16_t y)
{
  if (box_stt.contain(x, y))
  {
    sw_tone();
#if defined(ENABLE_CAMERA)
    avatar.set_isSubWindowEnable(false);
    if(isSubWindowON){
      String base64;
      bool ret = camera_capture_base64(base64);
      STT_ChatGPT(base64.c_str());
    }
    else{
      STT_ChatGPT();
    }
    avatar.set_isSubWindowEnable(isSubWindowON);
#else
    STT_ChatGPT();
#endif
  }
#ifdef USE_SERVO
  if (box_servo.contain(x, y))
  {
    //servo_home = !servo_home;
    //sw_tone();
  }
#endif
  if (box_BtnA.contain(x, y))
  {
#if defined(ENABLE_CAMERA)
    isSilentMode = !isSilentMode;
    if(isSilentMode){
      avatar.setSpeechText("サイレントモード");
    }
    else{
      avatar.setSpeechText("サイレントモード解除");
    }
    delay(2000);
    avatar.setSpeechText("");
#else
    //sw_tone();
#endif
  }
  if (box_BtnC.contain(x, y))
  {
    btnC_pressed();
  }
#if defined(ENABLE_CAMERA)
  if (box_subWindow.contain(x, y))
  {
    isSubWindowON = !isSubWindowON;
    avatar.set_isSubWindowEnable(isSubWindowON);
  }
#endif //ENABLE_CAMERA

}

void AiStackChanMod::doubleTapped(float ax, float ay, float az)
{
  Serial.printf("Mod double tapped. ax=%.3f ay=%.3f az=%.3f\n", ax, ay, az);
#if defined(ARDUINO_M5STACK_ATOMS3R)
  sw_tone();
  STT_ChatGPT();
#endif
}

void AiStackChanMod::idle(void)
{

  /// Face detect ///
#if defined(ENABLE_CAMERA)
  //顔が検出されれば音声認識を開始。
  bool isFaceDetected;
  isFaceDetected = camera_capture_and_face_detect();
  if(!isSilentMode){

#if defined(ENABLE_FACE_DETECT)
    if(isFaceDetected){
      avatar.set_isSubWindowEnable(false);
      sw_tone();
      STT_ChatGPT();                              //音声認識

      // フレームバッファを読み捨てる（ｽﾀｯｸﾁｬﾝが応答した後に、過去のフレームで顔検出してしまうのを防ぐため）
      M5.In_I2C.release();
      camera_fb_t *fb = esp_camera_fb_get();
      esp_camera_fb_return(fb);
      avatar.set_isSubWindowEnable(isSubWindowON);
    }
#endif
  }
  else{
#if defined(ENABLE_FACE_DETECT)
    if(isFaceDetected){
      avatar.setExpression(Expression::Happy);
      //delay(2000);
      //avatar.setExpression(Expression::Neutral);
    }
    else{
      avatar.setExpression(Expression::Neutral);
    }
#endif
  }
#endif  //ENABLE_CAMERA

  //Wakeword
  if(robot->m_config.getExConfig().wakeword.type == WAKEWORD_TYPE_MODULE_LLM_KWS){
#if defined(USE_LLM_MODULE)
    if( check_kws_wakeup() ){
      sw_tone();
      STT_ChatGPT();
    }
#else
    Serial.println("ModuleLLM is not enabled. Please define USE_LLM_MODULE.");
    delay(1000);
#endif
  }
  else{
#if defined(ENABLE_WAKEWORD)
    if (mode == 0) { /* return; */ }
    else if (mode < 0) {
      int idx = wakeword_regist();
      if(idx >= 0){
        String text = String("ウェイクワード#") + String(idx) + String("登録終了");
        avatar.setSpeechText(text.c_str());
        delay(1000);
        avatar.setSpeechText("");
        //mode = 0;
        //wakeword_is_enable = false;
        mode = 1;
        wakeword_is_enable = true;

      }
    }
    else if (mode > 0 && wakeword_is_enable) {
      int idx = wakeword_compare();
      if( idx >= 0){
        Serial.println("wakeword_compare OK!");
        String text = String("ウェイクワード#") + String(idx);
        avatar.setSpeechText(text.c_str());
        sw_tone();
        STT_ChatGPT();
      }
    }

#if defined(ARDUINO_M5STACK_CORES3)
    // Function Callからの要求でウェイクワード有効化
    if (wakeword_enable_required)
    {
      wakeword_enable_required = false;
      btnA_pressed();
    }

    // Function Callからの要求でウェイクワード登録
    if(register_wakeword_required)
    {
      register_wakeword_required = false;
      btnB_longPressed();
    }
#endif  //defined(ARDUINO_M5STACK_CORES3)
#endif  //ENABLE_WAKEWORD
  }

  /// Alarm ///
  if(xAlarmTimer != NULL){
    TickType_t xRemainingTime;

    /* Query the period of the timer that expires. */
    xRemainingTime = xTimerGetExpiryTime( xAlarmTimer ) - xTaskGetTickCount();
    avatarText = "残り" + String(xRemainingTime / 1000) + "秒";
    avatar.setSpeechText(avatarText.c_str());
  }

  if (alarmTimerCallbacked) {
    alarmTimerCallbacked = false;
    avatar.setSpeechText("");
#if defined(ENABLE_CAMERA)
    avatar.set_isSubWindowEnable(false);
#endif    
    if(!SD.begin(GPIO_NUM_4, SPI, 25000000)) {
    //if(!SPIFFS.begin(true)){
      Serial.println("Failed to mount SD card. Use alarm tone.");
      alarm_tone();
    }
    else{
      String fname = String(APP_DATA_PATH) + String(FNAME_ALARM_MP3);
      bool result = playMP3SD(fname.c_str());
      if(!result){
        alarm_tone();
      }
    }
#if defined(ENABLE_CAMERA)
    avatar.set_isSubWindowEnable(isSubWindowON);
#endif  
  }

  //スケジューラ処理
  if(!isOffline){
    run_schedule();
  }

  // 頭の動き更新
  _headCtrl.update();

  // 気分パラメータ更新
  _moodManager.update();

  // あくびトリガー（Sleepy になった瞬間に一度だけ）
  if (_moodManager.getSleepiness() >= MoodManager::SLEEPINESS_THRESHOLD && !_yawnPlayed) {
    _yawnPlayed = true;
    playYawnSound(system_config.getExConfig());
  }

  // 睡眠状態の処理
  if (_moodManager.isSleeping()) {
    if (!_sleepSoundPlayed) {
      _sleepSoundPlayed = true;
      playSleepSound(system_config.getExConfig());
    }
    avatar.setExpression(Expression::Sleeping);
    _headCtrl.stop();
    servo_home = true;

    // 起床: HeadTouch のいずれかのジェスチャーで復帰
    HeadTouch::Gesture g = _headTouch.update();
    if (g == HeadTouch::Gesture::LongPress ||
        g == HeadTouch::Gesture::SwipeForward ||
        g == HeadTouch::Gesture::SwipeBackward) {
      Serial.println("[Sleep] Wake up");
      playWakeupSound(system_config.getExConfig());
      _moodManager.onWakeUp();
      _yawnPlayed       = false;
      _sleepSoundPlayed = false;
      servo_home = false;
      _headCtrl.setMotion(new IdleLookAround());
    }
    return;
  }

  // 気分ベースの表情をアイドル時に適用（撫で中は上書きしない）
  if (millis() >= _petExpressionUntilMs) {
    avatar.setExpression(_moodManager.getDominantExpression());
  }

  // 自発発話チェック
  if (_moodManager.shouldSpeak()) {
    selfTalk();
  }

  // 頭部タッチセンサー処理
  switch (_headTouch.update()) {
    case HeadTouch::Gesture::LongPress:
      STT_ChatGPT();
      break;
    case HeadTouch::Gesture::SwipeForward:
    case HeadTouch::Gesture::SwipeBackward:
      _moodManager.addJoy(0.1f);
      _moodManager.addTrust(0.05f);
      {
        Expression current = _moodManager.getDominantExpression();
        bool isNegative = (current == Expression::Sad || current == Expression::Angry);
        Expression petExpr = isNegative ? Expression::Neutral : Expression::Happy;
        avatar.setExpression(petExpr);
        _petExpressionUntilMs = millis() + PET_EXPRESSION_DURATION_MS;
      }
      Serial.printf("[HeadTouch] Swipe joy=%.2f trust=%.2f\n", _moodManager.getJoy(), _moodManager.getTrust());
      break;
    default:
      break;
  }

}


