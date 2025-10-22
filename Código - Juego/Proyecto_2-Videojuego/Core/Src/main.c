/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body - Tank Selection & Map Selection Menu
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ili9341.h"
#include "bitmaps.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
  STATE_MENU = 0,
  STATE_TANK_SELECTION,
  STATE_MAP_SELECTION,
  STATE_GAME,
  STATE_GAME_OVER             // <--- NUEVO
} GameState;

typedef struct {
  int x, y;
  int prev_x, prev_y;
  int vy;
  uint8_t anim;
  uint32_t last_anim_ms;
} Cursor;

typedef struct {
  uint8_t tank_index;
  uint8_t selected;
  uint8_t explosion_frame;
  uint32_t last_explo_ms;
} PlayerTankSelection;

typedef struct {
  uint8_t p1_tank_type;
  uint8_t p2_tank_type;
  uint8_t map_id;
  uint8_t ready;
} GameSetup;

typedef enum {
  TILE_EMPTY = 0,
  TILE_BRICK,           // Destructible
  TILE_STEEL,           // Indestructible
  TILE_BUSH,            // Tanque puede pasar
  TILE_WATER,           // Tanque no puede pasar
  TILE_BASE,            // Base (águila)
  TILE_BASE_DESTROYED   // Base destruida
} TileType;

typedef struct {
  int x, y;
  int w, h;
  TileType type;
  uint8_t destructible;
  uint8_t passable;
  const uint8_t* bitmap;
  uint8_t owner_id;   // 0=ninguno, 1=P1, 2=P2  <-- NUEVO
} MapTile;

typedef struct {
  MapTile tiles[256];  // Máximo de tiles por mapa
  uint16_t tile_count;
  uint8_t map_id;
} GameMap;

typedef enum {
  DIR_UP = 0,
  DIR_LEFT = 1,
  DIR_DOWN = 2,
  DIR_RIGHT = 3
} Direction;

typedef enum {
  TANK_AMARILLO = 0,  // Coincide con índice de selección
  TANK_ROJO = 1,
  TANK_VERDE = 2,
  TANK_BLANCO = 3
} TankType;

typedef struct {
  int x, y;              // Posición actual en pantalla
  int prev_x, prev_y;    // Posición anterior (para borrar)
  Direction dir;         // Dirección actual (0=arriba, 1=izq, 2=abajo, 3=der)
  Direction pending_dir; // Dirección pendiente
  TankType type;         // Tipo de tanque
  uint8_t player_id;     // 1 o 2
  uint8_t alive;         // 1=vivo, 0=destruido
  uint8_t moving;        // 1=en movimiento, 0=detenido
  const uint8_t* sprite; // Puntero al sprite correspondiente
  int speed;             // Velocidad de movimiento (píxeles por actualización)
} PlayerTank;

typedef struct {
  int x, y;
  int prev_x, prev_y;
  Direction dir;
  uint8_t active;
  uint8_t owner_id; // 1 o 2
  int speed;
} Bullet;

typedef struct {
  int x, y;
  uint8_t frame;         // 0,1,2
  uint8_t active;        // 1=animando/mostrada, 0=libre
  uint32_t last_ms;
} Explosion;

typedef enum {
  MUS_NONE = 0,
  MUS_INTRO,
  MUS_MAP1,
  MUS_MAP2
} MusicTrack;


/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LCD_W           320
#define LCD_H           240

// ----- Pantalla de inicio -----
#define START_X           6
#define START_Y           0
#define START_W         308
#define START_H         240

// ----- Selector menú inicio -----
#define MENU_SPRITE_W     13
#define MENU_SPRITE_H     13
#define MENU_SPRITE_COLS   2
#define MENU_ANIM_MS     120

#define CURSOR_X        106
#define CURSOR_Y_OPT1   125
#define CURSOR_Y_OPT2   (CURSOR_Y_OPT1 + 23)
#define CURSOR_SPEED       2
#define BG_COLOR        0x0000

// ----- Pantalla de selección de tanques -----
#define TITLE_W         232
#define TITLE_H          13
#define TITLE_X         ((LCD_W - TITLE_W) / 2)
#define TITLE_Y          40

#define TANK_SPRITE_W    50
#define TANK_SPRITE_H    50
#define TANK_SPRITE_COLS  4

#define EXPLO_SPRITE_W   50
#define EXPLO_SPRITE_H   50
#define EXPLO_SPRITE_COLS 3
#define EXPLO_ANIM_MS    150

#define LABEL_W         125
#define LABEL_H          13

#define P1_TANK_X        60
#define P1_TANK_Y       100
#define P1_LABEL_X      ((P1_TANK_X + TANK_SPRITE_W/2) - LABEL_W/2)
#define P1_LABEL_Y      (P1_TANK_Y + TANK_SPRITE_H + 10)

#define P2_TANK_X       210
#define P2_TANK_Y       100
#define P2_LABEL_X      ((P2_TANK_X + TANK_SPRITE_W/2) - LABEL_W/2)
#define P2_LABEL_Y      (P2_TANK_Y + TANK_SPRITE_H + 10)

// ----- Pantalla de selección de mapas -----
#define MAP_TITLE_W     193
#define MAP_TITLE_H      13
#define MAP_TITLE_X     ((LCD_W - MAP_TITLE_W) / 2)
#define MAP_TITLE_Y      50

#define MAP_NAME_W       98
#define MAP_NAME_H       13
#define MAP_NAME_X      ((LCD_W - MAP_NAME_W) / 2)
#define MAP1_Y          110
#define MAP2_Y          150

#define MAP_SELECTOR_W   17
#define MAP_SELECTOR_H   17
#define MAP_SELECTOR_COLS 3
#define MAP_SELECTOR_X  (MAP_NAME_X - MAP_SELECTOR_W - 10)
#define MAP_SELECTOR_Y1  (MAP1_Y - 2)
#define MAP_SELECTOR_Y2  (MAP2_Y - 2)
#define MAP_SELECTOR_ANIM_MS 200

#define BLOCK_SIZE        8
#define BORDER_BLOCKS    ((LCD_W / BLOCK_SIZE) * 2 + (LCD_H / BLOCK_SIZE) * 2)

//=========TANQUES======================================
#define TANK_SPRITE_SIZE    16
#define TANK_SPRITE_COLS     4

// Posiciones iniciales de los tanques
#define P1_START_X         152  // Centrado cerca de base inferior
#define P1_START_Y         190
#define P2_START_X         152  // Centrado cerca de base superior
#define P2_START_Y          48

#define TANK_SPEED           2  // Píxeles por frame (ajusta según necesites)

// Límites del área de juego (ajusta según tu mapa)
#define GAME_MIN_X           0
#define GAME_MAX_X          (LCD_W - TANK_SPRITE_SIZE)
#define GAME_MIN_Y           0
#define GAME_MAX_Y          (LCD_H - TANK_SPRITE_SIZE)

//===================Balas====================================
// ====== BALAS & EXPLOSIONES ======
#define BULLET_SIZE           5
#define BULLET_SPEED          4
#define EXPLOSION_W          16
#define EXPLOSION_H          16
#define EXPLOSION_COLS        3
#define EXPLOSION_ANIM_MS    70

// Áreas de destrucción en ladrillo según eje
#define HITBOX_VERT_W        8   // ↑/↓  ancho
#define HITBOX_VERT_H         8   // ↑/↓  alto
#define HITBOX_HORZ_W         8   // ←/→  ancho
#define HITBOX_HORZ_H        8   // ←/→  alto

//============ GANADOR =================
#define GANADOR_W   108
#define GANADOR_H    95
#define NAME_W      125
#define NAME_H       13

#define EXPLOSION_HOLD_MS 120  // tiempo que permanece el último frame antes de desaparecer


// Comandos UART
#define CMD_UP        "UP"
#define CMD_DOWN      "DOWN"
#define CMD_LEFT      "RIGHT"
#define CMD_RIGHT     "LEFT"
#define CMD_STOP      "STOP"
#define CMD_ACT_A     "ACT_A"
#define CMD_ACT_B     "ACT_B"

// --- Audio (comandos hacia el ESP32 por UART1) ---
#define AUD_CMD_INTRO  "MUSIC_INTRO"
#define AUD_CMD_MAP1   "MUSIC_MAP1"
#define AUD_CMD_MAP2   "MUSIC_MAP2"
#define AUD_CMD_STOP   "MUSIC_STOP"   // opcional

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define CLAMP(v, lo, hi)   ((v)<(lo)?(lo):((v)>(hi)?(v)=(hi):(v)))

// Información de los mapas
#define MAP_1_ID          0
#define MAP_2_ID          1

// Mensaje de transición
#define LOADING_TEXT_X    ((LCD_W - 150) / 2)
#define LOADING_TEXT_Y    ((LCD_H / 2) - 10)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
extern uint8_t pantalla_inicio[];
extern unsigned char tanque_inicio[];
extern uint8_t elegir_tanque[];
extern unsigned char seleccion_tanque[];
extern unsigned char seleccion[];
extern uint8_t player1[];
extern uint8_t player2[];

// Nuevos recursos para selección de mapas
extern uint8_t elige_mapa[];      // 193x13
extern uint8_t mapa_1[];          // 98x13
extern uint8_t mapa_2[];          // 98x13
extern unsigned char tanque_mapa[]; // 17x18, 2 columnas
extern uint8_t bloqueladrillo_8_8[];      // 8x8
extern uint8_t monte [];
extern uint8_t bloquegris_16_16 [];
extern uint8_t casita_ladrillo [];
extern uint8_t aguila [];
extern uint8_t bloquegris_8_8[];

extern const uint8_t casita_gris[];

extern const uint8_t bloqueladrillo_16_8 [];
extern const uint8_t casitagris_invertida [];
extern const uint8_t bloquegris_16_8 [];
extern const uint8_t casitaladrillo_invertida[];

extern uint8_t tanque_blancoSprite[];    // 16x16, 4 columnas
extern uint8_t tanque_rojoSprite[];      // 16x16, 4 columnas
extern uint8_t tanque_verdeSprite[];     // 16x16, 4 columnas
extern uint8_t tanque_amarilloSprite[];  // 16x16, 4 columnas
extern uint8_t bala [];
extern uint8_t explosion_tanque[];

extern uint8_t ganador[];   // 108x95

// UART1 RX
static uint8_t  uart1_rx_byte = 0;
static uint8_t  rx_line_buf[128];
static volatile uint16_t rx_line_len = 0;

// Estado del juego
static GameState g_state = STATE_MENU;

// Cursor menú inicio
static Cursor g_cursor = {
  .x = CURSOR_X,
  .y = CURSOR_Y_OPT1,
  .prev_x = CURSOR_X,
  .prev_y = CURSOR_Y_OPT1,
  .vy = 0,
  .anim = 0,
  .last_anim_ms = 0
};

static int g_menu_index = 0;

// Selección de tanques
static PlayerTankSelection g_p1_tank = {0, 0, 0, 0};
static PlayerTankSelection g_p2_tank = {0, 0, 0, 0};

// Selección de mapa
static Cursor g_map_selector = {
  .x = MAP_SELECTOR_X,
  .y = MAP_SELECTOR_Y1,
  .prev_x = MAP_SELECTOR_X,
  .prev_y = MAP_SELECTOR_Y1,
  .vy = 0,
  .anim = 0,
  .last_anim_ms = 0
};

