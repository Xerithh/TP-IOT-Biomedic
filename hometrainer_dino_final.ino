#include <Arduino.h>
#include <WiFiS3.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Adafruit_MLX90614.h>
#include <DFRobot_Heartrate.h>

// --- Wi-Fi ---
const char *ssid = "MON_SSID";
const char *password = "MON_MOT_DE_PASSE";

// --- MQTT ---
const char *mqtt_broker = "broker.emqx.io";
const int mqtt_port = 1883;

// topics (les mêmes que dans Node-RED)
const char *topic_bpm         = "gaelle-capteurs/bpm";
const char *topic_temp        = "gaelle-capteurs/temperature";
const char *topic_bouton      = "gaelle-capteurs/bouton";
const char *topic_source      = "gaelle-capteurs/source";
const char *topic_signal      = "gaelle-capteurs/signal_bpm";
const char *topic_led1        = "gaelle-capteurs/led1";
const char *topic_alarme_bpm  = "gaelle-capteurs/alarme_bpm";
const char *topic_alarme_temp = "gaelle-capteurs/alarme_temp";
const char *topic_source_cmd  = "gaelle-capteurs/source_cmd";
const char *topic_dino        = "gaelle-capteurs/dino_score";

// --- Broches ---
const int POT_PIN             = A0;
const int BPM_SENSOR_PIN      = A1;  // lu avec analogRead, donc broche analogique
const int JOY_Y_PIN           = A2;  // axe vertical + clic
const int JOY_X_PIN           = A3;
const int SWITCH_PIN          = 2;  // interrupteur entre D2 et GND
const int BUZZER_PIN          = 3;
const int LED_DETECTION_VERTE = 4;  // témoin du mode santé
const int RGB_RED             = 11;
const int RGB_GREEN           = 10;
const int RGB_BLUE            = 12;

// --- Réglages ---
const int BPM_MAX_POT = 210;  // BPM maximum du potentiomètre
const int BPM_VALIDE_MIN = 40;  // le capteur réel est ignoré en dehors de 40-200
const int BPM_VALIDE_MAX = 200;
const unsigned long BPM_TIMEOUT = 20000;

// seuils utilisés si on n'a pas de connexion à Node-RED
const int   SEUIL_BPM_SECOURS  = 120;
const float SEUIL_TEMP_SECOURS = 37.5;
const int   HYST_BPM  = 3;
const float HYST_TEMP = 0.2;

// joystick : repos ~500, haut/gauche ~200, bas/droite ~700, clic ~1000
const int JOY_SEUIL_DIR  = 150;
const int JOY_SEUIL_CLIC = 900;
const unsigned long JOY_STABLE_MS = 30;

const int FREQ_BIP = 600;
const unsigned long DUREE_SEGMENT_BIPS = 2400;

// --- Écran et capteurs ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
bool mlxDetecte = false;

DFRobot_Heartrate heartrate(DIGITAL_MODE);
int bpmCapteur = 0;
unsigned long dernierEchantillonBpm = 0;
unsigned long derniereMesureBpm = 0;

// --- États système ---
bool systemeActif = false;
bool sourceCapteur = false;
int  pageEcran = 1;

bool alarmeBpmOn = false;
bool alarmeTempOn = false;
bool ledDashboardOn = false;

bool alerteBpmActive = false;
bool alerteTempActive = false;
bool repliSecours = false;
bool etatBpmSecours = false, etatTempSecours = false;

float derniereTempCorps = 0;

int reposX = 500, reposY = 500;

// --- Jeu du dino ---
#define DINO_ATTENTE 0
#define DINO_JEU     1
#define DINO_FIN     2
#define DINO_MAX_OBS 4
const int SOL_Y = 58;
const unsigned long DINO_FRAME_MS = 33;

// constantes du vrai jeu de Chrome (zone de jeu 600x150, 60 images par seconde)
const float VITESSE_INIT = 6.0;
const float VITESSE_MAX = 13.0;
const float ACCELERATION = 0.001;
const float GRAVITE = 0.6;
const float SAUT_INIT = 10.0;
const float HAUTEUR_MIN_SAUT = 35.0;
const float DELAI_OBSTACLES = 3000;
const float DINO_X = 50.0;
const float DINO_Y_SOL = 93.0;
const float SOL_LOGIQUE = 140.0;

// passage de la zone de jeu aux pixels de l'écran
const float ECH_X = 128.0 / 600.0;
const float ECH_Y = 0.3;

