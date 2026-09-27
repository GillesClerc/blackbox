#!/bin/bash
set -e

cp -rL /tmp/ssh_host /home/dev/.ssh
chown -R dev:dev /home/dev/.ssh

# ~/.local est monte depuis l'hote (persistance des updates claude) : vide au
# premier lancement, ce qui masque l'install native faite au build de l'image.
# On y recopie le seed sans ecraser une install deja mise a jour par l'utilisateur.
if [ ! -x /home/dev/.local/bin/claude ]; then
    cp -rn /opt/claude-local-seed/. /home/dev/.local/ 2>/dev/null || true
fi
chown -R dev:dev /home/dev/.claude 2>/dev/null || true
chown -R dev:dev /home/dev/.local 2>/dev/null || true
chmod 700 /home/dev/.ssh
chmod 600 /home/dev/.ssh/* 2>/dev/null || true

tty-nodes.sh

# Devcontainer VS Code : le container reste vivant, VS Code s'y attache en `dev`
# (l'env IDF vient de ~/.bashrc). Sinon (start.sh) : claude en direct.
if [ -n "$ESCAPEBOX_DEVCONTAINER" ]; then
    exec sleep infinity
fi

source /opt/esp/idf/export.sh 2>/dev/null
exec gosu dev claude --dangerously-skip-permissions
