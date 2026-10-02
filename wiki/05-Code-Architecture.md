# 5. Explications Clés du Code Arduino (Code Architecture)

> [!NOTE]
> Cette section se concentre exclusivement sur les modules d'ingénierie logicielle critiques du fichier [`hometrainer_dino_final.ino`](../hometrainer_dino_final.ino). Le code source complet et commenté est joint au dépôt.

---

## ⏱️ 1. Ordonnancement Non-Bloquant & Gestion du Temps (`millis()`)

Pour garantir la réactivité instantanée du système, la fluidité des animations graphiques et la précision de l'échantillonnage biomédical, **aucun appel bloquant `delay()` n'est utilisé dans la boucle principale `loop()`**.

L'ensemble des tâches est cadencé par des temporisateurs différentiels asynchrones :
* **Acquisition PPG Cardiaque** : 20 ms (50 Hz).
* **Affichage Graphique OLED** : 33 ms (~30 FPS en jeu) / 500 ms (mode Santé avec rafraîchissement différentiel).
* **Lecture Température IR MLX90614** : 500 ms.
* **Publication Télémétrique MQTT** : 1000 ms.
* **Tentative de Reconnexion Réseau** : 5000 ms.

```cpp
// Extrait de loop() : cadencement asynchrone non-bloquant
if (systemeActif) {
  traiterCapteurBpm(); // Échantillonnage à 50 Hz

  if (mlxDetecte && (maintenant - lastMlxReadTime >= MLX_READ_INTERVAL)) {
    lastMlxReadTime = maintenant;
    derniereTempCorps = mlx.readObjectTempC();
  }

  if (maintenant - lastPublishTime >= PUBLISH_INTERVAL) {
    lastPublishTime = maintenant;
    publierTelemetrieMQTT();
  }
}
```

---

## 🔀 2. Bascule Dynamique de Fréquence d'Horloge I2C (100 kHz <-> 400 kHz)

Le bus I2C est partagé entre deux composants aux contraintes opposées :
1. Le capteur **MLX90614** utilise le protocole SMBus et tolère mal les fréquences supérieures à 100 kHz.
2. L'écran **SSD1306** requiert un débit élevé (Fast-Mode 400 kHz) pour afficher les sprites du jeu Dino sans scintillement.

Le firmware gère cette contrainte dynamiquement lors des transitions de mode :

```cpp
void dinoEntrer() {
  Wire.setClock(400000);       // Accélération à 400 kHz pour le jeu
  u8g2.setBusClock(400000);
  dinoEtat = DINO_ATTENTE;
  dinoDessinerAttente();
}

void dinoSortir() {
  dinoEtat = DINO_ATTENTE;
  u8g2.setBusClock(100000);    // Retour au standard 100 kHz pour le capteur IR
  Wire.setClock(100000);
}
```

---

## 🛡️ 3. Redondance des Alarmes & Algorithme de Repli de Secours (Failsafe)

Le système garantit la sécurité du patient même en cas de rupture de communication Wi-Fi ou de défaillance du serveur Node-RED :
* **Liaison MQTT active** : Les décisions d'alarme proviennent du poste médical distant (Node-RED).
* **Perte du lien MQTT** : Le firmware active automatiquement son **mode de repli autonome**. Il évalue localement les données via un comparateur à **hystérésis** (`HYST_BPM = 3`, `HYST_TEMP = 0.2°C`) pour éviter les oscillations intempestives d'alarme autour des seuils de sécurité.

```cpp
bool seuilHyst(float valeur, float seuil, float hyst, bool &etat) {
  if (valeur > seuil) {
    etat = true;
  } else if (valeur < seuil - hyst) {
    etat = false;
  }
  return etat;
}
```

---

## 🎵 4. Moteur Audio Non-Bloquant & Entrelacement des Alertes

Pour diffuser des mélodies d'alerte identifiables sans bloquer le microcontrôleur, le générateur sonore implémente une machine d'états temporelle :
* Si **seule l'alarme cardiaque** est active : interprétation cadencée de la partition de Mozart (*Une petite musique de nuit* KV 525).
* Si **seule l'alarme thermique** est active : émission de bips d'urgence à 600 Hz.
* Si **les deux alarmes sont actives simultanément** : entrelacement automatique d'un cycle de mélodie avec une salve de bips d'urgence.

```cpp
int frequenceAlertes() {
  unsigned long maintenant = millis();
  // Gestion asynchrone des durées de notes et commutation de segments...
  if (segmentSon == SEG_MELODIE) {
    return (maintenant - debutNote < dureeNotes[noteCourante]) ? melodieMozart[noteCourante] : 0;
  }
  return (((maintenant - debutSegment) % 800) < 400) ? FREQ_BIP : 0;
}
```
