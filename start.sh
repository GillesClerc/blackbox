#!/bin/bash
docker run -it --rm \
  --privileged \
  -v ~/dev/blackbox:/workspaces/blackbox \
  -v ~/.ssh:/tmp/ssh_host:ro \
  -v ~/.gitconfig:/tmp/gitconfig_host:ro \
  -w /workspaces/blackbox \
  escapebox-dev \
  bash -c "
    mkdir -p /home/dev/.ssh && \
    cp -rL /tmp/ssh_host/. /home/dev/.ssh/ && \
    chmod 700 /home/dev/.ssh && \
    chmod 600 /home/dev/.ssh/* 2>/dev/null && \
    ssh-keyscan -H github.com >> /home/dev/.ssh/known_hosts 2>/dev/null && \
    cp /tmp/gitconfig_host /home/dev/.gitconfig && \
    source /opt/esp/idf/export.sh && \
    claude --dangerously-skip-permissions
  "
