FROM sirvestnik/cxx-conan-base

ADD libc++-dev_3.8.1-1_amd64.deb libc++1_3.8.1-1_amd64.deb libc++-helpers_3.8.1-1_all.deb libc++abi1_3.8.1-1_amd64.deb libc++abi-dev_3.8.1-1_amd64.deb /home/builder/

RUN echo "deb http://mirror.yandex.ru/debian sid main" > /etc/apt/sources.list.d/10-sid-backports.list && \
    echo "deb-src http://mirror.yandex.ru/debian sid main" >> /etc/apt/sources.list.d/10-sid-backports.list && \
    echo "APT::Default-Release "stable";" > /etc/apt/apt.conf.d/10-default-release && \
    apt-get update && \
    apt-get -y --no-install-recommends install clang-3.8 && \
    cd /home/builder && \
    dpkg -i libc++-dev_3.8.1-1_amd64.deb libc++1_3.8.1-1_amd64.deb libc++-helpers_3.8.1-1_all.deb libc++abi1_3.8.1-1_amd64.deb libc++abi-dev_3.8.1-1_amd64.deb && \
    rm libc++-dev_3.8.1-1_amd64.deb libc++1_3.8.1-1_amd64.deb libc++-helpers_3.8.1-1_all.deb libc++abi1_3.8.1-1_amd64.deb libc++abi-dev_3.8.1-1_amd64.deb && \
    apt-get -y autoremove && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists

VOLUME /home/builder/src
USER builder
RUN mkdir /home/builder/build
WORKDIR /home/builder/build
ENV CC clang-3.8
ENV CXX clang++-3.8

CMD ["libstdc++11", "Release"]
ENTRYPOINT ["/home/builder/src/.travis/autobuild.sh"]
