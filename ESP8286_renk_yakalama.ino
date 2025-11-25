#include <TFT_eSPI.h>
#include <SPI.h>
#include <EEPROM.h>

// === Pin Tanımlamaları ===
#define ENCODER_CLK  D1
#define ENCODER_DT   D2
#define BUZZER_PIN   D6
#define LED_PIN      D7

TFT_eSPI tft = TFT_eSPI();

// Encoder değişkenleri
volatile long encoderValue = 0;
volatile int lastEncoded = 0;

uint16_t palette[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_YELLOW, TFT_ORANGE, TFT_MAGENTA};
String colorNames[] = {"KIRMIZI", "YESIL", "MAVI", "SARI", "TURUNCU", "MOR"};
int paletteSize = 6;

int myBarColor = 0;
int score = 0;
int highScore = 0; 
int lives = 3;
int lastLifeBonus = 0;
int currentLevel = 1;
bool newRecord = false;

// --- KOMBO ve OYUN DURUMU ---
int comboCount = 0;
bool isPerfectRun = true; 
unsigned long comboTextTimer = 0; 
String currentComboText = ""; 

// Ekran Güncelleme Hafızası
int oldScore = -1;
int oldLives = -1;

// Zamanlayıcılar (Non-Blocking)
unsigned long previousMillis = 0;  
const long FRAME_INTERVAL = 20;    

// Görsel Efekt Zamanlayıcıları
unsigned long flashTimer = 0;
bool flashActive = false;
unsigned long catchEffectTimer = 0; 

// LED Zamanlayıcısı
unsigned long ledTimer = 0;
bool ledActive = false;

// Top Türleri
#define TYPE_NORMAL 0
#define TYPE_HEART  1  

struct Ball {
  float x, y, vx, vy, speed;
  int colorId, type, radius;
  bool active;
};
#define MAX_BALLS 10
Ball balls[MAX_BALLS];

// --- MENÜ EFEKTLERİ ---
struct MenuParticle { float x, y, vy; int color; };
#define MENU_PARTICLE_COUNT 20
MenuParticle menuParticles[MENU_PARTICLE_COUNT];

// Oyun içi arka plan efektleri
struct GameParticle { float x, y, vy; int color; };
#define GAME_PARTICLE_COUNT 8
GameParticle gameParticles[GAME_PARTICLE_COUNT];

unsigned long lastSpawn = 0;
int spawnInterval = 1200;

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define BALL_RADIUS_LVL1 12
#define BALL_RADIUS_LVL2 15
#define CATCH_Y 285
#define CATCH_WIDTH 60
#define CATCH_HEIGHT 22

int barX = 120;

// State Machine
int gameState = 0; 
int selectedOption = 1; 
unsigned long confirmTimer = 0;
unsigned long stateTimer = 0;

// Animasyon Değişkenleri
unsigned long lastAnimTime = 0;
unsigned long menuAnimTimer = 0;
int animFrame = 0;
int titleColorIdx = 0;

// Fonksiyonlar
void playSound(int freq, int duration); 
void triggerLed();
void handleLed();
void showStartScreen();
void animateStartScreen();
void showStartMenu();
void animateStartMenuBackground();
void drawAyYildiz(int x, int y); // GÜNCELLENDİ
void drawPlayIcon(int x, int y, uint16_t color); 
void drawExitIcon(int x, int y, uint16_t color); 
void updateStartMenuButtons();
void prepareNewGame(); 
void runGame();
void spawnBall();
void updateBalls();
void updateGameParticles(); 
void handleVisualEffects(); 
void drawStaticHUD();
void updateStatus();
void drawBar();
void showLevelComplete();
void showGameOver();
void showRestartConfirm();
void updateRestartConfirm();
void showGameWon();
void drawTrophy();
void checkHighScore(); 

