// =============================================================
//  PAC-MAN - ESP32 Console Project (For Batikan)
//  Hardware: ESP32 30-pin + 1.8" ST7735 TFT + 4x4 Keypad
// =============================================================
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <Keypad.h>

// ===== Pins =====
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4

// ===== TFT =====
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// ===== Colors (BGR panel - swapped) =====
#define COL_BLACK    0x0000
#define COL_WHITE    0xFFFF
#define COL_YELLOW   0x07FF
#define COL_BLUE     0xF800
#define COL_RED      0x001F
#define COL_PINK     0x041F
#define COL_CYAN     0xFFE0
#define COL_ORANGE   0x021F
#define COL_GREEN    0x07E0
#define COL_DARKBLUE 0x7800

// ===== 4x4 Keypad =====
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {25, 26, 27, 32};
byte colPins[COLS] = {22, 21, 14, 13};   // 33 -> 21 (pin 33 might be faulty)
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== Maze =====
const int CELL_SIZE      = 8;
const int GRID_COLS      = 20;
const int GRID_ROWS      = 14;
const int MAZE_Y_OFFSET  = 16;  // Score bar height

// '#' wall, '.' pellet, 'o' power pellet, ' ' empty corridor
const char mazeOrig[GRID_ROWS][GRID_COLS + 1] = {
  "####################",
  "#........##........#",
  "#o##.###.##.###.##o#",
  "#.##.###.##.###.##.#",
  "#..................#",
  "#.##.#.######.#.##.#",
  "#....#...##...#....#",
  "####.###.##.###.####",
  "####.###.##.###.####",
  "#....#...##...#....#",
  "#.##.#.######.#.##.#",
  "#..................#",
  "#o.......##.......o#",
  "####################"
};

bool pellets[GRID_ROWS][GRID_COLS];
bool powerPellets[GRID_ROWS][GRID_COLS];
int totalPellets = 0;

// ===== Game States =====
enum GameState { S_INTRO, S_WELCOME, S_PLAYING, S_GAMEOVER, S_WIN };
GameState gameState = S_INTRO;
unsigned long stateStartTime = 0;

// ===== Pac-Man =====
int pacRow, pacCol;
int pacX, pacY;
int pacTargetX, pacTargetY;
char pacDir = 'L';
char pacQueuedDir = 0;
bool pacMoving = false;

unsigned long pacLastMove = 0;
const int PAC_SPEED_MS = 35;   // ms per pixel

int chompFrame = 0;
unsigned long lastChompMs = 0;
const int CHOMP_MS = 130;

// ===== Ghosts =====
struct Ghost {
  int row, col;
  int x, y;
  int targetX, targetY;
  char dir;
  bool moving;
  uint16_t color;
  bool vulnerable;
  int aiType;   // 0=chase, 1=ambush, 2=random
  int spawnRow, spawnCol;
};
const int NUM_GHOSTS = 3;
Ghost ghosts[NUM_GHOSTS];

unsigned long ghostLastMove = 0;
const int GHOST_SPEED_MS = 50;

// ===== Game Variables =====
int score = 0;
int lives = 3;
bool powerMode = false;
unsigned long powerStartTime = 0;
const unsigned long POWER_DURATION = 6500;  // 6.5 sec

// ===== Animation Timers =====
unsigned long lastIntroBlink = 0;
bool introBlinkOn = true;
unsigned long lastPowerBlink = 0;
bool powerBlinkOn = true;

// =============================================================
//                        SETUP & LOOP
// =============================================================
void setup() {
  keypad.setDebounceTime(20); // Default is usually 50ms, 20ms speeds up response.
  Serial.begin(115200);
  randomSeed(analogRead(34));   // Use an unused analog pin

  tft.initR(INITR_GREENTAB);
  tft.setRotation(1);
  tft.fillScreen(COL_BLACK);

  enterState(S_INTRO);
}

void loop() {
  switch (gameState) {
    case S_INTRO:    handleIntro();    break;
    case S_WELCOME:  handleWelcome();  break;
    case S_PLAYING:  handlePlaying();  break;
    case S_GAMEOVER: handleGameOver(); break;
    case S_WIN:      handleWin();      break;
  }
}

