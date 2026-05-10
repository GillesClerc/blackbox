#!/bin/bash
docker run -it --rm \
  --privileged \
  -v ~/dev/blackbox:/workspaces/blackbox \
  -v ~/.ssh:/root/.ssh:ro \
  -v ~/.gitconfig:/root/.gitconfig:ro \
  -w /workspaces/blackbox \
  espressif/idf:latest \
  bash -c "source /opt/esp/idf/export.sh && claude --dangerously-skip-permissions"