int dinoEtat = DINO_ATTENTE;
unsigned long dinoDernierFrame = 0;
unsigned long dinoDebutFin = 0;
float dinoHauteur = 0, dinoVitesseY = 0;
bool dinoSaute = false, dinoBaisse = false, dinoMinAtteint = false;
float dinoVitesse = VITESSE_INIT;
float dinoDistance = 0;
float dinoTemps = 0;
float dinoSol = 0;
int dinoScore = 0, dinoRecord = 0, dinoJalon = 0;
float obsX[DINO_MAX_OBS];
int obsType[DINO_MAX_OBS];
int obsTaille[DINO_MAX_OBS];
int obsY[DINO_MAX_OBS];
float obsGap[DINO_MAX_OBS];
float obsDecal[DINO_MAX_OBS];
bool obsSuivant[DINO_MAX_OBS];
int dernierType1 = 0, dernierType2 = 0;
int sonFinIdx = -1;
unsigned long sonFinProchain = 0;
const int sonFinNotes[] = {392, 330, 262};
const int sonFinDurees[] = {160, 160, 320};

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
  NOTE_C5, 0, NOTE_A4, 0, NOTE_C5, NOTE_A4, NOTE_FS4, NOTE_A4, NOTE_D4, 0
};

const int dureeNotes[] = {
  180, 50, 180, 50, 110, 110, 110, 110, 360, 150,
  180, 50, 180, 50, 110, 110, 110, 110, 350, 200
};
const int nbNotes = sizeof(melodieMozart) / sizeof(melodieMozart[0]);

// enchaînement Mozart / bips quand les deux alertes sont actives
#define SEG_AUCUN   0
#define SEG_MELODIE 1
#define SEG_BIPS    2
int segmentSon = SEG_AUCUN;
int dernierSegment = SEG_BIPS;
unsigned long debutSegment = 0;
int noteCourante = 0;
unsigned long debutNote = 0;

// --- Réseau ---
char client_id[40] = "ArduinoR4-";
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// --- Intervalles de temps ---
unsigned long lastPublishTime = 0;
const unsigned long PUBLISH_INTERVAL = 1000;
unsigned long lastMlxReadTime = 0;
const unsigned long MLX_READ_INTERVAL = 500;
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;
unsigned long dernierChangementSwitch = 0;
const unsigned long DEBOUNCE_DELAY = 50;

// l'écran n'est redessiné que si quelque chose a changé
unsigned long lastOledTime = 0;
const unsigned long OLED_INTERVAL = 500;
int affMode = -1, affPage = -1, affBpm = -1, affTempX10 = -1000;
bool affAlerte = false, affSource = false, affSignal = false, affWifi = false, affMqtt = false, affRepli = false;

void setRgbColor(bool r, bool g, bool b);
void mettreAJourEcran(bool forcer);
void gererReconnexionMQTT();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void publierSource();
int bpmCourant();
void dinoClic();
void dinoEntrer();
void dinoSortir();
void dinoLoop();

// --- Capteurs ---
int lireBpmPotentiometre() {
  int rawValue = analogRead(POT_PIN);
  int bpm = map(rawValue, 0, 1023, 0, BPM_MAX_POT);
  return constrain(bpm, 0, BPM_MAX_POT);
}

int bpmCourant() {
  return sourceCapteur ? bpmCapteur : lireBpmPotentiometre();
}

void traiterCapteurBpm() {
  unsigned long maintenant = millis();

  if (maintenant - dernierEchantillonBpm >= 20) {
    dernierEchantillonBpm = maintenant;
    heartrate.getValue(BPM_SENSOR_PIN);
    uint16_t rate = heartrate.getRate();
    // 0 tant que la bibliothèque n'a pas fini de calculer
    if (rate >= BPM_VALIDE_MIN && rate <= BPM_VALIDE_MAX) {
      bpmCapteur = rate;
      derniereMesureBpm = maintenant;
    }
  }

  if (bpmCapteur > 0 && (maintenant - derniereMesureBpm > BPM_TIMEOUT)) {
    bpmCapteur = 0;
  }
}

// --- Joystick ---
void calibrerJoystick() {
  long sx = 0, sy = 0;
  for (int i = 0; i < 16; i++) {
    sx += analogRead(JOY_X_PIN);
    sy += analogRead(JOY_Y_PIN);
    delay(10);
  }
  reposX = sx / 16;
  reposY = sy / 16;
  if (reposX < 300 || reposX > 700) reposX = 500;
  if (reposY < 300 || reposY > 700) reposY = 500;
  Serial.print("Joystick repos : X=");
  Serial.print(reposX);
  Serial.print(" Y=");
  Serial.println(reposY);
}

int joyLectureBrute() {
  int y = analogRead(JOY_Y_PIN);
  int x = analogRead(JOY_X_PIN);
  if (y > JOY_SEUIL_CLIC) return 1;
  if (x < reposX - JOY_SEUIL_DIR) return 2;
  if (x > reposX + JOY_SEUIL_DIR) return 3;
  return 0;
}

void actionJoystick(int evenement) {
  // en offline le clic lance le jeu
  if (!systemeActif) {
    if (evenement == 1) dinoClic();
    return;
  }

  if (evenement == 1) {
    sourceCapteur = !sourceCapteur;
    Serial.print("Joystick : source BPM -> ");
    Serial.println(sourceCapteur ? "capteur" : "potentiometre");
    publierSource();
  } else if (evenement == 2 || evenement == 3) {
    pageEcran = (pageEcran == 1) ? 2 : 1;
  }
  mettreAJourEcran(true);
}

