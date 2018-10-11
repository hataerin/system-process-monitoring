
g++ -g -c -I./usr/include/libxml2 agent.cpp
g++ -g -c logger.cpp
g++ -g -c psinfo.cpp
#g++ -g -o agent agent.o logger.o psinfo.o -L./usr/lib64 -lxml2 -lz -liconv
g++ -g -o agent agent.o logger.o psinfo.cpp -L./usr/lib64 -lxml2 -lz -liconv


