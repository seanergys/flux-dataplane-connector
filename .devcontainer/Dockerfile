FROM sean-dev:latest

# Match the default user id for a single system so we aren't root
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=1000
ENV USERNAME=${USERNAME}
ENV USER_UID=${USER_UID}
ENV USER_GID=${USER_GID}
USER root

# Build deps and extra tools
RUN sudo apt-get update
RUN sudo apt-get -qq install -y --no-install-recommends \
        ninja-build \
        fd-find \
        less \
        bats \
        ripgrep

# Assuming installing to /usr/local
ENV LD_LIBRARY_PATH=/usr/lib:/usr/local/lib:/opt/flux/lib:/opt/seanerbus/lib
ENV PATH=$PATH:/opt/flux/bin:/opt/seanerbus/bin
ENV PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/opt/flux/lib/pkgconfig:/opt/seanerbus/lib/pkgconfig


# Add the group and user that match our ids
RUN groupadd -g ${USER_GID} ${USERNAME} && \
    adduser --disabled-password --uid ${USER_UID} --gid ${USER_GID} --gecos "" ${USERNAME} && \
    echo "${USERNAME} ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers
USER $USERNAME
