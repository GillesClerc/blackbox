#!/bin/bash
set -e

cp -rL /tmp/ssh_host /home/dev/.ssh
chown -R dev:dev /home/dev/.ssh
chmod 700 /home/dev/.ssh
chmod 600 /home/dev/.ssh/* 2>/dev/null || true

source /opt/esp/idf/export.sh 2>/dev/null
exec gosu dev claude --dangerously-skip-permissions