void enterState(GameState s) {
  gameState = s;
  stateStartTime = millis();

  switch (s) {
    case S_INTRO:
      tft.fillScreen(COL_BLACK);
      drawIntroStatic();
      break;
    case S_WELCOME:
      tft.fillScreen(COL_BLACK);
      drawWelcomeStatic();
      break;
    case S_PLAYING:
      resetGame();
      drawGameStatic();
      break;
    case S_GAMEOVER:
      drawGameOverScreen();
      break;
    case S_WIN:
      drawWinScreen();
      break;
  }
}

// =============================================================
//                             INTRO
// =============================================================
void drawIntroStatic() {
  // Large title "PAC-MAN"
  tft.setTextSize(2);
  tft.setTextColor(COL_YELLOW);
  tft.setCursor(35, 18);
  tft.print("PAC-MAN");

  // Small decorative ghosts
  drawDecoGhost(20, 55, COL_RED);
  drawDecoGhost(50, 55, COL_PINK);
  drawDecoGhost(80, 55, COL_CYAN);
  drawDecoGhost(110, 55, COL_ORANGE);

  // Large decorative Pac-Man
  int cx = 80, cy = 85;
  tft.fillCircle(cx, cy, 8, COL_YELLOW);
  tft.fillTriangle(cx, cy, cx + 10, cy - 6, cx + 10, cy + 6, COL_BLACK);
}

void handleIntro() {
  // "Press a key" text blinking
  if (millis() - lastIntroBlink > 450) {
    lastIntroBlink = millis();
    introBlinkOn = !introBlinkOn;
    tft.setTextSize(1);
    tft.setCursor(28, 110);
    tft.setTextColor(introBlinkOn ? COL_WHITE : COL_BLACK);
    tft.print("Press any key!");
  }

  char k = keypad.getKey();
  if (k) {
    Serial.print("INTRO: Key pressed -> '");
    Serial.print(k);
    Serial.println("'");
    enterState(S_WELCOME);
  }
}

// Decorative ghost (8x10)
void drawDecoGhost(int x, int y, uint16_t color) {
  tft.fillCircle(x + 4, y + 3, 4, color);
  tft.fillRect(x, y + 3, 9, 5, color);
  // Leg zig-zag
  tft.drawPixel(x,     y + 8, color);
  tft.drawPixel(x + 2, y + 8, color);
  tft.drawPixel(x + 4, y + 8, color);
  tft.drawPixel(x + 6, y + 8, color);
  tft.drawPixel(x + 8, y + 8, color);
  // Eyes
  tft.fillRect(x + 2, y + 3, 2, 2, COL_WHITE);
  tft.fillRect(x + 5, y + 3, 2, 2, COL_WHITE);
  tft.drawPixel(x + 3, y + 4, COL_BLUE);
  tft.drawPixel(x + 6, y + 4, COL_BLUE);
}

// =============================================================
//                            WELCOME
// =============================================================
void drawWelcomeStatic() {
  tft.setTextSize(1);
  tft.setTextColor(COL_WHITE);
  tft.setCursor(38, 30);
  tft.print("Welcome");

  tft.setTextSize(2);
  tft.setTextColor(COL_YELLOW);
  tft.setCursor(30, 55);
  tft.print("BATIKAN");

  tft.setTextSize(1);
  tft.setTextColor(COL_CYAN);
  tft.setCursor(20, 90);
  tft.print("Starting game...");
}

void handleWelcome() {
  if (millis() - stateStartTime > 2500) {
    enterState(S_PLAYING);
  }
}

// =============================================================
//                         GAME INITIALIZATION
// =============================================================
void resetGame() {
  score = 0;
  lives = 3;
  powerMode = false;
  initMazePellets();
  initPacMan();
  initGhosts();
}

void initMazePellets() {
  totalPellets = 0;
  for (int r = 0; r < GRID_ROWS; r++) {
    for (int c = 0; c < GRID_COLS; c++) {
      char ch = mazeOrig[r][c];
      pellets[r][c] = (ch == '.');
      powerPellets[r][c] = (ch == 'o');
      if (ch == '.' || ch == 'o') totalPellets++;
    }
  }
}

