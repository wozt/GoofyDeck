# GoofyDeck GUI

Interface graphique Qt6 native pour contrôler et configurer GoofyDeck.

## Fonctionnalités

- **Deck Control** : Visualisation interactive des 14 boutons avec simulation d'événements
- **Config Editor** : Éditeur YAML intégré avec coloration syntaxique
- **Event Log** : Log en temps réel des événements boutons
- **Connexion daemon** : Connexion automatique au daemon GoofyDeck via socket Unix

## Compilation

### Dépendances

```bash
# Debian/Ubuntu
sudo apt install qt6-base-dev qt6-base-dev-tools build-essential cmake

# Fedora
sudo dnf install qt6-qtbase-devel gcc-c++ cmake

# Arch
sudo pacman -S qt6-base base-devel cmake
```

### Build

```bash
cd gui
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Exécution

```bash
# Le daemon GoofyDeck doit être en cours d'exécution
./goofydeck-gui
```

## Utilisation

### Onglet "Deck Control"

- **Clic gauche** sur un bouton : Simule TAP / HOLD / LONGHOLD (selon durée)
- **Clic droit** sur un bouton : Menu contextuel pour simuler un événement spécifique
- **Boutons Connect/Disconnect** : Connexion au daemon
- **Sélecteur de page** : Navigation entre les pages (à implémenter)

### Onglet "Config Editor"

Édition directe du fichier `configuration.yml` avec sauvegarde.

### Menu Control

- **Stop Control** : Passe en mode miniapp (désactive la gestion des boutons)
- **Start Control** : Réactive la gestion des boutons
- **Reload Pages** : Recharge la dernière page

## Architecture

```
gui/
├── src/
│   ├── main.cpp              # Point d'entrée
│   ├── MainWindow.cpp/h      # Fenêtre principale
│   ├── DeckWidget.cpp/h      # Grille 14 boutons
│   ├── ButtonWidget.cpp/h    # Widget bouton individuel
│   ├── GoofyDeckClient.cpp/h # Client socket Unix
│   ├── ConfigEditor.cpp/h    # Éditeur YAML
│   └── YamlParser.cpp/h      # Parseur YAML simplifié
├── CMakeLists.txt
└── README.md
```

## TODO

- [ ] Parseur YAML complet avec édition visuelle
- [ ] Affichage des icônes des boutons
- [ ] Intégration Home Assistant (états en temps réel)
- [ ] Drag & drop pour réorganiser les boutons
- [ ] Preview des modifications avant sauvegarde
