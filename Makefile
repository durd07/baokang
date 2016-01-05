server:./src/baokang_pthread.cpp ./src/baokang_handler.cpp
	g++ -g -o $@ $^ -I./inc -std=c++11 -lpthread
