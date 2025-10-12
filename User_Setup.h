// User_Setup.h için yapılandırma
// Bu içeriği TFT_eSPI/User_Setup.h dosyasına kopyalayın

// ##################################################################################
//
// ESP8266 NodeMCU için 2.8" ILI9341 TFT Ekran Ayarları
//
// ##################################################################################

// Sürücü seçimi
#define ILI9341_DRIVER

// ESP8266 için pin tanımlamaları
#define TFT_CS   15  // Chip select control pin D8
#define TFT_DC    0  // Data Command control pin D3
#define TFT_RST   2  // Reset pin (could connect to NodeMCU RST, see next line) D4

// SPI pinleri otomatik (Hardware SPI kullanılacak)
// MOSI = GPIO13 (D7)
// MISO = GPIO12 (D6) 
// SCK  = GPIO14 (D5)

// Ekran boyutları
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Font desteği
#define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
#define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH
#define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH
#define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH
#define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH
#define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH

#define SMOOTH_FONT

// SPI frekansı (ESP8266 için optimize edilmiş)
#define SPI_FREQUENCY  27000000  // 27MHz - stabil çalışma için
// #define SPI_FREQUENCY  40000000  // 40MHz - daha hızlı ama bazı ekranlarda sorun olabilir

#define SPI_READ_FREQUENCY  20000000

// SPI Touch Controller desteği (kullanmıyorsak kapalı)
// #define TOUCH_CS 16  // Dokunmatik kullanacaksanız açın

// Diğer ayarlar
#define SUPPORT_TRANSACTIONS