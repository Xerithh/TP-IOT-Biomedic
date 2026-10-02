# 🚴 TP IoT Biomédical : Station Connectée pour Home-Trainer (Arduino Uno R4 WiFi + MQTT + Node-RED)

Projet d'Ingénierie Biomédicale & Systèmes Connectés - Cursus Ingénieur ISIS.

---

## 📖 Rapport de Projet & Documentation Technique (Wiki)

L'intégralité du rapport technique est rédigée selon la structure éditoriale de référence **Random Nerd Tutorials** et est directement accessible sur le **[Wiki Officiel du Dépôt](https://github.com/Xerithh/TP-IOT-Biomedic/wiki)** :

* **[🏠 Page d'Accueil du Wiki](https://github.com/Xerithh/TP-IOT-Biomedic/wiki/Home)**
* **[1. Aperçu du Projet (Project Overview)](https://github.com/Xerithh/TP-IOT-Biomedic/wiki/01-Project-Overview)**
* **[2. Composants et Matériel Requis (Parts Required)](https://github.com/Xerithh/TP-IOT-Biomedic/wiki/02-Parts-and-Hardware)**
* **[3. Schéma Électrique et Câblage (Circuit & Wiring)](https://github.com/Xerithh/TP-IOT-Biomedic/wiki/03-Circuit-and-Wiring)**
* **[4. Architecture MQTT et Dashboard Node-RED](https://github.com/Xerithh/TP-IOT-Biomedic/wiki/04-Node-RED-and-MQTT)**
* **[5. Explications Clés du Code Arduino](https://github.com/Xerithh/TP-IOT-Biomedic/wiki/05-Code-Architecture)**
* **[6. Analyse Critique et Perspectives d'Amélioration](https://github.com/Xerithh/TP-IOT-Biomedic/wiki/06-Critique-and-Improvements)**
* **[📑 Rapport Complet Monolithique](https://github.com/Xerithh/TP-IOT-Biomedic/wiki/Rapport-Complet-RandomNerdTutorials)**

---

## 📦 Fichiers et Livrables du Dépôt

1. **Code Arduino Principal** : [`hometrainer_dino_final.ino`](hometrainer_dino_final.ino) (Acquisition biomédicale, écran OLED multi-pages, moteur d'alarme sonore & visuelle, repli de secours local et jeu Dino hors-ligne).
2. **Flow Node-RED** : [`flows.json`](flows.json) (Flux Node-RED Dashboard 2.0 pour la télémétrie, jauges en direct et gestion des seuils d'alerte).
