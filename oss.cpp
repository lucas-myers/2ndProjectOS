#include <iostream>

int main () {

int launched;
int finished;
int gonnaLaunch;

    while (finished < gonnaLaunch){

    }

    //check for when clidren terminate
    int status;
    pid_t pid = waitpid();
    if (pid > 0){}

    if(launched < gonnaLaunch && running < maxSims){
        pid_t child = fork();
        if (child == 0) execl("./worker","./worker",SECONDS HERE, MILLISECONDS HERE, NULL);
        else{}
    }

}