void initPacMan() {
  pacRow = 11;
  pacCol = 10;
  pacX = pacCol * CELL_SIZE;
  pacY = MAZE_Y_OFFSET + pacRow * CELL_SIZE;
  pacTargetX = pacX;
  pacTargetY = pacY;
  pacDir = 'L';
  pacQueuedDir = 0;
  pacMoving = false;
}

void initGhosts() {
  int spawnCols[NUM_GHOSTS] = {9, 10, 11};
  uint16_t colors[NUM_GHOSTS] = {COL_RED, COL_PINK, COL_CYAN};
  for (int i = 0; i < NUM_GHOSTS; i++) {
    ghosts[i].spawnRow = 4;
    ghosts[i].spawnCol = spawnCols[i];
    ghosts[i].row = ghosts[i].spawnRow;
    ghosts[i].col = ghosts[i].spawnCol;
    ghosts[i].x = ghosts[i].col * CELL_SIZE;
    ghosts[i].y = MAZE_Y_OFFSET + ghosts[i].row * CELL_SIZE;
    ghosts[i].targetX = ghosts[i].x;
    ghosts[i].targetY = ghosts[i].y;
    ghosts[i].dir = 'D';
    ghosts[i].moving = false;
    ghosts[i].vulnerable = false;
    ghosts[i].aiType = i;   // 0=chase, 1=ambush, 2=random
    ghosts[i].color = colors[i];
  }
}

void drawGameStatic() {
  tft.fillScreen(COL_BLACK);
  drawMaze();
  drawAllPellets();
  drawScoreBar();
  drawPacMan();
  for (int i = 0; i < NUM_GHOSTS; i++) drawGhost(i);
}

// =============================================================
//                          PLAYING - LOOP
// =============================================================
void handlePlaying() {
  // Key read
  char key = keypad.getKey();
  if (key) {
    Serial.print("GAME: Key '");
    Serial.print(key);
    Serial.print("' -> ");
    switch (key) {
      case '2': pacQueuedDir = 'U'; Serial.println("UP"); break;
      case '8': pacQueuedDir = 'D'; Serial.println("DOWN"); break;
      case '4': pacQueuedDir = 'L'; Serial.println("LEFT"); break;
      case '6': pacQueuedDir = 'R'; Serial.println("RIGHT"); break;
      default:  Serial.println("(not a direction)"); break;
    }
  }

  // Pac-Man movement
  if (millis() - pacLastMove >= PAC_SPEED_MS) {
    pacLastMove = millis();
    updatePacMan();
  }

  // Ghosts movement
  if (millis() - ghostLastMove >= GHOST_SPEED_MS) {
    ghostLastMove = millis();
    for (int i = 0; i < NUM_GHOSTS; i++) updateGhost(i);
  }

  // Chomp animation
  if (millis() - lastChompMs >= CHOMP_MS) {
    lastChompMs = millis();
    chompFrame = 1 - chompFrame;
    erasePacMan();
    drawPacMan();
  }

  // Power mode end check
  if (powerMode && millis() - powerStartTime > POWER_DURATION) {
    powerMode = false;
    for (int i = 0; i < NUM_GHOSTS; i++) {
      ghosts[i].vulnerable = false;
      eraseGhost(i);
      drawGhost(i);
    }
  }

  // Collision check
  checkCollisions();

  // Win check
  if (totalPellets == 0) {
    enterState(S_WIN);
  }
}

// =============================================================
//                        PAC-MAN MOVEMENT
// =============================================================
void updatePacMan() {
  if (pacMoving) {
    erasePacMan();
    if (pacX < pacTargetX) pacX++;
    else if (pacX > pacTargetX) pacX--;
    if (pacY < pacTargetY) pacY++;
    else if (pacY > pacTargetY) pacY--;
    drawPacMan();

    if (pacX == pacTargetX && pacY == pacTargetY) {
      pacMoving = false;
      eatPelletAtCurrent();
    }
  } else {
    // Reached target cell, decide direction
    char tryDir = (pacQueuedDir != 0 && canStep(pacRow, pacCol, pacQueuedDir))
                  ? pacQueuedDir : pacDir;
    if (canStep(pacRow, pacCol, tryDir)) {
      pacDir = tryDir;
      if (tryDir == pacQueuedDir) pacQueuedDir = 0;
      startStep();
    }
  }
}

