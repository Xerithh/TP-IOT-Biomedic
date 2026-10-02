# 3. Schéma Électrique et Câblage (Circuit & Wiring)

Cette section documente le plan d'interconnexion complet entre l'Arduino Uno R4 WiFi et l'ensemble des périphériques d'acquisition et d'actionnement.

## 🔌 Tableau de Câblage des Broches (Pinout Mapping)

| Broche Arduino Uno R4 | Composant Associé | Broche Composant | Type de Signal | Description Fonctionnelle |
| :--- | :--- | :--- | :--- | :--- |
| **5V** | Tous les modules | VCC / +5V | Alimentation | Rail d'alimentation positive principale (+5V DC) |
| **GND** | Tous les modules | GND / Masse | Alimentation | Référence de masse commune |
| **SDA (ou A4)** | MLX90614 & OLED SSD1306 | SDA | Numérique I2C | Ligne de données partagée du bus I2C (adresse MLX: `0x5A`, OLED: `0x3C`) |
| **SCL (ou A5)** | MLX90614 & OLED SSD1306 | SCL | Numérique I2C | Ligne d'horloge partagée du bus I2C |
| **A0** | Potentiomètre B10K | Broche centrale (Curseur) | Entrée Analogique | Simulation manuelle du rythme cardiaque (0 - 1023 -> 0 - 210 BPM) |
| **A1** | Capteur de Pouls DFRobot | Sortie Signal (S) | Entrée Analogique | Échantillonnage photopléthysmographique du battement cardiaque |
| **A2** | Joystick analogique | VRy (Axe Y) & Clic SW | Entrée Analogique | Détection verticale (Saut Dino / Baisser) et clic SW (via pont diviseur interne) |
| **A3** | Joystick analogique | VRx (Axe X) | Entrée Analogique | Détection horizontale (Navigation page 1 / page 2 de l'écran OLED) |
| **D2** | Switch ON-OFF (Interrupteur) | Borne active | Entrée Numérique (PULLUP) | Bascule Mode Santé (`LOW`) / Mode Dino Hors-ligne (`HIGH`) |
| **D3** | Buzzer Piézoélectrique | Borne Positive (+) | Sortie Numérique (PWM) | Génération des tonalités et mélodies d'alarme via `tone()` |
| **D4** | LED Verte d'état | Anode (via R 220Ω) | Sortie Numérique | Témoin lumineux d'activation du système de surveillance |
| **D10** | LED RGB (Cathode commune) | Anode Verte (via R 220Ω) | Sortie Numérique | Indication statut nominal / heartbeat Node-RED |
| **D11** | LED RGB (Cathode commune) | Anode Rouge (via R 220Ω) | Sortie Numérique | Clignotement d'alarme tachycardie / dépassement seuil BPM |
| **D12** | LED RGB (Cathode commune) | Anode Bleue (via R 220Ω) | Sortie Numérique | Clignotement d'alarme hyperthermie / température anormale |

---

## 📐 Schéma Électrique Simplifié

<!-- ESPACE SCHÉMA : SCHÉMA ÉLECTRIQUE WOKWI / CIRKIT DESIGNER -->
> ### ⚡ [ESPACE SCHÉMA - Schéma de Câblage Électrique]
> *Générez votre schéma interactif sous [Wokwi.com](https://wokwi.com/) ou [Cirkit Designer](https://app.cirkitdesigner.com/project) et insérez l'export ci-dessous.*
> 
> ```markdown
> ![Schéma de Câblage Réalisé sous Wokwi / Cirkit Designer](images/schematic_circuit_wokwi.png)
> ```

---

## 📸 Montage Réel sur Platine d'Essai (Breadboard)

<!-- ESPACE PHOTO : PHOTO DU CÂBLAGE RÉEL SUR BREADBOARD -->
> ### 🖼️ [ESPACE PHOTO - Montage Électronique Réel]
> *Insérez une photo en plongée de la breadboard avec l'ensemble des composants et câbles repérés.*
> 
> ```markdown
> ![Montage Réel sur Breadboard](images/breadboard_wiring.jpg)
> ```

---

## ⚠️ Précautions et Recommandations de Câblage

1. **Partage du Bus I2C** :
   * L'écran OLED SSD1306 et le capteur Melexis MLX90614 partagent les mêmes lignes `SDA` et `SCL`.
   * Veillez à ce que les résistances de pull-up intégrées aux modules soient bien alimentées en 5V ou 3.3V stable.
2. **Entrée D2 avec Résistance de Tirage Interne (`INPUT_PULLUP`)** :
   * L'interrupteur est raccordé entre la broche `D2` et le `GND`. 
   * Lorsque l'interrupteur est fermé, `D2` lit un niveau logique `LOW` (Mode Santé Actif). Lorsqu'il est ouvert, la résistance de rappel interne tire le signal à `HIGH` (Mode Dino Hors-ligne).
3. **Protection des Sorties Numériques** :
   * N'omettez jamais les résistances de limitation de 220 Ω en série sur les anodes des LED pour éviter toute surintensité dommageable pour le microcontrôleur.