void traiterJoystick() {
  static int derniere = 0, stable = 0;
  static unsigned long t = 0;

  int brut = joyLectureBrute();
  if (brut != derniere) { t = millis(); derniere = brut; }
  if ((millis() - t >= JOY_STABLE_MS) && brut != stable) {
    stable = brut;
    if (stable != 0) actionJoystick(stable);
  }
}

// --- Alertes ---
bool seuilHyst(float valeur, float seuil, float hyst, bool &etat) {
  if (valeur > seuil) {
    etat = true;
  } else if (valeur < seuil - hyst) {
    etat = false;
  }
  return etat;
}

void majAlertes() {
  if (!systemeActif) {
    alerteBpmActive = false;
    alerteTempActive = false;
    repliSecours = false;
    return;
  }

  if (mqtt_client.connected()) {

    repliSecours = false;
    alerteBpmActive  = alarmeBpmOn;
    alerteTempActive = alarmeTempOn;
  } else {

    repliSecours = true;
    alerteBpmActive  = seuilHyst((float)bpmCourant(), (float)SEUIL_BPM_SECOURS, (float)HYST_BPM, etatBpmSecours);
    alerteTempActive = (derniereTempCorps > 0) &&
                       seuilHyst(derniereTempCorps, SEUIL_TEMP_SECOURS, HYST_TEMP, etatTempSecours);
  }
}

void setRgbColor(bool r, bool g, bool b) {
  digitalWrite(RGB_RED, r ? HIGH : LOW);
  digitalWrite(RGB_GREEN, g ? HIGH : LOW);
  digitalWrite(RGB_BLUE, b ? HIGH : LOW);
}

void majLedRgb() {
  if (!systemeActif) {
    if (dinoEtat == DINO_JEU)      setRgbColor(false, true, false);
    else if (dinoEtat == DINO_FIN) setRgbColor(true, false, false);
    else                           setRgbColor(false, false, false);
    return;
  }

  bool clignote = ((millis() / 250) % 2) == 0;

  if (alerteBpmActive && alerteTempActive) {
    if (clignote) setRgbColor(true, false, false);
    else          setRgbColor(false, false, true);
  } else if (alerteBpmActive) {
    setRgbColor(clignote, false, false);
  } else if (alerteTempActive) {
    setRgbColor(false, false, clignote);
  } else if (ledDashboardOn) {
    setRgbColor(false, true, false);
  } else {
    setRgbColor(false, false, false);
  }
}

void demarrerSegment(int seg) {
  unsigned long maintenant = millis();
  segmentSon = seg;
  debutSegment = maintenant;
  if (seg == SEG_MELODIE) {
    noteCourante = 0;
    debutNote = maintenant;
  }
}

int frequenceAlertes() {
  unsigned long maintenant = millis();
  bool bpmA = alerteBpmActive;
  bool tempA = alerteTempActive;

  if (!bpmA && !tempA) {
    segmentSon = SEG_AUCUN;
    dernierSegment = SEG_BIPS;
    return 0;
  }

  if (segmentSon == SEG_MELODIE && noteCourante < nbNotes) {
    if (maintenant - debutNote >= (unsigned long)dureeNotes[noteCourante] * 5UL / 4UL) {
      noteCourante++;
      debutNote = maintenant;
    }
  }

  if (segmentSon == SEG_MELODIE && (!bpmA || noteCourante >= nbNotes)) {
    dernierSegment = SEG_MELODIE;
    segmentSon = SEG_AUCUN;
  } else if (segmentSon == SEG_BIPS && (!tempA || (maintenant - debutSegment) >= DUREE_SEGMENT_BIPS)) {
    dernierSegment = SEG_BIPS;
    segmentSon = SEG_AUCUN;
  }

  if (segmentSon == SEG_AUCUN) {
    if (bpmA && tempA) {
      demarrerSegment(dernierSegment == SEG_MELODIE ? SEG_BIPS : SEG_MELODIE);
    } else if (bpmA) {
      demarrerSegment(SEG_MELODIE);
    } else {
      demarrerSegment(SEG_BIPS);
    }
  }

  if (segmentSon == SEG_MELODIE) {
    unsigned long ecoule = maintenant - debutNote;
    return (ecoule < (unsigned long)dureeNotes[noteCourante]) ? melodieMozart[noteCourante] : 0;
  }
  return (((maintenant - debutSegment) % 800) < 400) ? FREQ_BIP : 0;
}

void appliquerFrequence(int freq) {
  static int freqCourante = 0;
  if (freq != freqCourante) {
    if (freq > 0) tone(BUZZER_PIN, freq);
    else          noTone(BUZZER_PIN);
    freqCourante = freq;
  }
}

