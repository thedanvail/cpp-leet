compile:
	clang++ --std=c++20 Solution.cpp -v -o Solution
run:
	./Solution

analyze:
	clang --analyze Solution.o