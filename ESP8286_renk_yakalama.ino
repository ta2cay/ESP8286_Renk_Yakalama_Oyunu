#include <TFT_eSPI.h>
#include <SPI.h>

// === Pin Tanımlamaları ===
#define ENCODER_CLK  D1
#define ENCODER_DT   D2
#define ENCODER_BTN  D0
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
int currentLevel = 1; // Seviye sistemi

struct Ball {
  float x, y;
  float vx, vy; // x ve y hızları (zigzag için)
  float speed;
  int colorId;
  bool active;
  int radius; // Seviyeye göre değişecek
};

#define MAX_BALLS 10
Ball balls[MAX_BALLS];

unsigned long lastSpawn = 0;
int spawnInterval = 1200;
unsigned long lastUpdate = 0;

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define BALL_RADIUS_LVL1 12
#define BALL_RADIUS_LVL2 15
#define CATCH_Y 285
#define CATCH_WIDTH 60
#define CATCH_HEIGHT 22

int barX = 120;

// State Machine
int gameState = 0; // 0=Start Menu, 1=Playing, 2=Restart Confirm, 3=Level Complete, 4=Game Won
int selectedOption = 1; // 1=HAYIR (default)
unsigned long confirmTimer = 0;
unsigned long levelCompleteTimer = 0;
unsigned long gameWonTimer = 0;

// Animasyon değişkenleri
unsigned long lastAnimTime = 0;
int animFrame = 0;

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
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_BTN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  attachInterrupt(digitalPinToInterrupt(ENCODER_CLK), updateEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_DT), updateEncoder, CHANGE);
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  randomSeed(analogRead(A0));
  
  showStartScreen();
}

