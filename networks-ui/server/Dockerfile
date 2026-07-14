# Builds and runs the real socketsd C server.
# Works on Fly.io, AWS (ECS/EC2+Docker), Railway, or any Docker host.
#
#   docker build -t socketsd .
#   docker run -p 4642:4642 socketsd
FROM gcc:13 AS build
WORKDIR /src
COPY socketsd.c .
RUN gcc -O2 -Wall -o socketsd socketsd.c

FROM debian:bookworm-slim
WORKDIR /app
COPY --from=build /src/socketsd .
EXPOSE 4642
CMD ["./socketsd", "-p", "4642"]
