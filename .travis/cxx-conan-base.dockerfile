FROM ubuntu:16.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends cmake ninja-build python3 python3-pip python3-setuptools && \
    pip3 install conan && \
    useradd -ms /bin/bash builder && \
    apt-get -y --purge remove python3-pip && \
    apt-get -y --purge autoremove && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists
