FROM alpine as flashpoint

RUN apk add gcc g++ cmake make git
RUN apk add linux-headers

ENV CC=gcc CXX=g++
WORKDIR /requirements
RUN git clone https://github.com/ninja-build/ninja.git && \
    cd ninja && \
    cmake -DCMAKE_MAKE_PROGRAM=make -Bbuild-cmake && \
    cmake --build build-cmake

WORKDIR /service

ADD protos ./protos
ADD src ./src
ADD test ./test
ADD CMakeLists.txt .

# TODO: make work
# TODO: use ninja
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM=make -B cmake-build-release .
RUN cmake --build cmake-build-release --target flashpoint -j 12
RUN cmake --build cmake-build-release --target flashpoint_test -j 12

WORKDIR /build
RUN mv /service/cmake-build-release/CMake-flashpoint .
E

FROM flashpoint-base as flashpoint
RUN ls; echo; ls cmake-build-release
ENTRYPOINT ./CMakeFiles/flashpoint


FROM flashpoint-base as flashpoint-test
ENTRYPOINT ./CmakeFiles/flashpoint_test