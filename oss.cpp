#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <csignal>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

struct Clock {
    unsigned int seconds;
    unsigned int nanoseconds;
};

struct PCB {
    int occupied;
    pid_t pid;
    unsigned int startSeconds;
    unsigned int startNano;
    unsigned int endSeconds;
    unsigned int endNano;
};

//global constants
static const int MAX_PCB = 20; //max num
static const unsigned int BILLION = 1000000000u; // 1 sec in nano

static int g_shmid = -1; //store shmem id
static volatile Clock* g_clock = nullptr; //ptr to clock
static PCB g_ptable[MAX_PCB]; //p table
static volatile sig_atomic_t g_stop = 0; //alarm


//fix for nano to seconds (keeps nanos under a billion)
static void normalize(unsigned int &s, unsigned int &ns) {
    if (ns >= BILLION) {
        s += ns / BILLION;
        ns %= BILLION;
    }
}

//checks if time is greater then other time
static bool timeGE(unsigned int s1, unsigned int n1, unsigned int s2, unsigned int n2) {
    if (s1 > s2) return true;
    if (s1 < s2) return false;
    return n1 >= n2;
}

//converts seconds and nanoseconds into one nano second val
static unsigned long long toNs(unsigned int s, unsigned int ns) {
    return (unsigned long long)s * BILLION + ns;
}


static void incrementClock(unsigned int incNs) {
    unsigned int s = g_clock->seconds;
    unsigned int n = g_clock->nanoseconds;
    n += incNs;
    normalize(s, n);
    g_clock->seconds = s;
    g_clock->nanoseconds = n;
}

//how many proccesses are running
static int countRunning() {
    int c = 0;
    for (int i = 0; i < MAX_PCB; i++) c += (g_ptable[i].occupied != 0);
    return c;
}

//find first slot in PCB available
static int findFreeSlot() {
    for (int i = 0; i < MAX_PCB; i++) if (!g_ptable[i].occupied) return i;
    return -1;
}

//find the PCB index by what the child PID is
static int findByPid(pid_t pid) {
    for (int i = 0; i < MAX_PCB; i++)
        if (g_ptable[i].occupied && g_ptable[i].pid == pid) return i;
    return -1;
}

static void printTable() {
    std::cout << "\nOSS PID:" << getpid()
              << " SysClockS:" << g_clock->seconds
              << " SysClockNano:" << g_clock->nanoseconds << "\n";
    std::cout << "Active Processes:\n";
    std::cout << "Entry  PID     StartS     StartN     EndS       EndN\n";

    bool any = false;
    for (int i = 0; i < MAX_PCB; i++) {
        if (g_ptable[i].occupied) {
            any = true;
            const PCB &p = g_ptable[i];
            std::cout << std::setw(5) << i << "  "
                      << std::setw(6) << p.pid << "  "
                      << std::setw(9) << p.startSeconds << "  "
                      << std::setw(9) << p.startNano << "  "
                      << std::setw(9) << p.endSeconds << "  "
                      << std::setw(9) << p.endNano << "\n";
        }
    }
    if (!any) std::cout << "(none)\n";
    std::cout << std::flush;
}

static void cleanupShm() {
    if (g_clock && g_clock != (void*)-1) {
        shmdt((const void*)g_clock);
        g_clock = nullptr;
    }
    if (g_shmid != -1) {
        shmctl(g_shmid, IPC_RMID, NULL);
        g_shmid = -1;
    }
}

static void killChildren() {
    for (int i = 0; i < MAX_PCB; i++)
        if (g_ptable[i].occupied) kill(g_ptable[i].pid, SIGTERM);
}

static void onSignal(int) { g_stop = 1; }

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " [-h] [-n proc] [-s simul] [-t timelimit] [-i interval]\n"
              << "Example: " << prog << " -n 3 -s 2 -t 4 -i 0.6\n";
}