static uint8_t g_selected_map = 0; // 0 = mapa 1, 1 = mapa 2
static int g_map_menu_index = 0;
static GameSetup g_game_setup = {0, 0, 0, 0};

static GameMap g_current_map;

static PlayerTank g_player1_tank;
static PlayerTank g_player2_tank;

static Bullet g_bullet_p1 = {0};
static Bullet g_bullet_p2 = {0};

#define MAX_EXPLOSIONS 4
static Explosion g_explosions[MAX_EXPLOSIONS];

// Ganador actual (0=nadie, 1=P1, 2=P2)
static uint8_t g_winner = 0;

static MusicTrack g_music_current = MUS_NONE;

static uint8_t game_initialized = 0;

static void DebugPC(const char* s)
{
  HAL_UART_Transmit(&huart2, (uint8_t*)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);

/* USER CODE BEGIN PFP */
static void StartUart1RxIT(void);
static void ProcessCommandLine(const char* line);
static void ApplyMenuCommand(const char* cmd);
static void ApplyTankSelectionCommand(int player_id, const char* cmd);
static void ApplyMapSelectionCommand(const char* cmd);

static void Menu_DrawBackground(void);
static void Menu_UpdateCursor(void);
static void Menu_EraseCursorPrev(void);
static void Menu_DrawCursor(void);

static void TankSel_DrawScreen(void);
static void TankSel_UpdatePlayer(PlayerTankSelection* p, int x, int y);
static void TankSel_DrawTank(int x, int y, uint8_t tank_index);
static void TankSel_DrawExplosion(int x, int y, uint8_t frame);
static void TankSel_EraseTank(int x, int y);

static void MapSel_DrawScreen(void);
static void MapSel_DrawBorder(void);
static void MapSel_UpdateSelector(void);
static void MapSel_EraseSelector(void);
static void MapSel_DrawSelector(void);

static void MapSel_ConfirmSelection(void);
static void Game_Initialize(void);
static uint8_t ValidateGameSetup(void);

static void Map1_Load(void);
static void Map2_Load(void);
static void Map_Draw(GameMap* map);
static void Map_AddTile(GameMap* map, int x, int y, int w, int h,
                        TileType type, uint8_t destructible,
                        uint8_t passable, const uint8_t* bitmap);
static void Map_Clear(GameMap* map);

static void Tank_Init(PlayerTank* tank, uint8_t player_id, TankType type, int x, int y);
static void Tank_Draw(PlayerTank* tank);
static void Tank_Erase(PlayerTank* tank);
static void Tank_Update(PlayerTank* tank);
static void Tank_Move(PlayerTank* tank, Direction dir);
static void Tank_Stop(PlayerTank* tank);
static uint8_t Tank_CheckCollision(PlayerTank* tank, int new_x, int new_y);
static const uint8_t* Tank_GetSprite(TankType type);
static void Game_LoadPlayers(void);
static void Game_UpdateTanks(void);
static void ApplyGameCommand(int player_id, const char* cmd);

static void Tank_RedrawBushes(PlayerTank* tank);
static uint8_t Tank_CheckTankCollision(PlayerTank* tank, int new_x, int new_y);

// ===== Balas =====
static void Bullet_SpawnFromTank(PlayerTank* tank);
static void Bullet_Update(Bullet* b);
static void Bullet_Draw(Bullet* b);
static void Bullet_Erase(Bullet* b);
static void Bullet_RedrawBushes(Bullet* b);
static uint8_t Bullet_CheckCollision(Bullet* b, MapTile** out_tile);
static uint8_t Bullet_CheckHitTank(Bullet* b, PlayerTank* target);

// ===== Ladrillo destruible =====
static void Map_DestroyAround(int cx, int cy, Direction dir);

// ===== Explosiones =====
static void Explo_Spawn(int x, int y);
static void Explo_UpdateAndDrawAll(void);

// --- NUEVO ---
static void Bullet_Kill(Bullet* b);

static void Game_ShowWinner(uint8_t winner_id);  // NUEVO
static void Game_ResetToMenu(void);              // NUEVO

static void Win_PlayExplosionThenShow(uint32_t ms, uint8_t winner_id); // NUEVO

static void Explo_Kill(Explosion* e);
static void Explo_KillAll(void);

static void Audio_SendLine(const char* line);
static void Audio_Play(MusicTrack t);
static void Audio_Stop(void);




/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void StartUart1RxIT(void)
{
  HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
}




// ======= UTIL =======
static inline uint8_t RectsOverlap(int ax,int ay,int aw,int ah, int bx,int by,int bw,int bh)
{
  return (ax < bx + bw) && (ax + aw > bx) && (ay < by + bh) && (ay + ah > by);
}

static void Audio_SendLine(const char* line)
{
  if (!line || !*line) return;
  HAL_UART_Transmit(&huart1, (uint8_t*)line, (uint16_t)strlen(line), 50);
  const char nl = '\n';
  HAL_UART_Transmit(&huart1, (uint8_t*)&nl, 1, 50);

  char dbg[80];
  snprintf(dbg, sizeof(dbg), "[AUDIO->ESP32] %s\r\n", line);
  DebugPC(dbg);
}

static void Audio_Play(MusicTrack t)
{
  if (g_music_current == t) return;

  switch (t) {
    case MUS_INTRO: Audio_SendLine(AUD_CMD_INTRO); break;
    case MUS_MAP1:  Audio_SendLine(AUD_CMD_MAP1);  break;
    case MUS_MAP2:  Audio_SendLine(AUD_CMD_MAP2);  break;
    default: break;
  }
  g_music_current = t;
}

static void Audio_Stop(void)
{
  Audio_SendLine(AUD_CMD_STOP);
}

// ======= EXPLOSIONES =======
static void Explo_Spawn(int x, int y)
{
  for (int i=0;i<MAX_EXPLOSIONS;i++){
    if (!g_explosions[i].active){
      g_explosions[i].active = 1;
      g_explosions[i].frame = 0;
      g_explosions[i].x = x;
      g_explosions[i].y = y;
      g_explosions[i].last_ms = HAL_GetTick();
      // dibuja frame 0 de una
      LCD_Sprite(x, y, EXPLOSION_W, EXPLOSION_H, explosion_tanque, EXPLOSION_COLS, 0, 0, 0);
      return;
    }
  }
}

static void Explo_Kill(Explosion* e)
{
  // Limpiar el área de la explosión
  FillRect(e->x, e->y, EXPLOSION_W, EXPLOSION_H, BG_COLOR);

  // Redibujar tiles que crucen esa zona
  for (uint16_t i = 0; i < g_current_map.tile_count; i++) {
    MapTile* t = &g_current_map.tiles[i];
    if (!t->bitmap) continue;
    if (RectsOverlap(e->x, e->y, EXPLOSION_W, EXPLOSION_H, t->x, t->y, t->w, t->h)) {
      LCD_Bitmap(t->x, t->y, t->w, t->h, t->bitmap);
    }
  }

  e->active = 0;
}

static void Explo_KillAll(void)
{
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    if (g_explosions[i].active) {
      Explo_Kill(&g_explosions[i]);
    }
  }
}


static void Explo_UpdateAndDrawAll(void)
{
  uint32_t now = HAL_GetTick();
  for (int i = 0; i < MAX_EXPLOSIONS; i++) {
    Explosion* e = &g_explosions[i];
    if (!e->active) continue;

    // Avanzar frames cada EXPLOSION_ANIM_MS hasta el 2 (último)
    if (e->frame < 2 && (now - e->last_ms) >= EXPLOSION_ANIM_MS) {
      e->frame++;
      e->last_ms = now; // marca inicio del frame actual
    }

    // Dibujar frame actual
    LCD_Sprite(e->x, e->y, EXPLOSION_W, EXPLOSION_H,
               explosion_tanque, EXPLOSION_COLS, e->frame, 0, 0);

    // Al llegar al último frame, sostenerlo un ratito y luego limpiar
    if (e->frame == 2 && (now - e->last_ms) >= EXPLOSION_HOLD_MS) {
      Explo_Kill(e);
    }
  }
}


// ======= BALAS =============================================
static void Bullet_Start(Bullet* b, int x, int y, Direction dir, uint8_t owner)
{
  b->x = b->prev_x = x;
  b->y = b->prev_y = y;
  b->dir = dir;
  b->active = 1;
  b->owner_id = owner;
  b->speed = BULLET_SPEED;
  // primer draw
  Bullet_Draw(b);
  Bullet_RedrawBushes(b);
}

static void Bullet_SpawnFromTank(PlayerTank* tank)
{
  Bullet* b = (tank->player_id == 1) ? &g_bullet_p1 : &g_bullet_p2;
  if (b->active) return; // una bala a la vez por jugador

  // salida desde el “cañón” centrado sobre la cara del tanque
  int bx = tank->x + (TANK_SPRITE_SIZE - BULLET_SIZE)/2;
  int by = tank->y + (TANK_SPRITE_SIZE - BULLET_SIZE)/2;

  switch (tank->dir){
    case DIR_UP:    by = tank->y - BULLET_SIZE + 1; break;
    case DIR_DOWN:  by = tank->y + TANK_SPRITE_SIZE - 1; break;
    case DIR_LEFT:  bx = tank->x - BULLET_SIZE + 1; break;
    case DIR_RIGHT: bx = tank->x + TANK_SPRITE_SIZE - 1; break;
  }

  Bullet_Start(b, bx, by, tank->dir, tank->player_id);
}

static void Bullet_Erase(Bullet* b)
{
  if (b->prev_x==b->x && b->prev_y==b->y) return;

  FillRect(b->prev_x, b->prev_y, BULLET_SIZE, BULLET_SIZE, BG_COLOR);

  // Redibujar tiles debajo (igual que Tank_Erase)
  for (uint16_t i=0; i<g_current_map.tile_count; i++){
    MapTile* t = &g_current_map.tiles[i];
    if (t->bitmap==NULL) continue;
    if (RectsOverlap(b->prev_x, b->prev_y, BULLET_SIZE, BULLET_SIZE,
                     t->x, t->y, t->w, t->h)){
      LCD_Bitmap(t->x, t->y, t->w, t->h, t->bitmap);
    }
  }
}

static void Bullet_Draw(Bullet* b)
{
  if (!b->active) return;
  LCD_Sprite(b->x, b->y, BULLET_SIZE, BULLET_SIZE, bala, 4, (uint8_t)b->dir, 0, 0);
}

static void Bullet_RedrawBushes(Bullet* b)
{
  for (uint16_t i=0; i<g_current_map.tile_count; i++){
    MapTile* t = &g_current_map.tiles[i];
    if (t->type == TILE_BUSH){
      if (RectsOverlap(b->x, b->y, BULLET_SIZE, BULLET_SIZE, t->x, t->y, t->w, t->h)){
        LCD_Bitmap(t->x, t->y, t->w, t->h, t->bitmap);
      }
    }
  }
}

