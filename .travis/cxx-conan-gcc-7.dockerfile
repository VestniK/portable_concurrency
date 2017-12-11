FROM sirvestnik/cxx-conan-base:latest

RUN echo "deb http://mirror.yandex.ru/debian sid main" > /etc/apt/sources.list.d/10-sid-backports.list && \
    echo "APT::Default-Release "stable";" > /etc/apt/apt.conf.d/10-default-release && \
    apt-get update && \
    apt-get -y --no-install-recommends -t sid install \
      gcc-7 g++-7 libstdc++-7-dev libgcc-7-dev \
    && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists

VOLUME /home/builder/src
USER builder
ENV CC gcc-7
ENV CXX g++-7
RUN mkdir /home/builder/build && conan profile new --detect default
WORKDIR /home/builder/build

CMD ["libstdc++11", "Release"]
ENTRYPOINT ["/home/builder/src/.travis/autobuild.sh"]
