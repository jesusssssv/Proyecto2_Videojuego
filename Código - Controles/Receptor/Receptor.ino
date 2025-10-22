/*********
  RECEPTOR - Envía comandos por UART2 a STM32 + Sistema de música multinivel COD-style
*********/
#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>
#include "pitches.h"

// ======================= CONFIG AUDIO =======================
#define PIN_BUZZER 23
#define LEDC_RESOLUTION 8
// ============================================================

// ------------------ UART2 STM32 -----------------------------
#define RXD2 16
#define TXD2 17
// ------------------------------------------------------------

// --- Comandos de música que envía la Nucleo por UART ---
#define AUD_CMD_INTRO  "MUSIC_INTRO"
#define AUD_CMD_MAP1   "MUSIC_MAP1"
#define AUD_CMD_MAP2   "MUSIC_MAP2"
#define AUD_CMD_STOP   "MUSIC_STOP"

// ====== Estructuras ESP-NOW ===========
typedef struct struct_message {
  int id;
  bool actionA;
  bool actionB;
  bool left;
  bool down;
  bool up;
  bool right;
} struct_message;

struct_message myData;
struct_message board1;
struct_message board2;
struct_message boardsStruct[2] = {board1, board2};
struct_message lastBoard1;
struct_message lastBoard2;
// ============================================================

// ================== MÚSICA: ESTRUCTURA Y DATOS =======================
typedef struct {
  int freq;
  int durMs;
} NoteEvent;

// ========== ESTADOS MUSICALES ==========
enum MusicState {
  MUSIC_MENU,
  MUSIC_LEVEL1,
  MUSIC_LEVEL2
};

MusicState currentMusicState = MUSIC_MENU;
MusicState requestedMusicState = MUSIC_MENU;

float TEMPO_SCALE = 1.0;
int TRANSPOSITION = -2;
bool musicEnabled = true;
unsigned long evStartMs = 0;
int evIndex = 0;
bool toneActive = false;

float semitoneFactor(int s) {
  return powf(2.0f, s / 12.0f);
}

// ========== PARTITURAS (las tuyas, sin cambios) ==========
NoteEvent COD_Menu[] = { /* ... (igual que tu código) ... */ };
int COD_MENU_LEN = sizeof(COD_Menu) / sizeof(COD_Menu[0]);

NoteEvent COD_Level1[] = { /* ... (igual que tu código) ... */ };
int COD_LEVEL1_LEN = sizeof(COD_Level1) / sizeof(COD_Level1[0]);

NoteEvent COD_Level2[] = { /* ... (igual que tu código) ... */ };
int COD_LEVEL2_LEN = sizeof(COD_Level2) / sizeof(COD_Level2[0]);

// ========== FUNCIONES DE AUDIO (LEDC DIRECTO) ==========
void safeTone(int freq) {
  if (freq > 0) {
    ledcWriteTone(PIN_BUZZER, freq);
    toneActive = true;
  } else {
    ledcWriteTone(PIN_BUZZER, 0);
    toneActive = false;
  }
}

void safeNoTone() {
  ledcWriteTone(PIN_BUZZER, 0);
  toneActive = false;
}

void audioStartEvent(NoteEvent ev) {
  int dur = (int)(ev.durMs * TEMPO_SCALE);
  if (dur <= 0) dur = 1;

  if (ev.freq > 0) {
    int freq = (int)(ev.freq * semitoneFactor(TRANSPOSITION) + 0.5f);
    safeTone(freq);
  } else {
    safeNoTone();
  }
  evStartMs = millis();
}

// ========== OBTENER PARTITURA Y LONGITUD SEGÚN ESTADO ==========
NoteEvent* getCurrentScore(int* outLen) {
  switch(currentMusicState) {
    case MUSIC_LEVEL1:
      *outLen = COD_LEVEL1_LEN;
      return COD_Level1;
    case MUSIC_LEVEL2:
      *outLen = COD_LEVEL2_LEN;
      return COD_Level2;
    default: // MUSIC_MENU
      *outLen = COD_MENU_LEN;
      return COD_Menu;
  }
}

void audioAdvanceIfNeeded() {
  // Cambio de música ordenado por la Nucleo
  if (requestedMusicState != currentMusicState) {
    safeNoTone();
    currentMusicState = requestedMusicState;
    evIndex = 0;
    int len;
    NoteEvent* score = getCurrentScore(&len);
    audioStartEvent(score[evIndex]);
    Serial.print("🎵 Cambio de música a estado: ");
    Serial.println(currentMusicState);
    return;
  }

  if (!musicEnabled) {
    if (toneActive) {
      safeNoTone();
    }
    evIndex = 0;
    return;
  }

  int scoreLen;
  NoteEvent* score = getCurrentScore(&scoreLen);
  NoteEvent ev = score[evIndex];

  int dur = (int)(ev.durMs * TEMPO_SCALE);
  if (dur <= 0) dur = 1;

  unsigned long now = millis();
  if ((now - evStartMs) >= (unsigned long)dur) {
    if (ev.freq > 0 && toneActive) {
      safeNoTone();
    }
    evIndex++;
    if (evIndex >= scoreLen) evIndex = 0;
    audioStartEvent(score[evIndex]);
  }
}

void audioInit() {
  // OJO: si tu core usa ledcSetup/ledcAttachPin, usa eso.
  ledcAttach(PIN_BUZZER, 5000, LEDC_RESOLUTION);
  evIndex = 0;
  toneActive = false;
  int len;
  NoteEvent* score = getCurrentScore(&len);
  audioStartEvent(score[evIndex]);
}