// --- Écran ---
void ecranSante1(float temp, int bpm, bool alerte) {
  char buffer[32];

  u8g2.setPowerSave(0);
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 10, "TEMP CORPORELLE");
  u8g2.drawStr(92, 10, alerte ? "ALERTE" : "OK");

  u8g2.setFont(u8g2_font_helvB14_tf);
  if (temp > 0) {
    snprintf(buffer, sizeof(buffer), "%.1f C", temp);
  } else {
    snprintf(buffer, sizeof(buffer), "-- C");
  }
  u8g2.drawStr(0, 30, buffer);

  u8g2.drawHLine(0, 35, 128);

  u8g2.setFont(u8g2_font_6x12_tf);
  if (bpm > 0) {
    snprintf(buffer, sizeof(buffer), "BPM: %d (%s)", bpm, sourceCapteur ? "capt" : "pot");
  } else {
    snprintf(buffer, sizeof(buffer), "BPM: -- (%s)", sourceCapteur ? "capt" : "pot");
  }
  u8g2.drawStr(0, 48, buffer);

  if (sourceCapteur && bpmCapteur == 0) {
    u8g2.drawStr(0, 61, "Posez le doigt");
  }
  u8g2.drawStr(110, 61, "1/2");

  u8g2.sendBuffer();
}

void ecranSante2(bool wifiOK, bool mqttOK, bool signalOK) {
  char buffer[32];

  u8g2.setPowerSave(0);
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(0, 10, "DETAILS");

  snprintf(buffer, sizeof(buffer), "Source: %s", sourceCapteur ? "capteur" : "potentiometre");
  u8g2.drawStr(0, 24, buffer);

  snprintf(buffer, sizeof(buffer), "Capt: %s", signalOK ? "signal OK" : "pas de doigt");
  u8g2.drawStr(0, 36, buffer);

  snprintf(buffer, sizeof(buffer), "WiFi:%s MQTT:%s", wifiOK ? "ok" : "non", mqttOK ? "ok" : "non");
  u8g2.drawStr(0, 48, buffer);

  u8g2.drawStr(0, 61, mqttOK ? "Seuils: dashboard" : "Seuils: secours");
  u8g2.drawStr(110, 61, "2/2");

  u8g2.sendBuffer();
}

void mettreAJourEcran(bool forcer) {
  if (!systemeActif) return;
  unsigned long maintenant = millis();
  if (!forcer && (maintenant - lastOledTime < OLED_INTERVAL)) return;

  int mode = systemeActif ? 1 : 0;
  int bpm = systemeActif ? bpmCourant() : 0;
  int tempX10 = (int)roundf(derniereTempCorps * 10.0f);
  bool alerte = alerteBpmActive || alerteTempActive;
  bool wifiOK = (WiFi.status() == WL_CONNECTED);
  bool mqttOK = mqtt_client.connected();
  bool signalOK = (bpmCapteur > 0);

  if (!forcer && mode == affMode && pageEcran == affPage && bpm == affBpm &&
      tempX10 == affTempX10 && alerte == affAlerte && sourceCapteur == affSource &&
      signalOK == affSignal && wifiOK == affWifi && mqttOK == affMqtt && repliSecours == affRepli) {
    return;
  }

  lastOledTime = maintenant;
  affMode = mode; affPage = pageEcran; affBpm = bpm; affTempX10 = tempX10;
  affAlerte = alerte; affSource = sourceCapteur; affSignal = signalOK;
  affWifi = wifiOK; affMqtt = mqttOK; affRepli = repliSecours;

  if (pageEcran == 1) {
    ecranSante1(derniereTempCorps, bpm, alerte);
  } else {
    ecranSante2(wifiOK, mqttOK, signalOK);
  }
}

// --- Dino : affichage et logique ---
void dimsObstacle(int i, float &w, float &h, float &yHaut) {
  if (obsType[i] == 1)      { w = 17.0 * obsTaille[i]; h = 35; yHaut = SOL_LOGIQUE - 35; }
  else if (obsType[i] == 2) { w = 25.0 * obsTaille[i]; h = 50; yHaut = SOL_LOGIQUE - 50; }
  else                      { w = 46; h = 40; yHaut = obsY[i]; }
}

void dessinerDino(int x, int yPieds, bool baisse, int pas) {
  if (!baisse) {
    u8g2.drawBox(x + 3, yPieds - 14, 7, 5);
    u8g2.setDrawColor(0);
    u8g2.drawPixel(x + 8, yPieds - 13);
    u8g2.setDrawColor(1);
    u8g2.drawBox(x, yPieds - 10, 8, 7);
    u8g2.drawBox(x - 2, yPieds - 9, 2, 2);
    if (pas == 0) {
      u8g2.drawBox(x + 1, yPieds - 3, 2, 3);
      u8g2.drawBox(x + 5, yPieds - 2, 2, 2);
    } else {
      u8g2.drawBox(x + 1, yPieds - 2, 2, 2);
      u8g2.drawBox(x + 5, yPieds - 3, 2, 3);
    }
  } else {
    u8g2.drawBox(x, yPieds - 7, 9, 4);
    u8g2.drawBox(x + 8, yPieds - 7, 5, 4);
    u8g2.setDrawColor(0);
    u8g2.drawPixel(x + 11, yPieds - 6);
    u8g2.setDrawColor(1);
    if (pas == 0) {
      u8g2.drawBox(x + 1, yPieds - 3, 2, 3);
      u8g2.drawBox(x + 6, yPieds - 2, 2, 2);
    } else {
      u8g2.drawBox(x + 1, yPieds - 2, 2, 2);
      u8g2.drawBox(x + 6, yPieds - 3, 2, 3);
    }
  }
}

