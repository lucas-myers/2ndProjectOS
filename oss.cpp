#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main (int argCount, char* argValue[]) {

int launched = 0;
int finished = 0;
int gonnaLaunch = 0;

int running = 0;
int maxSims = 0;

int n =5;
int s = 3;
float t = 0;
float interval = 0;

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

    while (finished < gonnaLaunch){
        int going; //status of child
        pid_t pid = waitpid(-1, &going,WNOHANG);
        if (pid > 0){
            finished++;
            running--;
            std::cout << "Child: " << pid <<" terminated\n"
        }

    }


    if(launched < gonnaLaunch && running < maxSims){
        pid_t child = fork();
        if (child == 0){
         execl("./worker","./worker",SECONDS HERE, MILLISECONDS HERE, NULL);
        perror("fail");
        exit(1);
    } else if (child > 0){
        launched++;
        running++;
    } else {
        perror(" major fail");
    }

}