# 4. Architecture MQTT et Dashboard Node-RED (Node-RED Flow)

La communication bidirectionnelle entre la station embarquée Arduino et le poste de télésurveillance repose sur le protocole **MQTT** via le courtier public `broker.emqx.io:1883`.

---

## 📡 Matrice des Topics MQTT

| Topic MQTT | Direction | Format du Payload | Fréquence / Événement | Description Fonctionnelle |
| :--- | :---: | :--- | :--- | :--- |
| `gaelle-capteurs/bpm` | Arduino -> Node-RED | Entier (ex: `78`) | Périodique (1 Hz) | Fréquence cardiaque mesurée ou simulée |
| `gaelle-capteurs/temperature` | Arduino -> Node-RED | Décimal (ex: `36.8`) | Périodique (1 Hz) | Température corporelle sans contact (°C) |
| `gaelle-capteurs/bouton` | Arduino -> Node-RED | Chaîne (`"on"` / `"off"`) | Sur événement (Switch D2) | Notification d'état de la station (actif/veille) |
| `gaelle-capteurs/source` | Arduino -> Node-RED | Chaîne (`"capteur"` / `"pot"`) | Sur événement | Indication de la source de rythme cardiaque active |
| `gaelle-capteurs/signal_bpm` | Arduino -> Node-RED | Chaîne (`"ok"` / `"aucun"`) | Périodique (1 Hz) | Détection de la présence effective du doigt sur le capteur |
| `gaelle-capteurs/dino_score` | Arduino -> Node-RED | JSON (`{"score":420,"record":850}`) | Retained / Fin de partie | Envoi des scores du jeu Dino lors du mode pause |
| `gaelle-capteurs/alarme_bpm` | Node-RED -> Arduino | Chaîne (`"on"` / `"off"`) | Sur dépassement de seuil | Ordre d'activation de l'alarme sonore & visuelle BPM |
| `gaelle-capteurs/alarme_temp` | Node-RED -> Arduino | Chaîne (`"on"` / `"off"`) | Sur dépassement de seuil | Ordre d'activation de l'alarme sonore & visuelle Temp |
| `gaelle-capteurs/source_cmd` | Node-RED -> Arduino | Chaîne (`"capteur"` / `"pot"`) | Commande opérateur | Forçage distant de la source de rythme cardiaque |
| `gaelle-capteurs/led1` | Node-RED -> Arduino | Chaîne (`"on"` / `"off"`) | Commande opérateur | Allumage forcé du témoin vert de la LED RGB |

---

## 🖥️ Description du Flow Node-RED

Le flux Node-RED centralise le traitement de la télémétrie biomédicale et assure les fonctions suivantes :
1. **Acquisition & Parsing** : Réception asynchrone des flux MQTT et conversion numérique automatique des métriques.
2. **Supervision Graphique** :
   * Jauges dynamiques avec secteurs colorés de sécurité (vert), vigilance (orange) et critique (rouge).
   * Graphiques temporels glissants pour analyser l'évolution du rythme cardiaque et de la température lors de l'effort.
3. **Moteur d'Alerte Médicale & Hystérésis** :
   * Comparaison en continu des valeurs reçues avec des curseurs réglables de seuils critiques.
   * Déclenchement instantané d'une **Bannière d'Alerte Santé** visuelle et émission d'ordres MQTT vers l'Arduino pour le retour haptique/sonore.
4. **Fichier Source du Flow** : Le flux complet est versionné et disponible directement dans le dépôt sous le fichier [`flows.json`](../flows.json).

---

## 📸 Captures d'Écran du Flow et du Dashboard

<!-- ESPACE PHOTO : SCREENSHOT DU FLOW NODE-RED -->
> ### 📊 [ESPACE CAPTURE - Flow Node-RED]
> *Insérez ici une capture d'écran nette de l'ensemble de votre flux de nœuds Node-RED.*
> 
> ```markdown
> ![Flow Node-RED de Traitement et d'Alertes](images/nodered_flow.png)
> ```

<!-- ESPACE PHOTO : SCREENSHOT DU DASHBOARD NODE-RED EN ACTION -->
> ### 💻 [ESPACE CAPTURE - Dashboard Node-RED en Fonctionnement]
> *Insérez ici une capture d'écran du tableau de bord utilisateur (Gauges, Courbes temps réel, Commandes et Bannières d'alerte).*
> 
> ```markdown
> ![Dashboard Web de Télésurveillance](images/nodered_dashboard.png)
> ```
