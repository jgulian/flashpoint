FROM alpine

RUN apk add gcc cmake

WORKDIR /server

ADD protos .
ADD src .
ADD test .
ADD CMakeLists.txt .

RUN cmake .

