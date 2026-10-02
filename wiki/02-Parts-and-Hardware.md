# 2. Composants et Matériel Requis (Parts Required)

Pour concevoir et fabriquer ce prototype biomédical connecté, les composants électroniques et capteurs suivants sont nécessaires :

## 🧰 Nomenclature des Composants (Bill of Materials)

| Composant / Module | Référence & Spécifications | Quantité | Rôle dans le Système |
| :--- | :--- | :---: | :--- |
| **Carte Microcontrôleur** | **Arduino Uno R4 WiFi** (Renesas RA4M1 32-bit ARM Cortex-M4 + ESP32-S3 Wi-Fi/BLE) | 1 | Cœur de calcul, acquisition temps réel, gestion de l'IHM et connectivité MQTT |
| **Capteur de Température Infrarouge** | **GY-906 / Melexis MLX90614** (I2C, 3.3V-5V, précision médicale ±0.5°C) | 1 | Mesure sans contact de la température corporelle cutanée |
| **Capteur de Fréquence Cardiaque** | **DFRobot Heart Rate Sensor** (SEN0203 / SEN0213 ou équivalent optique PPG) | 1 | Mesure directe du pouls par photopléthysmographie au bout du doigt |
| **Écran Graphique OLED** | **SSD1306 0.96" I2C 128x64 pixels** (Monochrome blanc ou bleu) | 1 | Affichage dynamique multi-pages des constantes, diagnostics et jeu Dino |
| **Module Joystick Analogique** | **Joystick 2 axes + Bouton Poussoir intégré** (KY-023) | 1 | Navigation entre les pages OLED, bascule source BPM et contrôle du Dino |
| **Potentiomètre Rotatif** | **Potentiomètre linéaire 10 kΩ (B10K)** | 1 | Simulateur d'effort cardiaque (plage 0 à 210 BPM) pour tests et validation |
| **Buzzer Audio** | **Buzzer piézoélectrique 5V** (actif / passif avec support PWM `tone()`) | 1 | Signalisation acoustique d'urgence (mélodie Mozart, bips d'alarme, sons du jeu) |
| **LED Témoin d'État** | **LED Verte standard 5 mm** | 1 | Témoin visuel permanent d'activation du mode médical |
| **LED d'Alarme Multi-couleurs** | **Module LED RGB 5 mm (Cathode Commune)** | 1 | Indication visuelle des seuils d'alerte (Rouge: BPM, Bleu: Temp, Vert: Nominal) |
| **Interrupteur Principal** | **Switch à bascule 2 positions ON-OFF (SPST / SPDT)** | 1 | Bascule matérielle entre Mode Santé et Mode Hors-ligne / Divertissement |
| **Résistances de Protection** | **Résistances 220 Ω (1/4 W)** | 4 | Limitation de courant pour LED Verte et branches Rouge/Verte/Bleue de la LED RGB |
| **Planche d'Essai & Câblage** | **Breadboard standard 830 points + Fils Jumpers M-M / M-F** | 1 lot | Interconnexion sans soudure de l'ensemble du banc d'essai |

---

## 🔬 Description Détaillée des Circuits Clés

<!-- ESPACE PHOTO : VUE RAPPROCHÉE DES MODULES ET CAPTEURS -->
> ### 🖼️ [ESPACE PHOTO - Modules et Capteurs Utilisés]
> *Insérez ici une photo nette présentant les capteurs clés (MLX90614, capteur cardiaque, écran OLED et joystick).*
> 
> ```markdown
> ![Modules et Capteurs du Banc d'Essai](images/components_overview.jpg)
> ```

### 1. Capteur de Température Infrarouge sans contact (MLX90614)
Le capteur MLX90614 intègre une thermopile infrarouge ainsi qu'un ASIC de traitement de signal (DSP). Il communique sur le bus **I2C** à l'adresse fixe `0x5A` en utilisant le protocole standard SMBus. Il délivre directement la température de l'objet ciblé ainsi que la température ambiante avec une résolution de 0.02°C.

### 2. Capteur Cardiaque Optique (Photopléthysmographie)
Le capteur repose sur l'émission d'une lumière verte réfléchie ou transmise à travers les capillaires sanguins du doigt. Les micro-variations d'absorption lumineuse liées à chaque battement cardiaque génèrent un signal analogique filtré puis converti par l'algorithme `DFRobot_Heartrate` en fréquence instantanée.

### 3. Écran OLED SSD1306 128x64 I2C
Cet afficheur miniature offre un contraste élevé et une excellente lisibilité. Grâce à la bibliothèque graphique optimisée `U8g2`, l'affichage fonctionne en double buffering local sans scintillement et supporte une cadence d'horloge I2C dynamique :
* **100 kHz** en mode Santé (pour cohabiter harmonieusement avec le MLX90614 sur le bus partagé).
* **400 kHz Fast-Mode** en mode Dino (pour garantir une animation fluide à ~30-60 FPS).
