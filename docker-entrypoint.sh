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

# Crée ou expose les device nodes USB série (udev ne tourne pas dans le container)
for dev_path in /sys/class/tty/ttyUSB* /sys/class/tty/ttyACM*; do
    [ -e "$dev_path" ] || continue
    dev_name=$(basename "$dev_path")
    if [ ! -e "/dev/$dev_name" ]; then
        read -r major minor <<< "$(tr ':' ' ' < "$dev_path/dev")"
        mknod -m 666 "/dev/$dev_name" c "$major" "$minor" 2>/dev/null || true
    else
        chmod 666 "/dev/$dev_name" 2>/dev/null || true
    fi
done

source /opt/esp/idf/export.sh 2>/dev/null
exec gosu dev claude --dangerously-skip-permissions
