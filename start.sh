#!/bin/bash
docker run -it --rm \
  --privileged \
  -v ~/dev/blackbox:/workspaces/blackbox \
  -v ~/.ssh:/tmp/ssh_host:ro \
  -v ~/.gitconfig:/tmp/gitconfig_host:ro \
  -w /workspaces/blackbox \
  escapebox-dev \
  "claude --dangerously-skip-permissions"
