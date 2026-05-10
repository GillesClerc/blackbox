#!/bin/bash
docker run -it --rm \
  --privileged \
  -v ~/dev/blackbox:/workspaces/blackbox \
  -v ~/.ssh:/tmp/ssh_host:ro \
  -v ~/.claude:/home/dev/.claude \
  -w /workspaces/blackbox \
  escapebox-dev
