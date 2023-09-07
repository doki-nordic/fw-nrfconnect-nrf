
# Commands:
#    docker build -t dokinordic/ncs-doc-build:0.1 -f _docker/Dockerfile ../..
#    docker run -it --rm --name=my_doc_build -w `realpath ${PWD}/../..` --mount type=volume,source=`realpath ${PWD}/../..`,target=`realpath ${PWD}/../..` my/doc_build:0.1 bash
#    docker run -it --rm --name=my_doc_build -w `realpath ${PWD}/../..` -v`realpath ${PWD}/../..`:`realpath ${PWD}/../..` my/doc_build:0.1 bash

FROM ubuntu:22.04
LABEL Description="nRF Connect SDK documentation build environment"

ENV TZ=Etc/UTC

# Update packages info
RUN apt-get update

# Instructions from https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation/installing.html

# Install requred tools
RUN DEBIAN_FRONTEND=noninteractive \
    apt -y install --no-install-recommends \
    tzdata sudo doxygen mscgen plantuml graphviz \
    git cmake ninja-build gperf \
    device-tree-compiler wget \
    python3-dev python3-pip python3-setuptools python3-tk python3-wheel xz-utils file \
    make libmagic1
    # Not needed for doc: ccache dfu-util gcc gcc-multilib g++-multilib libsdl2-dev

# Install additional Python dependencies
COPY zephyr/scripts/*.txt /src/zephyr/scripts/
COPY nrf/scripts/*.txt /src/nrf/scripts/
COPY bootloader/mcuboot/scripts/*.txt /src/bootloader/mcuboot/scripts/
COPY nrf/doc/*.txt /src/nrf/doc/

RUN pip3 install west && \
    pip3 install -r /src/zephyr/scripts/requirements.txt && \
    pip3 install -r /src/nrf/scripts/requirements.txt && \
    pip3 install -r /src/bootloader/mcuboot/scripts/requirements.txt && \
    pip3 install -r /src/nrf/doc/requirements.txt && \
    rm -Rf /src