// Mata la bala y limpia rastro en pantalla (previo y actual) y restaura tiles tapados
static void Bullet_Kill(Bullet* b)
{
  // Limpia sprite previo y actual
  FillRect(b->prev_x, b->prev_y, BULLET_SIZE, BULLET_SIZE, BG_COLOR);
  FillRect(b->x,      b->y,      BULLET_SIZE, BULLET_SIZE, BG_COLOR);

  // Redibuja cualquier tile que cruce estas zonas (como en Bullet_Erase)
  for (uint16_t i=0; i<g_current_map.tile_count; i++){
    MapTile* t = &g_current_map.tiles[i];
    if (!t->bitmap) continue;

    if (RectsOverlap(b->prev_x, b->prev_y, BULLET_SIZE, BULLET_SIZE, t->x, t->y, t->w, t->h) ||
        RectsOverlap(b->x,      b->y,      BULLET_SIZE, BULLET_SIZE, t->x, t->y, t->w, t->h)) {
      LCD_Bitmap(t->x, t->y, t->w, t->h, t->bitmap);
    }
  }

  b->active = 0;
}


static uint8_t Bullet_CheckHitTank(Bullet* b, PlayerTank* target)
{
  if (!target->alive) return 0;

  if (RectsOverlap(b->x, b->y, BULLET_SIZE, BULLET_SIZE,
                   target->x, target->y, TANK_SPRITE_SIZE, TANK_SPRITE_SIZE)){
    // Explosión sobre el tanque, “muerte”
    Explo_Spawn(target->x, target->y);
    FillRect(target->x, target->y, TANK_SPRITE_SIZE, TANK_SPRITE_SIZE, BG_COLOR);
    target->alive = 0;

    // Borro la bala ya mismo para no tapar la animación (idempotente si se vuelve a llamar)
    Bullet_Kill(b);

    // Espera corta para que se vea la explosión y luego muestra ganador
    Win_PlayExplosionThenShow(450, b->owner_id);

    return 1;
  }
  return 0;
}

static uint8_t Bullet_CheckCollision(Bullet* b, MapTile** out_tile)
{
  // Bordes de pantalla
  if (b->x < GAME_MIN_X || b->x + BULLET_SIZE > LCD_W ||
      b->y < GAME_MIN_Y || b->y + BULLET_SIZE > LCD_H){
    *out_tile = NULL;
    return 1; // colisión con borde
  }

  // Tiles
  for (uint16_t i=0; i<g_current_map.tile_count; i++){
    MapTile* t = &g_current_map.tiles[i];

    // arbusto no bloquea bala
    if (t->type == TILE_BUSH) continue;
    if (t->type == TILE_EMPTY) continue;

    if (RectsOverlap(b->x, b->y, BULLET_SIZE, BULLET_SIZE,
                     t->x, t->y, t->w, t->h)){

      *out_tile = t;
      return 1;
    }
  }

  *out_tile = NULL;
  return 0;
}

static void Map_DestroyAround(int cx, int cy, Direction dir)
{
  // Área base del impacto (puedes ajustar los defines HITBOX_* si quieres)
  int w = (dir==DIR_UP || dir==DIR_DOWN) ? HITBOX_VERT_W : HITBOX_HORZ_W;
  int h = (dir==DIR_LEFT || dir==DIR_RIGHT) ? HITBOX_HORZ_H : HITBOX_VERT_H;

  int x0 = cx - w/2;
  int y0 = cy - h/2;

  // --- Bounding box de TODO lo limpiado (ladrillos impactados + zona base)
  int minx = x0, miny = y0;
  int maxx = x0 + w, maxy = y0 + h;
  uint8_t any_cleared = 0;

  // 1) Eliminar TODOS los ladrillos que intersecten el área y ampliar la bbox
  for (uint16_t i=0; i<g_current_map.tile_count; i++){
    MapTile* t = &g_current_map.tiles[i];
    if (t->type != TILE_BRICK) continue;

    if (RectsOverlap(x0, y0, w, h, t->x, t->y, t->w, t->h)){
      // Limpiar ladrillo
      FillRect(t->x, t->y, t->w, t->h, BG_COLOR);

      // Ampliar la caja envolvente con el rect del ladrillo borrado
      if (t->x < minx) minx = t->x;
      if (t->y < miny) miny = t->y;
      if (t->x + t->w > maxx) maxx = t->x + t->w;
      if (t->y + t->h > maxy) maxy = t->y + t->h;

      any_cleared = 1;

      // Vaciar el tile en el mapa
      t->type = TILE_EMPTY;
      t->destructible = 0;
      t->passable = 1;
      t->bitmap = NULL;
    }
  }

  // Si no se borró ningún ladrillo, igual consideramos la zona base del impacto.
  // (Esto ayuda en bordes de ladrillos donde el impacto cae casi fuera)
  if (!any_cleared){
    // Expandimos 1px para cubrir solapes sutiles
    minx -= 1; miny -= 1; maxx += 1; maxy += 1;
  }

  // Normalizar dimensiones de la bbox
  if (minx < 0) minx = 0;
  if (miny < 0) miny = 0;
  if (maxx > LCD_W) maxx = LCD_W;
  if (maxy > LCD_H) maxy = LCD_H;
  int rw = (maxx - minx);
  int rh = (maxy - miny);
  if (rw <= 0 || rh <= 0) return;

  // 2) Redibujar cualquier tile NO vacío que cruce la bbox (águila, acero, arbusto, etc.)
  for (uint16_t i=0; i<g_current_map.tile_count; i++){
    MapTile* t = &g_current_map.tiles[i];

    if (t->type == TILE_EMPTY) continue;   // los ladrillos borrados ya no se redibujan
    if (!t->bitmap) continue;

    if (RectsOverlap(minx, miny, rw, rh, t->x, t->y, t->w, t->h)){
      // Redibuja el tile completo para evitar “bordes mordidos”
      LCD_Bitmap(t->x, t->y, t->w, t->h, t->bitmap);
    }
  }
}



static void Bullet_Update(Bullet* b)
{
  if (!b->active) return;

  // Borrar rastro anterior
  Bullet_Erase(b);

  // Guardar prev
  b->prev_x = b->x;
  b->prev_y = b->y;

  // Avanzar
  switch (b->dir){
    case DIR_UP:    b->y -= b->speed; break;
    case DIR_DOWN:  b->y += b->speed; break;
    case DIR_LEFT:  b->x -= b->speed; break;
    case DIR_RIGHT: b->x += b->speed; break;
  }

  // Colisión con tanques (no te mates a ti mismo)
  PlayerTank* enemy = (b->owner_id==1) ? &g_player2_tank : &g_player1_tank;
  if (Bullet_CheckHitTank(b, enemy)){
    Bullet_Kill(b);
    return;
  }

  // Colisión con mapa/bordes
  MapTile* hit = NULL;
  if (Bullet_CheckCollision(b, &hit)){
    // si pegó al borde o tile
    if (hit == NULL){
      // borde: solo destruir bala
      Bullet_Kill(b);
      return;
    }

    if (hit->type == TILE_STEEL){
      // acero: no destruye el bloque
      Bullet_Kill(b);
      return;
    }

    if (hit->type == TILE_BRICK){
      // ladrillo: destruir área según dirección y eliminar bala
      // centro del impacto ~ centro de la bala
      int cx = b->x + BULLET_SIZE/2;
      int cy = b->y + BULLET_SIZE/2;
      Map_DestroyAround(cx, cy, b->dir);
      Bullet_Kill(b);
      return;
    }

    if (hit->type == TILE_BASE){
      // águila: explosión y “destruida”
      Explo_Spawn(hit->x, hit->y);
      // Puedes marcarla destruida:
      hit->type = TILE_BASE_DESTROYED;
      // limpiar águila (si no tienes sprite destruida)
      FillRect(hit->x, hit->y, hit->w, hit->h, BG_COLOR);
      Bullet_Kill(b);

      // --- VICTORIA POR ÁGUILA ---
      // Si destruye su propia base, gana el otro; si destruye la del otro, gana el tirador.
      uint8_t base_owner = hit->owner_id;          // 1 ó 2
      uint8_t winner = (base_owner == 1) ? 2 : 1;  // el contrario

      // Pausa breve para que se vea la explosión de la base
      Win_PlayExplosionThenShow(450, winner);

      return;
    }

    // otros: por seguridad solo destruimos bala
    Bullet_Kill(b);
    return;
  }

  // Sin colisión: dibujar y respetar arbustos al frente
  Bullet_Draw(b);
  Bullet_RedrawBushes(b);
}


// ===== FUNCIONES DE MANEJO DE MAPAS =====

static void Map_Clear(GameMap* map)
{
  map->tile_count = 0;
  map->map_id = 0;
}

static void Map_AddTile(GameMap* map, int x, int y, int w, int h,
                        TileType type, uint8_t destructible,
                        uint8_t passable, const uint8_t* bitmap)
{
  if (map->tile_count >= 256) return;

  MapTile* t = &map->tiles[map->tile_count++];
  t->x = x;
  t->y = y;
  t->w = w;
  t->h = h;
  t->type = type;
  t->destructible = destructible;
  t->passable = passable;
  t->bitmap = bitmap;
  t->owner_id = 0; // por defecto
}

static void Map_Draw(GameMap* map)
{
  char dbg[64];
  snprintf(dbg, sizeof(dbg), "[MAP_DRAW] Iniciando dibujo de %d tiles...\r\n", map->tile_count);
  DebugPC(dbg);

  if (map->tile_count == 0) {
    DebugPC("[MAP_DRAW ERROR] tile_count es 0!\r\n");
    return;
  }

  for (uint16_t i = 0; i < map->tile_count; i++) {
    MapTile* t = &map->tiles[i];

    if (t->bitmap == NULL) {
      snprintf(dbg, sizeof(dbg), "[MAP_DRAW WARN] Tile %d tiene bitmap NULL\r\n", i);
      DebugPC(dbg);
      continue;
    }

    LCD_Bitmap(t->x, t->y, t->w, t->h, t->bitmap);

    // Debug cada 10 tiles para no saturar UART
    if (i % 10 == 0) {
      snprintf(dbg, sizeof(dbg), "[MAP_DRAW] Dibujando tile %d/%d\r\n", i, map->tile_count);
      DebugPC(dbg);
    }
  }

  DebugPC("[MAP_DRAW] Dibujo completado\r\n");
}

