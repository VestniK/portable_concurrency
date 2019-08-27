FROM debian:buster

RUN echo "deb http://mirror.yandex.ru/debian buster-backports main" > /etc/apt/sources.list.d/20-backports.list && \
    apt-get update && \
    apt-get install -y --no-install-recommends -t buster-backports wget && \
    mkdir -p /opt/cmake && \
    cd /opt/cmake && \
    wget --no-check-certificate https://cmake.org/files/v3.9/cmake-3.9.6-Linux-x86_64.tar.gz && \
    tar -xzf cmake-3.9.6-Linux-x86_64.tar.gz && \
    ln -fs /opt/cmake/cmake-3.9.6-Linux-x86_64/bin/cmake /usr/bin/cmake && \
    apt-get install -y --no-install-recommends ninja-build python3 python3-pip python3-setuptools && \
    pip3 install conan && \
    useradd -ms /bin/bash builder && \
    apt-get -y --purge remove python3-pip wget && \
    apt-get -y --purge autoremove && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists
