# Build of the Socks5Balancer
FROM alpine:3.18 AS build-proxy

RUN apk update && apk upgrade && apk add boost-dev boost-static linux-headers cmake make openssl-dev gcc g++

WORKDIR /build

COPY CMakeLists.txt /build/
COPY src /build/src

RUN cmake .

RUN make -j$(nproc)

# Build of the Web interface for Socks5Balancer
FROM node:18 AS build-webui

RUN apt update && apt install git

WORKDIR /html

# Use the latest version on the git repository
RUN git clone https://github.com/Socks5Balancer/html.git .

# Was supposed to retrieve the submodule from the repository but doesn't work with github fork
#COPY html /html

RUN yarn
RUN yarn build

# Runtime image
FROM alpine:3.18 AS runtime

RUN apk update && apk upgrade && apk add boost-dev boost-static

WORKDIR /app

COPY --from=build-proxy /build/Socks5BalancerAsio /app/Socks5BalancerAsio
COPY --from=build-webui /html /app/html

CMD ./Socks5BalancerAsio
