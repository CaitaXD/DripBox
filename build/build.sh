mkdir bin -p &&\
gcc-11 --debug main.c\
  -I ./lib\
  -o bin/dripbox\
  -lpthread\
  #-fsanitize=address