void eatPelletAtCurrent() {
  if (pellets[pacRow][pacCol]) {
    pellets[pacRow][pacCol] = false;
    totalPellets--;
    score += 10;
    drawScoreBar();
  }
  if (powerPellets[pacRow][pacCol]) {
    powerPellets[pacRow][pacCol] = false;
    totalPellets--;
    score += 50;
    powerMode = true;
    powerStartTime = millis();
    for (int i = 0; i < NUM_GHOSTS; i++) {
      ghosts[i].vulnerable = true;
      eraseGhost(i);
      drawGhost(i);
    }
    drawScoreBar();
  }
}

void startStep() {
  int nr = pacRow, nc = pacCol;
  switch (pacDir) {
    case 'U': nr--; break;
    case 'D': nr++; break;
    case 'L': nc--; break;
    case 'R': nc++; break;
  }
  if (!inBounds(nr, nc) || isWall(nr, nc)) return;
  pacRow = nr;
  pacCol = nc;
  pacTargetX = pacCol * CELL_SIZE;
  pacTargetY = MAZE_Y_OFFSET + pacRow * CELL_SIZE;
  pacMoving = true;
}

bool canStep(int r, int c, char dir) {
  int nr = r, nc = c;
  switch (dir) {
    case 'U': nr--; break;
    case 'D': nr++; break;
    case 'L': nc--; break;
    case 'R': nc++; break;
  }
  return inBounds(nr, nc) && !isWall(nr, nc);
}

bool inBounds(int r, int c) {
  return r >= 0 && r < GRID_ROWS && c >= 0 && c < GRID_COLS;
}

bool isWall(int r, int c) {
  if (!inBounds(r, c)) return true;
  return mazeOrig[r][c] == '#';
}

// =============================================================
//                            GHOST AI
// =============================================================
void updateGhost(int idx) {
  Ghost& g = ghosts[idx];
  if (g.moving) {
    eraseGhost(idx);
    if (g.x < g.targetX) g.x++;
    else if (g.x > g.targetX) g.x--;
    if (g.y < g.targetY) g.y++;
    else if (g.y > g.targetY) g.y--;
    drawGhost(idx);
    if (g.x == g.targetX && g.y == g.targetY) g.moving = false;
  } else {
    char newDir = pickGhostDir(g);
    g.dir = newDir;
    int nr = g.row, nc = g.col;
    switch (newDir) {
      case 'U': nr--; break;
      case 'D': nr++; break;
      case 'L': nc--; break;
      case 'R': nc++; break;
    }
    if (inBounds(nr, nc) && !isWall(nr, nc)) {
      g.row = nr;
      g.col = nc;
      g.targetX = g.col * CELL_SIZE;
      g.targetY = MAZE_Y_OFFSET + g.row * CELL_SIZE;
      g.moving = true;
    }
  }
}

char pickGhostDir(Ghost& g) {
  char reverse = oppositeDir(g.dir);
  char dirs[4] = {'U', 'D', 'L', 'R'};

  long bestScore = 0x7FFFFFFF;
  char bestDir = g.dir;
  bool found = false;
  char fallback = 0;

  for (int i = 0; i < 4; i++) {
    char d = dirs[i];
    int nr = g.row, nc = g.col;
    switch (d) {
      case 'U': nr--; break;
      case 'D': nr++; break;
      case 'L': nc--; break;
      case 'R': nc++; break;
    }
    if (!inBounds(nr, nc) || isWall(nr, nc)) continue;
    if (d == reverse) { fallback = d; continue; }   // Last resort

    long sc;
    if (g.vulnerable) {
      // Run away from Pac-Man
      sc = -((long)abs(nr - pacRow) + abs(nc - pacCol));
    } else if (g.aiType == 2) {
      // Random movement
      sc = random(10000);
    } else if (g.aiType == 1) {
      // Ambush: Target 2 cells in front of Pac-Man
      int tr = pacRow, tc = pacCol;
      switch (pacDir) {
        case 'U': tr -= 2; break;
        case 'D': tr += 2; break;
        case 'L': tc -= 2; break;
        case 'R': tc += 2; break;
      }
      sc = (long)abs(nr - tr) + abs(nc - tc);
    } else {
      // Chase: Directly towards Pac-Man
      sc = (long)abs(nr - pacRow) + abs(nc - pacCol);
    }
    if (!found || sc < bestScore) {
      bestScore = sc;
      bestDir = d;
      found = true;
    }
  }

  if (!found) return fallback ? fallback : g.dir;
  return bestDir;
}

