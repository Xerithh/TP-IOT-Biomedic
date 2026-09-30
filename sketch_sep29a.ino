#include <Arduino.h>
#include <WiFiS3.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_MLX90614.h>

// Constructeur OLED U8g2 (I2C matériel)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Capteur IR MLX90614
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
bool mlxDetecte = false;

// --- Configuration Wi-Fi ---
const char *ssid = "HotspotCleo";
const char *password = "cleo2004";

// --- Configuration MQTT Broker ---
const char *mqtt_broker = "broker.emqx.io";
const int mqtt_port = 1883;

const char *topic_pot      = "hometrainer/sante/bpm";
const char *topic_alerte   = "hometrainer/sante/alerte";
const char *topic_systeme  = "hometrainer/systeme/etat";
const char *topic_led_cmd  = "hometrainer/led/cmd";

// --- Broches matérielles ---
const int POT_PIN             = A0;
const int SWITCH_PIN          = 2;  // Interrupteur (D2 vers GND)
const int BUZZER_PIN          = 3;  // Buzzer
const int LED_DETECTION_VERTE = 4;  // Témoin vert fixe détection

// Broches LED RGB
const int RGB_RED             = 11; // Rouge
const int RGB_GREEN           = 10; // Vert
const int RGB_BLUE            = 12; // Bleu

const int BPM_MAX        = 210; // Valeur maximale BPM pour le potentiomètre
const int SEUIL_CRITIQUE = 160; // Seuil d'alerte BPM cardiaque critique

// États système
bool systemeActif = false;
bool alerteEnCours = false;

// Clignotement Bleu (LED RGB)
unsigned long dernierBlinkBleu = 0;
const unsigned long PERIODE_BLINK_BLEU = 600;
bool etatBleu = false;

// --- Mozart : Une petite musique de nuit ---
#define NOTE_D4  294
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587

const int melodieMozart[] = {
  NOTE_G4, 0, NOTE_D4, 0, NOTE_G4, NOTE_D4, NOTE_G4, NOTE_B4, NOTE_D5, 0,
  NOTE_C5, 0, NOTE_A4, 0, NOTE_C5, NOTE_A4, NOTE_FS4, NOTE_A4, NOTE_D4, 0
};

const int dureeNotes[] = {
  180, 50, 180, 50, 110, 110, 110, 110, 360, 150,
  180, 50, 180, 50, 110, 110, 110, 110, 350, 200
};
const int nbNotes = sizeof(melodieMozart) / sizeof(melodieMozart[0]);

int noteCourante = 0;
unsigned long tempsNotePrecedente = 0;
bool musiqueEnLecture = false;

// Dernières températures valides mémorisées
float derniereTempCorps = 36.5;
float derniereTempAmb   = 22.0;

char client_id[40] = "ArduinoR4-";
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// Intervalles de temps
unsigned long lastPublishTime = 0;
const unsigned long PUBLISH_INTERVAL = 300;

unsigned long lastMlxReadTime = 0;
const unsigned long MLX_READ_INTERVAL = 500;

unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

// Anti-rebond interrupteur
unsigned long dernierChangementSwitch = 0;
const unsigned long DEBOUNCE_DELAY = 50;

void setRgbColor(bool r, bool g, bool b);
void actualiserMusiqueNonBloquante();
void actualiserEcran(float tempCorps, float tempAmb, int bpm, bool alerte);
void afficherEcranEteint();
void gererReconnexionMQTT();
int lireBPM();

int lireBPM() {
  int rawValue = analogRead(POT_PIN);
  int bpm = map(rawValue, 0, 1023, 0, BPM_MAX);
  return constrain(bpm, 0, BPM_MAX);
}

