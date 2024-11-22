FROM gcc:11
RUN mkdir /app
COPY .. /app
WORKDIR /app
#RUN apt update && apt install -y valgrind
RUN apt-get update && apt-get install -y vim
RUN gcc --debug main.c -I ./lib -o bin/dripbox -fsanitize=address
RUN chmod +x bin/dripbox
EXPOSE 25565-40000/udp 25565-40000/tcp
WORKDIR /app/bin
ARG KIND="server"
ARG IP="0.0.0.0"
ARG PORT=25565
ENV COMMAND_ARGS="${KIND} ${IP} ${PORT}"
#CMD "valgrind" "./dripbox" ${COMMAND_ARGS}
CMD "./dripbox" ${COMMAND_ARGS}