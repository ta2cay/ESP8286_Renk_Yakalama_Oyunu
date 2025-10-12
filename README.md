# ESP8266/ESP32 Renk Yakalama Oyunu (TFT Color Catch Game) 

Bu proje, bir **ESP8266** veya **ESP32** mikrodenetleyici, renkli TFT ekran ve bir döner enkoder kullanılarak geliştirilmiş, refleks ve renk eşleştirme becerisi gerektiren,iki aşamadan oluşan;  ilk ve orta derece kademesine yönelik bir oyundur.

##  Proje Özeti ve Temel Özellikler

Oyunun temel amacı, ekranın altındaki **çubuğun (bar)** rengiyle aynı renkte düşen topları yakalamaktır. **Döner Enkoder**, çubuğun rengini anlık olarak değiştirmenizi sağlar.

* **Platform:** ESP8266 / ESP32 (Hızlı işlemci ve Wi-Fi özellikli kartlar tercih edilmiştir).
* **Kontrol:** **Döner Enkoder** (Renk seçimi için).
* **İki Seviyeli Yapı:**
    * **Seviye 1 (50 puana kadar):** Düz düşen, küçük toplar ve yavaş hız.
    * **Seviye 2 (70 puana kadar):** **Büyük toplar**, **daha yüksek hız** ve **ZigZag (yatay)** hareket ile zorluk artışı.
* **Bonus Sistemi:** Her **30 puanda bir ekstra can** (Lives) kazanılır.
* **Geri Bildirim:** Buzzer ve LED ile hızlı sesli/görsel tepkiler.
* **Durum Makineleri:** Başlangıç, Oyun, Seviye Tamamlama, Oyun Bitti ve Zafer (Kupa Çizimi) ekranları arasında net geçiş.

---

## Donanım Kurulumu ve Bağlantılar

### Gerekli Malzemeler

1.  **Mikrokontrolcü:** ESP8266 (NodeMCU veya Wemos D1 Mini) veya ESP32 Geliştirme Kartı.
2.  **TFT Ekran:** SPI arayüzlü 2.4" / 2.8" renkli TFT ekran (ILI9341 veya ST7789 sürücülü).
3.  **Döner Enkoder:** Anahtarlı (butonlu) tip.
4.  **Pasif Buzzer** (Zil).
5.  **LED** (Gerekli direnç ile).

### Pin Bağlantı Şeması

Bu proje, özellikle ESP kartlarının GPIO pinlerini kullanmaktadır:

| Bileşen | Kod Pin Tanımı | Tipik ESP Pin (Örn: NodeMCU) | Fonksiyon |
| :--- | :--- | :--- | :--- |
| **Enkoder CLK** | `#define ENCODER_CLK D1` | GPIO5 | Kesme (Interrupt) tanımlı |
| **Enkoder DT** | `#define ENCODER_DT D2` | GPIO4 | Kesme (Interrupt) tanımlı |
| **Enkoder Buton** | `#define ENCODER_BTN D0` | GPIO16 | Menü/Seçim İşlemleri (Şu an kullanılmıyor) |
| **Buzzer** | `#define BUZZER_PIN D6` | GPIO12 | Ses Çıkışı |
| **LED** | `#define LED_PIN D7` | GPIO13 | Görsel Flaş Efekti |

> ⚠️ **ÖNEMLİ:** Enkoder pinleri için **`attachInterrupt()`** kullanılmıştır. ESP kartlarda bu pinlerin kesme desteği olduğundan emin olunmalıdır.

---

## 📚 Kütüphane ve Ortam Kurulumu

Projeyi derlemek için aşağıdaki kütüphanelerin Arduino IDE veya PlatformIO ortamına eklenmesi gerekir:

1.  **TFT_eSPI:** TFT ekran sürücüsü ve çizim fonksiyonları için optimize edilmiş kütüphane.
    * **Ayarlama:** Bu kütüphanenin doğru çalışması için, projenizle birlikte yüklediğiniz gibi, kullanılan ekran modeline ve SPI pinlerine uygun bir **`User_Setup.h`** dosyası oluşturulmalı veya düzenlenmelidir.
2.  **SPI:** TFT iletişim protokolü için standart kütüphane.
3.  **ESP Kart Desteği:** Arduino IDE Kart Yöneticisinden **ESP8266** veya **ESP32** kart paketinin kurulu olduğundan emin olun.

---

##  Oyun Mekaniği ve Kod Detayları

### 1. Skorlama ve Geri Bildirim

| Durum | Etki | Geri Bildirim (LED ve Buzzer) |
| :--- | :--- | :--- |
| **Doğru Renk Yakalama** | Skor **+10** | Yüksek zil sesi, LED bir kez yanıp söner. |
| **Yanlış Renk Yakalama** | Can **-1** | Kalın, alçak zil sesi, LED üç kez hızlı yanıp söner. |
| **Can Bonusu** | Skor her **30 puanda** bir Can **+1** | İki farklı yüksek zil sesi. |

### 2. Düşen Topların Davranışı (`updateBalls()`)

Kod, topların hareketini **Seviye 2**'de önemli ölçüde zorlaştırır:

| Özellik | Seviye 1 (50 puana kadar) | Seviye 2 (50 puandan sonra) |
| :--- | :--- | :--- |
| **Yarıçap (`radius`)** | 12 piksel (Daha küçük) | 15 piksel (Daha büyük) |
| **Hız (`vy`)** | Düz, yavaş düşüş. | Daha hızlı düşüş (Random 3.0 - 4.5). |
| **Yatay Hız (`vx`)** | 0 (Düz düşer) | Rastgele $\pm 1.5$ (ZigZag hareket) |
| **Engeller** | Yok | Yan duvarlara çarpıp yön değiştirir. |

### 3. Ekran Boyutları ve Çubuk Konumu

* `#define SCREEN_WIDTH 240`
* `#define SCREEN_HEIGHT 320`
* `#define CATCH_Y 285` (Yakalama çubuğunun dikey konumu)
* `#define CATCH_WIDTH 60`

### 4. Animasyonlar

* **`animateStartScreen()`:** Başlangıç menüsünde başlık rengini sürekli değiştirir.
* **`showGameWon()`:** Oyun kazanılınca Zafer melodisi, LED yanıp sönmeleri ve ekranda **Altın Kupa çizimi** ve konfeti efekti gösterilir.

---

##  Başlatma ve Çalıştırma

1.  **Kod Yükleme:** `.ino` dosyasını ESP kartınıza yükleyin.
2.  **Başlangıç Menüsü:** Ekran açıldığında "Oynamak İster Misin?" sorusu belirir.
3.  **Seçim:** Enkoderi çevirerek **EVET** veya **HAYIR** seçeneğini belirleyin.
4.  **Otomatik Başlatma:** Eğer **EVET** seçiliyse, 5 saniyelik geri sayım sonunda oyun otomatik başlar.
5.  **Oyun İçi:** Enkoderi çevirerek yakalama çubuğunuzun rengini değiştirin ve aynı renkli topları yakalamaya çalışın!
