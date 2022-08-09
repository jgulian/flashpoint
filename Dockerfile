FROM docker/cxx

WORKDIR /server

ADD protos .
ADD src .
ADD test .
ADD CMakeLists.txt .

RUN cmake .

