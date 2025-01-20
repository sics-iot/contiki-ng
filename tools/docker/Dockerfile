FROM ubuntu:22.04

ENV DEBIAN_FRONTEND noninteractive

# Set user for what comes next
USER root

# Tools
# build-essential: development tools.
# clang-15: additional warnings in tests.
# gdb: development tools.
# git: development tools.
# git-lfs: development tools (used by Gecko SDK).
# gosu: used for UID-remapping.
# iputils-ping: used by regression tests.
# less: convenience tool.
# libcanberra-gtk-module: remove warning message from renode.
# libcoap2-bin: used by regression tests.
# libgl1-mesa-glx: required by Simplicity Commander
# libxtst6: required for GraalVM/Java17 to run Cooja in GUI mode.
# mosquitto: used by the regression tests.
# mtr-tiny: used by the regression tests.
# net-tools: used by the regression tests.
# python3: used by scripts.
# snmp: used by the regression tests.
# sudo: used by the regression tests.
# tini: needed for cleaning up orphaned processes.
# udev: needed for jlink deb postinst script.
# unzip: required during docker image build for software installation.
# valgrind: used by the regression tests.
# wget: used by the regression tests.
RUN apt-get -qq update && \
    apt-get -qq -y --no-install-recommends install \
      ca-certificates > /dev/null && \
  apt-get -qq -y --no-install-recommends install \
    build-essential \
    clang-15 \
    gdb \
    git \
    git-lfs \
    gosu \
    iputils-ping \
    less \
    libcanberra-gtk-module \
    libcoap2-bin \
    libgl1-mesa-glx \
    libpng-dev \
    libxtst6 \
    mosquitto \
    mosquitto-clients \
    mtr-tiny \
    net-tools \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-serial \
    python3-venv \
    rlwrap \
    sudo \
    screen \
    srecord \
    tini \
    udev \
    unzip \
    libusb-1.0-0 \
    valgrind \
    wget \
    smitools \
    snmp \
    snmp-mibs-downloader \
    > /dev/null && \
  apt-get -qq clean

ARG JAVA_VERSION=21
RUN wget -nv https://download.oracle.com/graalvm/${JAVA_VERSION}/latest/graalvm-jdk-${JAVA_VERSION}_linux-x64_bin.tar.gz && \
  tar xf graalvm-jdk-${JAVA_VERSION}_linux-x64_bin.tar.gz -C /usr/local --strip-components=1 --no-same-owner && \
  rm -f graalvm*gz

# Install ARM toolchain
RUN wget -nv https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2 && \
  tar xjf gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2 -C /usr/local --strip-components=1 --no-same-owner && \
  rm -f gcc-arm-none-eabi-*-linux.tar.bz2

# Install msp430 toolchain
RUN wget -nv https://github.com/pjonsson/msp430gcc-binary/releases/download/v1.1/mspgcc-4.7.4-linux-x86_64.tar.bz2 && \
  tar xf mspgcc*.tar.bz2 -C /usr/local --strip-components=1 --no-same-owner && \
  rm -f mspgcc*.tar.bz2

# Install nRF Util+nRF Command Line tools.
RUN wget -nv -O /usr/local/bin/nrfutil https://developer.nordicsemi.com/.pc-tools/nrfutil/x64-linux/nrfutil && \
  chmod 755 /usr/local/bin/nrfutil && \
  wget -nv https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-22-0/nrf-command-line-tools_10.22.0_amd64.deb && \
  apt-get -qq -y --no-install-recommends install ./nrf-command-line-tools_10.22.0_amd64.deb > /dev/null && \
  apt-get -qq -y --no-install-recommends install /opt/nrf-command-line-tools/share/JLink_Linux_V780c_x86_64.deb > /dev/null && \
  rm -f *.deb *.tar.gz /opt/nrf-command-line-tools/share/JLink_*deb && \
  apt-get -qq clean

# Install Renode from github releases
ARG RENODE_VERSION=1.15.2
RUN wget -nv https://github.com/renode/renode/releases/download/v${RENODE_VERSION}/renode_${RENODE_VERSION}_amd64.deb \
  && apt-get -qq -y --no-install-recommends install ./renode_${RENODE_VERSION}_amd64.deb > /dev/null \
  && python3 -m pip -q install -r /opt/renode/tests/requirements.txt \
  && rm renode_${RENODE_VERSION}_amd64.deb \
  && rm -rf /root/.cache \
  && apt-get -qq clean

