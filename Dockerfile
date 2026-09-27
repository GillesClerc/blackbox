FROM espressif/idf:latest

RUN apt-get update && \
    apt-get install -y curl gosu sudo && \
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs

RUN useradd -m -s /bin/bash dev && usermod -aG dialout dev

# Env IDF (idf.py, esptool, pyserial) dans tout shell de dev : terminaux VS Code,
# extension Claude Code, shell de claude lance par start.sh.
RUN echo '. /opt/esp/idf/export.sh >/dev/null 2>&1' >> /home/dev/.bashrc && \
    echo '. /opt/esp/idf/export.sh >/dev/null 2>&1' > /etc/profile.d/esp-idf.sh

USER dev
RUN curl -fsSL https://claude.ai/install.sh | bash -s stable
USER root
RUN cp -r /home/dev/.local /opt/claude-local-seed

ENV PATH="/home/dev/.local/bin:$PATH"

RUN git config --system user.name "Gilles" && \
    git config --system user.email "gillesclerc@gmail.com" && \
    git config --system safe.directory /workspaces/blackbox

COPY docker-entrypoint.sh tty-nodes.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh /usr/local/bin/tty-nodes.sh && \
    echo 'dev ALL=(root) NOPASSWD: /usr/local/bin/tty-nodes.sh' > /etc/sudoers.d/tty-nodes && \
    chmod 440 /etc/sudoers.d/tty-nodes

WORKDIR /workspaces/blackbox
ENTRYPOINT ["docker-entrypoint.sh"]
