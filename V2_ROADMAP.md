# Oskitone Scout V2 - Plan d'Améliorations (Overhaul V2)

Ce document détaille les ajustements, nouvelles fonctionnalités et modifications matérielles prévus pour la nouvelle version du projet.

## 1. Ajustements Logiciels (Software)

### 1.1 Gestion de la LED bleue (Désactivation lors du jeu)
Actuellement, la LED bleue intégrée à la carte ESP32 s'allume à chaque fois qu'une note est jouée. Ce comportement sera modifié pour que la LED reste éteinte lors du jeu normal, afin d'éviter une pollution visuelle et d'économiser l'énergie.

### 1.2 Mode Contrôleur MIDI (via USB-C)
Le synthétiseur pourra agir comme un véritable **contrôleur MIDI** lorsqu'il est connecté à un ordinateur via son port USB-C.
- **Activation** : Pour passer en mode MIDI, l'utilisateur devra maintenir enfoncés simultanément les boutons **Mode** et **Octave** pendant une longue durée.
- **Indicateur visuel** : Une fois le mode MIDI activé, la LED bleue de la carte ESP32 s'allumera de façon continue pour confirmer l'état.

---

## 2. Plan Matériel : Alimentation par Batterie LiPo

*L'architecture matérielle a été adaptée pour conserver la carte ESP32 de base et n'utiliser que son port USB-C (pour la charge et la programmation).*

### 2.1 Le "Paradoxe de la boucle infinie"
Utiliser le port USB-C de l'ESP32 pour recharger la pile est tout à fait possible, mais cela crée un défi électronique majeur sur une carte générique :
- Si on branche le câble USB, l'ESP32 reçoit du 5V sur sa broche `5V`. On peut s'en servir pour alimenter l'entrée du chargeur TP4056.
- **Le problème** : Quand on débranche le câble, c'est la pile qui doit prendre le relais et renvoyer du 5V dans l'ESP32 (via un convertisseur Boost). Mais si la pile renvoie du courant dans la broche `5V` de l'ESP32... le chargeur TP4056 va croire que le câble USB est branché ! Il va essayer de recharger la pile en utilisant l'énergie de la pile elle-même. C'est une boucle infinie qui draine la batterie instantanément.

### 2.2 La Solution : L'Interrupteur "Intelligent" (DPDT)
Puisqu'on ne veut pas modifier la carte ESP32, la solution la plus élégante et la plus simple est d'utiliser un **interrupteur à 6 broches (DPDT - Double Pole Double Throw)**. Il agit comme un aiguillage pour séparer les circuits. Il aura deux positions :

**Position "ON" (Mode Synthétiseur)**
- **Ce qu'il fait** : Il connecte la pile à l'ESP32 pour jouer. Il déconnecte l'entrée du chargeur.
- **Programmation** : Si on branche le câble USB dans ce mode, on peut programmer l'ESP32 sans problème. Par contre, la pile ne se rechargera pas.

**Position "OFF / CHARGE" (Mode Recharge)**
- **Ce qu'il fait** : Il déconnecte la pile de l'ESP32 (le synthé s'éteint). À la place, il connecte la broche `5V` de l'ESP32 à l'entrée du chargeur TP4056.
- **Recharge** : Quand on branche le câble USB, l'énergie passe par l'ESP32 et va directement dans le chargeur pour remplir la pile en toute sécurité.

### 2.3 Schéma de Câblage
Un interrupteur DPDT possède 2 colonnes de 3 broches :
- `A1` `B1` (Haut)
- `A2` `B2` (Milieu - Commun)
- `A3` `B3` (Bas)

**Câblage :**
1. **Pile et Chargeur** : La pile est toujours connectée aux broches `B+` et `B-` du TP4056.
2. **Chemin du Chargeur (Colonne A)** :
   - `A1` : Connecté à la broche `IN+` du TP4056.
   - `A2` : Connecté à la broche `5V` de l'ESP32.
   - `A3` : Rien (Vide).
3. **Chemin de l'Alimentation (Colonne B)** :
   - `B1` : Rien (Vide).
   - `B2` : Connecté à la broche `IN+` du module Boost (qui va vers le 5V de l'ESP32).
   - `B3` : Connecté à la broche `OUT+` du TP4056.

*(Note : Tous les GND / négatifs sont reliés ensemble en permanence).*

### 2.4 Matériel Requis
- 1x **ESP32 Standard** (port USB exposé au boîtier).
- 1x **Pile LiPo 3.7V 900mAh** (603048).
- 1x **Module chargeur TP4056** (port USB non utilisé/non exposé).
- 1x **Mini convertisseur Boost 5V**.
- 1x **Interrupteur DPDT** (qui remplacera l'interrupteur actuel).

### 2.5 Modifications du Boîtier (OpenSCAD)
- Aucun trou USB supplémentaire n'est nécessaire ; le design extérieur restera épuré.
- Il faudra adapter les dimensions du trou de l'interrupteur (un DPDT est légèrement plus large qu'un SPDT simple) et concevoir les fixations internes pour la pile et les modules.
