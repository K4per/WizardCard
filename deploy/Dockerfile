FROM debian:trixie-slim AS build

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates cmake git libssl-dev ninja-build python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt CMakePresets.json ./
COPY cmake ./cmake
COPY include ./include
COPY src ./src
COPY assets/catalog.json ./assets/catalog.json
COPY assets/release-v260 ./assets/release-v260
COPY docs/central-server-deploy.md ./docs/central-server-deploy.md
COPY deploy ./deploy
COPY THIRD_PARTY_NOTICES.md ./
COPY licenses ./licenses

RUN cmake --preset server-release -DBUILD_TESTING=OFF \
    && cmake --build --preset server-release --target wizard_server --parallel 2 \
    && install -D -m 0755 build/server-release/wizard_server /opt/wizard/bin/wizard_server \
    && install -D -m 0755 deploy/bin/wizard-server /opt/wizard/bin/wizard-server \
    && install -d -m 0755 /opt/wizard/assets \
    && cp -a assets/release-v260 /opt/wizard/assets/ \
    && cp -a THIRD_PARTY_NOTICES.md licenses /opt/wizard/

FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl libssl3t64 \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --system --gid 10001 wizard \
    && useradd --system --uid 10001 --gid wizard --home-dir /var/lib/wizard \
       --no-create-home --shell /usr/sbin/nologin wizard \
    && install -d -o wizard -g wizard -m 0750 /var/lib/wizard-server

COPY --from=build --chown=wizard:wizard /opt/wizard /opt/wizard
USER 10001:10001
ENV WIZARD_DATA_DIR=/var/lib/wizard-server
EXPOSE 24443/tcp
ENTRYPOINT ["/opt/wizard/bin/wizard-server"]
