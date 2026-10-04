# GoofyDeck Web GUI

Interface web locale pour piloter GoofyDeck avec les mêmes mécanismes que la GUI Qt.

## Lancer

```bash
cd /home/wozt/GoofyDeck
python3 webgui/server.py --host 127.0.0.1 --port 8765
```

Ouvrir ensuite `http://127.0.0.1:8765`.

## Fonctionnalités

- Grille 14 boutons au format Ulanzi D200.
- Clic sur un bouton: `TAP`; `Shift` + clic: `HOLD`; `Alt` + clic: `LONGHOLD`; clic droit: `RELEASED`.
- Suivi temps réel par Server-Sent Events des changements de page, du YAML et des icônes rendues.
- Commandes daemon: `ping`, `load-last-page`, `reload-config`, `stop-control`, `start-control`.
- Édition directe de `config/configuration.yml`.
- Édition structurée d’un bouton via PyYAML, suivie d’un `reload-config`.

Variables utiles:

```bash
GOOFYDECK_CONFIG=/path/to/configuration.yml
GOOFYDECK_PAGING_SOCKET=/tmp/goofydeck_paging_control.sock
GOOFYDECK_SESSION_DIR=/dev/shm/goofydeck/paging
GOOFYDECK_WEB_HOST=0.0.0.0
GOOFYDECK_WEB_PORT=8765
```