void dessinerObstacle(int i, int pas) {
  float w, h, yHaut;
  dimsObstacle(i, w, h, yHaut);
  int x = (int)(obsX[i] * ECH_X);
  int top = SOL_Y - (int)((SOL_LOGIQUE - yHaut) * ECH_Y);

  if (obsType[i] == 3) {
    u8g2.drawBox(x + 2, top + 4, 8, 3);
    u8g2.drawBox(x, top + 3, 3, 3);
    if (pas == 0) u8g2.drawLine(x + 5, top + 4, x + 8, top);
    else          u8g2.drawLine(x + 5, top + 7, x + 8, top + 11);
    return;
  }

  int hauteur = (int)(h * ECH_Y);
  int unite = (obsType[i] == 1) ? 4 : 5;
  for (int k = 0; k < obsTaille[i]; k++) {
    int xk = x + k * unite;
    u8g2.drawBox(xk + 1, top, 3, hauteur);
    if (obsType[i] == 2) {
      u8g2.drawBox(xk, top + 4, 1, 4);
      u8g2.drawBox(xk + 4, top + 3, 1, 4);
    }
  }
}

bool chevauche(float ax1, float ay1, float ax2, float ay2, float bx1, float by1, float bx2, float by2) {
  return ax1 < bx2 && ax2 > bx1 && ay1 < by2 && ay2 > by1;
}

bool dinoCollision() {
  float haut = DINO_Y_SOL - dinoHauteur;
  float x = DINO_X;

  for (int i = 0; i < DINO_MAX_OBS; i++) {
    if (obsType[i] == 0) continue;
    float w, h, yHaut;
    dimsObstacle(i, w, h, yHaut);

    float ox1 = obsX[i] + 2, ox2, oy1, oy2;
    if (obsType[i] == 3) {
      ox2 = obsX[i] + 42;
      oy1 = yHaut + 8;
      oy2 = yHaut + 27;
    } else {
      ox2 = obsX[i] + w - 2;
      oy1 = yHaut + 2;
      oy2 = yHaut + h - 2;
    }

    if (dinoBaisse) {
      if (chevauche(x + 1, haut + 18, x + 56, haut + 43, ox1, oy1, ox2, oy2)) return true;
    } else {
      if (chevauche(x + 22, haut, x + 39, haut + 16, ox1, oy1, ox2, oy2)) return true;
      if (chevauche(x + 1, haut + 18, x + 31, haut + 43, ox1, oy1, ox2, oy2)) return true;
    }
  }
  return false;
}

bool nouvelObstacle() {
  int libre = -1;
  for (int i = 0; i < DINO_MAX_OBS; i++) {
    if (obsType[i] == 0) { libre = i; break; }
  }
  if (libre < 0) return false;

  // les oiseaux n'arrivent qu'à partir de la vitesse 8.5, et pas 3 fois de suite le même obstacle
  int type;
  int essais = 0;
  do {
    type = random(1, 4);
    essais++;
  } while (essais < 20 && ((type == 3 && dinoVitesse < 8.5) || (type == dernierType1 && type == dernierType2)));
  if (type == 3 && dinoVitesse < 8.5) type = 1;
  dernierType2 = dernierType1;
  dernierType1 = type;

  int taille = 1;
  if (type != 3) {
    taille = random(1, 4);
    if (taille > 1 && ((type == 1 && dinoVitesse < 4) || (type == 2 && dinoVitesse < 7))) taille = 1;
  }

  const int hauteursOiseau[] = {100, 75, 50};
  obsType[libre] = type;
  obsTaille[libre] = taille;
  obsY[libre] = hauteursOiseau[random(0, 3)];
  obsDecal[libre] = (random(0, 2) == 0) ? 0.8 : -0.8;
  obsX[libre] = 600;
  obsSuivant[libre] = false;

  float w, h, yHaut;
  dimsObstacle(libre, w, h, yHaut);
  int espaceMin = (type == 3) ? 150 : 120;
  long gapMin = (long)(w * dinoVitesse + espaceMin * 0.6 + 0.5);
  long gapMax = (long)(gapMin * 1.5 + 0.5);
  obsGap[libre] = random(gapMin, gapMax + 1);
  return true;
}

void dinoApparition() {
  int dernier = -1;
  for (int i = 0; i < DINO_MAX_OBS; i++) {
    if (obsType[i] != 0 && (dernier < 0 || obsX[i] > obsX[dernier])) dernier = i;
  }
  if (dernier < 0) {
    nouvelObstacle();
    return;
  }
  float w, h, yHaut;
  dimsObstacle(dernier, w, h, yHaut);
  if (!obsSuivant[dernier] && obsX[dernier] + w + obsGap[dernier] < 600) {
    if (nouvelObstacle()) obsSuivant[dernier] = true;
  }
}

