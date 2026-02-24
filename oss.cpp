#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct Clock{
    int seconds;
    int nanoseconds;
};

struct PCB{
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
    int secEnd;
    int nanosecEnd;


};

Clock simClock = {0, 0};

void incrementClock(){
    simClock.nanoseconds += 10000000; //10ms

    if (simClock.nanoseconds >= 1000000000) {
        simClock.seconds++;
        simClock.nanoseconds -= 1000000000;
    }

}
struct PCB processTable[20];

int main (int argCount, char* argValue[]) {

int launched = 0;
int finished = 0;
int running = 0;

int n =5;
int s = 3;
float t = 0;
float i = 0;

for (int x = 0; x < 20; x++){ //processes values in the table
    processTable[x].occupied = 0;
}

int option;
while ((option = getopt(argCount, argValue, "n:s:t:i:")) != -1) {
    switch (option) {
        case 'n':
            n = atoi(optarg);
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 't':
            t = atof(optarg);
            break;
        case 'i':
            i = atof(optarg);
            break;
        default:
            std::cerr << "Usage: ./oss -n num -s maxSim -t timeLimit -i interval\n";
            exit(1);
    }
}

    while (launched < n || running > 0) {

    incrementClock();

    int status;
    pid_t pid;

    // Check for terminated children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {

        finished++;
        running--;

        std::cout << "Child: " << pid << " terminated\n";

        for (int x = 0; x < 20; x++) {
            if (processTable[x].pid == pid) {
                processTable[x].occupied = 0;
                break;
            }
        }
    }

    // decide if we can launch new child
    if (launched < n && running < s) {

        pid_t child = fork();

        if (child == 0) {
            execl("./worker", "./worker", "1", "500000000", NULL);
            perror("exec fail");
            exit(1);
        }
        else if (child > 0) {

            launched++;
            running++;

            for (int x = 0; x < 20; x++) {
                if (!processTable[x].occupied) {
                    processTable[x].occupied = 1;
                    processTable[x].pid = child;
                    break;
                }
            }
        }
        else {
            perror("fork fail");
        }
    }
}