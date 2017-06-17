FROM ubuntu:16.04

RUN apt-get update
RUN apt-get install -y cmake clang libc++-dev python3-pip

RUN pip3 install conan

RUN conan install --build=missing -s compiler=clang -s compiler.version=3.8 -s compiler.libcxx=libc++
RUN mkdir -p build
WORKDIR build
RUN cmake -DCONAN_LIBCXX=libc++ -DCMAKE_CXX_FLAGS=-stdlib=libc++ -DCMAKE_BUILD_TYPE=Release ..
RUN make
RUN build/bin/unit_tests
