#include <Arduino.h>
#include <WiFiS3.h>
#include <PubSubClient.h>

// --- Configuration Wi-Fi ---
const char *ssid = "HotspotCleo";
const char *password = "cleo2004";

// --- Configuration MQTT Broker ---
const char *mqtt_broker = "broker.emqx.io";
const int mqtt_port = 1883;

// Topics
const char *topic_pot      = "hometrainer/sante/bpm";
const char *topic_alerte   = "hometrainer/sante/alerte";
const char *topic_systeme  = "hometrainer/systeme/etat";
const char *topic_led_cmd  = "hometrainer/led/cmd";

// --- Broches matérielles ---
const int POT_PIN      = A0;
const int SWITCH_PIN   = 2;  // Interrupteur 2 broches entre D2 et GND
const int BUZZER_PIN   = 3;

// Broches LED RGB
const int RGB_RED      = 11;
const int RGB_GREEN    = 10;
const int RGB_BLUE     = 12;

const int SEUIL_CRITIQUE = 800;

// --- États du système ---
bool systemeActif = false;
bool alerteEnCours = false;

// Clignotement lent en Bleu (mode actif sans alerte)
unsigned long dernierBlinkBleu = 0;
const unsigned long PERIODE_BLINK_BLEU = 600;
bool etatBleu = false;

// --- Mozart : Une petite musique de nuit (KV 525) ---
#define NOTE_D4  294
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587

const int melodieMozart[] = {
  NOTE_G4, 0, NOTE_D4, 0, NOTE_G4, NOTE_D4, NOTE_G4, NOTE_B4, NOTE_D5, 0,
  NOTE_C5, 0, NOTE_A4, 0, NOTE_C5, NOTE_A4, NOTE_FS4, NOTE_A4, NOTE_D4
};

const int dureeNotes[] = {
  180, 50, 180, 50, 110, 110, 110, 110, 360, 150,
  180, 50, 180, 50, 110, 110, 110, 110, 400
};

const int nbNotes = sizeof(melodieMozart) / sizeof(melodieMozart[0]);

int noteCourante = 0;
unsigned long tempsNotePrecedente = 0;
bool musiqueEnLecture = false;

String client_id = "ArduinoR4-";
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

unsigned long lastPublishTime = 0;
const unsigned long PUBLISH_INTERVAL = 250;

void connectToWiFi();
void connectToMQTTBroker();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void setRgbColor(bool r, bool g, bool b);
void actualiserMusiqueNonBloquante();

void setup() {
  Serial.begin(9600);

  // Configuration de l'interrupteur avec résistance de tirage interne
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  pinMode(RGB_RED, OUTPUT);
  pinMode(RGB_GREEN, OUTPUT);
  pinMode(RGB_BLUE, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  setRgbColor(false, false, false);

  connectToWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
  connectToMQTTBroker();
}

void setRgbColor(bool r, bool g, bool b) {
  digitalWrite(RGB_RED, r ? HIGH : LOW);
  digitalWrite(RGB_GREEN, g ? HIGH : LOW);
  digitalWrite(RGB_BLUE, b ? HIGH : LOW);
}

void connectToWiFi() {
  Serial.print("Connexion au Wi-Fi ");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay(200);
    Serial.print("+");
  }

  Serial.println("\nWi-Fi connecte !");
  Serial.print("Adresse IP : ");
  Serial.println(WiFi.localIP());
  
  client_id = "ArduinoR4-" + String(WiFi.localIP()[3]) + "-" + String(millis());
}

void connectToMQTTBroker() {
  while (!mqtt_client.connected()) {
    Serial.print("Connexion broker MQTT...");
    if (mqtt_client.connect(client_id.c_str())) {
      Serial.println(" Connecte !");
      mqtt_client.subscribe(topic_led_cmd);
    } else {
      delay(3000);
    }
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  // Prise en charge d'éventuels ordres manuels
}

void actualiserMusiqueNonBloquante() {
  if (!musiqueEnLecture) {
    noTone(BUZZER_PIN);
    return;
  }

  unsigned long maintenant = millis();
  unsigned long dureeTotaleNote = dureeNotes[noteCourante] * 1.25;

  if (maintenant - tempsNotePrecedente >= dureeTotaleNote) {
    tempsNotePrecedente = maintenant;
    noteCourante++;

    if (noteCourante >= nbNotes) {
      musiqueEnLecture = false;
      noteCourante = 0;
      noTone(BUZZER_PIN);
      return;
    }

    if (melodieMozart[noteCourante] == 0) {
      noTone(BUZZER_PIN);
    } else {
      tone(BUZZER_PIN, melodieMozart[noteCourante], dureeNotes[noteCourante]);
    }
  }
}

void loop() {
  if (!mqtt_client.connected()) {
    connectToMQTTBroker();
  }
  mqtt_client.loop();

  actualiserMusiqueNonBloquante();

  // --- 1. Lecture directe de la position de l'interrupteur ---
  // Circuit fermé (ON) vers GND = LOW | Circuit ouvert (OFF) = HIGH
  bool interrupteurON = (digitalRead(SWITCH_PIN) == LOW);

  // Détection d'un basculement d'état
  if (interrupteurON != systemeActif) {
    systemeActif = interrupteurON;
    
    Serial.print("Interrupteur bascule -> ");
    Serial.println(systemeActif ? "ON (Actif)" : "OFF (Veille)");

    mqtt_client.publish(topic_systeme, systemeActif ? "1" : "0");

    if (!systemeActif) {
      // Coupure totale immédiate si basculé sur OFF
      setRgbColor(false, false, false);
      musiqueEnLecture = false;
      noTone(BUZZER_PIN);
      alerteEnCours = false;
      mqtt_client.publish(topic_pot, "0");
    }
  }

  // --- 2. Logique active quand l'interrupteur est sur ON ---
  unsigned long maintenant = millis();

  if (systemeActif) {
    int potValue = analogRead(POT_PIN);

    // Publication cadencée
    if (maintenant - lastPublishTime >= PUBLISH_INTERVAL) {
      lastPublishTime = maintenant;
      String payload = String(potValue);
      mqtt_client.publish(topic_pot, payload.c_str());

      Serial.print("BPM: ");
      Serial.println(potValue);
    }

    // Gestion de l'alerte vs état normal
    if (potValue > SEUIL_CRITIQUE) {
      // Alerte : Rouge fixe + Jingle Mozart
      setRgbColor(true, false, false);

      if (!alerteEnCours) {
        mqtt_client.publish(topic_alerte, "🚨 ALERTE CRITIQUE : Seuil depasse !");
        alerteEnCours = true;

        noteCourante = 0;
        tempsNotePrecedente = maintenant;
        musiqueEnLecture = true;
        if (melodieMozart[0] != 0) {
          tone(BUZZER_PIN, melodieMozart[0], dureeNotes[0]);
        }
      }
    } else {
      // Normal : Clignotement Bleu régulier
      alerteEnCours = false;
      musiqueEnLecture = false;
      noTone(BUZZER_PIN);

      if (maintenant - dernierBlinkBleu >= PERIODE_BLINK_BLEU) {
        dernierBlinkBleu = maintenant;
        etatBleu = !etatBleu;
      }
      setRgbColor(false, false, etatBleu);
    }

  } else {
    // Mode veille : tout est maintenu éteint
    setRgbColor(false, false, false);
  }
}