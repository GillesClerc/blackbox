#!/bin/bash
set -e

# Copy SSH keys from host mount (root can read them) and fix ownership for dev
if [ -d /tmp/ssh_host ]; then
    mkdir -p /home/dev/.ssh
    cp -rL /tmp/ssh_host/. /home/dev/.ssh/ 2>/dev/null || true
    chown -R dev:dev /home/dev/.ssh
    chmod 700 /home/dev/.ssh
    chmod 600 /home/dev/.ssh/* 2>/dev/null || true
    ssh-keyscan -H github.com >> /home/dev/.ssh/known_hosts 2>/dev/null || true
fi

# Copy gitconfig from host mount
if [ -f /tmp/gitconfig_host ]; then
    cp /tmp/gitconfig_host /home/dev/.gitconfig
    chown dev:dev /home/dev/.gitconfig
fi

# Drop to dev user and run the command
exec su -s /bin/bash dev -c "source /opt/esp/idf/export.sh 2>/dev/null; $*"
