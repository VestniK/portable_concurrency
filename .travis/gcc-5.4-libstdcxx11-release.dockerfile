FROM ubuntu:16.04

RUN apt-get update
RUN apt-get install -y cmake build-essential python3-pip

RUN pip3 install conan

RUN conan install --build=missing -s compiler=gcc -s compiler.version=5.4 -s compiler.libcxx=libstdc++11
RUN mkdir -p build
WORKDIR build
RUN cmake -DCONAN_LIBCXX=libstdc++11 -DCMAKE_BUILD_TYPE=Release ..
RUN make VERBOSE=1
RUN build/bin/unit_tests