void dinoDessinerJeu() {
  char buffer[24];
  int pas = (millis() / 83) % 2;
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_5x8_tr);
  snprintf(buffer, sizeof(buffer), "HI %05d %05d", dinoRecord, dinoScore);
  u8g2.drawStr(128 - 5 * (int)strlen(buffer), 8, buffer);

  u8g2.drawHLine(0, SOL_Y, 128);
  int decalage = (int)(dinoSol * ECH_X) % 128;
  for (int i = 0; i < 6; i++) {
    u8g2.drawPixel((i * 27 + 128 - decalage) % 128, SOL_Y + 3);
  }

  for (int i = 0; i < DINO_MAX_OBS; i++) {
    if (obsType[i] != 0) dessinerObstacle(i, pas);
  }
  dessinerDino((int)(DINO_X * ECH_X), SOL_Y - (int)(dinoHauteur * ECH_Y), dinoBaisse, dinoSaute ? 0 : pas);

  u8g2.sendBuffer();
}

void dinoDessinerAttente() {
  char buffer[24];
  u8g2.setPowerSave(0);
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(0, 14, "DINO");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(0, 26, "Mode OFFLINE");
  u8g2.drawStr(0, 37, "CLIC : jouer");
  u8g2.drawStr(0, 47, "HAUT saut / BAS baisser");
  snprintf(buffer, sizeof(buffer), "Record : %d", dinoRecord);
  u8g2.drawStr(0, 57, buffer);

  dessinerDino(100, 40, false, 0);
  u8g2.drawHLine(90, 41, 38);

  u8g2.sendBuffer();
}

void dinoDessinerFin() {
  char buffer[24];
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(18, 18, "GAME OVER");
  u8g2.setFont(u8g2_font_5x8_tr);
  snprintf(buffer, sizeof(buffer), "Score : %d", dinoScore);
  u8g2.drawStr(30, 33, buffer);
  snprintf(buffer, sizeof(buffer), "Record : %d", dinoRecord);
  u8g2.drawStr(30, 43, buffer);
  u8g2.drawStr(30, 58, "CLIC : rejouer");

  u8g2.sendBuffer();
}

void dinoDemarrer() {
  randomSeed(micros());
  for (int i = 0; i < DINO_MAX_OBS; i++) obsType[i] = 0;
  dinoHauteur = 0;
  dinoVitesseY = 0;
  dinoSaute = false;
  dinoBaisse = false;
  dinoMinAtteint = false;
  dinoVitesse = VITESSE_INIT;
  dinoDistance = 0;
  dinoTemps = 0;
  dinoSol = 0;
  dinoScore = 0;
  dinoJalon = 0;
  dernierType1 = 0;
  dernierType2 = 0;
  sonFinIdx = -1;
  dinoEtat = DINO_JEU;
  dinoDernierFrame = millis();
  tone(BUZZER_PIN, 660, 60);
}

void publierDino() {
  if (!mqtt_client.connected()) return;
  char payload[40];
  snprintf(payload, sizeof(payload), "{\"score\":%d,\"record\":%d}", dinoScore, dinoRecord);
  mqtt_client.publish(topic_dino, payload, true);
}

void dinoFinDePartie() {
  dinoEtat = DINO_FIN;
  dinoDebutFin = millis();
  if (dinoScore > dinoRecord) dinoRecord = dinoScore;
  sonFinIdx = 0;
  sonFinProchain = millis();
  dinoDessinerFin();
  publierDino();
  Serial.print("[DINO] Game over - score ");
  Serial.println(dinoScore);
}

// délai de 750 ms après un game over, comme le vrai jeu
void dinoClic() {
  if (dinoEtat == DINO_ATTENTE) {
    dinoDemarrer();
  } else if (dinoEtat == DINO_FIN && (millis() - dinoDebutFin > 750)) {
    dinoDemarrer();
  }
}

