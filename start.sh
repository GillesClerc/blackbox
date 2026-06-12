#!/bin/bash
docker run -it --rm \
  --privileged \
  -p 3000:3000 \
  -v ~/dev/blackbox:/workspaces/blackbox \
  -v ~/.ssh:/tmp/ssh_host:ro \
  -v ~/.claude:/home/dev/.claude \
  -w /workspaces/blackbox \
  escapebox-dev
