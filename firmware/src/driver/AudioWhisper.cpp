#include <M5Unified.h>
#include "AudioWhisper.h"

//constexpr size_t record_number = 300/2;
//constexpr size_t record_number = 400;
//constexpr size_t record_number = 200;
constexpr size_t record_number = 600;   // dma_buf_len=1024時は約9.6秒（600 × 16ms）
constexpr size_t record_length = 150;
constexpr size_t record_size = record_number * record_length;
constexpr size_t record_samplerate = 16000;
constexpr int headerSize = 44;

AudioWhisper::AudioWhisper() {
  const auto size = record_size * sizeof(int16_t) + headerSize;
  record_buffer = static_cast<byte*>(::heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  ::memset(record_buffer, 0, size);

  // VAD用ダブルバッファをDMA RAMに永続確保
  chunk_buf = (int16_t*)heap_caps_malloc(
      record_length * sizeof(int16_t), MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (!chunk_buf) {
    Serial.println("[VAD] chunk_buf DMA確保失敗 - PSRAMにフォールバック");
    chunk_buf = (int16_t*)heap_caps_malloc(
        record_length * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
}

AudioWhisper::~AudioWhisper() {
  if (record_buffer) heap_caps_free(record_buffer);
  if (chunk_buf)     heap_caps_free(chunk_buf);
}


int16_t* MakeHeader(byte* header) {
  const auto wavDataSize = record_number * record_length * 2;
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int fileSizeMinus8 = wavDataSize + headerSize - 8;
  header[4] = (byte)(fileSizeMinus8 & 0xFF);
  header[5] = (byte)((fileSizeMinus8 >> 8) & 0xFF);
  header[6] = (byte)((fileSizeMinus8 >> 16) & 0xFF);
  header[7] = (byte)((fileSizeMinus8 >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10;  // linear PCM
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;
  header[20] = 0x01;  // linear PCM
  header[21] = 0x00;
  header[22] = 0x01;  // monoral
  header[23] = 0x00;
  header[24] = 0x80;  // sampling rate 16000
  header[25] = 0x3E;
  header[26] = 0x00;
  header[27] = 0x00;
  header[28] = 0x00;  // Byte/sec = 16000x2x1 = 32000
  header[29] = 0x7D;
  header[30] = 0x00;
  header[31] = 0x00;
  header[32] = 0x02;  // 16bit monoral
  header[33] = 0x00;
  header[34] = 0x10;  // 16bit
  header[35] = 0x00;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(wavDataSize & 0xFF);
  header[41] = (byte)((wavDataSize >> 8) & 0xFF);
  header[42] = (byte)((wavDataSize >> 16) & 0xFF);
  header[43] = (byte)((wavDataSize >> 24) & 0xFF);
  return (int16_t*)&header[headerSize];
}

// 無音検出パラメータ
static constexpr int MIN_SPEECH_CHUNKS = 20;  // 最低録音チャンク数（誤検知防止）
static constexpr int SILENCE_CHUNKS    = 59;   // 無音が続くチャンク数で終了（約0.94秒・dma_buf_len=1024時）
static constexpr int THRESHOLD_FACTOR  = 3;   // 環境ノイズ × この係数を閾値にする
static constexpr int THRESHOLD_MIN     = 50;
static constexpr int THRESHOLD_MAX     = 2000;

static int16_t s_silenceThreshold = 200;  // 適応的に更新される閾値

void AudioWhisper::Record() {
  M5.Mic.end();    // 前回の begin() が残っている場合に備えてリセット
  M5.Mic.begin();
  auto *wavData = MakeHeader(record_buffer);

  bool    speechStarted    = false;
  int     silentChunks     = 0;
  int     actualChunks     = 0;
  long    preSpeechAmpSum  = 0;
  int     preSpeechChunks  = 0;
  long    postSpeechAmpSum = 0;
  int     postSpeechChunks = 0;
  int16_t peakAmp          = 0;

  if (!chunk_buf) {
    Serial.println("[VAD] chunk_buf が未確保のため録音スキップ");
    M5.Mic.end();
    return;
  }

  for (int i = 0; i < (int)record_number; ++i) {
    // DMA RAM に録音（キャッシュ問題なし）
    M5.Mic.record(chunk_buf, record_length, record_samplerate);

    // PSRAM のメインバッファへコピー
    int16_t* dest = &wavData[i * record_length];
    memcpy(dest, chunk_buf, record_length * sizeof(int16_t));
    actualChunks = i + 1;

    // デバッグ: 50チャンクごとにヒープ整合性・状態を確認
    if (actualChunks % 50 == 0) {
      bool heapOk = heap_caps_check_integrity(MALLOC_CAP_DEFAULT, false);
      Serial.printf("[DBG] chunk=%d heap=%u dmaFree=%u heapOk=%d buf[0]=%d\n",
                    actualChunks,
                    ESP.getFreeHeap(),
                    heap_caps_get_free_size(MALLOC_CAP_DMA),
                    heapOk,
                    chunk_buf[0]);
      if (!heapOk) {
        Serial.println("[DBG] !!! ヒープ破壊を検出 !!!");
      }
    }

    // DMA RAM から振幅計算（キャッシュ問題なし）
    int16_t maxAmp = 0;
    for (int j = 0; j < (int)record_length; j++) {
      int16_t v = chunk_buf[j] < 0 ? -chunk_buf[j] : chunk_buf[j];
      if (v > maxAmp) maxAmp = v;
    }
    if (maxAmp > peakAmp) peakAmp = maxAmp;

    if (!speechStarted) {
      if (maxAmp > s_silenceThreshold) {
        speechStarted = true;
        silentChunks  = 0;
      } else {
        preSpeechAmpSum += maxAmp;
        preSpeechChunks++;
      }
    } else {
      if (maxAmp <= s_silenceThreshold) {
        silentChunks++;
        postSpeechAmpSum += maxAmp;
        postSpeechChunks++;
      } else {
        silentChunks = 0;
        postSpeechAmpSum = 0;
        postSpeechChunks = 0;
      }
    }

    if (speechStarted && silentChunks >= SILENCE_CHUNKS && actualChunks >= MIN_SPEECH_CHUNKS) {
      Serial.printf("[VAD] 無音検出で録音終了: %d チャンク (peak=%d)\n", actualChunks, peakAmp);
      break;
    }
  }

  M5.Mic.end();

  Serial.printf("[VAD] 録音完了: %d チャンク 最大振幅=%d\n", actualChunks, peakAmp);
  Serial.printf("[MEM] heap=%u minHeap=%u dmaFree=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                heap_caps_get_free_size(MALLOC_CAP_DMA));

  if (!speechStarted) {
    // 発話未検出: 閾値を下げて次回の検出感度を上げる
    int newThreshold = s_silenceThreshold / 2;
    if (newThreshold < THRESHOLD_MIN) newThreshold = THRESHOLD_MIN;
    s_silenceThreshold = (int16_t)newThreshold;
    Serial.printf("[VAD] 発話未検出 → 閾値を下げます: %d\n", s_silenceThreshold);
  } else {
    // 発話検出: 環境ノイズから閾値を更新
    // 発話前の無音 → 発話後の無音 の順で優先使用
    int ambientChunks = (preSpeechChunks > 0) ? preSpeechChunks : postSpeechChunks;
    long  ambientSum  = (preSpeechChunks > 0) ? preSpeechAmpSum : postSpeechAmpSum;
    const char* src   = (preSpeechChunks > 0) ? "発話前" : "発話後";

    if (ambientChunks > 0) {
      int avgAmbient = (int)(ambientSum / ambientChunks);
      int newThreshold = avgAmbient * THRESHOLD_FACTOR;
      if (newThreshold < THRESHOLD_MIN) newThreshold = THRESHOLD_MIN;
      if (newThreshold > THRESHOLD_MAX) newThreshold = THRESHOLD_MAX;
      s_silenceThreshold = (int16_t)newThreshold;
      Serial.printf("[VAD] 閾値更新(%s): ambient=%d → threshold=%d\n", src, avgAmbient, s_silenceThreshold);
    }
  }

  // WAVヘッダを実際の録音サイズで更新
  const int wavDataSize = actualChunks * record_length * 2;
  byte* header = record_buffer;
  unsigned int fileSizeMinus8 = wavDataSize + headerSize - 8;
  header[4] = (byte)(fileSizeMinus8 & 0xFF);
  header[5] = (byte)((fileSizeMinus8 >> 8) & 0xFF);
  header[6] = (byte)((fileSizeMinus8 >> 16) & 0xFF);
  header[7] = (byte)((fileSizeMinus8 >> 24) & 0xFF);
  header[40] = (byte)(wavDataSize & 0xFF);
  header[41] = (byte)((wavDataSize >> 8) & 0xFF);
  header[42] = (byte)((wavDataSize >> 16) & 0xFF);
  header[43] = (byte)((wavDataSize >> 24) & 0xFF);

  _actualSize = wavDataSize + headerSize;
}