// un pas de simulation, pas = durée en images à 60 par seconde (1.0 = 16,7 ms)
void dinoPas(float pas, bool haut, bool bas) {
  if (dinoVitesse < VITESSE_MAX) {
    dinoVitesse += ACCELERATION * pas;
    if (dinoVitesse > VITESSE_MAX) dinoVitesse = VITESSE_MAX;
  }
  dinoDistance += dinoVitesse * pas;
  dinoTemps += pas * (1000.0 / 60.0);
  dinoSol += dinoVitesse * pas;

  if (haut && !dinoSaute) {
    dinoSaute = true;
    dinoMinAtteint = false;
    dinoVitesseY = SAUT_INIT + dinoVitesse / 10.0;
    tone(BUZZER_PIN, 880, 40);
  }
  if (dinoSaute) {
    if (bas) dinoVitesseY = -3.0;
    else if (!haut && dinoMinAtteint && dinoVitesseY > 5.0) dinoVitesseY = 5.0;
    dinoHauteur += dinoVitesseY * pas;
    dinoVitesseY -= GRAVITE * pas;
    if (dinoHauteur >= HAUTEUR_MIN_SAUT) dinoMinAtteint = true;
    if (dinoHauteur <= 0) {
      dinoHauteur = 0;
      dinoVitesseY = 0;
      dinoSaute = false;
    }
  }
  dinoBaisse = bas && !dinoSaute;

  for (int i = 0; i < DINO_MAX_OBS; i++) {
    if (obsType[i] == 0) continue;
    float v = dinoVitesse + ((obsType[i] == 3) ? obsDecal[i] : 0);
    obsX[i] -= v * pas;
    float w, h, yHaut;
    dimsObstacle(i, w, h, yHaut);
    if (obsX[i] + w < 0) obsType[i] = 0;
  }
  if (dinoTemps > DELAI_OBSTACLES) dinoApparition();

  dinoScore = (int)(dinoDistance * 0.025);
  if (dinoScore / 100 > dinoJalon) {
    dinoJalon = dinoScore / 100;
    tone(BUZZER_PIN, 1200, 60);
  }

  if (dinoCollision()) dinoFinDePartie();
}

void dinoMiseAJour() {
  unsigned long maintenant = millis();
  float dt = (maintenant - dinoDernierFrame) / (1000.0 / 60.0);
  dinoDernierFrame = maintenant;
  if (dt > 6.0) dt = 6.0;

  int y = analogRead(JOY_Y_PIN);
  bool haut = (y < reposY - JOY_SEUIL_DIR);
  bool bas = (y > reposY + JOY_SEUIL_DIR);

  while (dt > 0 && dinoEtat == DINO_JEU) {
    float pas = (dt > 1.0) ? 1.0 : dt;
    dinoPas(pas, haut, bas);
    dt -= pas;
  }
}

void majSonFin() {
  if (sonFinIdx < 0) return;
  if (millis() < sonFinProchain) return;
  if (sonFinIdx >= 3) { sonFinIdx = -1; return; }
  tone(BUZZER_PIN, sonFinNotes[sonFinIdx], sonFinDurees[sonFinIdx]);
  sonFinProchain = millis() + sonFinDurees[sonFinIdx] + 30;
  sonFinIdx++;
}

// écran à 400 kHz pour que le jeu soit fluide
void dinoEntrer() {
  Wire.setClock(400000);
  u8g2.setBusClock(400000);
  dinoEtat = DINO_ATTENTE;
  sonFinIdx = -1;
  dinoDessinerAttente();
}

// retour à 100 kHz pour le MLX90614
void dinoSortir() {
  noTone(BUZZER_PIN);
  sonFinIdx = -1;
  dinoEtat = DINO_ATTENTE;
  u8g2.setBusClock(100000);
  Wire.setClock(100000);
}

void dinoLoop() {
  majSonFin();
  if (dinoEtat != DINO_JEU) return;
  if (millis() - dinoDernierFrame < DINO_FRAME_MS) return;
  dinoMiseAJour();
  if (dinoEtat == DINO_JEU) dinoDessinerJeu();
}

// --- Communication MQTT ---
void publierSource() {
  if (mqtt_client.connected()) {
    mqtt_client.publish(topic_source, sourceCapteur ? "capteur" : "pot");
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];
  message.trim();
  bool on = (message == "on");

  Serial.print("Recu [");
  Serial.print(topic);
  Serial.print("] : ");
  Serial.println(message);

  String t = String(topic);
  if (t == topic_led1)              ledDashboardOn = on;
  else if (t == topic_alarme_bpm)   alarmeBpmOn = on;
  else if (t == topic_alarme_temp)  alarmeTempOn = on;
  else if (t == topic_source_cmd) {
    if (message == "capteur")   sourceCapteur = true;
    else if (message == "pot")  sourceCapteur = false;
    publierSource();
  }
}

void gererReconnexionMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!mqtt_client.connected()) {
    unsigned long maintenant = millis();
    if (maintenant - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectAttempt = maintenant;
      Serial.println("[MQTT] Tentative de connexion non-bloquante...");
      if (mqtt_client.connect(client_id)) {
        Serial.println("[MQTT] Connecte !");
        mqtt_client.subscribe(topic_led1);
        mqtt_client.subscribe(topic_alarme_bpm);
        mqtt_client.subscribe(topic_alarme_temp);
        mqtt_client.subscribe(topic_source_cmd);
        mqtt_client.publish(topic_bouton, systemeActif ? "on" : "off");
        publierSource();
        publierDino();
      } else {
        Serial.print("[MQTT] Echec, code ");
        Serial.println(mqtt_client.state());
      }
    }
  } else {
    mqtt_client.loop();
  }
}

