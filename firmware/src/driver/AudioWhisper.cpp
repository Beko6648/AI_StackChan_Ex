#include <M5Unified.h>
#include "AudioWhisper.h"
#include "esp32s3/rom/cache.h"

//constexpr size_t record_number = 300/2;
constexpr size_t record_number = 400;
//constexpr size_t record_number = 200;
constexpr size_t record_length = 150;
constexpr size_t record_size = record_number * record_length;
constexpr size_t record_samplerate = 16000;
constexpr int headerSize = 44;

AudioWhisper::AudioWhisper() {
  const auto size = record_size * sizeof(int16_t) + headerSize;
  record_buffer = static_cast<byte*>(::heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  ::memset(record_buffer, 0, size);
}

AudioWhisper::~AudioWhisper() {
  delete record_buffer;
}

size_t AudioWhisper::GetSize() const {
  return actualSize > 0 ? actualSize : record_size * sizeof(int16_t) + headerSize;
}

static float calcRMS(const int16_t* data, int length);

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

// キャッシュ無効化後に RMS を計算（DMA 書き込み後に CPU で正しく読むため）
static float calcRMSWithCacheInvalidate(int16_t* data, int length) {
  Cache_Invalidate_DCache_Items((uint32_t)data, length * sizeof(int16_t));
  return calcRMS(data, length);
}

void AudioWhisper::Record() {
  M5.Mic.begin();

  // VAD パラメータ
  static const int   CALIB_CHUNKS          = 5;
  static const float SPEECH_THRESHOLD_MULT = 3.0f;
  static const int   SILENCE_HOLD_CHUNKS   = 15;  // 約550ms
  static const int   MIN_SPEECH_CHUNKS     = 8;

  auto *wavData = MakeHeader(record_buffer);

  // フェーズ1: ノイズフロアキャリブレーション
  float noiseFloor = 0;
  for (int i = 0; i < CALIB_CHUNKS; i++) {
    auto data = &wavData[i * record_length];
    M5.Mic.record(data, record_length, record_samplerate);
    noiseFloor = max(noiseFloor, calcRMSWithCacheInvalidate(data, record_length));
  }
  noiseFloor      = max(noiseFloor, 100.0f);
  float threshold = noiseFloor * SPEECH_THRESHOLD_MULT;

  Serial.printf("[Audio] noiseFloor=%.1f threshold=%.1f\n", noiseFloor, threshold);

  // フェーズ2〜4: 発話録音（リアルタイム VAD）
  int  silence_count  = 0;
  bool speech_started = false;
  int  actual_chunks  = CALIB_CHUNKS;

  for (int i = CALIB_CHUNKS; i < (int)record_number; i++) {
    auto data = &wavData[i * record_length];
    M5.Mic.record(data, record_length, record_samplerate);
    actual_chunks = i + 1;

    float rms = calcRMSWithCacheInvalidate(data, record_length);

    if (rms > threshold) {
      speech_started = true;
      silence_count  = 0;
    } else if (speech_started) {
      silence_count++;
      if (silence_count >= SILENCE_HOLD_CHUNKS && actual_chunks >= MIN_SPEECH_CHUNKS) {
        Serial.printf("[Audio] VAD: silence detected. chunks=%d\n", actual_chunks);
        break;
      }
    }
  }

  M5.Mic.end();

  // 実際の録音サイズで WAV ヘッダーを書き直す
  const int actualWavDataSize = actual_chunks * (int)record_length * 2;
  MakeHeader(record_buffer);  // まず全体サイズで作成
  // ヘッダのデータサイズフィールドだけ上書き
  record_buffer[40] = (byte)(actualWavDataSize & 0xFF);
  record_buffer[41] = (byte)((actualWavDataSize >> 8) & 0xFF);
  record_buffer[42] = (byte)((actualWavDataSize >> 16) & 0xFF);
  record_buffer[43] = (byte)((actualWavDataSize >> 24) & 0xFF);
  unsigned int fileSizeMinus8 = actualWavDataSize + headerSize - 8;
  record_buffer[4] = (byte)(fileSizeMinus8 & 0xFF);
  record_buffer[5] = (byte)((fileSizeMinus8 >> 8) & 0xFF);
  record_buffer[6] = (byte)((fileSizeMinus8 >> 16) & 0xFF);
  record_buffer[7] = (byte)((fileSizeMinus8 >> 24) & 0xFF);

  actualSize = actualWavDataSize + headerSize;

  Serial.printf("[Audio] recorded %d/%d chunks (%.2fs)\n",
                actual_chunks, (int)record_number,
                (float)actual_chunks * record_length / record_samplerate);
}

static float calcRMS(const int16_t* data, int length) {
  float sum = 0;
  for (int i = 0; i < length; i++) {
    float s = data[i];
    sum += s * s;
  }
  return sqrtf(sum / length);
}
