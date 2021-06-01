output:	MachineProblem.o
	g++ MachineProblem.o -o sim_cache

final_me.o: MachineProblem.cpp
	g++ -c MachineProblem.cpp

clean:
	rm *.o sim_cache