void setup() {
  Serial.begin(9600);

  pinMode(SWITCH_PIN, INPUT_PULLUP);
  pinMode(LED_DETECTION_VERTE, OUTPUT);
  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_DETECTION_VERTE, LOW);
  setRgbColor(false, false, false);

  // Initialisation I2C standard à 100 kHz (MLX90614 + SSD1306)
  Wire.begin();
  Wire.setClock(100000);

  u8g2.setBusClock(100000);
  u8g2.begin();
  u8g2.setPowerSave(0); // Allume l'écran OLED

  // Détection matérielle réelle du capteur IR sur l'adresse 0x5A
  Wire.beginTransmission(0x5A);
  if (Wire.endTransmission() == 0) {
    if (mlx.begin()) {
      Serial.println("-> Capteur IR MLX90614 détecté avec succès à l'adresse 0x5A !");
      mlxDetecte = true;
    }
  } else {
    Serial.println("⚠️ MLX90614 non détecté sur le bus I2C (adresse 0x5A)");
    mlxDetecte = false;
  }

  // Connexion Wi-Fi
  Serial.print("Connexion Wi-Fi ");
  WiFi.begin(ssid, password);
  unsigned long debutWifi = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - debutWifi < 10000)) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
      delay(200);
      Serial.print("+");
    }
    Serial.println("\nWi-Fi connecté !");
    snprintf(client_id, sizeof(client_id), "ArduinoR4-%u-%lu", WiFi.localIP()[3], (unsigned long)millis());
  } else {
    Serial.println("\n⚠️ Wi-Fi non connecté (délai dépassé) - mode hors-ligne");
    snprintf(client_id, sizeof(client_id), "ArduinoR4-offline-%lu", (unsigned long)millis());
  }

  // Configuration MQTT avec socket timeout court (1s) pour ne jamais geler l'Arduino
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setSocketTimeout(1);

  // Premier affichage selon l'état réel de l'interrupteur
  systemeActif = (digitalRead(SWITCH_PIN) == LOW);
  Serial.print("État initial interrupteur : ");
  Serial.println(systemeActif ? "ON (Détection active)" : "OFF (Détection éteinte)");

  if (systemeActif) {
    digitalWrite(LED_DETECTION_VERTE, HIGH);
    int initBpm = lireBPM();
    actualiserEcran(derniereTempCorps, derniereTempAmb, initBpm, (initBpm > SEUIL_CRITIQUE));
  } else {
    afficherEcranEteint();
  }
}

void setRgbColor(bool r, bool g, bool b) {
  digitalWrite(RGB_RED, r ? HIGH : LOW);
  digitalWrite(RGB_GREEN, g ? HIGH : LOW);
  digitalWrite(RGB_BLUE, b ? HIGH : LOW);
}

void actualiserMusiqueNonBloquante() {
  if (!musiqueEnLecture) {
    noTone(BUZZER_PIN);
    return;
  }

  unsigned long maintenant = millis();
  unsigned long dureeTotaleNote = (unsigned long)(dureeNotes[noteCourante] * 1.25);

  if (maintenant - tempsNotePrecedente >= dureeTotaleNote) {
    tempsNotePrecedente = maintenant;
    noteCourante++;

    if (noteCourante >= nbNotes) {
      noteCourante = 0;
    }

    if (melodieMozart[noteCourante] == 0) {
      noTone(BUZZER_PIN);
    } else {
      tone(BUZZER_PIN, melodieMozart[noteCourante], dureeNotes[noteCourante]);
    }
  }
}

void afficherEcranEteint() {
  u8g2.setPowerSave(0);
  u8g2.clearBuffer();
  
  // Titre aligné à gauche
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(0, 16, "HOME TRAINER");
  u8g2.drawHLine(0, 22, 128);

  // État aligné à gauche
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(0, 40, "DETECTION");
  u8g2.drawStr(80, 40, "OFF");

  // Consigne alignée à gauche
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 56, "Activez interrupteur");
  
  u8g2.sendBuffer();
}

// Écran pendant la détection (utilisation de buffers statiques pour 0 allocation dynamique)
void actualiserEcran(float tempCorps, float tempAmb, int bpm, bool alerte) {
  char buffer[32];

  u8g2.setPowerSave(0);
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 10, "TEMP CORPORELLE");

  // Température en grand
  u8g2.setFont(u8g2_font_helvB14_tf);
  snprintf(buffer, sizeof(buffer), "%.1f C", tempCorps);
  u8g2.drawStr(0, 30, buffer);

  // Ligne de séparation
  u8g2.drawHLine(0, 35, 128);

  // BPM & Statut
  u8g2.setFont(u8g2_font_6x12_tf);
  snprintf(buffer, sizeof(buffer), "BPM: %d", bpm);
  u8g2.drawStr(0, 48, buffer);

  if (alerte) {
    u8g2.drawStr(68, 48, "! ALERTE !");
  } else {
    u8g2.drawStr(80, 48, "STATUT: OK");
  }

  // Température Ambiante
  snprintf(buffer, sizeof(buffer), "Ambiante: %.1f C", tempAmb);
  u8g2.drawStr(0, 61, buffer);

  u8g2.sendBuffer();
}

void gererReconnexionMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return; // Évite tout blocage si le Wi-Fi est déconnecté
  }

  if (!mqtt_client.connected()) {
    unsigned long maintenant = millis();
    if (maintenant - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectAttempt = maintenant;
      Serial.println("[MQTT] Tentative de connexion non-bloquante...");
      if (mqtt_client.connect(client_id)) {
        Serial.println("[MQTT] Connecté !");
        mqtt_client.subscribe(topic_led_cmd);
      }
    }
  } else {
    mqtt_client.loop();
  }
}

