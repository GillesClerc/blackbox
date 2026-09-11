FROM espressif/idf:latest

RUN apt-get update && \
    apt-get install -y curl gosu && \
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs

RUN useradd -m -s /bin/bash dev && usermod -aG dialout dev

USER dev
RUN curl -fsSL https://claude.ai/install.sh | bash -s stable
USER root
RUN cp -r /home/dev/.local /opt/claude-local-seed

ENV PATH="/home/dev/.local/bin:$PATH"

RUN git config --system user.name "Gilles" && \
    git config --system user.email "gillesclerc@gmail.com" && \
    git config --system safe.directory /workspaces/blackbox

COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

WORKDIR /workspaces/blackbox
ENTRYPOINT ["docker-entrypoint.sh"]
