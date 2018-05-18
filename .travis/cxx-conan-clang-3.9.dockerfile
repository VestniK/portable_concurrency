FROM sirvestnik/cxx-conan-base

RUN echo "deb http://mirror.yandex.ru/debian sid main" > /etc/apt/sources.list.d/10-sid-backports.list && \
    echo "deb http://dl.bintray.com/pdeps/libcxx-versions stretch main" > /etc/apt/sources.list.d/20-libcxx-versions.list && \
    echo "APT::Default-Release "stable";" > /etc/apt/apt.conf.d/10-default-release && \
    apt-get update && \
    apt-get -y --no-install-recommends --allow-unauthenticated install -t sid \
      clang-3.9 libc++-dev=3.9.1-1 libc++abi-dev=3.9.1-1 libc++1=3.9.1-1 libc++abi1=3.9.1-1 libc6-dev && \
    cd /home/builder && \
    apt-get -y autoremove && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists && \
    ln -s /usr/include/locale.h /usr/include/xlocale.h

VOLUME /home/builder/src
USER builder
ENV CC clang-3.9
ENV CXX clang++-3.9
RUN mkdir /home/builder/build && conan profile new --detect default
WORKDIR /home/builder/build

CMD ["libstdc++11", "Release"]
ENTRYPOINT ["/home/builder/src/.travis/autobuild.sh"]
