
g++ -g -c -I$ORACLE_HOME/rdbms/public db.cpp
g++ -g -c sockserver.cpp
g++ -g -c logger.cpp

g++ -g -o sockserver sockserver.o logger.o db.o -lpthread -L$ORACLE_HOME/lib -lclntsh


