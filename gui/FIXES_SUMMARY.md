# Résumé des corrections GUI GoofyDeck

## Corrections effectuées

### 1. ✅ Configuration centralisée (Priorité HAUTE)
**Problème**: Chemins hardcodés dans tout le code
**Solution**: Création d'une classe `Config` centralisée (`Config.h`/`Config.cpp`)
- Gère tous les chemins configurables (config, sockets, cache, assets)
- Supporte les variables d'environnement (`GOOFYDECK_*`)
- Sauvegarde les préférences utilisateur avec QSettings
- Fallbacks intelligents pour trouver les fichiers

**Fichiers modifiés**:
- `gui/src/Config.h` (nouveau)
- `gui/src/Config.cpp` (nouveau)
- `gui/src/MainWindow.cpp`
- `gui/src/GoofyDeckClient.cpp`
- `gui/src/HaClient.cpp`
- `gui/src/IconCache.cpp`
- `gui/src/StateManager.cpp`
- `gui/src/ButtonWidget.cpp`
- `gui/CMakeLists.txt`

### 2. ✅ Timeout Home Assistant configurable (Priorité HAUTE)
**Problème**: Timeout HA hardcodé à 1000ms, causant des déconnexions
**Solution**: Rendre le timeout configurable via Config
- Timeout par défaut: 5000ms
- Configurable via `GOOFYDECK_SOCKET_TIMEOUT` ou QSettings

**Fichiers modifiés**:
- `gui/src/HaClient.cpp`
- `gui/src/GoofyDeckClient.cpp`

### 3. ✅ Fenêtre redimensionnable (Priorité HAUTE)
**Problème**: `setFixedSize(580, 520)` empêchait le redimensionnement
**Solution**: Remplacer par `setMinimumSize(580, 520)` avec option configurable
- Par défaut: fenêtre redimensionnable
- Configurable via `GOOFYDECK_USE_FIXED_SIZE` ou QSettings

**Fichiers modifiés**:
- `gui/src/MainWindow.cpp`

### 4. ✅ Parsing YAML amélioré (Priorité HAUTE)
**Problème**: Parsing YAML fragile avec regex, pas de gestion d'erreurs
**Solution**: Amélioration de la robustesse du parsing
- Ajout de validation de contenu vide
- Vérification que des pages sont trouvées
- Meilleure gestion des erreurs avec qWarning()
- Amélioration des regex existantes

**Fichiers modifiés**:
- `gui/src/YamlParser.cpp`

### 5. ✅ Polling interval configurable (Priorité MOYENNE)
**Problème**: Polling agressif à 500ms dans StateManager
**Solution**: Rendre l'intervalle configurable
- Intervalle par défaut: 1000ms (au lieu de 500ms)
- Configurable via `GOOFYDECK_POLLING_INTERVAL` ou QSettings

**Fichiers modifiés**:
- `gui/src/StateManager.cpp`

### 6. ✅ Système de logging configurable (Priorité MOYENNE)
**Problème**: Logs excessifs avec qDebug(), pas de contrôle
**Solution**: Implémentation de Qt Logging Categories
- Catégories: `goofydeck.gui`, `goofydeck.network`, `goofydeck.config`, `goofydeck.parser`
- Configurable via `GOOFYDECK_DEBUG` ou QSettings
- Filtres de logging par défaut: debug désactivé, info activé

**Fichiers modifiés**:
- `gui/src/main.cpp`

### 7. ✅ Validation des entrées (Priorité MOYENNE)
**Problème**: Pas de validation dans les dialogs d'édition
**Solution**: Ajout de validation dans ButtonEditDialog
- Validation du nom (requis, max 20 caractères)
- Validation du format d'icône (mdi:, url:, local:, /path)
- Validation du format d'action ($page., $cmd., service HA)
- Messages d'erreur clairs

**Fichiers modifiés**:
- `gui/src/ButtonEditDialog.h`
- `gui/src/ButtonEditDialog.cpp`

### 8. ✅ Sauvegarde des préférences (Priorité BASSE)
**Problème**: Pas de sauvegarde des préférences utilisateur
**Solution**: Implémentation dans la classe Config avec QSettings
- Sauvegarde automatique des chemins configurés
- Sauvegarde des timeouts et intervalles
- Sauvegarde des préférences UI
- Sauvegarde des préférences de logging

**Fichiers modifiés**:
- `gui/src/Config.cpp` (intégré)

## Variables d'environnement supportées

- `GOOFYDECK_CONFIG`: Chemin du fichier de configuration YAML
- `GOOFYDECK_PAGING_SOCKET`: Chemin du socket paging daemon
- `GOOFYDECK_HA_SOCKET`: Chemin du socket Home Assistant daemon
- `GOOFYDECK_CACHE_ROOT`: Répertoire racine du cache
- `GOOFYDECK_DEBUG`: Active le logging debug (1/true)
- `GOOFYDECK_SOCKET_TIMEOUT`: Timeout des sockets en ms
- `GOOFYDECK_POLLING_INTERVAL`: Intervalle de polling en ms

## Configuration QSettings

Les préférences sont sauvegardées dans:
- Linux: `~/.config/goofydeck/goofydeck-gui.conf`
- Groupes: `Paths`, `Timing`, `UI`, `Logging`

## Résultat de la compilation

✅ Compilation réussie avec seulement des avertissements de déprecation Qt (non critiques)
✅ L'application se lance correctement
✅ La classe Config détecte automatiquement les fichiers de configuration

## Tests recommandés

1. **Test de connexion**: Vérifier que le GUI se connecte correctement au daemon
2. **Test de redimensionnement**: Vérifier que la fenêtre peut être redimensionnée
3. **Test de configuration**: Modifier les préférences et vérifier qu'elles sont sauvegardées
4. **Test de validation**: Essayer de créer un bouton avec des données invalides
5. **Test de logging**: Activer le mode debug et vérifier les logs

## Améliorations futures suggérées

- Remplacer les regex YAML par un vrai parser YAML (libyaml)
- Ajouter des tests unitaires pour le parsing YAML
- Ajouter un éditeur visuel pour la configuration YAML
- Implémenter le drag & drop pour réorganiser les boutons
- Ajouter des indicateurs de chargement pour les opérations longues
