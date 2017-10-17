FROM sirvestnik/cxx-conan-base:latest

RUN echo "deb http://mirror.yandex.ru/debian jessie main" > /etc/apt/sources.list.d/10-jessie-backports.list && \
    echo "APT::Default-Release "stable";" > /etc/apt/apt.conf.d/10-default-release && \
    apt-get update && \
    apt-get -y --no-install-recommends install libc6-dev binutils && \
    apt-get -y --no-install-recommends -t jessie install gcc-4.9 g++-4.9 libstdc++-4.9-dev && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists

VOLUME /home/builder/src
USER builder
ENV CC gcc-4.9
ENV CXX g++-4.9
RUN mkdir /home/builder/build && conan profile new --detect default
WORKDIR /home/builder/build

CMD ["libstdc++", "Release"]
ENTRYPOINT ["/home/builder/src/.travis/autobuild.sh"]