void loop() {
  unsigned long now = millis();
  
  if (gameState == 0) {
    // Start Menu - EVET/HAYIR seçimi + Animasyonlu
    animateStartScreen();
    
    int newOption = (abs(encoderValue / 2) % 2);
    if (newOption != selectedOption) {
      selectedOption = newOption;
      confirmTimer = now;
      updateStartMenu();
    }
    
    // 5 saniye sonra otomatik başlat (SADECE EVET SEÇİLİYSE!)
    if (now - confirmTimer >= 5000 && selectedOption == 0) {
      startNewGame();
    }
    
    // Geri sayım göster (SADECE EVET SEÇİLİYSE!)
    if (selectedOption == 0) {
      int countdown = 5 - ((now - confirmTimer) / 1000);
      if (countdown >= 0) {
        tft.fillRect(100, 285, 40, 30, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(String(countdown + 1), 120, 300, 4);
      }
    } else {
      // HAYIR seçiliyse geri sayımı temizle
      tft.fillRect(100, 285, 40, 30, TFT_BLACK);
    }
  }
  else if (gameState == 1) {
    // Playing
    runGame();
  }
  else if (gameState == 2) {
    // Restart Confirm
    int newOption = (abs(encoderValue / 2) % 2);
    if (newOption != selectedOption) {
      selectedOption = newOption;
      confirmTimer = now;
      updateRestartConfirm();
    }
    
    // 5 saniye sonra otomatik işlem (SADECE EVET SEÇİLİYSE!)
    if (now - confirmTimer >= 5000 && selectedOption == 0) {
      startNewGame();
    } else if (now - confirmTimer >= 5000 && selectedOption == 1) {
      // HAYIR seçiliyse ana ekrana dön
      gameState = 0;
      encoderValue = 0;
      selectedOption = 1;
      confirmTimer = millis();
      showStartScreen();
    }
    
    // Geri sayım göster
    if (selectedOption == 0) {
      int countdown = 5 - ((now - confirmTimer) / 1000);
      if (countdown >= 0) {
        tft.fillRect(100, 275, 40, 30, TFT_BLACK);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(String(countdown + 1), 120, 290, 4);
      }
    } else {
      // HAYIR seçiliyse geri sayımı temizle
      tft.fillRect(100, 275, 40, 30, TFT_BLACK);
    }
  }
  else if (gameState == 3) {
    // Level Complete - 3 saniye göster sonra devam
    if (now - levelCompleteTimer >= 3000) {
      currentLevel = 2;
      lives = 3; // Yeni seviye, yeni canlar
      lastLifeBonus = score; // Bonus hesabını güncelle
      tft.fillScreen(TFT_BLACK);
      gameState = 1;
      lastSpawn = millis();
    }
  }
  else if (gameState == 4) {
    // Game Won - 5 saniye göster sonra restart ekranı
    if (now - gameWonTimer >= 5000) {
      gameState = 2;
      encoderValue = 0;
      selectedOption = 1; // Default HAYIR
      confirmTimer = millis();
      showRestartConfirm();
    }
  }
  
  delay(10);
}

void animateStartScreen() {
  unsigned long now = millis();
  if (now - lastAnimTime > 500) {
    lastAnimTime = now;
    animFrame++;
    
    int colorIndex = animFrame % paletteSize;
    tft.setTextColor(palette[colorIndex], TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("RENK YAKALAMA", 120, 45, 4);
    tft.drawString("OYUNU", 120, 75, 4);
    
    digitalWrite(LED_PIN, animFrame % 2);
  }
}

void showStartScreen() {
  tft.fillScreen(TFT_BLACK);
  lastAnimTime = millis();
  animFrame = 0;
  confirmTimer = millis();
  
  // Dekoratif çerçeve
  tft.drawRect(5, 5, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 10, TFT_CYAN);
  tft.drawRect(7, 7, SCREEN_WIDTH - 14, SCREEN_HEIGHT - 14, TFT_MAGENTA);
  
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("RENK YAKALAMA", 120, 45, 4);
  tft.drawString("OYUNU", 120, 75, 4);
  
  // Dekoratif toplar
  tft.fillCircle(30, 35, 8, TFT_RED);
  tft.fillCircle(210, 35, 8, TFT_GREEN);
  tft.fillCircle(30, 85, 8, TFT_BLUE);
  tft.fillCircle(210, 85, 8, TFT_YELLOW);
  
  // Kurallar bölümü - daha geniş ve kompakt
  tft.fillRoundRect(10, 105, 220, 135, 8, TFT_DARKGREY);
  tft.drawRoundRect(10, 105, 220, 135, 8, TFT_WHITE);
  
  tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("KURALLAR", 120, 117, 2);
  
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("* Encoder sag-sol", 15, 138, 2);
  tft.drawString("* Cubuk rengi degisir", 15, 158, 2);
  
  tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
  tft.drawString("+ Dogru renk +10p", 15, 178, 2);
  
  tft.setTextColor(TFT_RED, TFT_DARKGREY);
  tft.drawString("- Yanlis renk -1 can", 15, 198, 2);
  
  tft.setTextColor(TFT_CYAN, TFT_DARKGREY);
  tft.drawString("+ 30p = +1 can", 15, 218, 2);
  
  if (highScore > 0) {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("EN YUKSEK: " + String(highScore), 120, 248, 2);
  }
  
  // "Oynamak ister misin?" sorusu - ortalanmış
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("OYNAMAK ISTER MISIN?", 120, 267, 2);
  
  updateStartMenu();
}

void startNewGame() {
  gameState = 1;
  score = 0;
  lives = 3;
  lastLifeBonus = 0;
  currentLevel = 1;
  encoderValue = 0;
  barX = 120;
  spawnInterval = 1200;
  
  myBarColor = random(0, paletteSize);
  
  for (int i = 0; i < MAX_BALLS; i++) {
    balls[i].active = false;
  }
  
  tft.fillScreen(TFT_BLACK);
  lastSpawn = millis();
  lastUpdate = millis();
  
  // Başlangıç melodisi
  tone(BUZZER_PIN, 1000, 80);
  delay(100);
  tone(BUZZER_PIN, 1200, 80);
  delay(100);
  tone(BUZZER_PIN, 1500, 80);
  delay(100);
  
  // Seviye ve renk göster
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("SEVIYE 1", 120, 140, 4);
  tft.setTextColor(palette[myBarColor], TFT_BLACK);
  tft.drawString("RENGIN: " + colorNames[myBarColor], 120, 180, 4);
  delay(1500);
  tft.fillScreen(TFT_BLACK);
}

void runGame() {
  unsigned long now = millis();
  
  // Encoder'dan pozisyon hesapla
  barX = 120 + (encoderValue * 2);
  barX = constrain(barX, CATCH_WIDTH/2, SCREEN_WIDTH - CATCH_WIDTH/2);
  
  // Seviye 1'de 50 puana ulaşınca seviye atla
  if (currentLevel == 1 && score >= 50 && lives > 0) {
    showLevelComplete();
    return;
  }
  
  // Seviye 2'de 70 puana ulaşınca (50+20) oyunu kazan!
  if (currentLevel == 2 && score >= 70 && lives > 0) {
    showGameWon();
    return;
  }
  
  // Top spawn
  if (now - lastSpawn > spawnInterval) {
    spawnBall();
    lastSpawn = now;
    if (currentLevel == 1) {
      if (spawnInterval > 800) spawnInterval -= 40;
    } else {
      if (spawnInterval > 500) spawnInterval -= 50;
    }
  }
  
  // Topları güncelle
  updateBalls();
  
  // HUD
  drawHUD();
  
  // Bar çiz
  drawBar();
  
  // Can bitti mi?
  if (lives <= 0) {
    delay(500);
    showGameOver();
  }
  
  delay(30);
}

void spawnBall() {
  for (int i = 0; i < MAX_BALLS; i++) {
    if (!balls[i].active) {
      balls[i].x = random(BALL_RADIUS_LVL2 + 10, SCREEN_WIDTH - BALL_RADIUS_LVL2 - 10);
      balls[i].y = 35;
      balls[i].colorId = random(0, paletteSize);
      
      if (currentLevel == 1) {
        // Seviye 1: Düz düşüş
        balls[i].radius = BALL_RADIUS_LVL1;
        balls[i].speed = random(25, 35) / 10.0;
        balls[i].vx = 0;
        balls[i].vy = balls[i].speed;
      } else {
        // Seviye 2: Büyük toplar + ZigZag hareket!
        balls[i].radius = BALL_RADIUS_LVL2;
        balls[i].speed = random(30, 45) / 10.0;
        balls[i].vx = random(-15, 15) / 10.0; // Yatay hareket (-1.5 ile +1.5 arası)
        balls[i].vy = balls[i].speed;
      }
      
      balls[i].active = true;
      break;
    }
  }
}

void updateBalls() {
  for (int i = 0; i < MAX_BALLS; i++) {
    if (balls[i].active) {
      // Eski pozisyonu sil
      tft.fillCircle((int)balls[i].x, (int)balls[i].y, balls[i].radius, TFT_BLACK);
      
      // Hareket
      balls[i].y += balls[i].vy;
      
      // Seviye 2: Zigzag hareketi
      if (currentLevel == 2) {
        balls[i].x += balls[i].vx;
        
        // Duvara çarpınca yön değiştir
        if (balls[i].x - balls[i].radius < 5 || balls[i].x + balls[i].radius > SCREEN_WIDTH - 5) {
          balls[i].vx = -balls[i].vx;
        }
      }
      
      // Yakalama kontrolü
      if (balls[i].y >= CATCH_Y - 10 && balls[i].y <= CATCH_Y + CATCH_HEIGHT) {
        if (abs(balls[i].x - barX) < CATCH_WIDTH/2 + balls[i].radius - 5) {
          // Yakalandı!
          if (balls[i].colorId == myBarColor) {
            // Doğru renk
            score += 10;
            tone(BUZZER_PIN, 1500, 80);
            digitalWrite(LED_PIN, HIGH);
            delay(50);
            digitalWrite(LED_PIN, LOW);
            
            // Her 30 puanda 1 can ver
            if (score - lastLifeBonus >= 30) {
              lives++;
              lastLifeBonus = score;
              tone(BUZZER_PIN, 2000, 100);
              delay(100);
              tone(BUZZER_PIN, 2500, 100);
            }
          } else {
            // Yanlış renk
            lives--;
            tone(BUZZER_PIN, 300, 150);
            for(int b=0; b<3; b++) {
              digitalWrite(LED_PIN, HIGH);
              delay(50);
              digitalWrite(LED_PIN, LOW);
              delay(50);
            }
          }
          balls[i].active = false;
          continue;
        }
      }
      
      // Ekrandan çıktı
      if (balls[i].y > SCREEN_HEIGHT) {
        balls[i].active = false;
      } else {
        // Yeni pozisyonda çiz
        tft.fillCircle((int)balls[i].x, (int)balls[i].y, balls[i].radius, palette[balls[i].colorId]);
      }
    }
  }
}

void drawHUD() {
  // Arka plan temizle
  tft.fillRect(0, 0, SCREEN_WIDTH, 28, TFT_BLACK);
  
  // Seviye göstergesi - SOL TARAF (süslü)
  tft.fillRoundRect(2, 2, 110, 24, 5, TFT_DARKGREY);
  tft.drawRoundRect(2, 2, 110, 24, 5, TFT_CYAN);
  
  tft.setTextColor(TFT_YELLOW, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Seviye", 6, 7, 2);
  
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(String(currentLevel), 108, 7, 2);
  
  // Skor göstergesi - SOL ALT
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(String(score) + "p", 6, 30, 2);
  
  // Can göstergesi - SAĞ TARAF (süslü)
  tft.fillRoundRect(128, 2, 110, 24, 5, TFT_DARKGREY);
  tft.drawRoundRect(128, 2, 110, 24, 5, TFT_RED);
  
  tft.setTextColor(TFT_RED, TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Can", 132, 7, 2);
  
  // Can kalpleri çiz 
  int heartX = 175;
  for (int i = 0; i < lives && i < 5; i++) {
    tft.fillCircle(heartX + i*12, 14, 4, TFT_RED);
  }
  
  if (score > highScore) highScore = score;
}

void drawBar() {
  static int oldBarX = 120;
  
  if (abs(barX - oldBarX) > 2) {
    tft.fillRect(0, CATCH_Y - 5, SCREEN_WIDTH, CATCH_HEIGHT + 10, TFT_BLACK);
  }
  
  int barLeft = barX - CATCH_WIDTH/2;
  
  tft.fillRoundRect(barLeft, CATCH_Y, CATCH_WIDTH, CATCH_HEIGHT, 6, palette[myBarColor]);
  tft.drawRoundRect(barLeft, CATCH_Y, CATCH_WIDTH, CATCH_HEIGHT, 6, TFT_WHITE);
  
  tft.setTextColor(TFT_WHITE, palette[myBarColor]);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(colorNames[myBarColor], barX, CATCH_Y + CATCH_HEIGHT/2, 2);
  
  oldBarX = barX;
}

void showLevelComplete() {
  gameState = 3;
  levelCompleteTimer = millis();
  
  // Topları temizle
  for (int i = 0; i < MAX_BALLS; i++) {
    if (balls[i].active) {
      tft.fillCircle((int)balls[i].x, (int)balls[i].y, balls[i].radius, TFT_BLACK);
      balls[i].active = false;
    }
  }
  
  // Kutlama efekti
  for (int i = 0; i < 5; i++) {
    tone(BUZZER_PIN, 1500 + i*200, 100);
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(50);
  }
  
  tft.fillRoundRect(20, 100, 200, 120, 10, TFT_GREEN);
  tft.drawRoundRect(20, 100, 200, 120, 10, TFT_YELLOW);
  tft.drawRoundRect(22, 102, 196, 116, 8, TFT_WHITE);
  
  tft.setTextColor(TFT_WHITE, TFT_GREEN);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("SEVIYE 1", 120, 130, 4);
  tft.drawString("TAMAMLANDI!", 120, 160, 4);
  
  tft.setTextColor(TFT_YELLOW, TFT_GREEN);
  tft.drawString("SEVIYE 2'ye", 120, 195, 2);
  tft.drawString("hazirlaniyor...", 120, 215, 2);
  
  // Yeni renk seç
  myBarColor = random(0, paletteSize);
  spawnInterval = 1000; // Seviye 2 için başlangıç hızı
}

void showGameOver() {
  // Topları temizle
  for (int i = 0; i < MAX_BALLS; i++) {
    if (balls[i].active) {
      tft.fillCircle((int)balls[i].x, (int)balls[i].y, balls[i].radius, TFT_BLACK);
      balls[i].active = false;
    }
  }
  
  // Ses efekti
  for (int i = 0; i < 4; i++) {
    tone(BUZZER_PIN, 400 - i*60, 120);
    digitalWrite(LED_PIN, HIGH);
    delay(120);
    digitalWrite(LED_PIN, LOW);
    delay(120);
  }
  
  tft.fillRoundRect(20, 100, 200, 120, 10, TFT_RED);
  tft.drawRoundRect(20, 100, 200, 120, 10, TFT_YELLOW);
  tft.drawRoundRect(22, 102, 196, 116, 8, TFT_WHITE);
  
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("OYUN BITTI!", 120, 125, 4);
  
  tft.setTextColor(TFT_YELLOW, TFT_RED);
  tft.drawString("Seviye: " + String(currentLevel), 120, 160, 2);
  tft.drawString("Skor: " + String(score), 120, 180, 2);
  
  if (score >= highScore && score > 0) {
    tft.setTextColor(TFT_GREEN, TFT_RED);
    tft.drawString("YENI REKOR!", 120, 200, 2);
  }
  
  delay(2000);
  
  gameState = 2;
  encoderValue = 0;
  selectedOption = 1; // Default HAYIR
  confirmTimer = millis();
  showRestartConfirm();
}

void showRestartConfirm() {
  tft.fillScreen(TFT_BLACK);
  
  tft.drawRect(10, 10, SCREEN_WIDTH - 20, SCREEN_HEIGHT - 20, TFT_CYAN);
  
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("TEKRAR OYNAMAK", 120, 70, 4);
  tft.drawString("ISTER MISIN?", 120, 100, 4);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Encoder ile sec", 120, 150, 2);
  
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("5 saniye sonra baslayacak", 120, 175, 2);
  
  updateRestartConfirm();
}

void updateRestartConfirm() {
  if (selectedOption == 0) {
    tft.fillRoundRect(30, 210, 90, 45, 10, TFT_GREEN);
    tft.drawRoundRect(30, 210, 90, 45, 10, TFT_WHITE);
    tft.fillRoundRect(130, 210, 90, 45, 10, TFT_DARKGREY);
  } else {
    tft.fillRoundRect(30, 210, 90, 45, 10, TFT_DARKGREY);
    tft.fillRoundRect(130, 210, 90, 45, 10, TFT_RED);
    tft.drawRoundRect(130, 210, 90, 45, 10, TFT_WHITE);
  }
  
  tft.setTextColor(selectedOption == 0 ? TFT_BLACK : TFT_WHITE, selectedOption == 0 ? TFT_GREEN : TFT_DARKGREY);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("EVET", 75, 232, 4);
  
  tft.setTextColor(TFT_WHITE, selectedOption == 1 ? TFT_RED : TFT_DARKGREY);
  tft.drawString("HAYIR", 175, 232, 4);
}

void updateStartMenu() {
  if (selectedOption == 0) {
    tft.fillRoundRect(30, 285, 90, 30, 8, TFT_GREEN);
    tft.drawRoundRect(30, 285, 90, 30, 8, TFT_WHITE);
    tft.fillRoundRect(130, 285, 90, 30, 8, TFT_DARKGREY);
  } else {
    tft.fillRoundRect(30, 285, 90, 30, 8, TFT_DARKGREY);
    tft.fillRoundRect(130, 285, 90, 30, 8, TFT_RED);
    tft.drawRoundRect(130, 285, 90, 30, 8, TFT_WHITE);
  }
  
  tft.setTextColor(selectedOption == 0 ? TFT_BLACK : TFT_WHITE, selectedOption == 0 ? TFT_GREEN : TFT_DARKGREY);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("EVET", 75, 300, 2);
  
  tft.setTextColor(TFT_WHITE, selectedOption == 1 ? TFT_RED : TFT_DARKGREY);
  tft.drawString("HAYIR", 175, 300, 2);
}

void showGameWon() {
  gameState = 4;
  gameWonTimer = millis();
  
  // Topları temizle
  for (int i = 0; i < MAX_BALLS; i++) {
    if (balls[i].active) {
      tft.fillCircle((int)balls[i].x, (int)balls[i].y, balls[i].radius, TFT_BLACK);
      balls[i].active = false;
    }
  }
  
  tft.fillScreen(TFT_BLACK);
  
  // Zafer melodisi!
  tone(BUZZER_PIN, 1000, 150);
  delay(150);
  tone(BUZZER_PIN, 1200, 150);
  delay(150);
  tone(BUZZER_PIN, 1400, 150);
  delay(150);
  tone(BUZZER_PIN, 1600, 150);
  delay(150);
  tone(BUZZER_PIN, 2000, 300);
  delay(300);
  
  // LED kutlaması
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  // Renkli konfeti animasyonu
  for (int i = 0; i < 30; i++) {
    int x = random(10, SCREEN_WIDTH - 10);
    int y = random(10, SCREEN_HEIGHT - 10);
    int colorIdx = random(0, paletteSize);
    tft.fillCircle(x, y, 3, palette[colorIdx]);
  }
  
  delay(500);
  
  // Altın kupa çiz! 
  drawTrophy();
  
  // Tebrik mesajı
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("TEBRIKLER!", 120, 30, 4);
  
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("OYUNU KAZANDIN!", 120, 60, 4);
  
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Toplam Skor:", 120, 240, 2);
  tft.drawString(String(score) + " PUAN", 120, 265, 4);
  
  if (score > highScore) {
    highScore = score;
    tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
    tft.drawString("YENI REKOR!", 120, 295, 2);
  }
}

void drawTrophy() {
  // ALTIN KUPA ÇİZİMİ 
  int cx = 120; // Merkez X
  int cy = 150; // Merkez Y
  
  // Kulplar
  tft.drawArc(cx - 35, cy - 20, 15, 12, 90, 270, TFT_YELLOW, TFT_BLACK);
  tft.drawArc(cx + 35, cy - 20, 15, 12, 270, 90, TFT_YELLOW, TFT_BLACK);
  
  // Kupa gövdesi (trapezoid)
  tft.fillRect(cx - 25, cy - 35, 50, 10, TFT_YELLOW);
  tft.fillTriangle(cx - 25, cy - 25, cx + 25, cy - 25, cx + 30, cy + 15, TFT_YELLOW);
  tft.fillTriangle(cx - 25, cy - 25, cx - 30, cy + 15, cx + 30, cy + 15, TFT_YELLOW);
  
  // Kupa tabanı
  tft.fillRect(cx - 35, cy + 15, 70, 8, TFT_YELLOW);
  tft.fillRect(cx - 20, cy + 23, 40, 10, TFT_YELLOW);
  tft.fillRect(cx - 25, cy + 33, 50, 5, TFT_YELLOW);
  
  // Detaylar (parlama efekti)
  tft.drawLine(cx - 15, cy - 30, cx - 15, cy + 10, TFT_ORANGE);
  tft.drawLine(cx - 10, cy - 30, cx - 10, cy + 10, TFT_WHITE);
  tft.drawLine(cx, cy - 32, cx, cy + 12, TFT_WHITE);
  
  // Yıldızlar 
  for (int i = 0; i < 5; i++) {
    int angle = i * 72;
    int x1 = cx + cos(angle * 3.14159 / 180) * 60;
    int y1 = cy - 20 + sin(angle * 3.14159 / 180) * 60;
    tft.fillCircle(x1, y1, 3, TFT_YELLOW);
  }
}
