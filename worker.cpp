#include <iostream>
#include <sys/shm.h>
#include <unistd.h>
#include <cstdlib>

struct Clock {
    int seconds;
    int nanoseconds;
};

int main(int argCount, char* argValue[]){
    if (argCount !=3){
        std::cerr << "Usage: worker seconds nanoseconds\n";
        exit(1);
    }

    int wSeconds = atoi(argValue[1]);
    int wNano = atoi(argValue[2]);

    std::cout << "Worker started. Will terminate after "
              << wSeconds << " sec and "
              << wNano << " ns\n";

    return 0;

    
}