static void Map1_Load(void)
{
  char dbg[64];
  DebugPC("[MAP1] Iniciando carga del Mapa 1...\r\n");

  Map_Clear(&g_current_map);
  g_current_map.map_id = MAP_1_ID;

  // ===== BASE INFERIOR (Águila) =====
  Map_AddTile(&g_current_map, 144, 208, 32, 16, TILE_STEEL, 0, 0, casita_gris);
  Map_AddTile(&g_current_map, 168, 224, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 168, 232, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 144, 232, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 144, 224, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 152, 232, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 152, 216, 16, 16, TILE_BASE, 1, 0, aguila);
  g_current_map.tiles[g_current_map.tile_count-1].owner_id = 1;

  // ===== BASE SUPERIOR (Águila invertida) =====
  Map_AddTile(&g_current_map, 144, 16, 32, 16, TILE_STEEL, 0, 0, casitagris_invertida);
  Map_AddTile(&g_current_map, 168, 8, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 168, 0, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 144, 8, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 144, 0, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 152, 0, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 152, 8, 16, 16, TILE_BASE, 1, 0, aguila);
  g_current_map.tiles[g_current_map.tile_count-1].owner_id = 2;

  // ===== COLUMNAS IZQUIERDA =====
  for (int y = 175; y < 216; y += 8) {
    Map_AddTile(&g_current_map, 32, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  for (int y = 192; y < 232; y += 8) {
    Map_AddTile(&g_current_map, 48, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // ===== ESQUINA INFERIOR DERECHA =====
  for (int y = 176; y < 240; y += 16) {
    Map_AddTile(&g_current_map, 304, y, 16, 16, TILE_BUSH, 0, 1, monte);
  }

  for (int y = 208; y < 240; y += 16) {
    Map_AddTile(&g_current_map, 288, y, 16, 16, TILE_BUSH, 0, 1, monte);
  }

  for (int x = 256; x < 288; x += 16) {
    Map_AddTile(&g_current_map, x, 208, 16, 16, TILE_BUSH, 0, 1, monte);
  }

  for (int x = 240; x < 304; x += 16) {
    Map_AddTile(&g_current_map, x, 192, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
    Map_AddTile(&g_current_map, x, 200, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // ===== BLOQUE INDIVIDUAL =====
  Map_AddTile(&g_current_map, 64, 224, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);

  // ===== COLUMNAS VERTICALES CENTRO =====
  for (int y = 56; y < 184; y += 8) {
    Map_AddTile(&g_current_map, 120, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  for (int y = 56; y < 184; y += 8) {
    Map_AddTile(&g_current_map, 184, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // ===== ARBUSTOS CENTRO =====
  for (int y = 88; y < 136; y += 16) {
    Map_AddTile(&g_current_map, 136, y, 16, 16, TILE_BUSH, 0, 1, monte);
  }

  for (int y = 88; y < 136; y += 16) {
    Map_AddTile(&g_current_map, 168, y, 16, 16, TILE_BUSH, 0, 1, monte);
  }

  for (int y = 104; y < 152; y += 16) {
    Map_AddTile(&g_current_map, 152, y, 16, 16, TILE_BUSH, 0, 1, monte);
  }

  // ===== BLOQUES HORIZONTALES CENTRO =====
  Map_AddTile(&g_current_map, 136, 184, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 168, 184, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 136, 48, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 168, 48, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);

  // ===== COLUMNA CENTRO (MIXTA) =====
  Map_AddTile(&g_current_map, 152, 104, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 152, 96, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 152, 88, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 152, 80, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  Map_AddTile(&g_current_map, 152, 72, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);

  // ===== BLOQUES GRISES LATERALES =====
  for (int y = 104; y < 132; y += 8) {
    Map_AddTile(&g_current_map, 0, y, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  }

  for (int y = 104; y < 132; y += 8) {
    Map_AddTile(&g_current_map, 304, y, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  }

  for (int x = 16; x < 64; x += 16) {
    Map_AddTile(&g_current_map, x, 128, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  }

  for (int x = 256; x < 304; x += 16) {
    Map_AddTile(&g_current_map, x, 128, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  }

  // ===== PLATAFORMAS INFERIORES =====
  for (int x = 16; x < 96; x += 16) {
    Map_AddTile(&g_current_map, x, 112, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
    Map_AddTile(&g_current_map, x, 120, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  for (int x = 0; x < 80; x += 16) {
    Map_AddTile(&g_current_map, x, 72, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
    Map_AddTile(&g_current_map, x, 80, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  for (int x = 224; x < 304; x += 16) {
    Map_AddTile(&g_current_map, x, 112, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
    Map_AddTile(&g_current_map, x, 120, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  for (int x = 240; x < 320; x += 16) {
    Map_AddTile(&g_current_map, x, 72, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
    Map_AddTile(&g_current_map, x, 80, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // ===== COLUMNA IZQUIERDA SUPERIOR =====
  for (int y = 168; y < 216; y += 8) {
    Map_AddTile(&g_current_map, 0, y, 8, 8, TILE_STEEL, 0, 0, bloquegris_8_8);
  }

  for (int y = 0; y < 48; y += 8) {
    Map_AddTile(&g_current_map, 32, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  for (int y = 0; y < 24; y += 8) {
    Map_AddTile(&g_current_map, 48, y, 8, 8, TILE_STEEL, 0, 0, bloquegris_8_8);
  }

  for (int y = 0; y < 48; y += 16) {
    Map_AddTile(&g_current_map, 80, y, 16, 16, TILE_BUSH, 0, 1, monte);
  }

  // ===== ESQUINA SUPERIOR DERECHA =====
  for (int x = 272; x < 320; x += 16) {
    Map_AddTile(&g_current_map, x, 0, 16, 16, TILE_BUSH, 0, 1, monte);
  }

  Map_AddTile(&g_current_map, 304, 16, 16, 16, TILE_BUSH, 0, 1, monte);
  Map_AddTile(&g_current_map, 304, 32, 16, 16, TILE_BUSH, 0, 1, monte);
  Map_AddTile(&g_current_map, 288, 16, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 288, 24, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);

  // ===== BLOQUES CENTRALES ADICIONALES =====
  Map_AddTile(&g_current_map, 100, 200, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  Map_AddTile(&g_current_map, 100, 208, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);

  for (int x = 210; x < 242; x += 16) {
    Map_AddTile(&g_current_map, x, 27, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
    Map_AddTile(&g_current_map, x, 19, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  Map_AddTile(&g_current_map, 210, 35, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  Map_AddTile(&g_current_map, 210, 43, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);

  snprintf(dbg, sizeof(dbg), "[MAP1] Cargados %d tiles\r\n", g_current_map.tile_count);
  DebugPC(dbg);

  DebugPC("[MAP1] Dibujando mapa...\r\n");
  Map_Draw(&g_current_map);

  DebugPC("[MAP1] Mapa 1 cargado exitosamente\r\n");
}

static void Map2_Load(void)
{
  char dbg[64];
  DebugPC("\r\n========================================\r\n");
  DebugPC("[MAP2] Iniciando carga del Mapa 2...\r\n");
  DebugPC("========================================\r\n");

  Map_Clear(&g_current_map);
  g_current_map.map_id = MAP_2_ID;

  DebugPC("[MAP2] Estructura limpiada, agregando tiles...\r\n");

  // ===== BASE INFERIOR (Águila) =====
  DebugPC("[MAP2] Agregando base inferior...\r\n");
  Map_AddTile(&g_current_map, 144, 208, 32, 16, TILE_BRICK, 1, 0, casita_ladrillo);
  Map_AddTile(&g_current_map, 168, 224, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 168, 232, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 144, 232, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 144, 224, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 152, 232, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 152, 216, 16, 16, TILE_BASE, 1, 0, aguila);
  g_current_map.tiles[g_current_map.tile_count-1].owner_id = 1;

  // ===== BASE SUPERIOR (Águila invertida) =====
  DebugPC("[MAP2] Agregando base superior...\r\n");
  Map_AddTile(&g_current_map, 144, 16, 32, 16, TILE_BRICK, 1, 0, casitaladrillo_invertida);
  Map_AddTile(&g_current_map, 168, 8, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 168, 0, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 144, 8, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 144, 0, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 152, 0, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 152, 8, 16, 16, TILE_BASE, 1, 0, aguila);
  g_current_map.tiles[g_current_map.tile_count-1].owner_id = 2;

  snprintf(dbg, sizeof(dbg), "[MAP2] Bases agregadas. Tiles: %d\r\n", g_current_map.tile_count);
  DebugPC(dbg);

  // ===== COLUMNAS LATERALES IZQUIERDAS =====
  DebugPC("[MAP2] Agregando columnas izquierdas...\r\n");

  // Columna izquierda inferior (y=120 a 240)
  for (int y = 120; y < 240; y += 8) {
    Map_AddTile(&g_current_map, 0, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
    Map_AddTile(&g_current_map, 16, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // Bloques grises centro-izquierda (y=104 a 136)
  for (int y = 104; y < 136; y += 8) {
    Map_AddTile(&g_current_map, 16, y, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  }

  // Bloques grises adicionales
  Map_AddTile(&g_current_map, 16, 152, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  Map_AddTile(&g_current_map, 16, 160, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);

  // ===== COLUMNAS LATERALES DERECHAS =====
  DebugPC("[MAP2] Agregando columnas derechas...\r\n");

  // Columna derecha superior (y=0 a 120)
  for (int y = 0; y < 120; y += 8) {
    Map_AddTile(&g_current_map, 304, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
    Map_AddTile(&g_current_map, 288, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // Bloques grises centro-derecha (y=104 a 136)
  for (int y = 104; y < 136; y += 8) {
    Map_AddTile(&g_current_map, 288, y, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  }

  // Bloques grises adicionales
  Map_AddTile(&g_current_map, 288, 72, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  Map_AddTile(&g_current_map, 288, 80, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);

  snprintf(dbg, sizeof(dbg), "[MAP2] Columnas laterales agregadas. Tiles: %d\r\n", g_current_map.tile_count);
  DebugPC(dbg);

  // ===== ESTRUCTURA SUPERIOR IZQUIERDA =====
  DebugPC("[MAP2] Agregando estructura superior izquierda...\r\n");

  // Columna vertical (y=16 a 88)
  for (int y = 16; y < 88; y += 8) {
    Map_AddTile(&g_current_map, 32, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // Filas horizontales superiores (x=32 a 112)
  for (int x = 32; x < 112; x += 16) {
    Map_AddTile(&g_current_map, x, 16, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
    Map_AddTile(&g_current_map, x, 24, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // Bloques grises en esquina
  Map_AddTile(&g_current_map, 32, 48, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  Map_AddTile(&g_current_map, 32, 56, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);

  // ===== ESTRUCTURA INFERIOR DERECHA =====
  DebugPC("[MAP2] Agregando estructura inferior derecha...\r\n");

  // Columna vertical (y=152 a 224)
  for (int y = 152; y < 224; y += 8) {
    Map_AddTile(&g_current_map, 272, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // Filas horizontales inferiores (x=208 a 272)
  for (int x = 208; x < 272; x += 16) {
    Map_AddTile(&g_current_map, x, 216, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
    Map_AddTile(&g_current_map, x, 208, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // Bloques grises en esquina
  Map_AddTile(&g_current_map, 272, 176, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  Map_AddTile(&g_current_map, 272, 184, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);

  snprintf(dbg, sizeof(dbg), "[MAP2] Estructuras esquinas agregadas. Tiles: %d\r\n", g_current_map.tile_count);
  DebugPC(dbg);

  // ===== ESTRUCTURA CENTRAL (ARBUSTOS Y BLOQUES) =====
  DebugPC("[MAP2] Agregando estructura central...\r\n");

  // Arbustos centrales (x=112 a 208, y=104 y 120)
  for (int x = 112; x < 208; x += 16) {
    Map_AddTile(&g_current_map, x, 104, 16, 16, TILE_BUSH, 0, 1, monte);
    Map_AddTile(&g_current_map, x, 120, 16, 16, TILE_BUSH, 0, 1, monte);
  }

  // Arbustos laterales
  Map_AddTile(&g_current_map, 112, 88, 16, 16, TILE_BUSH, 0, 1, monte);
  Map_AddTile(&g_current_map, 112, 136, 16, 16, TILE_BUSH, 0, 1, monte);
  Map_AddTile(&g_current_map, 192, 88, 16, 16, TILE_BUSH, 0, 1, monte);
  Map_AddTile(&g_current_map, 192, 136, 16, 16, TILE_BUSH, 0, 1, monte);

  // Bloques grises horizontales superiores (x=128 a 192)
  for (int x = 128; x < 192; x += 16) {
    Map_AddTile(&g_current_map, x, 96, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
    Map_AddTile(&g_current_map, x, 136, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  }

  // Bloques grises centrales (x=144 a 176)
  for (int x = 144; x < 176; x += 16) {
    Map_AddTile(&g_current_map, x, 88, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
    Map_AddTile(&g_current_map, x, 144, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  }

  snprintf(dbg, sizeof(dbg), "[MAP2] Estructura central agregada. Tiles: %d\r\n", g_current_map.tile_count);
  DebugPC(dbg);

  // ===== DETALLES ESQUINA SUPERIOR IZQUIERDA =====
  DebugPC("[MAP2] Agregando detalles superiores izquierdos...\r\n");

  Map_AddTile(&g_current_map, 96, 72, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 96, 80, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 112, 72, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 96, 88, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 96, 96, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);

  // ===== DETALLES ESQUINA INFERIOR IZQUIERDA =====
  DebugPC("[MAP2] Agregando detalles inferiores izquierdos...\r\n");

  Map_AddTile(&g_current_map, 96, 152, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 96, 160, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 112, 160, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 96, 144, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 96, 136, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);

  // ===== DETALLES ESQUINA SUPERIOR DERECHA =====
  DebugPC("[MAP2] Agregando detalles superiores derechos...\r\n");

  Map_AddTile(&g_current_map, 208, 72, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 208, 80, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 192, 72, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 216, 88, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 216, 96, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);

  // ===== DETALLES ESQUINA INFERIOR DERECHA =====
  DebugPC("[MAP2] Agregando detalles inferiores derechos...\r\n");

  Map_AddTile(&g_current_map, 208, 152, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 208, 160, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 192, 160, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  Map_AddTile(&g_current_map, 216, 144, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);
  Map_AddTile(&g_current_map, 216, 136, 8, 8, TILE_BRICK, 1, 0, bloqueladrillo_8_8);

  snprintf(dbg, sizeof(dbg), "[MAP2] Detalles esquinas agregados. Tiles: %d\r\n", g_current_map.tile_count);
  DebugPC(dbg);

  // ===== COLUMNAS VERTICALES ADICIONALES =====
  DebugPC("[MAP2] Agregando columnas adicionales...\r\n");

  // Columna izquierda (x=55, y=150 a 206)
  for (int y = 150; y < 206; y += 8) {
    Map_AddTile(&g_current_map, 55, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // Columna derecha (x=248, y=34 a 90)
  for (int y = 34; y < 90; y += 8) {
    Map_AddTile(&g_current_map, 248, y, 16, 8, TILE_BRICK, 1, 0, bloqueladrillo_16_8);
  }

  // ===== BLOQUES GRISES INDIVIDUALES =====
  DebugPC("[MAP2] Agregando bloques grises finales...\r\n");

  Map_AddTile(&g_current_map, 60, 60, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);
  Map_AddTile(&g_current_map, 245, 175, 16, 8, TILE_STEEL, 0, 0, bloquegris_16_8);

  // ===== FINALIZACIÓN =====
  snprintf(dbg, sizeof(dbg), "\r\n[MAP2] ✓ Total de tiles cargados: %d\r\n", g_current_map.tile_count);
  DebugPC(dbg);

  if (g_current_map.tile_count == 0) {
    DebugPC("[MAP2 ERROR] No se agregaron tiles!\r\n");
    return;
  }

  DebugPC("[MAP2] Llamando a Map_Draw()...\r\n");
  Map_Draw(&g_current_map);

  DebugPC("[MAP2] ✓ Mapa 2 cargado y dibujado exitosamente\r\n");
  DebugPC("========================================\r\n\r\n");
}


static void ProcessCommandLine(const char* line_raw)
{
  char line[128];
  size_t n=0;
  for (size_t i=0; line_raw[i] && n < sizeof(line)-1; ++i) {
    if (line_raw[i] != '\r' && line_raw[i] != '\n') line[n++] = line_raw[i];
  }
  line[n] = '\0';

  const char* colon = strchr(line, ':');
  const char* cmd = NULL;
  int player_id = 1;

  if (line[0]=='P' && colon && (colon > line+1)) {
    player_id = atoi(&line[1]);
    cmd = colon + 1;
  } else {
    cmd = line;
  }

  char dbg[128];
  snprintf(dbg, sizeof(dbg), "[RX1] P%d:%s (Estado:%d)\r\n", player_id, cmd ? cmd : "NULL", g_state);
  DebugPC(dbg);

  if (player_id < 1 || player_id > 2) {
    snprintf(dbg, sizeof(dbg), "[ERROR] Player ID inválido: %d\r\n", player_id);
    DebugPC(dbg);
    return;
  }

  // AGREGAR ESTA VERIFICACIÓN EXTRA
  if (cmd == NULL || *cmd == '\0') {
    DebugPC("[ERROR] Comando vacío o NULL\r\n");
    return;
  }

  if (g_state == STATE_MENU && cmd && *cmd) {
    if (player_id == 1) {
      DebugPC("[PROCESS] Procesando comando de menú\r\n");
      ApplyMenuCommand(cmd);
    }
  } else if (g_state == STATE_TANK_SELECTION && cmd && *cmd) {
    DebugPC("[PROCESS] Procesando comando de selección de tanque\r\n");
    ApplyTankSelectionCommand(player_id, cmd);
  } else if (g_state == STATE_MAP_SELECTION && cmd && *cmd) {
    if (player_id == 1) {
      DebugPC("[PROCESS] Procesando comando de selección de mapa\r\n");
      ApplyMapSelectionCommand(cmd);
    } else {
      snprintf(dbg, sizeof(dbg), "[MAP] Ignorando comando de P%d (solo P1 puede seleccionar mapa)\r\n", player_id);
      DebugPC(dbg);
    }
  }

  else if (g_state == STATE_GAME_OVER && cmd && *cmd) {
    if (strcmp(cmd, CMD_ACT_A) == 0 || strcmp(cmd, CMD_ACT_B) == 0) {
      DebugPC("[WIN] Volviendo al menú por tecla de acción\r\n");
      Game_ResetToMenu();
    }
    return; // no procesar más cuando estamos en pantalla de ganador
  }

  else if (g_state == STATE_GAME && cmd && *cmd) {
    DebugPC("[PROCESS] Procesando comando de juego\r\n");
    ApplyGameCommand(player_id, cmd);
  }
}

/**
 * @brief Obtiene el sprite correspondiente al tipo de tanque
 */
static const uint8_t* Tank_GetSprite(TankType type)
{
  switch(type) {
    case TANK_AMARILLO: return tanque_amarilloSprite;
    case TANK_ROJO:     return tanque_rojoSprite;
    case TANK_VERDE:    return tanque_verdeSprite;
    case TANK_BLANCO:   return tanque_blancoSprite;
    default:            return tanque_amarilloSprite;
  }
}

/**
 * @brief Inicializa un tanque con sus parámetros
 */
static void Tank_Init(PlayerTank* tank, uint8_t player_id, TankType type, int x, int y)
{
  tank->x = x;
  tank->y = y;
  tank->prev_x = x;
  tank->prev_y = y;
  tank->player_id = player_id;
  tank->type = type;
  tank->alive = 1;
  tank->moving = 0;
  tank->speed = TANK_SPEED;
  tank->sprite = Tank_GetSprite(type);

  // Dirección inicial según jugador
  tank->dir = (player_id == 1) ? DIR_UP : DIR_DOWN;
  tank->pending_dir = tank->dir;

  char dbg[128];
  snprintf(dbg, sizeof(dbg),
           "[TANK_INIT] P%d: Tipo=%d, Pos=(%d,%d), Dir=%d\r\n",
           player_id, type, x, y, tank->dir);
  DebugPC(dbg);
}

/**
 * @brief Verifica colisiones del tanque con los límites y tiles del mapa
 * @return 1 si hay colisión, 0 si puede moverse
 */
static uint8_t Tank_CheckCollision(PlayerTank* tank, int new_x, int new_y)
{
  // Verificar límites de pantalla
  if (new_x < GAME_MIN_X || new_x > GAME_MAX_X ||
      new_y < GAME_MIN_Y || new_y > GAME_MAX_Y) {
    return 1; // Colisión con borde
  }

  // ⭐ NUEVO: Verificar colisión con el otro tanque
  if (Tank_CheckTankCollision(tank, new_x, new_y)) {
    return 1; // Colisión con otro tanque
  }

  // Verificar colisión con tiles del mapa
  for (uint16_t i = 0; i < g_current_map.tile_count; i++) {
    MapTile* tile = &g_current_map.tiles[i];

    // Solo verificar tiles no pasables (BUSH es pasable, no debe colisionar)
    if (!tile->passable) {
      // Verificar superposición de rectángulos
      if (new_x < tile->x + tile->w &&
          new_x + TANK_SPRITE_SIZE > tile->x &&
          new_y < tile->y + tile->h &&
          new_y + TANK_SPRITE_SIZE > tile->y) {
        return 1; // Colisión con tile
      }
    }
  }

  return 0; // Sin colisión
}


/**
 * @brief Verifica si el tanque colisiona con el otro tanque
 * @return 1 si hay colisión, 0 si no hay
 */
static uint8_t Tank_CheckTankCollision(PlayerTank* tank, int new_x, int new_y)
{
  // Obtener el otro tanque
  PlayerTank* other = (tank->player_id == 1) ? &g_player2_tank : &g_player1_tank;

  // Si el otro tanque no está vivo, no hay colisión
  if (!other->alive) return 0;

  // Verificar superposición de rectángulos (hitbox)
  if (new_x < other->x + TANK_SPRITE_SIZE &&
      new_x + TANK_SPRITE_SIZE > other->x &&
      new_y < other->y + TANK_SPRITE_SIZE &&
      new_y + TANK_SPRITE_SIZE > other->y) {

    char dbg[64];
    snprintf(dbg, sizeof(dbg), "[COLLISION] P%d chocó con P%d\r\n",
             tank->player_id, other->player_id);
    DebugPC(dbg);

    return 1; // Hay colisión
  }

  return 0; // No hay colisión
}

/**
 * @brief Redibuja los arbustos (TILE_BUSH) que se superponen con el tanque
 * Esto crea el efecto de que el tanque pasa "por debajo" de los arbustos
 */
static void Tank_RedrawBushes(PlayerTank* tank)
{
  // Recorrer todos los tiles del mapa
  for (uint16_t i = 0; i < g_current_map.tile_count; i++) {
    MapTile* tile = &g_current_map.tiles[i];

    // Solo procesar tiles de tipo BUSH
    if (tile->type == TILE_BUSH) {
      // Verificar si el tanque se superpone con este arbusto
      if (tank->x < tile->x + tile->w &&
          tank->x + TANK_SPRITE_SIZE > tile->x &&
          tank->y < tile->y + tile->h &&
          tank->y + TANK_SPRITE_SIZE > tile->y) {

        // Redibujar el arbusto encima del tanque
        LCD_Bitmap(tile->x, tile->y, tile->w, tile->h, tile->bitmap);
      }
    }
  }
}



/**
 * @brief Actualiza la posición del tanque si está en movimiento
 */
static void Tank_Update(PlayerTank* tank)
{
  if (!tank->alive || !tank->moving) return;

  tank->prev_x = tank->x;
  tank->prev_y = tank->y;

  int new_x = tank->x;
  int new_y = tank->y;

  // Calcular nueva posición según dirección
  switch(tank->dir) {
    case DIR_UP:    new_y -= tank->speed; break;
    case DIR_DOWN:  new_y += tank->speed; break;
    case DIR_LEFT:  new_x -= tank->speed; break;
    case DIR_RIGHT: new_x += tank->speed; break;
  }

  // Verificar colisiones antes de mover
  if (!Tank_CheckCollision(tank, new_x, new_y)) {
    tank->x = new_x;
    tank->y = new_y;
  } else {
    // Si hay colisión, detener el tanque
    tank->moving = 0;
  }
}

/**
 * @brief Establece la dirección de movimiento del tanque
 */
static void Tank_Move(PlayerTank* tank, Direction dir)
{
  if (!tank->alive) return;

  tank->dir = dir;
  tank->pending_dir = dir;
  tank->moving = 1;
}

/**
 * @brief Detiene el movimiento del tanque
 */
static void Tank_Stop(PlayerTank* tank)
{
  if (!tank->alive) return;
  tank->moving = 0;
}

/**
 * @brief Dibuja el tanque en pantalla
 */
static void Tank_Draw(PlayerTank* tank)
{
  if (!tank->alive) return;

  LCD_Sprite(tank->x, tank->y,
             TANK_SPRITE_SIZE, TANK_SPRITE_SIZE,
             tank->sprite,
             TANK_SPRITE_COLS,
             tank->dir,  // El frame coincide con la dirección
             0, 0);
}

/**
 * @brief Borra el tanque de su posición anterior
 */
static void Tank_Erase(PlayerTank* tank)
{
  // Solo borrar si realmente se movió
  if (tank->prev_x != tank->x || tank->prev_y != tank->y) {
    // Llenar con color de fondo
    FillRect(tank->prev_x, tank->prev_y,
             TANK_SPRITE_SIZE, TANK_SPRITE_SIZE,
             BG_COLOR);

    // ⭐ NUEVO: Redibujar tiles que puedan estar en la posición anterior
    for (uint16_t i = 0; i < g_current_map.tile_count; i++) {
      MapTile* tile = &g_current_map.tiles[i];

      // Verificar si el tile se superpone con la posición anterior del tanque
      if (tank->prev_x < tile->x + tile->w &&
          tank->prev_x + TANK_SPRITE_SIZE > tile->x &&
          tank->prev_y < tile->y + tile->h &&
          tank->prev_y + TANK_SPRITE_SIZE > tile->y) {

        // Redibujar el tile en su posición
        if (tile->bitmap != NULL) {
          LCD_Bitmap(tile->x, tile->y, tile->w, tile->h, tile->bitmap);
        }
      }
    }
  }
}


/**
 * @brief Carga ambos jugadores en el mapa
 */
static void Game_LoadPlayers(void)
{
  DebugPC("\r\n[GAME] === CARGANDO JUGADORES ===\r\n");

  // Convertir el índice de selección a TankType
  TankType p1_type = (TankType)g_game_setup.p1_tank_type;
  TankType p2_type = (TankType)g_game_setup.p2_tank_type;

  char dbg[128];
  snprintf(dbg, sizeof(dbg),
           "[GAME] P1: Tipo %d | P2: Tipo %d\r\n",
           p1_type, p2_type);
  DebugPC(dbg);

  // Inicializar jugador 1 (cerca de base inferior)
  Tank_Init(&g_player1_tank, 1, p1_type, P1_START_X, P1_START_Y);

  // Inicializar jugador 2 (cerca de base superior)
  Tank_Init(&g_player2_tank, 2, p2_type, P2_START_X, P2_START_Y);

  // Dibujar ambos tanques
  DebugPC("[GAME] Dibujando tanques...\r\n");
  Tank_Draw(&g_player1_tank);
  Tank_Draw(&g_player2_tank);

  DebugPC("[GAME] ✓ Jugadores cargados exitosamente\r\n\r\n");
}

/**
 * @brief Actualiza el estado de ambos tanques (movimiento y dibujo)
 */
static void Game_UpdateTanks(void)
{
  // Borrar posiciones anteriores
  Tank_Erase(&g_player1_tank);
  Tank_Erase(&g_player2_tank);

  // Actualizar posiciones
  Tank_Update(&g_player1_tank);
  Tank_Update(&g_player2_tank);

  // Redibujar en nuevas posiciones
  Tank_Draw(&g_player1_tank);
  Tank_Draw(&g_player2_tank);

  // ⭐ NUEVO: Redibujar arbustos encima de los tanques
  Tank_RedrawBushes(&g_player1_tank);
  Tank_RedrawBushes(&g_player2_tank);
}


/**
 * @brief Procesa comandos de movimiento durante el juego
 */
static void ApplyGameCommand(int player_id, const char* cmd)
{
  if (player_id < 1 || player_id > 2) return;

  PlayerTank* tank = (player_id == 1) ? &g_player1_tank : &g_player2_tank;

  char dbg[96];

  if (strcmp(cmd, CMD_UP) == 0) {
    Tank_Move(tank, DIR_UP);
    snprintf(dbg, sizeof(dbg), "[P%d] Moviendo ARRIBA\r\n", player_id);
    DebugPC(dbg);
  }
  else if (strcmp(cmd, CMD_DOWN) == 0) {
    Tank_Move(tank, DIR_DOWN);
    snprintf(dbg, sizeof(dbg), "[P%d] Moviendo ABAJO\r\n", player_id);
    DebugPC(dbg);
  }
  else if (strcmp(cmd, CMD_LEFT) == 0) {
    Tank_Move(tank, DIR_LEFT);
    snprintf(dbg, sizeof(dbg), "[P%d] Moviendo IZQUIERDA\r\n", player_id);
    DebugPC(dbg);
  }
  else if (strcmp(cmd, CMD_RIGHT) == 0) {
    Tank_Move(tank, DIR_RIGHT);
    snprintf(dbg, sizeof(dbg), "[P%d] Moviendo DERECHA\r\n", player_id);
    DebugPC(dbg);
  }
  else if (strcmp(cmd, CMD_STOP) == 0) {
    Tank_Stop(tank);
    snprintf(dbg, sizeof(dbg), "[P%d] DETENIDO\r\n", player_id);
    DebugPC(dbg);
  }

  else if (strcmp(cmd, CMD_ACT_A) == 0) {
    Bullet_SpawnFromTank(tank);
    snprintf(dbg, sizeof(dbg), "[P%d] DISPARO A\r\n", player_id);
    DebugPC(dbg);
  }

  else if (strcmp(cmd, CMD_ACT_B) == 0) {
    Bullet_SpawnFromTank(tank);
    snprintf(dbg, sizeof(dbg), "[P%d] DISPARO B\r\n", player_id);
    DebugPC(dbg);
  }



}

static void ApplyMenuCommand(const char* cmd)
{
  if (strcmp(cmd, CMD_UP) == 0) {
    g_menu_index = 0;
    g_cursor.vy = -CURSOR_SPEED;
  } else if (strcmp(cmd, CMD_DOWN) == 0) {
    g_menu_index = 1;
    g_cursor.vy = CURSOR_SPEED;
  } else if (strcmp(cmd, CMD_STOP) == 0) {
    g_cursor.vy = 0;
  } else if (strcmp(cmd, CMD_ACT_A) == 0) {
    if (g_cursor.y == CURSOR_Y_OPT1 || g_cursor.y == CURSOR_Y_OPT2) {
      LCD_Clear(BG_COLOR);

      if (g_menu_index == 1) {
        DebugPC("[MENU] Modo 2 jugadores -> Pantalla de selección\r\n");
        g_state = STATE_TANK_SELECTION;

        g_p1_tank.tank_index = 0;
        g_p1_tank.selected = 0;
        g_p1_tank.explosion_frame = 0;
        g_p2_tank.tank_index = 0;
        g_p2_tank.selected = 0;
        g_p2_tank.explosion_frame = 0;

        TankSel_DrawScreen();
      } else {
        DebugPC("[MENU] Modo 1 jugador -> Estado juego\r\n");
        g_state = STATE_GAME;
      }
    }
  }
}

static void ApplyTankSelectionCommand(int player_id, const char* cmd)
{
  if (player_id < 1 || player_id > 2) return;

  PlayerTankSelection* p = (player_id == 1) ? &g_p1_tank : &g_p2_tank;

  if (p->selected) return;

  char dbg[96];

  if (strcmp(cmd, CMD_LEFT) == 0) {
    if (p->tank_index > 0) p->tank_index--;
    else p->tank_index = 3;
    snprintf(dbg, sizeof(dbg), "[P%d] LEFT -> Tank: %d\r\n", player_id, p->tank_index);
    DebugPC(dbg);
  }
  else if (strcmp(cmd, CMD_RIGHT) == 0) {
    if (p->tank_index < 3) p->tank_index++;
    else p->tank_index = 0;
    snprintf(dbg, sizeof(dbg), "[P%d] RIGHT -> Tank: %d\r\n", player_id, p->tank_index);
    DebugPC(dbg);
  }
  else if (strcmp(cmd, CMD_UP) == 0) {
    if (p->tank_index > 0) p->tank_index--;
    else p->tank_index = 3;
    snprintf(dbg, sizeof(dbg), "[P%d] UP -> Tank: %d\r\n", player_id, p->tank_index);
    DebugPC(dbg);
  }
  else if (strcmp(cmd, CMD_DOWN) == 0) {
    if (p->tank_index < 3) p->tank_index++;
    else p->tank_index = 0;
    snprintf(dbg, sizeof(dbg), "[P%d] DOWN -> Tank: %d\r\n", player_id, p->tank_index);
    DebugPC(dbg);
  }
  else if (strcmp(cmd, CMD_ACT_B) == 0) {
    p->selected = 1;
    p->explosion_frame = 0;
    p->last_explo_ms = HAL_GetTick();

    int x = (player_id == 1) ? P1_TANK_X : P2_TANK_X;
    int y = (player_id == 1) ? P1_TANK_Y : P2_TANK_Y;
    TankSel_EraseTank(x, y);

    snprintf(dbg, sizeof(dbg), "[P%d] ACT_B -> Tanque seleccionado: %d\r\n", player_id, p->tank_index);
    DebugPC(dbg);
  }
}

static void ApplyMapSelectionCommand(const char* cmd)
{
  char dbg[64];

  if (strcmp(cmd, CMD_UP) == 0) {
    g_map_menu_index = 0;
    g_map_selector.vy = -CURSOR_SPEED;
    DebugPC("[MAP] UP -> Mapa 1\r\n");
  }
  else if (strcmp(cmd, CMD_DOWN) == 0) {
    g_map_menu_index = 1;
    g_map_selector.vy = CURSOR_SPEED;
    DebugPC("[MAP] DOWN -> Mapa 2\r\n");
  }
  else if (strcmp(cmd, CMD_STOP) == 0) {
    g_map_selector.vy = 0;
    DebugPC("[MAP] STOP\r\n");
  }
  else if (strcmp(cmd, CMD_ACT_A) == 0) {  // SOLO ACT_A
    snprintf(dbg, sizeof(dbg), "[MAP] ACT_A presionado en posición: %d\r\n", g_map_menu_index);
    DebugPC(dbg);

    MapSel_ConfirmSelection();
  }
  else if (strcmp(cmd, CMD_ACT_B) == 0) {  // IGNORAR ACT_B
    DebugPC("[MAP] ACT_B ignorado en selección de mapa\r\n");
  }
  else {
    snprintf(dbg, sizeof(dbg), "[MAP] Comando desconocido: %s\r\n", cmd);
    DebugPC(dbg);
  }
}


// ===== FUNCIONES DE MENÚ INICIO =====
static void Menu_DrawBackground(void)
{
  LCD_Bitmap(START_X, START_Y, START_W, START_H, pantalla_inicio);
}

static void Menu_UpdateCursor(void)
{
  g_cursor.prev_x = g_cursor.x;
  g_cursor.prev_y = g_cursor.y;

  int target_y = (g_menu_index == 0) ? CURSOR_Y_OPT1 : CURSOR_Y_OPT2;

  if (abs(g_cursor.y - target_y) <= CURSOR_SPEED) {
    g_cursor.y = target_y;
    g_cursor.vy = 0;
  } else {
    g_cursor.y += (g_cursor.y < target_y) ? CURSOR_SPEED : -CURSOR_SPEED;
  }

  if (g_cursor.y < 0) g_cursor.y = 0;
  if (g_cursor.y > (LCD_H - MENU_SPRITE_H)) g_cursor.y = (LCD_H - MENU_SPRITE_H);

  uint32_t now = HAL_GetTick();
  if (now - g_cursor.last_anim_ms >= MENU_ANIM_MS) {
    g_cursor.anim ^= 1;
    g_cursor.last_anim_ms = now;
  }
}

static void Menu_EraseCursorPrev(void)
{
  FillRect(g_cursor.prev_x, g_cursor.prev_y, MENU_SPRITE_W, MENU_SPRITE_H, BG_COLOR);
}

static void Menu_DrawCursor(void)
{
  LCD_Sprite(g_cursor.x, g_cursor.y, MENU_SPRITE_W, MENU_SPRITE_H,
             tanque_inicio, MENU_SPRITE_COLS, g_cursor.anim, 0, 0);
}

// ===== FUNCIONES DE SELECCIÓN DE TANQUES =====
static void TankSel_DrawScreen(void)
{
  LCD_Bitmap(TITLE_X, TITLE_Y, TITLE_W, TITLE_H, elegir_tanque);
  TankSel_DrawTank(P1_TANK_X, P1_TANK_Y, g_p1_tank.tank_index);
  TankSel_DrawTank(P2_TANK_X, P2_TANK_Y, g_p2_tank.tank_index);
  LCD_Bitmap(P1_LABEL_X, P1_LABEL_Y, LABEL_W, LABEL_H, player1);
  LCD_Bitmap(P2_LABEL_X, P2_LABEL_Y, LABEL_W, LABEL_H, player2);

  // Borde superior
  for (int x = 0; x < LCD_W; x += BLOCK_SIZE) {
    LCD_Bitmap(x, 0, BLOCK_SIZE, BLOCK_SIZE, bloquegris_8_8);
  }

  // Borde inferior
  for (int x = 0; x < LCD_W; x += BLOCK_SIZE) {
    LCD_Bitmap(x, LCD_H - BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, bloquegris_8_8);
  }

}

static void TankSel_EraseTank(int x, int y)
{
  FillRect(x, y, TANK_SPRITE_W, TANK_SPRITE_H, BG_COLOR);
}

static void TankSel_DrawTank(int x, int y, uint8_t tank_index)
{
  LCD_Sprite(x, y, TANK_SPRITE_W, TANK_SPRITE_H,
             seleccion_tanque, TANK_SPRITE_COLS, tank_index, 0, 0);
}

static void TankSel_DrawExplosion(int x, int y, uint8_t frame)
{
  LCD_Sprite(x, y, EXPLO_SPRITE_W, EXPLO_SPRITE_H,
             seleccion, EXPLO_SPRITE_COLS, frame, 0, 0);
}

static void TankSel_UpdatePlayer(PlayerTankSelection* p, int x, int y)
{
  if (p->selected) {
    uint32_t now = HAL_GetTick();

    if (p->explosion_frame < 2 && (now - p->last_explo_ms) >= EXPLO_ANIM_MS) {
      p->explosion_frame++;
      p->last_explo_ms = now;
    }

    TankSel_DrawExplosion(x, y, p->explosion_frame);
  }
}

// ===== FUNCIONES DE SELECCIÓN DE MAPAS =====
static void MapSel_DrawScreen(void)
{
  // Dibujar borde decorativo
  MapSel_DrawBorder();

  // Título centrado
  LCD_Bitmap(MAP_TITLE_X, MAP_TITLE_Y, MAP_TITLE_W, MAP_TITLE_H, elige_mapa);

  // Opciones de mapas centradas
  LCD_Bitmap(MAP_NAME_X, MAP1_Y, MAP_NAME_W, MAP_NAME_H, mapa_1);
  LCD_Bitmap(MAP_NAME_X, MAP2_Y, MAP_NAME_W, MAP_NAME_H, mapa_2);

  // ESPACIO PARA DECORACIÓN ADICIONAL
  LCD_Bitmap(0, 168, 16, 16, monte);
  LCD_Bitmap(0, 184, 16, 16, monte);
  LCD_Bitmap(16, 184, 16, 16, monte);
  LCD_Bitmap(16, 200, 16, 16, monte);
  LCD_Bitmap(32, 200, 16, 16, monte);
  LCD_Bitmap(32, 216, 16, 16, monte);
  LCD_Bitmap(48, 216, 16, 16, monte);

  LCD_Bitmap(0, 216, 16, 16, bloquegris_16_16);
  LCD_Bitmap(0, 200, 16, 16, bloquegris_16_16);
  LCD_Bitmap(16, 216, 16, 16, bloquegris_16_16);

  LCD_Bitmap(304, 168, 16, 16, monte);
  LCD_Bitmap(304, 184, 16, 16, monte);
  LCD_Bitmap(288, 184, 16, 16, monte);
  LCD_Bitmap(288, 200, 16, 16, monte);
  LCD_Bitmap(272, 200, 16, 16, monte);
  LCD_Bitmap(272, 216, 16, 16, monte);
  LCD_Bitmap(256, 216, 16, 16, monte);

  LCD_Bitmap(304, 216, 16, 16, bloquegris_16_16);
  LCD_Bitmap(304, 200, 16, 16, bloquegris_16_16);
  LCD_Bitmap(288, 216, 16, 16, bloquegris_16_16);

  LCD_Bitmap(144, 208, 32, 16, casita_ladrillo);
  LCD_Bitmap(152, 216, 16, 16, aguila);
  LCD_Bitmap(144, 224, 8, 8, bloqueladrillo_8_8);
  LCD_Bitmap(168, 224, 8, 8, bloqueladrillo_8_8);
  // Aquí puedes agregar más imágenes decorativas, por ejemplo:
  // LCD_Bitmap(x, y, width, height, tu_imagen_decorativa);

  // Selector inicial
  g_map_selector.x = MAP_SELECTOR_X;
  g_map_selector.y = MAP_SELECTOR_Y1;
  g_map_selector.prev_x = MAP_SELECTOR_X;
  g_map_selector.prev_y = MAP_SELECTOR_Y1;
  g_map_selector.anim = 0;
  g_map_menu_index = 0;
}

static void MapSel_DrawBorder(void)
{
  // Borde superior
  for (int x = 0; x < LCD_W; x += BLOCK_SIZE) {
    LCD_Bitmap(x, 0, BLOCK_SIZE, BLOCK_SIZE, bloqueladrillo_8_8);
  }

  // Borde inferior
  for (int x = 0; x < LCD_W; x += BLOCK_SIZE) {
    LCD_Bitmap(x, LCD_H - BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, bloqueladrillo_8_8);
  }

  /*// Borde izquierdo
  for (int y = BLOCK_SIZE; y < LCD_H - BLOCK_SIZE; y += BLOCK_SIZE) {
    LCD_Bitmap(0, y, BLOCK_SIZE, BLOCK_SIZE, bloque_8_8);
  }

  // Borde derecho
  for (int y = BLOCK_SIZE; y < LCD_H - BLOCK_SIZE; y += BLOCK_SIZE) {
    LCD_Bitmap(LCD_W - BLOCK_SIZE, y, BLOCK_SIZE, BLOCK_SIZE, bloque_8_8);
  }*/
}

static void MapSel_UpdateSelector(void)
{
  g_map_selector.prev_x = g_map_selector.x;
  g_map_selector.prev_y = g_map_selector.y;

  int target_y = (g_map_menu_index == 0) ? MAP_SELECTOR_Y1 : MAP_SELECTOR_Y2;

  if (abs(g_map_selector.y - target_y) <= CURSOR_SPEED) {
    g_map_selector.y = target_y;
    g_map_selector.vy = 0;
  } else {
    g_map_selector.y += (g_map_selector.y < target_y) ? CURSOR_SPEED : -CURSOR_SPEED;
  }

  // Animación del selector
  uint32_t now = HAL_GetTick();
  if (now - g_map_selector.last_anim_ms >= MAP_SELECTOR_ANIM_MS) {
    g_map_selector.anim ^= 1;
    g_map_selector.last_anim_ms = now;
  }
}

static void MapSel_EraseSelector(void)
{
  FillRect(g_map_selector.prev_x, g_map_selector.prev_y,
           MAP_SELECTOR_W, MAP_SELECTOR_H, BG_COLOR);
}

static void MapSel_DrawSelector(void)
{
  LCD_Sprite(g_map_selector.x, g_map_selector.y,
             MAP_SELECTOR_W, MAP_SELECTOR_H,
             tanque_mapa, MAP_SELECTOR_COLS, g_map_selector.anim, 0, 0);
}

static void MapSel_ConfirmSelection(void)
{
  // Detener el selector antes de hacer cualquier cosa
  g_map_selector.vy = 0;

  g_selected_map = g_map_menu_index;

  // Guardar configuración del juego
  g_game_setup.p1_tank_type = g_p1_tank.tank_index;
  g_game_setup.p2_tank_type = g_p2_tank.tank_index;
  g_game_setup.map_id = g_selected_map;
  g_game_setup.ready = 1;

  char dbg[128];
  snprintf(dbg, sizeof(dbg),
           "[CONFIRM] Mapa seleccionado: %d\r\n"
           "[GAME SETUP] P1 Tank:%d | P2 Tank:%d\r\n",
           g_selected_map + 1,
           g_game_setup.p1_tank_type,
           g_game_setup.p2_tank_type);
  DebugPC(dbg);

  // SIN ANIMACIÓN - Directo a limpiar pantalla
  DebugPC("[CONFIRM] Limpiando pantalla...\r\n");

  // Limpiar pantalla con color negro
  LCD_Clear(BG_COLOR);

  DebugPC("[CONFIRM] Pantalla limpiada\r\n");

  snprintf(dbg, sizeof(dbg), "[MAP] Iniciando Mapa %d...\r\n", g_selected_map + 1);
  DebugPC(dbg);


  // Transición al estado de juego
  DebugPC("[STATE] Cambiando a STATE_GAME\r\n");
  game_initialized = 0;
  g_state = STATE_GAME;

  DebugPC("[STATE] Estado cambiado exitosamente\r\n");
}


static void Game_Initialize(void)
{
  char dbg[256];

  DebugPC("\r\n");
  DebugPC("╔════════════════════════════════════════╗\r\n");
  DebugPC("║     INICIALIZANDO JUEGO                ║\r\n");
  DebugPC("╚════════════════════════════════════════╝\r\n");

  snprintf(dbg, sizeof(dbg),
           "  P1: Tanque Tipo %d\r\n"
           "  P2: Tanque Tipo %d\r\n"
           "  Mapa ID: %d\r\n",
           g_game_setup.p1_tank_type,
           g_game_setup.p2_tank_type,
           g_game_setup.map_id);
  DebugPC(dbg);

  // Cargar el mapa correspondiente
  switch(g_game_setup.map_id) {
    case MAP_1_ID:
      DebugPC("\r\n[GAME] >>> Cargando Mapa 1 <<<\r\n");
      Map1_Load();
      Audio_Play(MUS_MAP1);   // en el caso MAP_1_ID
      DebugPC("[GAME] >>> Mapa 1 cargado <<<\r\n");
      break;

    case MAP_2_ID:
      DebugPC("\r\n[GAME] >>> Cargando Mapa 2 <<<\r\n");
      Map2_Load();
      Audio_Play(MUS_MAP2);   // en el caso MAP_2_ID
      DebugPC("[GAME] >>> Mapa 2 cargado <<<\r\n");
      break;

    default:
      DebugPC("[ERROR] ID de mapa inválido\r\n");
      break;
  }

  Game_LoadPlayers();

  DebugPC("\r\n[GAME] ✓✓✓ Inicialización completa ✓✓✓\r\n\r\n");
}

static uint8_t ValidateGameSetup(void)
{
  if (g_game_setup.p1_tank_type > 3 || g_game_setup.p2_tank_type > 3) {
    DebugPC("[ERROR] Tipo de tanque inválido\r\n");
    return 0;
  }

  if (g_game_setup.map_id > 1) {
    DebugPC("[ERROR] ID de mapa inválido\r\n");
    return 0;
  }

  if (!g_game_setup.ready) {
    DebugPC("[ERROR] Configuración de juego no está lista\r\n");
    return 0;
  }

  return 1;
}

static void Game_ShowWinner(uint8_t winner_id)
{
  if (g_state != STATE_GAME) return; // evita re-entradas
  g_winner = winner_id;

  LCD_Clear(BG_COLOR);

  // Imagen "ganador" centrada
  int gx = (LCD_W - GANADOR_W)/2;
  int gy = (LCD_H - GANADOR_H)/2;
  LCD_Bitmap(gx, gy, GANADOR_W, GANADOR_H, ganador);

  // Nombre debajo
  int nx = (LCD_W - NAME_W)/2;
  int ny = gy + GANADOR_H + 2;
  if (g_winner == 1) {
    LCD_Bitmap(nx, ny, NAME_W, NAME_H, player1);
  } else {
    LCD_Bitmap(nx, ny, NAME_W, NAME_H, player2);
  }

  g_state = STATE_GAME_OVER;
}

static void Win_PlayExplosionThenShow(uint32_t ms, uint8_t winner_id)
{
  // Reproduce la animación de explosiones durante 'ms' antes de mostrar ganador
  uint32_t t0 = HAL_GetTick();
  while ((HAL_GetTick() - t0) < ms) {
    Explo_UpdateAndDrawAll();
    HAL_Delay(16); // ~60 fps
  }

  // Asegura que no quede ninguna explosión visible
  Explo_KillAll();

  Game_ShowWinner(winner_id);
}



static void Game_ResetToMenu(void)
{
  g_winner = 0;
  memset(&g_bullet_p1, 0, sizeof(g_bullet_p1));
  memset(&g_bullet_p2, 0, sizeof(g_bullet_p2));
  memset(&g_player1_tank, 0, sizeof(g_player1_tank));
  memset(&g_player2_tank, 0, sizeof(g_player2_tank));
  memset(&g_current_map, 0, sizeof(g_current_map));
  for (int i=0;i<MAX_EXPLOSIONS;i++) g_explosions[i].active = 0;

  LCD_Clear(BG_COLOR);
  Menu_DrawBackground();
  Audio_Play(MUS_INTRO);
  g_state = STATE_MENU;

  g_game_setup.ready = 0;
  g_p1_tank = (PlayerTankSelection){0,0,0,0};
  g_p2_tank = (PlayerTankSelection){0,0,0,0};
  g_map_menu_index = 0;
  g_selected_map = 0;
  game_initialized = 0;

}



/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
  LCD_Init();
  LCD_Clear(0x0000);
  Menu_DrawBackground();
  Audio_Play(MUS_INTRO);

  DebugPC("\r\n=== BATTLE CITY - Menu Inicio ===\r\n");
  StartUart1RxIT();
  /* USER CODE END 2 */

  uint8_t prev_p1_index = 0;
  uint8_t prev_p2_index = 0;

  while (1) {

	  static GameState prev_state = STATE_MENU;
	  if (g_state != prev_state) {
	    char dbg[64];
	    snprintf(dbg, sizeof(dbg), "[MAIN] Cambio de estado: %d -> %d\r\n", prev_state, g_state);
	    DebugPC(dbg);
	    prev_state = g_state;
	  }

    switch (g_state) {
      case STATE_MENU: {
        Menu_UpdateCursor();
        Menu_EraseCursorPrev();
        Menu_DrawCursor();
      } break;

      case STATE_TANK_SELECTION: {
        if (g_p1_tank.selected && g_p2_tank.selected &&
            g_p1_tank.explosion_frame >= 2 && g_p2_tank.explosion_frame >= 2) {
          HAL_Delay(500);
          LCD_Clear(BG_COLOR);
          DebugPC("[SELECTION] Tanques seleccionados -> Selección de mapa\r\n");

          // Transición a selección de mapas
          g_state = STATE_MAP_SELECTION;
          MapSel_DrawScreen();
          break;
        }

        if (!g_p1_tank.selected) {
          if (prev_p1_index != g_p1_tank.tank_index) {
            TankSel_EraseTank(P1_TANK_X, P1_TANK_Y);
            TankSel_DrawTank(P1_TANK_X, P1_TANK_Y, g_p1_tank.tank_index);
            prev_p1_index = g_p1_tank.tank_index;
          }
        } else {
          TankSel_UpdatePlayer(&g_p1_tank, P1_TANK_X, P1_TANK_Y);
        }

        if (!g_p2_tank.selected) {
          if (prev_p2_index != g_p2_tank.tank_index) {
            TankSel_EraseTank(P2_TANK_X, P2_TANK_Y);
            TankSel_DrawTank(P2_TANK_X, P2_TANK_Y, g_p2_tank.tank_index);
            prev_p2_index = g_p2_tank.tank_index;
          }
        } else {
          TankSel_UpdatePlayer(&g_p2_tank, P2_TANK_X, P2_TANK_Y);
        }
      } break;

      case STATE_MAP_SELECTION: {
        // Solo actualizar si NO estamos confirmando
        if (g_state == STATE_MAP_SELECTION) {  // Verificación adicional
          MapSel_UpdateSelector();
          MapSel_EraseSelector();
          MapSel_DrawSelector();
        }
      } break;

      case STATE_GAME: {

        // ⭐ Inicializar el juego solo UNA VEZ
        if (!game_initialized) {
          DebugPC("[GAME] Entrando al estado de juego por primera vez\r\n");

          if (!g_game_setup.ready) {
            DebugPC("[ERROR] Intento de iniciar juego sin configuración completa\r\n");
            g_state = STATE_MENU;
            game_initialized = 0;
            LCD_Clear(BG_COLOR);
            Menu_DrawBackground();
            break;
          }

          if (!ValidateGameSetup()) {
            DebugPC("[ERROR] Validación de setup falló. Regresando al menú.\r\n");
            g_state = STATE_MENU;
            g_game_setup.ready = 0;
            game_initialized = 0;
            LCD_Clear(BG_COLOR);
            Menu_DrawBackground();
            break;
          }

          DebugPC("[GAME] Validación OK, llamando a Game_Initialize()...\r\n");
          Game_Initialize();
          game_initialized = 1;
          DebugPC("[GAME] Inicialización completa\r\n");
        }

        Game_UpdateTanks();

        // Balas y explosiones
        Bullet_Update(&g_bullet_p1);
        Bullet_Update(&g_bullet_p2);
        Explo_UpdateAndDrawAll();

        // ===== LOOP PRINCIPAL DEL JUEGO =====
        // Aquí irá la lógica del juego (movimiento de tanques, balas, etc.)

        // Debug periódico
        static uint32_t last_debug = 0;
        uint32_t now = HAL_GetTick();
        if (now - last_debug > 2000) {
          char dbg[128];
          snprintf(dbg, sizeof(dbg),
                   "[GAME LOOP] Map:%d, Tiles:%d, P1:%d, P2:%d\r\n",
                   g_game_setup.map_id + 1,
                   g_current_map.tile_count,
                   g_game_setup.p1_tank_type,
                   g_game_setup.p2_tank_type);
          DebugPC(dbg);
          last_debug = now;
        }

      } break;

      case STATE_GAME_OVER: {
        // Ya se dibujó la pantalla en Game_ShowWinner();
        // Acá no hacemos nada: solo esperamos ACT_A / ACT_B para volver al menú.
      } break;
    }

    HAL_Delay(16); // ~60 FPS
  }
}


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_CS_Pin|SD_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_RESET_Pin */
  GPIO_InitStruct.Pin = LCD_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(LCD_RESET_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_DC_Pin */
  GPIO_InitStruct.Pin = LCD_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(LCD_DC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_CS_Pin SD_CS_Pin */
  GPIO_InitStruct.Pin = LCD_CS_Pin|SD_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    // Acumular hasta '\n'
    if (rx_line_len < sizeof(rx_line_buf)-1) {
      rx_line_buf[rx_line_len++] = uart1_rx_byte;
    } else {
      rx_line_len = 0; // overflow -> reset
    }

    if (uart1_rx_byte == '\n') {
      rx_line_buf[rx_line_len] = '\0';
      ProcessCommandLine((char*)rx_line_buf);
      rx_line_len = 0;
    }

    // Re-armar recepción
    HAL_UART_Receive_IT(&huart1, &uart1_rx_byte, 1);
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1) {}
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
