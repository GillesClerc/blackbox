FROM espressif/idf:latest

# Installer Node.js et Claude Code
RUN apt-get update && \
    apt-get install -y curl && \
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    npm install -g @anthropic-ai/claude-code

# Créer un user dev
RUN useradd -m -s /bin/bash dev && \
    usermod -aG sudo dev && \
    echo 'dev ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Source ESP-IDF pour le user dev
RUN echo 'source /opt/esp/idf/export.sh 2>/dev/null' >> /home/dev/.bashrc

WORKDIR /workspaces/blackbox
USER dev
