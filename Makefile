##
# SSD Simulator
#
# @file
# @version 0.1

# TODO: Once we start adding other .cpp files to our project, uncomment this
# 		and add the srcfile names to it. Then uncomment the recipe requirement
# 		under "main".
# SRCFILE =

CC = g++
CFLAGS = -std=c++17 -O3
CFLAGSDBG = -std=c++17

main: # $(SRCFILE)
	$(CC) $(CFLAGS) -o simulator main.cpp

% : %.cpp
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o simulator

# end