char oppositeDir(char d) {
  switch (d) {
    case 'U': return 'D';
    case 'D': return 'U';
    case 'L': return 'R';
    case 'R': return 'L';
  }
  return 0;
}

// =============================================================
//                           COLLISION
// =============================================================
void checkCollisions() {
  for (int i = 0; i < NUM_GHOSTS; i++) {
    if (ghosts[i].row == pacRow && ghosts[i].col == pacCol) {
      if (ghosts[i].vulnerable) {
        // Ghost eaten
        score += 200;
        // Return to spawn
        eraseGhost(i);
        ghosts[i].row = ghosts[i].spawnRow;
        ghosts[i].col = ghosts[i].spawnCol;
        ghosts[i].x = ghosts[i].col * CELL_SIZE;
        ghosts[i].y = MAZE_Y_OFFSET + ghosts[i].row * CELL_SIZE;
        ghosts[i].targetX = ghosts[i].x;
        ghosts[i].targetY = ghosts[i].y;
        ghosts[i].moving = false;
        ghosts[i].vulnerable = false;
        drawGhost(i);
        drawScoreBar();
      } else {
        // Pac-Man died
        lives--;
        drawScoreBar();
        if (lives <= 0) {
          delay(400);
          enterState(S_GAMEOVER);
        } else {
          delay(600);
          initPacMan();
          initGhosts();
          drawGameStatic();
        }
        return;
      }
    }
  }
}

// =============================================================
//                        DRAWING FUNCTIONS
// =============================================================
void drawMaze() {
  for (int r = 0; r < GRID_ROWS; r++) {
    for (int c = 0; c < GRID_COLS; c++) {
      if (mazeOrig[r][c] == '#') {
        tft.fillRect(c * CELL_SIZE, MAZE_Y_OFFSET + r * CELL_SIZE,
                     CELL_SIZE, CELL_SIZE, COL_BLUE);
      }
    }
  }
}

void drawAllPellets() {
  for (int r = 0; r < GRID_ROWS; r++) {
    for (int c = 0; c < GRID_COLS; c++) {
      if (pellets[r][c]) drawPellet(r, c);
      if (powerPellets[r][c]) drawPowerPellet(r, c);
    }
  }
}

void drawPellet(int r, int c) {
  int x = c * CELL_SIZE + 3;
  int y = MAZE_Y_OFFSET + r * CELL_SIZE + 3;
  tft.fillRect(x, y, 2, 2, COL_WHITE);
}

void drawPowerPellet(int r, int c) {
  int x = c * CELL_SIZE + CELL_SIZE / 2;
  int y = MAZE_Y_OFFSET + r * CELL_SIZE + CELL_SIZE / 2;
  tft.fillCircle(x, y, 3, COL_WHITE);
}

