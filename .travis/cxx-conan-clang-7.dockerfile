FROM sirvestnik/cxx-conan-base

RUN echo "deb http://mirror.yandex.ru/debian sid main" > /etc/apt/sources.list.d/10-sid-backports.list && \
    echo "APT::Default-Release "stable";" > /etc/apt/apt.conf.d/10-default-release && \
    apt-get update && \
    apt-get -y --no-install-recommends install -t sid clang-7 libc++-dev libc++abi-dev && \
    cd /home/builder && \
    apt-get -y autoremove && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists

VOLUME /home/builder/src
USER builder
ENV CC clang-7
ENV CXX clang++-7
RUN mkdir /home/builder/build && conan profile new --detect default
WORKDIR /home/builder/build

CMD ["libstdc++11", "Release"]
ENTRYPOINT ["/home/builder/src/.travis/autobuild.sh"]
