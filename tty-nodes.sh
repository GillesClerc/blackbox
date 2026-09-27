#!/bin/bash
# Crée ou expose les device nodes USB série (udev ne tourne pas dans le container).
# Appelé au démarrage par docker-entrypoint.sh ; à relancer après un
# `usbipd attach` à chaud : `sudo tty-nodes.sh` (autorisé sans mot de passe pour dev).
for dev_path in /sys/class/tty/ttyUSB* /sys/class/tty/ttyACM*; do
    [ -e "$dev_path" ] || continue
    dev_name=$(basename "$dev_path")
    if [ ! -e "/dev/$dev_name" ]; then
        read -r major minor <<< "$(tr ':' ' ' < "$dev_path/dev")"
        mknod -m 666 "/dev/$dev_name" c "$major" "$minor" 2>/dev/null || true
    else
        chmod 666 "/dev/$dev_name" 2>/dev/null || true
    fi
    echo "/dev/$dev_name prêt"
done