void drawScoreBar() {
  tft.fillRect(0, 0, 160, 14, COL_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(COL_WHITE);
  tft.setCursor(2, 3);
  tft.print("SCORE:");
  tft.setTextColor(COL_YELLOW);
  tft.setCursor(40, 3);
  tft.print(score);

  tft.setTextColor(COL_WHITE);
  tft.setCursor(85, 3);
  tft.print("LIFE:");
  for (int i = 0; i < lives; i++) {
    int x = 115 + i * 10;
    tft.fillCircle(x, 7, 3, COL_YELLOW);
    tft.fillTriangle(x, 7, x + 4, 5, x + 4, 9, COL_BLACK);
  }
}

void erasePacMan() {
  tft.fillRect(pacX, pacY, CELL_SIZE, CELL_SIZE, COL_BLACK);
  if (pellets[pacRow][pacCol]) drawPellet(pacRow, pacCol);
  if (powerPellets[pacRow][pacCol]) drawPowerPellet(pacRow, pacCol);
}

void drawPacMan() {
  int cx = pacX + CELL_SIZE / 2;
  int cy = pacY + CELL_SIZE / 2;
  tft.fillCircle(cx, cy, 3, COL_YELLOW);
  if (chompFrame == 0) {
    switch (pacDir) {
      case 'R': tft.fillTriangle(cx, cy, cx + 4, cy - 3, cx + 4, cy + 3, COL_BLACK); break;
      case 'L': tft.fillTriangle(cx, cy, cx - 4, cy - 3, cx - 4, cy + 3, COL_BLACK); break;
      case 'U': tft.fillTriangle(cx, cy, cx - 3, cy - 4, cx + 3, cy - 4, COL_BLACK); break;
      case 'D': tft.fillTriangle(cx, cy, cx - 3, cy + 4, cx + 3, cy + 4, COL_BLACK); break;
    }
  }
}

void eraseGhost(int idx) {
  Ghost& g = ghosts[idx];
  tft.fillRect(g.x, g.y, CELL_SIZE, CELL_SIZE, COL_BLACK);
  if (pellets[g.row][g.col]) drawPellet(g.row, g.col);
  if (powerPellets[g.row][g.col]) drawPowerPellet(g.row, g.col);
}

void drawGhost(int idx) {
  Ghost& g = ghosts[idx];
  uint16_t bodyColor = g.vulnerable ? COL_DARKBLUE : g.color;
  int x = g.x, y = g.y;

  // Head
  tft.fillCircle(x + 4, y + 3, 3, bodyColor);
  // Body
  tft.fillRect(x + 1, y + 3, 6, 4, bodyColor);
  // Legs zig-zag
  tft.drawPixel(x + 1, y + 7, bodyColor);
  tft.drawPixel(x + 3, y + 7, bodyColor);
  tft.drawPixel(x + 5, y + 7, bodyColor);
  tft.drawPixel(x + 7, y + 7, bodyColor);
  // Eyes
  if (g.vulnerable) {
    // Scared face
    tft.drawPixel(x + 2, y + 3, COL_WHITE);
    tft.drawPixel(x + 5, y + 3, COL_WHITE);
    tft.drawPixel(x + 2, y + 5, COL_WHITE);
    tft.drawPixel(x + 4, y + 5, COL_WHITE);
    tft.drawPixel(x + 6, y + 5, COL_WHITE);
  } else {
    tft.fillRect(x + 1, y + 2, 2, 2, COL_WHITE);
    tft.fillRect(x + 5, y + 2, 2, 2, COL_WHITE);
    // Pupils
    int px = x + 2, py = y + 3;
    switch (g.dir) {
      case 'L': px = x + 1; break;
      case 'R': px = x + 2; break;
      case 'U': py = y + 2; break;
      case 'D': py = y + 3; break;
    }
    tft.drawPixel(px, py, COL_BLUE);
    tft.drawPixel(px + 4, py, COL_BLUE);
  }
}

// =============================================================
//                   GAME OVER / WIN SCREENS
// =============================================================
void drawGameOverScreen() {
  tft.fillScreen(COL_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(COL_RED);
  tft.setCursor(20, 30);
  tft.print("GAME OVER");

  tft.setTextSize(1);
  tft.setTextColor(COL_WHITE);
  tft.setCursor(50, 65);
  tft.print("Score: ");
  tft.setTextColor(COL_YELLOW);
  tft.print(score);

  tft.setTextColor(COL_CYAN);
  tft.setCursor(28, 95);
  tft.print("Press key to restart");
}

void handleGameOver() {
  if (keypad.getKey()) enterState(S_INTRO);
}

void drawWinScreen() {
  tft.fillScreen(COL_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(COL_YELLOW);
  tft.setCursor(28, 25);
  tft.print("YOU WIN");

  tft.setTextSize(1);
  tft.setTextColor(COL_WHITE);
  tft.setCursor(50, 60);
  tft.print("Score: ");
  tft.setTextColor(COL_YELLOW);
  tft.print(score);

  tft.setTextSize(1);
  tft.setTextColor(COL_GREEN);
  tft.setCursor(18, 80);
  tft.print("Well done BATIKAN!");

  tft.setTextColor(COL_CYAN);
  tft.setCursor(28, 105);
  tft.print("Press key to restart");
}

void handleWin() {
  if (keypad.getKey()) enterState(S_INTRO);
}