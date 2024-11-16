FROM gcc:latest
RUN mkdir /app
COPY .. /app
WORKDIR /app
#RUN apt update && apt install -y valgrind
RUN gcc --debug main.c -I ./lib -o bin/dripbox
RUN chmod +x bin/dripbox
EXPOSE 25565-40000/udp 25565-40000/tcp
WORKDIR /app/bin
#CMD ["valgrind", "--track-origins=yes"]
ARG KIND="server"
ARG IP="0.0.0.0"
ARG PORT=25565
ENV COMMAND_ARGS="${KIND} ${IP} ${PORT}"
CMD "./dripbox" ${COMMAND_ARGS}