void ICACHE_RAM_ATTR updateEncoder() {
  int MSB = digitalRead(ENCODER_CLK);
  int LSB = digitalRead(ENCODER_DT);
  int encoded = (MSB << 1) | LSB;
  int sum = (lastEncoded << 2) | encoded;
  if(sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue++;
  if(sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue--;
  lastEncoded = encoded;
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(64); 
  EEPROM.get(0, highScore);
  if (highScore < 0 || highScore > 50000) { highScore = 0; EEPROM.put(0, highScore); EEPROM.commit(); }
  
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_DT), updateEncoder, CHANGE);
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  randomSeed(analogRead(A0));
  
  // Menü parçacıkları - DÜZELTİLDİ: Bayrağın üstüne gelmemesi için 60'tan başlatıyoruz.
  for(int i=0; i<MENU_PARTICLE_COUNT; i++){
    menuParticles[i].x = random(0, SCREEN_WIDTH);
    menuParticles[i].y = random(60, SCREEN_HEIGHT); // Y > 60
    menuParticles[i].vy = random(10, 30) / 10.0; menuParticles[i].color = palette[random(0, paletteSize)];
  }
  // Oyun içi parçacıkları
  for(int i=0; i<GAME_PARTICLE_COUNT; i++){
    gameParticles[i].x = random(0, SCREEN_WIDTH); gameParticles[i].y = random(30, SCREEN_HEIGHT);
    gameParticles[i].vy = random(5, 15) / 10.0; gameParticles[i].color = TFT_DARKGREY;
  }
  
  showStartScreen();
}