RUN wget -nv https://www.silabs.com/documents/public/software/SimplicityCommander-Linux.zip && \
  unzip -q SimplicityCommander-Linux.zip && \
  mkdir /opt/SimplicityCommander && \
  tar xf SimplicityCommander-Linux/Commander_linux_x86_64_*.tar.bz \
    -C /opt/SimplicityCommander --strip-components=1 --no-same-owner && \
  rm -rf SimplicityCommander-Linux.zip SimplicityCommander-Linux

ENV PATH="/opt/SimplicityCommander:${PATH}"

# Sphinx is required for building the readthedocs API documentation.
# RTD requirements are shared with .readthedocs.yaml for build consistency - check RTD build if modifying.
# Matplotlib is required for result visualization.
# Keep the image size down by removing the pip cache when done.
COPY files/rtd_requirements.txt /tmp
RUN python3 -m pip -q install \
      matplotlib && \
    python3 -m pip -q install -r /tmp/rtd_requirements.txt && \
    rm -rf /root/.cache /tmp/rtd_requirements.txt

# Create user, add to groups dialout and sudo, and configure sudoers.
RUN adduser --disabled-password --gecos '' user && \
    usermod -aG dialout,plugdev,sudo user && \
    echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Set user for what comes next
USER user

# Environment variables
ENV HOME                /home/user
ENV CONTIKI_NG          ${HOME}/contiki-ng
ENV COOJA               ${CONTIKI_NG}/tools/cooja
ENV                     PATH="${HOME}/.local/bin:${PATH}"
ENV                     LC_ALL=C.UTF-8
ENV                     LANG=C.UTF-8
WORKDIR                 ${HOME}

# Create Cooja shortcut
COPY --chown=user:user files/cooja ${HOME}/.local/bin/cooja
# Add aliases for CI. Use a file to avoid quoting issues.
COPY --chown=user:user files/bash_aliases ${HOME}/.bash_aliases
# Add login-script for UID/GID-remapping.
COPY --chown=root:root files/remap-user.sh /usr/local/bin/remap-user.sh

# Doxygen 1.8.17 in Ubuntu 20.04 gives (false) warnings on mqtt.h.
# Use a binary from the Doxygen homepage, static linking started with 1.9.3.
# Remove the PDF manual and html directory to reduce image size.
# Use the most recent version of ccache to ensure it supports the compiler
# versions in the docker image.
RUN wget -nv https://sourceforge.net/projects/doxygen/files/rel-1.10.0/doxygen-1.10.0.linux.bin.tar.gz/download -O doxygen.linux.bin.tar.gz && \
    tar zxf doxygen.linux.bin.tar.gz -C ${HOME}/.local && \
    rm -rf ${HOME}/.local/doxygen-1.10.0/html ${HOME}/.local/doxygen-1.10.0/*.pdf doxygen.linux.bin.tar.gz && \
    (cd ${HOME}/.local/bin && ln -s ../doxygen-1.10.0/bin/doxygen .) && \
    wget -nv https://github.com/ccache/ccache/releases/download/v4.10.2/ccache-4.10.2-linux-x86_64.tar.xz && \
    tar xf ccache-4.10.2-linux-x86_64.tar.xz -C ${HOME}/.local/bin --strip-components=1 ccache-4.10.2-linux-x86_64/ccache && \
    rm ccache-*-linux-x86_64.tar.xz

# Install additional tools for the nRF5 SDK.
RUN nrfutil install nrf5sdk-tools

# By default, we use a Docker bind mount to share the repo with the host,
# with Docker run option:
# -v <HOST_CONTIKI_NG_ABS_PATH>:/home/user/contiki-ng
# Alternatively, uncomment the next two lines to download Contiki-NG and pre-compile Cooja.
#RUN git clone --recursive https://github.com/contiki-ng/contiki-ng.git ${CONTIKI_NG}
#RUN ant -q -f ${CONTIKI_NG}/tools/cooja/build.xml jar

# Working directory
WORKDIR ${CONTIKI_NG}

# Start a bash
USER root
ENTRYPOINT ["/usr/local/bin/remap-user.sh"]
CMD bash --login
