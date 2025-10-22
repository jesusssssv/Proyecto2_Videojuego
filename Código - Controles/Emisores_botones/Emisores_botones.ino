/*********
  Código actualizado para ESP32 v3.x - EMISOR CON BOTONES
*********/
#include <esp_now.h>
#include <WiFi.h>

// REEMPLAZAR CON LA DIRECCIÓN MAC DEL RECEPTOR
uint8_t broadcastAddress[] = {0x58, 0xBF, 0x25, 0xB0, 0xB1, 0xF4};

// Definir pines de los botones
#define BTN_ACTION_A  23
#define BTN_ACTION_B  22
#define BTN_LEFT      21
#define BTN_DOWN      19
#define BTN_UP        18
#define BTN_RIGHT     5

// Estructura para enviar datos
// Debe coincidir con la estructura del receptor
typedef struct struct_message {
    int id;           // ID único del emisor (1 o 2)
    bool actionA;     // Estado botón Acción A
    bool actionB;     // Estado botón Acción B
    bool left;        // Estado botón Izquierda
    bool down;        // Estado botón Abajo
    bool up;          // Estado botón Arriba
    bool right;       // Estado botón Derecha
} struct_message;

// Crear una struct_message llamada myData
struct_message myData;

// Crear interfaz de par (peer)
esp_now_peer_info_t peerInfo;

// función callback cuando se envían los datos (ACTUALIZADA para v3.x)
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("\r\nEstado del Último Paquete Enviado:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Entrega Exitosa" : "Fallo en Entrega");
}
 
void setup() {
  // Inicializar Monitor Serial
  Serial.begin(115200);

  // Configurar pines de botones como INPUT_PULLUP
  // (Conecta los botones entre el pin y GND)
  pinMode(BTN_ACTION_A, INPUT_PULLUP);
  pinMode(BTN_ACTION_B, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  // Configurar dispositivo como Estación Wi-Fi
  WiFi.mode(WIFI_STA);

  // Inicializar ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    return;
  }

  // Registrar callback de envío
  esp_now_register_send_cb(OnDataSent);
  
  // Registrar par (peer)
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Agregar par (peer)        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Fallo al agregar par");
    return;
  }

  Serial.println("Sistema iniciado - Esperando botones...");
}
 
void loop() {
  // Leer estado de todos los botones
  // NOTA: Como usamos INPUT_PULLUP, el botón presionado lee LOW (0)
  // Por eso usamos ! para invertir la lógica
  myData.id = 2;  // CAMBIAR A 2 EN EL SEGUNDO EMISOR
  myData.actionA = !digitalRead(BTN_ACTION_A);
  myData.actionB = !digitalRead(BTN_ACTION_B);
  myData.left = !digitalRead(BTN_LEFT);
  myData.down = !digitalRead(BTN_DOWN);
  myData.up = !digitalRead(BTN_UP);
  myData.right = !digitalRead(BTN_RIGHT);

  // Mostrar estado de los botones en el Monitor Serial
  if(myData.actionA || myData.actionB || myData.left || myData.down || myData.up || myData.right) {
    Serial.println("--- Botones Presionados ---");
    if(myData.actionA) Serial.println("  ✓ Acción A");
    if(myData.actionB) Serial.println("  ✓ Acción B");
    if(myData.left) Serial.println("  ✓ Izquierda");
    if(myData.down) Serial.println("  ✓ Abajo");
    if(myData.up) Serial.println("  ✓ Arriba");
    if(myData.right) Serial.println("  ✓ Derecha");
  }

  // Enviar mensaje vía ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
   
  if (result == ESP_OK) {
    Serial.println("✓ Enviado con éxito");
  }
  else {
    Serial.println("✗ Error enviando los datos");
  }

  delay(50);  // Reducido a 50ms para mejor respuesta de los botones
}
