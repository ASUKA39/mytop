#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>

struct ProcInfo{
    int pid;
    char user[10];
    int pr;
    int ni;
    int virt;
    int res;
    int shr;
    char s;
    double cpu;
    double mem;
    char time[10];
    char command[20];
};

// read /proc/xxx to buffer
int readProcFile(const char *filename, char *buf) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return -1;
    }

    while (fread(buf, 1, 1024, file)) {
        buf += 1024;
    }

    fclose(file);
}

// parse file, return the block between head and tail
char* parseFile(char* path, char* buf, char* head, char* tail){
    int h = 0, t = 0;
    char *block;
    if(readProcFile(path, buf) == -1){
        return NULL;
    }
    if(head != ""){
        h = strstr(buf, head) - buf;
    }
    if(tail != ""){
        char *result = strstr(buf, tail);
        t = result - buf;
    }
    else{
        t = strlen(buf);
    }

    block = calloc(1, t - h + 1);
    memcpy(block, buf + h, t - h);
    return block;
}

// parse buffer, return the block between head and tail
char* parseBuf(char* buf, char* head, char* tail){
    int h = 0, t = 0;
    char *block;
    if(head != ""){
        h = strstr(buf, head) - buf;
    }
    if(tail != ""){
        char *result = strstr(buf, tail);
        t = result - buf;
    }
    else{
        t = strlen(buf);
    }

    block = calloc(1, t - h + 1);
    memcpy(block, buf + h, t - h);
    return block;
}

// parse buffer, return numbers in arr
void parseNum(char *buf, int *arr, int len){
    int flag = len;
    int head, tail;
    for(int i = 0; i < strlen(buf) || flag != 0; i++){
        if(buf[i] >= '0' && buf[i] <= '9' && buf[i-1] == ' '){
            int j = 0;
            char numbuf[10] = {0};
            while(buf[i] >= '0' && buf[i] <= '9' && i < strlen(buf)){
                numbuf[j] = buf[i];
                j++;
                i++;
            }
            arr[len - flag] = atoi(numbuf);
            flag--;
        }
    }
}

// parse buffer, return the len-th word in str
void parseStr(char *buf, char **str, int len){
    int flag = 0;
    char *strbuf = calloc(1, 30);
    if(len == 1){
        int j, i;
        i = j = 0;
        while(buf[i] != ' '){
            strbuf[j] = buf[i];
            j++;
            i++;
        }
    }
    else if(len > 1){
        for(int i = 0; i < strlen(buf) && flag != len; i++){
            if(buf[i] != ' ' && buf[i-1] == ' ' || i == 0){
                flag++;
                if(flag == len){
                    int j = 0;
                    while(buf[i] != ' '){
                        strbuf[j] = buf[i];
                        j++;
                        i++;
                    }
                    break;
                }
            }
        }
    }
    *str = strbuf;
    return;
}

int Tasks = 0;

// get tasks info
void getTasksInfo(){
    DIR *dir;
    struct dirent *ptr;
    int running, sleeping, stopped, zombie;
    running = sleeping = stopped = zombie = 0;
    
    dir = opendir("/proc");
    while((ptr = readdir(dir)) != NULL){    
        if(strspn(ptr->d_name, "0123456789") == strlen(ptr->d_name)){
            char buf[2048] = {0};
            char *tmp;
            char path[30];
            sprintf(path, "/proc/%d/stat", atoi(ptr->d_name));
            tmp = parseFile(path, buf, ptr->d_name, "");
            if(tmp == NULL){
                continue;
            }
            else{
                Tasks++;
                char *state;
                parseStr(tmp, &state, 3);
                if(strcmp(state, "R") == 0){
                    running++;
                }
                else if(strcmp(state, "S") == 0){
                    sleeping++;
                }
                else if(strcmp(state, "T") == 0){
                    stopped++;
                }
                else if(strcmp(state, "Z") == 0){
                    zombie++;
                }
                free(tmp);
            }
        }
    }

    printf("Tasks:%4d total,", Tasks);
    printf("%4d running,", running);
    printf("%4d sleeping,", sleeping);
    printf("%4d stopped,", stopped);
    printf("%4d zombie\n", zombie);

    closedir(dir);
    return;
}

int CPUInfo1[10];
double CPUInfo[10];

// get cpu info
void getCPUInfo(int time){ 
    if(time == 2){
        char buf[2048] = {0};
        char *bbuf, *tmp;
        // user, nice, system, idle, iowait,
        // irrq, softirq, steal, guest, guest_nice
        int CPUInfo2[10];

        bbuf = parseFile("/proc/stat", buf, "", "");
        tmp = parseBuf(bbuf, "cpu", "cpu0");
        parseNum(tmp, CPUInfo2, 10);
        free(tmp);

        for(int i = 0; i < 10; i++){
            CPUInfo[i] = (CPUInfo2[i]-CPUInfo1[i])*100. / ((CPUInfo2[0]+CPUInfo2[1]+CPUInfo2[2]+CPUInfo2[3])*1.-(CPUInfo1[0]+CPUInfo1[1]+CPUInfo1[2]+CPUInfo1[3])*1.);
        }
        for(int i = 0; i < 10; i++){
            CPUInfo1[i] = CPUInfo2[i];
        }
    }

    printf("%%Cpu(s):");
    printf(" %4.1lf us,", CPUInfo[0]);
    printf(" %4.1lf sy,", CPUInfo[2]);
    printf(" %4.1lf ni,", CPUInfo[1]);
    printf(" %4.1lf id,", CPUInfo[3]);
    printf(" %4.1lf wa,", CPUInfo[4]);
    printf(" %4.1lf hi,", CPUInfo[5]);
    printf(" %4.1lf si,", CPUInfo[6]);
    printf(" %4.1lf st\n", CPUInfo[7]);
    // printf(" guest: %3.1lf", CPUInfo[8]);
    // printf(" guest_nice: %3.1lf\n", CPUInfo[9]);
}