// ========== TAREA FREERTOS PARA MÚSICA ==========
void taskMusic(void *param) {
  audioInit();
  for (;;) {
    audioAdvanceIfNeeded();
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}
// ================== FIN AUDIO =======================

// ==================== UART helper STM32 =======================
void sendCommandToSTM32(int playerID, String command) {
  String fullCommand = "P" + String(playerID) + ":" + command + "\n";
  Serial2.print(fullCommand);
  Serial.print("Enviado a STM32: ");
  Serial.println(fullCommand);
}

// ==================== ESP-NOW helpers =======================
bool buttonsChanged(struct_message current, struct_message last) {
  return (current.actionA != last.actionA ||
          current.actionB != last.actionB ||
          current.left != last.left ||
          current.down != last.down ||
          current.up != last.up ||
          current.right != last.right);
}

// --- Procesa comandos de audio que vienen de la Nucleo ---
void handleNucleoAudioCommand(const String& cmdRaw) {
  String cmd = cmdRaw;
  cmd.trim();

  if (cmd.equalsIgnoreCase(AUD_CMD_INTRO)) {
    musicEnabled = true;
    requestedMusicState = MUSIC_MENU;
    Serial.println("🎵 CMD RX: INTRO -> MUSIC_MENU");
  } else if (cmd.equalsIgnoreCase(AUD_CMD_MAP1)) {
    musicEnabled = true;
    requestedMusicState = MUSIC_LEVEL1;
    Serial.println("🎵 CMD RX: MAP1 -> MUSIC_LEVEL1");
  } else if (cmd.equalsIgnoreCase(AUD_CMD_MAP2)) {
    musicEnabled = true;
    requestedMusicState = MUSIC_LEVEL2;
    Serial.println("🎵 CMD RX: MAP2 -> MUSIC_LEVEL2");
  } else if (cmd.equalsIgnoreCase(AUD_CMD_STOP)) {
    musicEnabled = false;
    safeNoTone();
    evIndex = 0;
    Serial.println("🎵 CMD RX: STOP -> silencio");
  } else {
    Serial.print("STM32 dice: ");
    Serial.println(cmd);
  }
}

void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  char macStr[18];
  Serial.print("📡 Paquete recibido de: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);

  memcpy(&myData, incomingData, sizeof(myData));
  Serial.printf("Emisor ID %d: %u bytes\n", myData.id, len);

  int boardIndex = myData.id - 1;

  bool hasChanged = false;
  if (myData.id == 1) {
    hasChanged = buttonsChanged(myData, lastBoard1);
    lastBoard1 = myData;
  } else if (myData.id == 2) {
    hasChanged = buttonsChanged(myData, lastBoard2);
    lastBoard2 = myData;
  }

  boardsStruct[boardIndex] = myData;

  if (hasChanged) {
    // >>> SOLO enviamos a STM32. La música ahora la decide la Nucleo <<<
    if (myData.actionA) {
      sendCommandToSTM32(myData.id, "ACT_A");
    }
    else if (myData.actionB) {
      sendCommandToSTM32(myData.id, "ACT_B");
    }
    else if (myData.up || myData.down || myData.left || myData.right) {
      if (myData.up)         sendCommandToSTM32(myData.id, "UP");
      else if (myData.down)  sendCommandToSTM32(myData.id, "DOWN");
      else if (myData.left)  sendCommandToSTM32(myData.id, "LEFT");
      else if (myData.right) sendCommandToSTM32(myData.id, "RIGHT");
    }
    else {
      sendCommandToSTM32(myData.id, "STOP");
    }
  }

  Serial.println("--- Estado de Botones ---");
  Serial.printf("  Acción A: %s\n", myData.actionA ? "PRESIONADO" : "suelto");
  Serial.printf("  Acción B: %s\n", myData.actionB ? "PRESIONADO" : "suelto");
  Serial.printf("  Izquierda: %s\n", myData.left ? "PRESIONADO" : "suelto");
  Serial.printf("  Abajo: %s\n", myData.down ? "PRESIONADO" : "suelto");
  Serial.printf("  Arriba: %s\n", myData.up ? "PRESIONADO" : "suelto");
  Serial.printf("  Derecha: %s\n", myData.right ? "PRESIONADO" : "suelto");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.println("UART2 inicializado (TX=GPIO17, RX=GPIO16)");

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  Serial.println("Receptor iniciado - Esperando datos...");
  Serial2.println("READY");
  Serial.println("Mensaje READY enviado a STM32");

  xTaskCreatePinnedToCore(
    taskMusic,
    "taskMusic",
    4096,
    NULL,
    1,
    NULL,
    0
  );

  Serial.println("Sistema de música multinivel iniciado en Core 0");
  Serial.println("🎵 La Nucleo controla la música via UART:");
  Serial.println("   MUSIC_INTRO -> menú | MUSIC_MAP1 -> nivel1 | MUSIC_MAP2 -> nivel2 | MUSIC_STOP -> silencio");
}

void loop() {
  // Lee y procesa comandos de música provenientes de la Nucleo
  while (Serial2.available()) {
    String line = Serial2.readStringUntil('\n');
    if (line.length() == 0) continue;
    handleNucleoAudioCommand(line);
  }

  delay(5);
}