int main(int argc, char* argv[]) {
    
    int n = 5;              // total workers
    int s = 3;              // max simultaneous
    float t = 4.0f;         // kept for printing
    float interval = 0.2f;  // min interval between launches 

    int opt;
    while ((opt = getopt(argc, argv, "hn:s:t:i:")) != -1) {
        switch (opt) {
            case 'h': usage(argv[0]); return 0;
            case 'n': n = atoi(optarg); break;
            case 's': s = atoi(optarg); break;
            case 't': t = atof(optarg); break;
            case 'i': interval = atof(optarg); break;
            default: usage(argv[0]); return 1;
        }
    }

    // Validation
    if (n < 1) n = 1;
    if (s < 1) s = 1;
    if (s > MAX_PCB) s = MAX_PCB;
    if (t < 0.0f) t = 0.0f;
    if (interval < 0.0f) interval = 0.0f;

    std::cout << "OSS starting, PID:" << getpid() << " PPID:" << getppid() << "\n";
    std::cout << "Called with:\n";
    std::cout << "-n " << n << "\n";
    std::cout << "-s " << s << "\n";
    std::cout << "-t " << t << "\n";
    std::cout << "-i " << interval << "\n" << std::flush;

    for (int i = 0; i < MAX_PCB; i++) g_ptable[i] = {0, 0, 0, 0, 0, 0};

    // Signals
    signal(SIGINT, onSignal);
    signal(SIGALRM, onSignal);
    alarm(60);

    // Shared memory
    g_shmid = shmget(IPC_PRIVATE, sizeof(Clock), IPC_CREAT | 0666);
    if (g_shmid == -1) { perror("shmget"); return 1; }

    g_clock = (volatile Clock*)shmat(g_shmid, NULL, 0);
    if (g_clock == (void*)-1) { perror("shmat"); cleanupShm(); return 1; }

    g_clock->seconds = 0;
    g_clock->nanoseconds = 0;

    // Print every 0.5 simulated seconds
    unsigned int nextPrintS = 0;
    unsigned int nextPrintN = 500000000u;

    unsigned int intervalNs = (unsigned int)(interval * 1000000000.0f);

    // Sim clock tick per loop
    const unsigned int incNs = 100000u; // 0.1 ms

    int launched = 0;
    int finished = 0;
    unsigned long long totalRuntimeNs = 0;
    unsigned long long lastLaunchNs = 0;

    while (!g_stop) {
        bool stillChildrenToLaunch = (launched < n);
        bool haveChildrenInSystem = (countRunning() > 0);

        if (!stillChildrenToLaunch && !haveChildrenInSystem) break;

        incrementClock(incNs);

        // periodic table output
        if (timeGE(g_clock->seconds, g_clock->nanoseconds, nextPrintS, nextPrintN)) {
            printTable();
            nextPrintN += 500000000u;
            normalize(nextPrintS, nextPrintN);
        }

        // gather terminated children
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            int idx = findByPid(pid);
            if (idx != -1) {
                unsigned long long start = toNs(g_ptable[idx].startSeconds, g_ptable[idx].startNano);
                unsigned long long end   = toNs(g_clock->seconds, g_clock->nanoseconds);
                if (end >= start) totalRuntimeNs += (end - start);
                g_ptable[idx].occupied = 0;
            }
            finished++;
        }

        // launch new child if allowed
        if (stillChildrenToLaunch && countRunning() < s) {
            unsigned long long nowNs = toNs(g_clock->seconds, g_clock->nanoseconds);
            bool intervalOk = (launched == 0) ? true : (nowNs >= lastLaunchNs + intervalNs);

            if (intervalOk) {
                int slot = findFreeSlot();
                if (slot != -1) {
                    unsigned int startS = g_clock->seconds;
                    unsigned int startN = g_clock->nanoseconds;

                    int wSec  = rand() % 5;
                    int wNano = rand() % 1000000000;

                    unsigned int estEndS = startS + (unsigned int)wSec;
                    unsigned int estEndN = startN + (unsigned int)wNano;
                    normalize(estEndS, estEndN);

                    pid_t child = fork();
                    if (child == 0) {
                        char sBuf[32], nBuf[32], idBuf[32];
                        snprintf(sBuf, sizeof(sBuf), "%d", wSec);
                        snprintf(nBuf, sizeof(nBuf), "%d", wNano);
                        snprintf(idBuf, sizeof(idBuf), "%d", g_shmid);

                        execl("./worker", "./worker", sBuf, nBuf, idBuf, (char*)NULL);
                        perror("execl");
                        _exit(1);
                    } else if (child > 0) {
                        g_ptable[slot] = {1, child, startS, startN, estEndS, estEndN};
                        launched++;
                        lastLaunchNs = nowNs;
                    } else {
                        perror("fork");
                    }
                }
            }
        }
    }

    //  timeout cleanup
    if (g_stop) {
        killChildren();

        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, 0)) > 0) {
            int idx = findByPid(pid);
            if (idx != -1) {
                unsigned long long start = toNs(g_ptable[idx].startSeconds, g_ptable[idx].startNano);
                unsigned long long end   = toNs(g_clock->seconds, g_clock->nanoseconds);
                if (end >= start) totalRuntimeNs += (end - start);
                g_ptable[idx].occupied = 0;
            }
            finished++;
        }
    }

    std::cout << "\nOSS PID:" << getpid() << " Terminating\n";
    std::cout << finished << " workers were launched and terminated\n";
    std::cout << "Workers ran for a combined time of "
              << (totalRuntimeNs / BILLION) << " seconds "
              << (totalRuntimeNs % BILLION) << " nanoseconds.\n";

    cleanupShm();
    return 0;
}
