
g++ -g -c -I$ORACLE_HOME/rdbms/public edaemon.cpp
g++ -g -c logger.cpp
g++ -g -c smtp.cpp

g++ -g -o edaemon edaemon.o logger.o smtp.o -L$ORACLE_HOME/lib -lclntsh


