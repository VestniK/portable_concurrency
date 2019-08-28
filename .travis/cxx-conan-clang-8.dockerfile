FROM sirvestnik/cxx-conan-base

RUN apt-get update && \
    apt-get -y --no-install-recommends install wget gnupg2 && \
    echo "deb http://apt.llvm.org/buster/ llvm-toolchain-buster-8 main" > /etc/apt/sources.list.d/50-llvm-8.list && \
    echo "APT::Default-Release "stable";" > /etc/apt/apt.conf.d/10-default-release && \
    wget -O - 'https://apt.llvm.org/llvm-snapshot.gpg.key' | apt-key add - && \
    apt-get update && \
    apt-get -y --no-install-recommends install clang-8 libc++-8-dev libc++abi-8-dev && \
    cd /home/builder && \
    apt-get -y purge wget gnupg2 && \	
    apt-get -y autoremove && \
    apt-get -y clean && \
    rm -rf /var/lib/apt/lists

VOLUME /home/builder/src
USER builder
ENV CC clang-8
ENV CXX clang++-8
RUN mkdir /home/builder/build && conan profile new --detect default
WORKDIR /home/builder/build

CMD ["libstdc++11", "Release"]
ENTRYPOINT ["/home/builder/src/.travis/autobuild.sh"]