// get mem info
void getMemInfo(){
    char buf[2048] = {0};
    char *bbuf, *tmp;
    // MenTotal, MemFree, MemAvailable, Buffers, Cache
    // SwapTotal, SwapFree, 
    int Mem[5], Swap[2];

    bbuf = parseFile("/proc/meminfo", buf, "", "Dirty");
    tmp = parseBuf(bbuf, "", "SwapCached");
    parseNum(tmp, Mem, 5);
    free(tmp);
    tmp = parseBuf(bbuf, "SwapTotal", "");
    parseNum(tmp, Swap, 2);
    free(tmp);
    free(bbuf);

    printf("MiB Mem :");
    printf("%9.1f total,", Mem[0]/1024.);
    printf("%9.1f free,", Mem[1]/1024.);
    printf("%9.1f used,", Mem[0]/1024.-Mem[1]/1024.-Mem[3]/1024.-Mem[4]/1024.);
    printf("%9.1f buff/cache\n", Mem[3]/1024.+Mem[4]/1024.);
    
    printf("MiB Swap:");
    printf("%9.1f total,", Swap[0]/1024.);
    printf("%9.1f free,", Swap[1]/1024.);
    printf("%9.1f used.", Swap[0]/1024.-Swap[1]/1024.);
    printf("%9.1f avail Mem\n", Mem[2]/1024.);
}

void getStatus(){
    char buf[2048] = {0};
    char *tmp;

    printf("Hostname: ");
    readProcFile("/proc/sys/kernel/hostname", buf);
    printf("%s", buf);
    memset(buf, 0, sizeof(buf));

    printf("System Uptime: ");
    readProcFile("/proc/uptime", buf);
    printf("%s", buf);
    memset(buf, 0, sizeof(buf));
}

int row, cow;

// get proc info
/* not done yet */
void getProcInfo(int row, int time){
    DIR *dir;
    struct dirent *ptr;
    int running, sleeping, stopped, zombie;
    running = sleeping = stopped = zombie = 0;

    int order[Tasks];
    char info[Tasks][20];
    
    dir = opendir("/proc");
    int i = 0;
    while((ptr = readdir(dir)) != NULL){    
        if(strspn(ptr->d_name, "0123456789") == strlen(ptr->d_name)){
            char buf[2048] = {0};
            char *tmp;
            char path[30];
            
            // user
            sprintf(path, "/proc/%d", atoi(ptr->d_name));
            tmp = parseFile(path, buf, ptr->d_name, "");
            if(tmp == NULL){
                continue;
            }
            else{
                
                free(tmp);
            }

            // sprintf(path, "/proc/%d", atoi(ptr->d_name));
            // tmp = parseFile(path, buf, ptr->d_name, "");
            // if(tmp == NULL){
            //     continue;
            // }
            // else{
                
            //     free(tmp);
            // }

            // mem
            sprintf(path, "/proc/%d/statum", atoi(ptr->d_name));
            tmp = parseFile(path, buf, ptr->d_name, "");
            if(tmp == NULL){
                continue;
            }
            else{
                char *state;

                // shr
                parseStr(tmp, &state, 3);
                memcpy(&info[i][2], state, strlen(state));

                // res

                free(tmp);
            }

            // s pr ni
            sprintf(path, "/proc/%d/stat", atoi(ptr->d_name));
            tmp = parseFile(path, buf, ptr->d_name, "");
            if(tmp == NULL){
                continue;
            }
            else{
                char *state;
                // pr
                parseStr(tmp, &state, 18);
                memcpy(&info[i][2], state, strlen(state));
                // ni
                parseStr(tmp, &state, 19);
                memcpy(&info[i][3], state, strlen(state));
                // s
                parseStr(tmp, &state, 3);
                memcpy(&info[i][7], state, strlen(state));
                free(tmp);
            }

            // %cpu

            // %mem

            // time

            // command
            sprintf(path, "/proc/%d/status", atoi(ptr->d_name));
            tmp = parseFile(path, buf, ptr->d_name, "");
            if(tmp == NULL){
                continue;
            }
            else{
                char *state;
                // command
                parseStr(tmp, &state, 2);
                memcpy(&info[i][11], state, strlen(state));

                // vm
                char *ttmp;
                ttmp = parseBuf(tmp, "VmSize", "VmLck");
                parseStr(ttmp, &state, 2);
                memcpy(&info[i][4], state, strlen(state));
                free(state);

                // uid
                ttmp = parseBuf(tmp, "uid:", "");
                parseStr(ttmp, &state, 2);
                memcpy(&info[i][0], state, strlen(state));
                free(state);
                
                free(tmp);
            }

            // pid (info line invaild if pid == 0)
            info[i][0] = atoi(ptr->d_name);
            i++;
        }
        for(int i = 0; i < Tasks; i++){
            for(int j = 0; j < 12; j++){
                printf("%s ", &info[i][j]);
            }
            printf("\n");
        }
    }
}

int main() {
    int time = 0;
    while (1) {
        system("clear");
        char buf[2048] = {0};
        char *tmp, *line;
        
        // get size info of terminal
        struct winsize size;
        ioctl(STDIN_FILENO,TIOCGWINSZ,&size);
        row = size.ws_row - 9;

        getStatus();
        getTasksInfo();
        getCPUInfo(time);
        getMemInfo();

        printf("\n    PID USER      PR  NI    VIRT    RES    SHR S  %%CPU  %%MEM     TIME+ COMMAND\n");
        // getProcInfo(row, time);
        
        time = time == 2 ? 0 : time + 1;
        sleep(1);
    }

    return 0;
}