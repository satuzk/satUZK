
CPP = g++
LD = gcc
CPP_ARGS = -pthread $(CPP_ADDS) -std=c++0x -Wall
CPP_SOURCE = src/sys/Linux.cpp
LINK_FLAGS =
LIBS = -lrt

testing:
	./mkconfig.sh testing
	$(CPP) -o satUZK-seq $(CPP_ARGS) $(LINK_FLAGS) -O3 src/MainSeq.cpp $(CPP_SOURCE) $(LIBS)
	$(CPP) -o satUZK-par $(CPP_ARGS) $(LINK_FLAGS) -O3 src/MainPar.cpp $(CPP_SOURCE) $(LIBS)

debug:
	./mkconfig.sh debug
	$(CPP) -o satUZK-seq $(CPP_ARGS) $(LINK_FLAGS) -g src/MainSeq.cpp $(CPP_SOURCE) $(LIBS)
	$(CPP) -o satUZK-par $(CPP_ARGS) $(LINK_FLAGS) -g src/MainPar.cpp $(CPP_SOURCE) $(LIBS)
optdebug:
	./mkconfig.sh optdebug
	$(CPP) -o satUZK-seq $(CPP_ARGS) $(LINK_FLAGS) -g -O3 -fno-omit-frame-pointer src/MainSeq.cpp $(CPP_SOURCE) $(LIBS)
	$(CPP) -o satUZK-par $(CPP_ARGS) $(LINK_FLAGS) -g -O3 -fno-omit-frame-pointer src/MainPar.cpp $(CPP_SOURCE) $(LIBS)

profile-google:
	./mkconfig.sh profile-google
	$(CPP) -o satUZK-seq $(CPP_ARGS) -DFEATURE_GOOGLE_PROFILE $(LINK_FLAGS) -fno-omit-frame-pointer -g -O3 src/MainSeq.cpp $(CPP_SOURCE) $(LIBS) -lprofiler
	$(CPP) -o satUZK-par $(CPP_ARGS) -DFEATURE_GOOGLE_PROFILE $(LINK_FLAGS) -fno-omit-frame-pointer -g -O3 src/MainPar.cpp $(CPP_SOURCE) $(LIBS) -lprofiler

