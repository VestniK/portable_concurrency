FROM debian:stretch

RUN echo "deb http://mirror.yandex.ru/debian stretch-backports main" > /etc/apt/sources.list.d/20-backports.list && \
    apt-get update && \
    apt-get install -y --no-install-recommends -t stretch-backports cmake && \
    apt-get install -y --no-install-recommends ninja-build python3 python3-pip python3-setuptools && \
    pip3 install conan && \
    useradd -ms /bin/bash builder && \
    apt-get -y --purge remove python3-pip && \
    apt-get -y --purge autoremove && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists
