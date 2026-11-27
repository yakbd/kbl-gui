# Local build image, the same environment as the CI job: docker build -f tools/docker/build.Dockerfile tools
FROM ubuntu:22.04
COPY linux-build-deps.sh /tmp/linux-build-deps.sh
RUN sh /tmp/linux-build-deps.sh && rm -rf /var/lib/apt/lists/* /tmp/linux-build-deps.sh