void loop() {
  unsigned long maintenant = millis();

  // Reconnexion non-bloquante et traitement MQTT
  gererReconnexionMQTT();

  // Gestion du buzzer non-bloquant
  actualiserMusiqueNonBloquante();

  // --- 1. Gestion de l'interrupteur avec anti-rebond ---
  bool interrupteurON = (digitalRead(SWITCH_PIN) == LOW);

  if ((interrupteurON != systemeActif) && (maintenant - dernierChangementSwitch >= DEBOUNCE_DELAY)) {
    dernierChangementSwitch = maintenant;
    systemeActif = interrupteurON;
    
    Serial.print(">> [INTERRUPTEUR] Basculement vers -> ");
    Serial.println(systemeActif ? "ON (Détection active)" : "OFF (Détection éteinte)");

    if (mqtt_client.connected()) {
      mqtt_client.publish(topic_systeme, systemeActif ? "1" : "0");
    }

    if (!systemeActif) {
      // Extinction des témoins et musique
      digitalWrite(LED_DETECTION_VERTE, LOW);
      setRgbColor(false, false, false);
      musiqueEnLecture = false;
      noteCourante = 0;
      noTone(BUZZER_PIN);
      alerteEnCours = false;
      if (mqtt_client.connected()) {
        mqtt_client.publish(topic_pot, "0");
      }

      // Affichage immédiat du message d'extinction
      Serial.println("[OLED] Affichage écran veille (OFF)...");
      afficherEcranEteint();
    } else {
      // Remise en marche immédiate de la LED verte D4 et actualisation écran
      digitalWrite(LED_DETECTION_VERTE, HIGH);
      lastPublishTime = maintenant;
      lastMlxReadTime = maintenant;
      dernierBlinkBleu = maintenant;
      etatBleu = true;
      setRgbColor(false, false, true);

      int bpm = lireBPM();
      Serial.println("[OLED] Affichage écran détection (ON)...");
      actualiserEcran(derniereTempCorps, derniereTempAmb, bpm, (bpm > SEUIL_CRITIQUE));
    }
  }

  // --- 2. Détection Active ---
  if (systemeActif) {
    int bpm = lireBPM();

    // Lecture cadencée du MLX90614 (toutes les 500ms max pour ne pas saturer le bus I2C)
    if (mlxDetecte && (maintenant - lastMlxReadTime >= MLX_READ_INTERVAL)) {
      lastMlxReadTime = maintenant;

      float tObj = mlx.readObjectTempC();
      float tAmb = mlx.readAmbientTempC();

      // Si la lecture est valide, on actualise la mémoire
      if (!isnan(tObj) && tObj > -40.0 && tObj < 120.0) {
        derniereTempCorps = tObj;
      }
      if (!isnan(tAmb) && tAmb > -40.0 && tAmb < 120.0) {
        derniereTempAmb = tAmb;
      }
    }

    // Publication cadencée & Rafraîchissement écran
    if (maintenant - lastPublishTime >= PUBLISH_INTERVAL) {
      lastPublishTime = maintenant;

      char payload[16];
      snprintf(payload, sizeof(payload), "%d", bpm);
      if (mqtt_client.connected()) {
        mqtt_client.publish(topic_pot, payload);
      }

      Serial.print("BPM: ");
      Serial.print(bpm);
      Serial.print(" | Corps: ");
      Serial.print(derniereTempCorps, 1);
      Serial.print(" C | Amb: ");
      Serial.print(derniereTempAmb, 1);
      Serial.println(" C");

      actualiserEcran(derniereTempCorps, derniereTempAmb, bpm, (bpm > SEUIL_CRITIQUE));
    }

    // Gestion de l'alerte
    if (bpm > SEUIL_CRITIQUE) {
      setRgbColor(true, false, false); // Rouge continu

      if (!alerteEnCours) {
        if (mqtt_client.connected()) {
          mqtt_client.publish(topic_alerte, "🚨 ALERTE CRITIQUE : Seuil dépassé !");
        }
        alerteEnCours = true;

        noteCourante = 0;
        tempsNotePrecedente = maintenant;
        musiqueEnLecture = true;
        if (melodieMozart[0] != 0) {
          tone(BUZZER_PIN, melodieMozart[0], dureeNotes[0]);
        }
      }
    } else {
      if (alerteEnCours) {
        alerteEnCours = false;
        musiqueEnLecture = false;
        noteCourante = 0;
        noTone(BUZZER_PIN);
      }

      if (maintenant - dernierBlinkBleu >= PERIODE_BLINK_BLEU) {
        dernierBlinkBleu = maintenant;
        etatBleu = !etatBleu;
        setRgbColor(false, false, etatBleu); // Clignotement Bleu (Pin 12)
      }
    }
  }
}