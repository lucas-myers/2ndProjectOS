#include <iostream>
#include <cstdlib>
#include <sys/shm.h>
#include <unistd.h>

struct Clock {
    unsigned int seconds;
    unsigned int nanoseconds;
};

static const unsigned int BILLION = 1000000000u;

static void normalize(unsigned int &s, unsigned int &ns) {
    if (ns >= BILLION) {
        s += ns / BILLION;
        ns %= BILLION;
    }
}

static bool timeGE(unsigned int s1, unsigned int n1, unsigned int s2, unsigned int n2) {
    if (s1 > s2) return true;
    if (s1 < s2) return false;
    return n1 >= n2;
}

int main(int argc, char* argv[]) {
   
    if (argc != 4) {
        std::cerr << "Usage: ./worker seconds nanoseconds shmid\n";
        return 1;
    }
	//how long to live
    unsigned int addS = (unsigned int)atoi(argv[1]);
    unsigned int addN = (unsigned int)atoi(argv[2]);
    normalize(addS, addN);
	
    //gets shmid from the oss file
    int shmid = atoi(argv[3]);

    //Attached to the memory in the clock
    volatile Clock* c = (volatile Clock*)shmat(shmid, NULL, 0);
    if (c == (void*)-1) { perror("shmat"); return 1; }
    
    
    //stores the time
    unsigned int startS = c->seconds;
    unsigned int startN = c->nanoseconds;
	
    //termination time
    unsigned int termS = startS + addS;
    unsigned int termN = startN + addN;
    normalize(termS, termN);

    std::cout << "WORKER PID:" << getpid() << " PPID:" << getppid() << "\n";
    std::cout << "SysClockS: " << startS
              << " SysclockNano: " << startN
              << " TermTimeS: " << termS
              << " TermTimeNano: " << termN << "\n";
    std::cout << "--Just Starting\n" << std::flush;

    unsigned int lastSec = startS;

    while (true) {//keeps checking the clock
        unsigned int nowS = c->seconds;
        unsigned int nowN = c->nanoseconds;

        if (timeGE(nowS, nowN, termS, termN)) {
            std::cout << "WORKER PID:" << getpid() << " PPID:" << getppid() << "\n";
            std::cout << "SysClockS: " << nowS
                      << " SysclockNano: " << nowN
                      << " TermTimeS: " << termS
                      << " TermTimeNano: " << termN << "\n";
            std::cout << "--Terminating\n" << std::flush;
            break;
        }

        if (nowS != lastSec) {
            std::cout << "WORKER PID:" << getpid() << " PPID:" << getppid() << "\n";
            std::cout << "SysClockS: " << nowS
                      << " SysclockNano: " << nowN
                      << " TermTimeS: " << termS
                      << " TermTimeNano: " << termN << "\n";
            std::cout << "--" << (nowS - startS) << " seconds have passed since starting\n"
                      << std::flush;
            lastSec = nowS;
        }
        
    }

    shmdt((const void*)c);
    return 0;
}
