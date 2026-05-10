#!/bin/bash
docker run -it --rm \
  --privileged \
  -v ~/dev/blackbox:/workspaces/blackbox \
  -v ~/.ssh:/home/dev/.ssh:ro \
  -v ~/.gitconfig:/home/dev/.gitconfig:ro \
  -w /workspaces/blackbox \
  escapebox-dev \
  bash -c "source /opt/esp/idf/export.sh && claude --dangerously-skip-permissions"
