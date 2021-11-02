#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <string>
#include <random>

using namespace std;

#define DEBUG

int main(){
    int pid1 = fork();
    int pid2 = fork();
    int id = !pid1 * 2 + !pid2 + 1;
    mt19937 rng(time(0));
    int seed;
    for(int i=0;i<id;++i){
        seed = rng() % 1000000000;
    }
    #ifdef DEBUG
        printf("process #%d starts with pid %d and seed %d.\n",id,(int)getpid(),seed);
    #endif
    string command = "cd simu" + to_string(id) + ";./my-judge " + to_string(seed);
    system(command.c_str());
    #ifdef DEBUG
        printf("process #%d finishes calculation.\n",id);
    #endif
    if(pid2){
        {
            int status;
            int child = wait(&status);
            #ifdef DEBUG
                if(child > 0)
                    printf("process with pid %d revoked.\n",child);
                else
                    printf("error\n");
            #endif
        }
        if(pid1){
            {
                int status;
                int child = wait(&status);
                #ifdef DEBUG
                    if(child > 0)
                        printf("process with pid %d revoked.\n",child);
                    else
                        printf("error\n");
                #endif
            }
        }
    }
    return 0;
}