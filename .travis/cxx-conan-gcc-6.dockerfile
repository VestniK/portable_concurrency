FROM cxx-conan-base:latest

RUN apt-get update && \
    apt-get install -y --no-install-recommends software-properties-common && \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get -y --no-install-recommends install gcc-6 g++-6 libstdc++-6-dev && \
    apt-get -y --purge remove software-properties-common && \
    apt-get -y --purge autoremove && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists
