# 🚴 Prototype Biomédical Connecté pour Home-Trainer & Télémédecine (Arduino Uno R4 WiFi + Node-RED + MQTT)

Bienvenue sur le Wiki officiel du projet **IoT Biomédical - Home-Trainer Connecté**. Ce projet a été développé dans le cadre du cursus Ingénieur ISIS (Informatique et Systèmes d'Information pour la Santé).

Ce guide technique est structuré selon les standards de documentation de référence (style *Random Nerd Tutorials*), fournissant une vue exhaustive de l'architecture matérielle, logicielle, réseau et des interfaces de télésurveillance.

---

## 📑 Sommaire du Wiki

1. [Aperçu du Projet (Project Overview)](01-Project-Overview)
2. [Composants et Matériel Requis (Parts Required)](02-Parts-and-Hardware)
3. [Schéma Électrique et Câblage (Circuit & Wiring)](03-Circuit-and-Wiring)
4. [Architecture MQTT et Dashboard Node-RED (Node-RED Flow)](04-Node-RED-and-MQTT)
5. [Explications Clés du Code Arduino (Code Architecture)](05-Code-Architecture)
6. [Analyse Critique et Perspectives d'Amélioration (Critique & Improvements)](06-Critique-and-Improvements)

---

## 📸 Aperçu Visuel du Prototype

<!-- ESPACE PHOTO 1 : PHOTO GLOBALE DU PROTOTYPE EN FONCTIONNEMENT -->
> ### 🖼️ [ESPACE PHOTO - Prototype en fonctionnement]
> *Insérez ici une belle photo explicite du banc d'essai complet (Arduino Uno R4, capteurs, écran OLED et dashboard Node-RED actif).*
> 
> ```markdown
> ![Prototype Global du Home-Trainer Connecté](images/prototype_global.jpg)
> ```

---

## 💡 En Bref : Ce que fait le prototype

Le système est une station biomédicale embarquée double-mode conçue pour équiper un home-trainer de rééducation cardiaque ou d'entraînement sportif :

* **Mode Santé / Télésurveillance Active** :
  * Mesure continue de la **fréquence cardiaque (BPM)** via un capteur optique DFRobot ou par **simulation potentiométrique** (ajustable en temps réel).
  * Mesure sans contact de la **température corporelle** via capteur infrarouge médical **MLX90614** en I2C.
  * Affichage local sur écran **OLED SSD1306 128x64** (page 1 : métriques biomédicales en direct, page 2 : statut réseau Wi-Fi/MQTT et signaux).
  * Transmission télémétrique temps réel via protocole **MQTT** vers un courtier Cloud et un serveur de supervision **Node-RED Dashboard 2.0**.
  * **Gestion intelligente des alarmes** avec redondance : pilotage à distance par Node-RED avec seuils personnalisés et mode de repli autonome local (algorithme à hystérésis).
  * Signalisation audiovisuelle contextuelle : LED RGB multi-états + Buzzer musical non-bloquant (Mélodie de Mozart *Une petite musique de nuit* pour l'alerte cardiaque, bips d'urgence cadencés pour l'alerte thermique).
* **Mode Hors-ligne / Détente (Dino Game)** :
  * Lorsque l'interrupteur principal bascule en mode pause, l'écran OLED charge instantanément le jeu de course d'obstacles *Chrome Dino*, piloté au joystick analogique avec accélération progressive, physique de saut, scores records et publication du high-score sur MQTT.

---

## 🔗 Liens Rapides & Ressources du Dépôt

* **Code Arduino Principal** : [`hometrainer_dino_final.ino`](../hometrainer_dino_final.ino)
* **Flow Node-RED** : [`flows.json`](../flows.json)
* **Navigateur Wiki** : Utilisez le menu latéral `_Sidebar` pour naviguer entre les différentes sections détaillées.