void loop() {
  unsigned long now = millis();
  handleLed(); 

  // --- INTRO ---
  if (gameState == 0) { 
    animateStartScreen();
    if (now - stateTimer >= 6000) {
      gameState = 5; encoderValue = 2; selectedOption = 1; confirmTimer = now;
      showStartMenu();
    }
  }
  // --- MENÜ ---
  else if (gameState == 5) { 
    animateStartMenuBackground(); 
    int newOption = (abs(encoderValue / 2) % 2);
    if (newOption != selectedOption || (now - menuAnimTimer > 150)) {
       if (newOption != selectedOption) { selectedOption = newOption; confirmTimer = now; }
       updateStartMenuButtons();
    }
    if (selectedOption == 0) { 
      int countdown = 3 - ((now - confirmTimer) / 1000);
      if (countdown >= 0 && countdown <= 3) {
        tft.fillRect(100, 270, 40, 30, TFT_BLACK); tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString(String(countdown), 120, 285, 4);
      }
      if (now - confirmTimer >= 3000) prepareNewGame();
    } else { 
      confirmTimer = now; tft.fillRect(100, 270, 40, 30, TFT_BLACK); 
    }
  }
  // --- HAZIRLIK ---
  else if (gameState == 6) {
    if (now - stateTimer >= 1500) {
      tft.fillScreen(TFT_BLACK); drawStaticHUD();
      gameState = 1; lastSpawn = now; previousMillis = now;
    }
  }
  // --- OYUN ---
  else if (gameState == 1) { 
    if (now - previousMillis >= FRAME_INTERVAL) {
      previousMillis = now;
      runGame();
    }
  }
  // --- BEKLEME EKRANLARI ---
  else if (gameState == 7) { // Game Over Wait
    if (now - stateTimer >= 3000) { 
       gameState = 2; encoderValue = 0; selectedOption = 0; confirmTimer = millis(); 
       showRestartConfirm();
    }
  }
  else if (gameState == 3) { // Level Complete Wait
    if (now - stateTimer >= 3000) {
      currentLevel = 2; 
      lastLifeBonus = score; isPerfectRun = true; comboCount = 0;
      tft.fillScreen(TFT_BLACK); drawStaticHUD();
      gameState = 1; lastSpawn = millis();
    }
  }
  else if (gameState == 4) { // Won Wait
    if (now - stateTimer >= 5000) {
      gameState = 2; encoderValue = 0; selectedOption = 0; confirmTimer = millis(); 
      showRestartConfirm();
    }
  }
  // --- YENİDEN DENE ---
  else if (gameState == 2) { 
    int newOption = (abs(encoderValue / 2) % 2);
    if (newOption != selectedOption) { selectedOption = newOption; confirmTimer = now; updateRestartConfirm(); }
    int countdown = 3 - ((now - confirmTimer) / 1000);
    if (countdown >= 0) {
        tft.fillRect(100, 275, 40, 30, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.setTextDatum(MC_DATUM);
        tft.drawString(String(countdown), 120, 290, 4);
    }
    if (now - confirmTimer >= 3000) {
       if (selectedOption == 0) { prepareNewGame(); } 
       else { gameState = 0; stateTimer = millis(); showStartScreen(); }
    }
  }
}

void playSound(int freq, int duration) {
  tone(BUZZER_PIN, freq, duration);
}

void triggerLed() { digitalWrite(LED_PIN, HIGH); ledActive = true; ledTimer = millis(); }
void handleLed() { if (ledActive && (millis() - ledTimer > 100)) { digitalWrite(LED_PIN, LOW); ledActive = false; } }
void checkHighScore() { if (score > highScore) { highScore = score; EEPROM.put(0, highScore); EEPROM.commit(); newRecord = true; } else newRecord = false; }

// --- İKON ÇİZİMLERİ ---

// GÜNCELLENMİŞ BAYRAK VE YILDIZ TASARIMI
void drawAyYildiz(int x, int y) {
  // Kırmızı bayrak zemini
  int w = 80;
  int h = 40;
  tft.fillRect(x - w/2, y - h/2, w, h, TFT_RED);
  tft.drawRect(x - w/2, y - h/2, w, h, TFT_WHITE);

  // Hilal
  int cx = x - 10;    // Hilalin merkezi biraz sola
  int cy = y;
  int rDis = 12;
  int rIc  = 8;
  tft.fillCircle(cx,     cy, rDis, TFT_WHITE); // dış beyaz daire
  tft.fillCircle(cx + 3, cy, rIc,  TFT_RED);   // iç kırmızı daire (sağa kayık)

  // Yıldız (Daha güzel 8 köşeli parıltı şekli)
  int sx = x + 10;
  int sy = y;
  // Ana artı
  tft.drawLine(sx - 5, sy, sx + 5, sy, TFT_WHITE);
  tft.drawLine(sx, sy - 5, sx, sy + 5, TFT_WHITE);
  // Çaprazlar (daha kısa)
  tft.drawLine(sx - 3, sy - 3, sx + 3, sy + 3, TFT_WHITE);
  tft.drawLine(sx + 3, sy - 3, sx - 3, sy + 3, TFT_WHITE);

  // Alt yazı
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("TC", x, y + h/2 + 8, 2);
}

void drawPlayIcon(int x, int y, uint16_t color) {
  tft.fillTriangle(x-4, y-7, x-4, y+7, x+6, y, color);
}

void drawExitIcon(int x, int y, uint16_t color) {
  tft.drawLine(x-5, y-5, x+5, y+5, color); tft.drawLine(x+5, y-5, x-5, y+5, color);
}

// --- MENÜLER ---
void showStartScreen() {
  tft.fillScreen(TFT_BLACK);
  lastAnimTime = millis(); animFrame = 0; stateTimer = millis();
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK); tft.setTextDatum(MC_DATUM); tft.drawString("SASU", 123, 38, 4); 
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.drawString("SASU", 120, 35, 4); 
  tft.setTextColor(TFT_MAGENTA, TFT_BLACK); tft.drawString("RENK AVCISI", 120, 70, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.setTextDatum(MC_DATUM);
  tft.drawString("Sehit Astsubay Salim Ucar", 120, 92, 1); tft.drawString("ORTAOKULU", 120, 105, 2);
  for(int i=0; i<SCREEN_WIDTH; i+=40) { int cIdx = (i/40) % paletteSize; tft.fillRect(i, 115, 40, 4, palette[cIdx]); }
  
  tft.drawRoundRect(10, 125, 220, 140, 10, TFT_WHITE); tft.fillRoundRect(12, 127, 216, 136, 10, 0x2104); 
  tft.setTextColor(TFT_YELLOW, 0x2104); tft.setTextDatum(MC_DATUM); tft.drawString("NASIL OYNANIR?", 120, 140, 2);
  tft.drawLine(40, 150, 200, 150, TFT_YELLOW);
  tft.drawCircle(40, 175, 8, TFT_CYAN); tft.drawLine(40, 175, 40, 169, TFT_CYAN); 
  tft.setTextColor(TFT_WHITE, 0x2104); tft.setTextDatum(TL_DATUM); tft.drawString("Encoderi cevir", 65, 168, 2);
  tft.fillCircle(40, 205, 5, TFT_RED); tft.fillRect(30, 215, 20, 4, TFT_RED); tft.drawString("Ayni rengi yakala", 65, 198, 2);
  tft.setTextColor(TFT_GREEN, 0x2104); tft.drawString("+10 Puan", 30, 230, 2);
  tft.setTextColor(TFT_RED, 0x2104); tft.drawString("-1 Can", 140, 230, 2);
  
  // Can Bilgisi
  tft.setTextColor(TFT_LIGHTGREY, 0x2104); tft.setTextDatum(MC_DATUM); tft.drawString("(Can Topla!)", 120, 255, 2);

  if (highScore > 0) { tft.setTextColor(TFT_ORANGE, TFT_BLACK); tft.setTextDatum(MC_DATUM); tft.drawString("REKOR: " + String(highScore), 120, 280, 2); }
  tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setTextDatum(MC_DATUM); tft.drawString("Oyun baslamak uzere...", 120, 305, 2);
}

void animateStartScreen() {
  unsigned long now = millis();
  if (now - lastAnimTime > 400) { 
    lastAnimTime = now; animFrame++; int colorIndex = animFrame % paletteSize;
    tft.setTextColor(palette[colorIndex], TFT_BLACK); tft.setTextDatum(MC_DATUM); tft.drawString("SASU", 120, 35, 4);
    digitalWrite(LED_PIN, animFrame % 2); 
    if (animFrame % 2 == 0) { tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.drawString("Oyun baslamak uzere...", 120, 305, 2); } 
    else { tft.setTextColor(TFT_BLACK, TFT_BLACK); tft.drawString("Oyun baslamak uzere...", 120, 305, 2); }
  }
}

void showStartMenu() {
  tft.fillScreen(TFT_BLACK); drawAyYildiz(120, 30);
  // Menü kutularını bayrağın altına aldık
  tft.drawRect(10, 60, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 70, TFT_CYAN); 
  tft.drawRect(12, 62, SCREEN_WIDTH - 24, SCREEN_HEIGHT - 74, TFT_MAGENTA);
  updateStartMenuButtons();
}

void animateStartMenuBackground() {
  unsigned long now = millis();
  for(int i=0; i<MENU_PARTICLE_COUNT; i++) {
    tft.drawPixel((int)menuParticles[i].x, (int)menuParticles[i].y, TFT_BLACK); menuParticles[i].y += menuParticles[i].vy;
    
    // DÜZELTİLDİ: Parçacıklar bayrağın üstüne çıkamaz (Y=60'tan başlar)
    if(menuParticles[i].y > SCREEN_HEIGHT) { menuParticles[i].y = 60; menuParticles[i].x = random(0, SCREEN_WIDTH); }
    
    bool insideButtonArea = (menuParticles[i].y > 250 && menuParticles[i].y < 300); bool insideTextArea = (menuParticles[i].y > 60 && menuParticles[i].y < 200);
    if (!insideButtonArea && !insideTextArea) { tft.drawPixel((int)menuParticles[i].x, (int)menuParticles[i].y, menuParticles[i].color); }
  }
  if (now - menuAnimTimer > 200) {
    menuAnimTimer = now; titleColorIdx = (titleColorIdx + 1) % paletteSize;
    tft.setTextSize(1); tft.setTextDatum(MC_DATUM); tft.setTextColor(palette[titleColorIdx], TFT_BLACK); tft.drawString("SASU", 120, 80, 4);
    int oppositeColor = (titleColorIdx + 3) % paletteSize; tft.setTextColor(palette[oppositeColor], TFT_BLACK); tft.drawString("RENK AVCISI", 120, 115, 4);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.drawString("OYNAMAK", 120, 165, 4); tft.drawString("ISTER MISIN?", 120, 200, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.drawString("Encoder ile sec", 120, 235, 2);
  }
}

void updateStartMenuButtons() {
  int pulse = (millis() / 100) % 2; uint16_t pulseColor = (pulse == 0) ? TFT_WHITE : TFT_YELLOW;
  if (selectedOption == 0) { 
    tft.fillRoundRect(30, 255, 90, 45, 10, TFT_GREEN); tft.drawRoundRect(30, 255, 90, 45, 10, pulseColor);
    tft.fillRoundRect(130, 255, 90, 45, 10, TFT_DARKGREY); tft.drawRoundRect(130, 255, 90, 45, 10, TFT_BLACK); 
    drawPlayIcon(75, 265, TFT_BLACK);
    tft.fillRect(0, 305, SCREEN_WIDTH, 15, TFT_BLACK); tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setTextDatum(MC_DATUM); tft.drawString("3 saniyede baslayacak!", 120, 310, 2);
  } else { 
    tft.fillRoundRect(30, 255, 90, 45, 10, TFT_DARKGREY); tft.drawRoundRect(30, 255, 90, 45, 10, TFT_BLACK); 
    tft.fillRoundRect(130, 255, 90, 45, 10, TFT_RED); tft.drawRoundRect(130, 255, 90, 45, 10, pulseColor);
    drawExitIcon(175, 265, TFT_WHITE);
    tft.fillRect(0, 305, SCREEN_WIDTH, 15, TFT_BLACK); tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextDatum(MC_DATUM); tft.drawString("EVET'i sec ve bekle", 120, 310, 2);
  }
  tft.setTextColor(selectedOption == 0 ? TFT_BLACK : TFT_WHITE, selectedOption == 0 ? TFT_GREEN : TFT_DARKGREY); tft.setTextDatum(MC_DATUM); tft.drawString("EVET", 75, 285, 4);
  tft.setTextColor(TFT_WHITE, selectedOption == 1 ? TFT_RED : TFT_DARKGREY); tft.drawString("HAYIR", 175, 285, 4);
}

// --- OYUN LOJİĞİ ---
void prepareNewGame() {
  score = 0; lives = 3; lastLifeBonus = 0; currentLevel = 1;
  encoderValue = 0; barX = 120; spawnInterval = 1200; newRecord = false;
  oldScore = -1; oldLives = -1; 
  comboCount = 0; isPerfectRun = true; comboTextTimer = 0;
  myBarColor = random(0, paletteSize);
  for (int i = 0; i < MAX_BALLS; i++) balls[i].active = false;
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextDatum(MC_DATUM); tft.drawString("SEVIYE 1", 120, 140, 4);
  tft.setTextColor(palette[myBarColor], TFT_BLACK); tft.drawString("RENGIN: " + colorNames[myBarColor], 120, 180, 4);
  playSound(1500, 100); delay(100); playSound(2000, 200); 
  gameState = 6; stateTimer = millis(); 
}

void runGame() {
  unsigned long now = millis();
  barX = 120 + (encoderValue * 2); barX = constrain(barX, CATCH_WIDTH/2, SCREEN_WIDTH - CATCH_WIDTH/2);
  
  if (currentLevel == 1 && score >= 50 && lives > 0) { showLevelComplete(); return; }
  if (currentLevel == 2 && score >= 70 && lives > 0) { showGameWon(); return; }
  
  if (now - lastSpawn > spawnInterval) {
    spawnBall(); lastSpawn = now;
    if (currentLevel == 1) { if (spawnInterval > 800) spawnInterval -= 40; } else { if (spawnInterval > 500) spawnInterval -= 50; }
  }
  
  updateGameParticles();
  updateBalls();
  handleVisualEffects();
  updateStatus(); 
  drawBar();
  if (lives <= 0) { showGameOver(); }
}

void updateGameParticles() {
  for(int i=0; i<GAME_PARTICLE_COUNT; i++) {
    tft.drawPixel((int)gameParticles[i].x, (int)gameParticles[i].y, TFT_BLACK);
    gameParticles[i].y += gameParticles[i].vy;
    if(gameParticles[i].y > SCREEN_HEIGHT) { gameParticles[i].y = 30; gameParticles[i].x = random(0, SCREEN_WIDTH); }
    if(gameParticles[i].y > 30) tft.drawPixel((int)gameParticles[i].x, (int)gameParticles[i].y, gameParticles[i].color);
  }
}

void handleVisualEffects() {
  // Görsel Efektler (Flash söndürme)
  if (flashActive && millis() - flashTimer > 100) {
    tft.fillRect(0, 30, SCREEN_WIDTH, 10, TFT_BLACK); 
    flashActive = false;
  }
  // Kombo yazısı silme
  if (comboTextTimer > 0 && millis() - comboTextTimer > 1000) {
    tft.setTextColor(TFT_BLACK, TFT_BLACK); tft.setTextDatum(MC_DATUM);
    tft.drawString(currentComboText, 120, 100, 4); // Yazıyı sil
    comboTextTimer = 0;
  }
}

void spawnBall() {
  for (int i = 0; i < MAX_BALLS; i++) {
    if (!balls[i].active) {
      balls[i].x = random(BALL_RADIUS_LVL2 + 10, SCREEN_WIDTH - BALL_RADIUS_LVL2 - 10); balls[i].y = 35;
      int chance = random(100); 
      // Bomba yok, sadece Kalp (%20) veya Normal
      if (chance < 20) { balls[i].type = TYPE_HEART; balls[i].radius = 12; } 
      else { balls[i].type = TYPE_NORMAL; balls[i].radius = (currentLevel == 1) ? BALL_RADIUS_LVL1 : BALL_RADIUS_LVL2; }
      
      balls[i].colorId = random(0, paletteSize);
      if (currentLevel == 1) { balls[i].speed = random(25, 35) / 10.0; balls[i].vx = 0; } 
      else { balls[i].speed = random(30, 45) / 10.0; balls[i].vx = random(-15, 15) / 10.0; }
      balls[i].vy = balls[i].speed; balls[i].active = true;
      break;
    }
  }
}

void updateBalls() {
  for (int i = 0; i < MAX_BALLS; i++) {
    if (balls[i].active) {
      tft.fillCircle((int)balls[i].x, (int)balls[i].y, balls[i].radius + 3, TFT_BLACK);
      balls[i].y += balls[i].vy;
      if (currentLevel == 2) {
        balls[i].x += balls[i].vx;
        if (balls[i].x - balls[i].radius < 5 || balls[i].x + balls[i].radius > SCREEN_WIDTH - 5) { balls[i].vx = -balls[i].vx; }
      }
      
      if (balls[i].y >= CATCH_Y - 10 && balls[i].y <= CATCH_Y + CATCH_HEIGHT) {
        if (abs(balls[i].x - barX) < CATCH_WIDTH/2 + balls[i].radius - 5) {
          
          bool correct = false;
          if (balls[i].type == TYPE_HEART) {
            if(lives < 10) lives++; // CAN ARTIRMA SINIRI 10
            playSound(2000, 100); triggerLed(); correct = true;
          }
          else {
            if (balls[i].colorId == myBarColor) {
              score += 10; playSound(1500, 80); triggerLed(); correct = true;
              if(score - lastLifeBonus >= 50) { if(lives < 10) lives++; lastLifeBonus = score; }
              
              // Pozitif Efekt
              catchEffectTimer = millis(); 
            } else {
              lives--; isPerfectRun = false; comboCount = 0;
              playSound(300, 150); triggerLed();
              // Negatif Efekt (Flash)
              flashActive = true; flashTimer = millis();
              tft.fillRect(0, 30, SCREEN_WIDTH, 10, TFT_RED);
            }
          }
          
          // Kombo Mantığı
          if(correct) {
            comboCount++;
            if(comboCount == 3) { currentComboText = "SUPER!"; comboTextTimer = millis(); tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.drawString(currentComboText, 120, 100, 4); }
            if(comboCount == 5) { currentComboText = "EFSANE!"; comboTextTimer = millis(); tft.setTextColor(TFT_MAGENTA, TFT_BLACK); tft.drawString(currentComboText, 120, 100, 4); }
          }
          
          balls[i].active = false; continue;
        }
      }
      if (balls[i].y > SCREEN_HEIGHT) balls[i].active = false;
      else {
        int bx = (int)balls[i].x; int by = (int)balls[i].y;
        if (balls[i].type == TYPE_HEART) {
           tft.fillCircle(bx, by, balls[i].radius, TFT_WHITE); tft.fillCircle(bx - 4, by - 2, 4, TFT_RED);
           tft.fillCircle(bx + 4, by - 2, 4, TFT_RED); tft.fillTriangle(bx - 9, by, bx + 9, by, bx, by + 9, TFT_RED);
        } else { tft.fillCircle(bx, by, balls[i].radius, palette[balls[i].colorId]); }
      }
    }
  }
}

void drawStaticHUD() {
  uint16_t hudColor = (currentLevel == 2) ? TFT_GREEN : TFT_CYAN;
  tft.fillRect(0, 0, SCREEN_WIDTH, 28, TFT_BLACK);
  tft.fillRoundRect(2, 2, 90, 24, 5, TFT_DARKGREY); tft.drawRoundRect(2, 2, 90, 24, 5, hudColor);
  tft.fillRoundRect(100, 2, 130, 24, 5, TFT_DARKGREY); tft.drawRoundRect(100, 2, 130, 24, 5, TFT_RED);
  tft.setTextColor(TFT_YELLOW, TFT_DARKGREY); tft.setTextDatum(TL_DATUM); tft.drawString("Sev", 6, 7, 2);
  tft.setTextColor(TFT_RED, TFT_DARKGREY); tft.setTextDatum(TL_DATUM); tft.drawString("CAN:", 105, 7, 2);
  oldScore = -1; oldLives = -1;
}

void updateStatus() {
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY); tft.setTextDatum(TR_DATUM); tft.drawString(String(currentLevel), 88, 7, 2);
  if (score != oldScore) {
    tft.fillRect(6, 30, 90, 20, TFT_BLACK); tft.setTextColor(TFT_CYAN, TFT_BLACK); 
    tft.setTextDatum(TL_DATUM); tft.drawString(String(score) + "p", 6, 30, 2); oldScore = score;
  }
  if (lives != oldLives) {
    tft.fillRect(180, 4, 45, 20, TFT_DARKGREY); 
    if (lives <= 1) tft.setTextColor(TFT_RED, TFT_DARKGREY); else tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextDatum(TR_DATUM); tft.drawString(String(lives), 225, 7, 2); oldLives = lives;
  }
}

void drawBar() {
  static int oldBarX = 120;
  if (abs(barX - oldBarX) > 2) tft.fillRect(0, CATCH_Y - 10, SCREEN_WIDTH, CATCH_HEIGHT + 15, TFT_BLACK);
  tft.fillRect(0, CATCH_Y - 8, SCREEN_WIDTH, 3, palette[myBarColor]); // Tema Şeridi
  int barLeft = barX - CATCH_WIDTH/2;
  tft.fillRoundRect(barLeft, CATCH_Y, CATCH_WIDTH, CATCH_HEIGHT, 6, palette[myBarColor]);
  
  // Çerçeve Parlama Efekti
  uint16_t borderColor = TFT_WHITE;
  if(millis() - catchEffectTimer < 150) borderColor = TFT_WHITE;
  else if(flashActive) borderColor = TFT_RED;
  
  if (millis() - catchEffectTimer < 150) tft.drawRoundRect(barLeft-1, CATCH_Y-1, CATCH_WIDTH+2, CATCH_HEIGHT+2, 6, borderColor);
  else tft.drawRoundRect(barLeft, CATCH_Y, CATCH_WIDTH, CATCH_HEIGHT, 6, borderColor);

  tft.setTextColor(TFT_WHITE, palette[myBarColor]); tft.setTextDatum(MC_DATUM);
  tft.drawString(colorNames[myBarColor], barX, CATCH_Y + CATCH_HEIGHT/2, 2);
  oldBarX = barX;
}

void showLevelComplete() {
  gameState = 3; stateTimer = millis();
  for (int i = 0; i < MAX_BALLS; i++) if (balls[i].active) { tft.fillCircle((int)balls[i].x, (int)balls[i].y, balls[i].radius, TFT_BLACK); balls[i].active = false; }
  playSound(1500, 200); delay(100); playSound(2000, 400); 
  tft.fillRoundRect(20, 100, 200, 120, 10, TFT_GREEN); tft.drawRoundRect(20, 100, 200, 120, 10, TFT_YELLOW); tft.drawRoundRect(22, 102, 196, 116, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_GREEN); tft.setTextDatum(MC_DATUM); tft.drawString("SEVIYE 1", 120, 130, 4); tft.drawString("TAMAMLANDI!", 120, 160, 4);
  
  if(isPerfectRun) {
     tft.setTextColor(TFT_MAGENTA, TFT_GREEN);
     tft.drawString("MUKEMMEL OYNADIN!", 120, 195, 2);
  }
  myBarColor = random(0, paletteSize); spawnInterval = 1000;
}

void showGameOver() {
  checkHighScore(); gameState = 7; stateTimer = millis();
  for (int i = 0; i < MAX_BALLS; i++) if (balls[i].active) { tft.fillCircle((int)balls[i].x, (int)balls[i].y, balls[i].radius, TFT_BLACK); balls[i].active = false; }
  playSound(400, 200); delay(100); playSound(200, 500);
  tft.fillRoundRect(20, 100, 200, 120, 10, TFT_RED); tft.drawRoundRect(20, 100, 200, 120, 10, TFT_YELLOW); tft.drawRoundRect(22, 102, 196, 116, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_RED); tft.setTextDatum(MC_DATUM); 
  tft.drawString("OYUN BITTI :(", 120, 125, 4);
  tft.setTextColor(TFT_YELLOW, TFT_RED);
  if (newRecord) { tft.drawString("YENI REKOR!", 120, 160, 4); tft.drawString(String(score), 120, 190, 4); } 
  else { tft.drawString("Skor: " + String(score), 120, 165, 2); tft.drawString("Tekrar dene, rekoru kir!", 120, 190, 2); }
}

void showRestartConfirm() {
  tft.fillScreen(TFT_BLACK); tft.drawRect(10, 10, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 20, TFT_CYAN);
  tft.setTextColor(TFT_CYAN, TFT_BLACK); tft.setTextDatum(MC_DATUM); 
  tft.drawString("BIR TUR DAHA?", 120, 70, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.drawString("Encoder ile sec", 120, 150, 2);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.drawString("3 saniye...", 120, 175, 2); updateRestartConfirm();
}

void updateRestartConfirm() {
  if (selectedOption == 0) { 
    tft.fillRoundRect(30, 210, 90, 45, 10, TFT_GREEN); tft.drawRoundRect(30, 210, 90, 45, 10, TFT_WHITE); tft.fillRoundRect(130, 210, 90, 45, 10, TFT_DARKGREY); 
    drawPlayIcon(75, 222, TFT_BLACK);
  } else { 
    tft.fillRoundRect(30, 210, 90, 45, 10, TFT_DARKGREY); tft.fillRoundRect(130, 210, 90, 45, 10, TFT_RED); tft.drawRoundRect(130, 210, 90, 45, 10, TFT_WHITE); 
    drawExitIcon(175, 222, TFT_WHITE);
  }
  tft.setTextColor(selectedOption == 0 ? TFT_BLACK : TFT_WHITE, selectedOption == 0 ? TFT_GREEN : TFT_DARKGREY); tft.setTextDatum(MC_DATUM); tft.drawString("EVET", 75, 242, 4);
  tft.setTextColor(TFT_WHITE, selectedOption == 1 ? TFT_RED : TFT_DARKGREY); tft.drawString("HAYIR", 175, 242, 4);
}

void showGameWon() {
  checkHighScore(); gameState = 4; stateTimer = millis();
  for (int i = 0; i < MAX_BALLS; i++) if (balls[i].active) { tft.fillCircle((int)balls[i].x, (int)balls[i].y, balls[i].radius, TFT_BLACK); balls[i].active = false; }
  tft.fillScreen(TFT_BLACK); playSound(1000, 200); delay(200); playSound(2000, 400);
  drawTrophy(); tft.setTextColor(TFT_GREEN, TFT_BLACK); tft.setTextDatum(MC_DATUM); tft.drawString("SAMPIYON!", 120, 60, 4);
  if (newRecord) { tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.drawString("YENI REKOR!", 120, 240, 4); tft.drawString(String(score), 120, 275, 4); } 
  else { tft.setTextColor(TFT_WHITE, TFT_BLACK); tft.drawString("Skor: " + String(score), 120, 265, 4); }
}

void drawTrophy() {
  int cx = 120; int cy = 150; tft.drawArc(cx - 35, cy - 20, 15, 12, 90, 270, TFT_YELLOW, TFT_BLACK); tft.drawArc(cx + 35, cy - 20, 15, 12, 270, 90, TFT_YELLOW, TFT_BLACK);
  tft.fillRect(cx - 25, cy - 35, 50, 10, TFT_YELLOW); tft.fillTriangle(cx - 25, cy - 25, cx + 25, cy - 25, cx + 30, cy + 15, TFT_YELLOW);
  tft.fillTriangle(cx - 25, cy - 25, cx - 30, cy + 15, cx + 30, cy + 15, TFT_YELLOW); tft.fillRect(cx - 35, cy + 15, 70, 8, TFT_YELLOW);
  tft.fillRect(cx - 20, cy + 23, 40, 10, TFT_YELLOW); tft.fillRect(cx - 25, cy + 33, 50, 5, TFT_YELLOW);
  tft.drawLine(cx - 15, cy - 30, cx - 15, cy + 10, TFT_ORANGE); tft.drawLine(cx - 10, cy - 30, cx - 10, cy + 10, TFT_WHITE); tft.drawLine(cx, cy - 32, cx, cy + 12, TFT_WHITE);
  for (int i = 0; i < 5; i++) { int angle = i * 72; int x1 = cx + cos(angle * 3.14159 / 180) * 60; int y1 = cy - 20 + sin(angle * 3.14159 / 180) * 60; tft.fillCircle(x1, y1, 3, TFT_YELLOW); }
}
