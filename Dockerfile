FROM espressif/idf:latest

RUN apt-get update && \
    apt-get install -y curl gosu && \
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    npm install -g @anthropic-ai/claude-code

RUN useradd -m -s /bin/bash dev && usermod -aG dialout dev

RUN git config --system user.name "Gilles" && \
    git config --system user.email "gillesclerc@gmail.com" && \
    git config --system safe.directory /workspaces/blackbox

COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

WORKDIR /workspaces/blackbox
ENTRYPOINT ["docker-entrypoint.sh"]