void printMacAddress() {
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC Address: ");
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 16) Serial.print("0");
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
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

  Wire.begin();
  Wire.setClock(100000);

  u8g2.setBusClock(100000);
  u8g2.begin();
  u8g2.setPowerSave(0);

  Wire.beginTransmission(0x5A);
  if (Wire.endTransmission() == 0) {
    if (mlx.begin()) {
      Serial.println("-> Capteur IR MLX90614 detecte a l'adresse 0x5A");
      mlxDetecte = true;
    }
  } else {
    Serial.println("MLX90614 non detecte sur le bus I2C (adresse 0x5A)");
    mlxDetecte = false;
  }

  calibrerJoystick();

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
    Serial.println("\nWi-Fi connecte !");
    printMacAddress();
    Serial.print("Adresse IP : ");
    Serial.println(WiFi.localIP());
    snprintf(client_id, sizeof(client_id), "ArduinoR4-%u-%lu", WiFi.localIP()[3], (unsigned long)millis());
  } else {
    Serial.println("\nWi-Fi non connecte (delai depasse) - mode hors-ligne");
    printMacAddress();
    snprintf(client_id, sizeof(client_id), "ArduinoR4-offline-%lu", (unsigned long)millis());
  }

  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
  mqtt_client.setSocketTimeout(1);

  systemeActif = (digitalRead(SWITCH_PIN) == LOW);
  Serial.print("Etat initial interrupteur : ");
  Serial.println(systemeActif ? "ON (mode sante)" : "OFF (offline)");

  if (systemeActif) {
    digitalWrite(LED_DETECTION_VERTE, HIGH);
    derniereMesureBpm = millis();
  } else {
    dinoEntrer();
  }
  majAlertes();
  mettreAJourEcran(true);
}

void loop() {
  unsigned long maintenant = millis();

  // pas de reconnexion pendant une partie, ça fige l'image
  if (dinoEtat != DINO_JEU) gererReconnexionMQTT();

  traiterJoystick();

  // interrupteur avec anti-rebond
  bool interrupteurON = (digitalRead(SWITCH_PIN) == LOW);

  if ((interrupteurON != systemeActif) && (maintenant - dernierChangementSwitch >= DEBOUNCE_DELAY)) {
    dernierChangementSwitch = maintenant;
    systemeActif = interrupteurON;

    Serial.print(">> [INTERRUPTEUR] -> ");
    Serial.println(systemeActif ? "ON (mode sante)" : "OFF (offline)");

    if (mqtt_client.connected()) {
      mqtt_client.publish(topic_bouton, systemeActif ? "on" : "off");
    }

    if (systemeActif) {
      digitalWrite(LED_DETECTION_VERTE, HIGH);
      bpmCapteur = 0;
      derniereMesureBpm = maintenant;
      pageEcran = 1;
      dinoSortir();
    } else {
      digitalWrite(LED_DETECTION_VERTE, LOW);
      alarmeBpmOn = false;
      alarmeTempOn = false;
      if (mqtt_client.connected()) {

        mqtt_client.publish(topic_bpm, "0");
        mqtt_client.publish(topic_temp, "0");
        mqtt_client.publish(topic_signal, "aucun");
      }
    }

    etatBpmSecours = false;
    etatTempSecours = false;
    segmentSon = SEG_AUCUN;
    appliquerFrequence(0);
    majAlertes();
    if (!systemeActif) dinoEntrer();
    mettreAJourEcran(true);
  }

  if (systemeActif) {
    traiterCapteurBpm();

    if (mlxDetecte && (maintenant - lastMlxReadTime >= MLX_READ_INTERVAL)) {
      lastMlxReadTime = maintenant;
      float tObj = mlx.readObjectTempC();
      if (!isnan(tObj) && tObj > -40.0 && tObj < 120.0) {
        derniereTempCorps = tObj;
      }
    }

    if (maintenant - lastPublishTime >= PUBLISH_INTERVAL) {
      lastPublishTime = maintenant;

      int bpm = bpmCourant();
      char payload[16];
      if (mqtt_client.connected()) {
        snprintf(payload, sizeof(payload), "%d", bpm);
        mqtt_client.publish(topic_bpm, payload);
        snprintf(payload, sizeof(payload), "%.1f", derniereTempCorps);
        mqtt_client.publish(topic_temp, payload);
        mqtt_client.publish(topic_signal, (bpmCapteur > 0) ? "ok" : "aucun");
      }

      Serial.print("[SANTE] BPM: ");
      Serial.print(bpm);
      Serial.print(sourceCapteur ? " (capteur)" : " (pot)");
      Serial.print(" | capteur: ");
      Serial.print(bpmCapteur);
      Serial.print(" | Corps: ");
      Serial.print(derniereTempCorps, 1);
      Serial.print(" C | alerte BPM: ");
      Serial.print(alerteBpmActive ? "OUI" : "non");
      Serial.print(" | alerte temp: ");
      Serial.println(alerteTempActive ? "OUI" : "non");
    }
  }

  majAlertes();
  majLedRgb();
  appliquerFrequence(frequenceAlertes());

  if (systemeActif) {
    mettreAJourEcran(false);
  } else {
    dinoLoop();
  }
}
