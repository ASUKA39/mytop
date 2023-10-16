#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

struct ProcInfo{
    int pid;
    char user[30];
    int pr;
    int ni;
    int virt;
    int res;
    int shr;
    char s;
    double cpu;
    double mem;
    int time;
    char command[20];
};

// read /proc/xxx to buffer
int readProcFile(const char *filename, char *buf, int len) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        return -1;
    }

    fread(buf, 1, len, file);
    // printf(">> %s", buf);
    fclose(file);
}

// parse file, return the block between head and tail
char* parseFile(char* path, char* buf, char* head, char* tail, int len){
    int h = 0, t = 0;
    char *block;
    if(readProcFile(path, buf, len) == -1){
        return NULL;
    }
    if(head != ""){
        char *result = strstr(buf, head);
        h = result - buf;
    }
    else{
        h = 0;
    }
    if(tail != ""){
        char *result = strstr(buf, tail);
        t = result - buf;
    }
    else{
        t = strlen(buf);
    	// t = len;
    }
    
    block = calloc(1, t - h + 1);
    // printf("parse: %s, %d, %d\n\n", path, h, t-h);
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
    for(int i = 0; i < strlen(buf) && flag != 0; i++){
        if(buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' || buf[i] == '\r'){
            continue;
        }
        if(buf[i] >= '0' && buf[i] <= '9' ){
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
    char *strbuf = calloc(1, 50);
    if(len == 1){
        int j, i;
        i = j = 0;
        while(buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\t' && buf[i] != '\r'){
            strbuf[j] = buf[i];
            j++;
            i++;
        }
    }
    else if(len > 1){
        for(int i = 0; i < strlen(buf) && flag != len; i++){
            if(buf[i] == ' ' || buf[i] == '\n' || buf[i] == '\t' || buf[i] == '\r'){
                continue;
            }
            else{
                flag++;
                if(flag == len){
                    int j = 0;
                    while(buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\t' && buf[i] != '\r' && i < strlen(buf)){
                        strbuf[j] = buf[i];
                        j++;
                        i++;
                    }
                    break;
                }
                else{
                    while(buf[i] != ' ' && buf[i] != '\n' && buf[i] != '\t' && buf[i] != '\r' && i < strlen(buf)){
                        i++;
                    }
                }
            }
        }
    }
    *str = strbuf;
    return;
}



// get tasks info
void getTasksInfo(){
    DIR *dir;
    struct dirent *ptr;
    int Tasks = 0;
    int running, sleeping, stopped, zombie;
    running = sleeping = stopped = zombie = 0;
    
    dir = opendir("/proc");
    while((ptr = readdir(dir)) != NULL){    
        if(strspn(ptr->d_name, "0123456789") == strlen(ptr->d_name)){
            char buf[2048] = {0};
            char *tmp;
            char path[30];
            sprintf(path, "/proc/%d/stat", atoi(ptr->d_name));
            tmp = parseFile(path, buf, ptr->d_name, "", 2048);
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

// user, nice, system, idle, iowait,
// irrq, softirq, steal, guest, guest_nice
int CPUInfo1[10];
double CPUInfo[10];

// get cpu info
void getCPUInfo(int time){ 
    if(time == 2){
        char *buf = calloc(1, 1024);
	char *bbuf, *tmp;
        int CPUInfo2[10];

        bbuf = parseFile("/proc/stat", buf, "", "", 1024);
        tmp = parseBuf(bbuf, "cpu", "cpu0");
       	printf(">>>> %s\n", bbuf);
       	parseNum(tmp, CPUInfo2, 10);
	free(buf);
	// puts("free >>> buf");
        free(tmp);
	// puts("free >>> tmp");
	free(bbuf);
	// puts("free >>> bbuf");

        for(int i = 0; i < 10; i++){
	    int ttmp = (CPUInfo2[0]+CPUInfo2[1]+CPUInfo2[2]+CPUInfo2[3])- \
		      (CPUInfo1[0]+CPUInfo1[1]+CPUInfo1[2]+CPUInfo1[3]);
            if(ttmp > 0)
		CPUInfo[i] = (CPUInfo2[i]-CPUInfo1[i])*100. / ttmp;
	    else
		CPUInfo[i] = 0;
	    CPUInfo1[i] = CPUInfo2[i];
	    printf(">>> %d", CPUInfo2[i]);
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
int Mem[5], Swap[2];
void getMemInfo(){
    char buf[2048] = {0};
    char *bbuf, *tmp;
    // MenTotal, MemFree, MemAvailable, Buffers, Cache
    // SwapTotal, SwapFree, 

    bbuf = parseFile("/proc/meminfo", buf, "", "Dirty", 2048);
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
    readProcFile("/proc/sys/kernel/hostname", buf, 2048);
    printf("%s", buf);
    memset(buf, 0, sizeof(buf));

    printf("System Uptime: ");
    readProcFile("/proc/uptime", buf, 2048);
    printf("%s", buf);
    memset(buf, 0, sizeof(buf));
}

int row, cow;
int proctime = 0;

// get proc info
/* not done yet */
void getProcInfo(){
    DIR *dir;
    struct dirent *ptr;
    struct ProcInfo info;
    
    dir = opendir("/proc");
    int i = 0;
    while((ptr = readdir(dir)) != NULL){
        if(strspn(ptr->d_name, "0123456789") == strlen(ptr->d_name)){
            char buf[2048] = {0};
            char *tmp;
            char path[30];
            
            sprintf(path, "/proc/%d/statm", atoi(ptr->d_name));            
            tmp = parseFile(path, buf, "", "", 2048);
            if(tmp == NULL){
                info.pid = 0;
                continue;
            }
            else{
                char *state;
                // shr
                parseStr(tmp, &state, 3);
                info.shr = atoi(state) * 4;

                free(tmp);
            }
            
            // pr ni s
            sprintf(path, "/proc/%d/stat", atoi(ptr->d_name));
            tmp = parseFile(path, buf, ptr->d_name, "", 2048);
            if(tmp == NULL){
                info.pid = 0;
                continue;
            }
            else{
                char *state;
                // pr
                parseStr(tmp, &state, 18);
                info.pr = atoi(state);
                free(state);
                // ni
                parseStr(tmp, &state, 19);
                info.ni = atoi(state);
                free(state);
                // s
                parseStr(tmp, &state, 3);
                info.s = state[0];
                free(state);

                // time
                int proctime1 = 0;
                parseStr(tmp, &state, 14);
                proctime1 += atoi(state);
                free(state);
                parseStr(tmp, &state, 15);
                proctime1 += atoi(state);
                free(state);
                info.time = proctime1 * sysconf(_SC_CLK_TCK);

                free(tmp);
            }

            // user command virt res
            sprintf(path, "/proc/%d/status", atoi(ptr->d_name));
            tmp = parseFile(path, buf, "", "", 2048);
            if(tmp == NULL){
                info.pid = 0;
                continue;
            }
            else{
                char *state;
                char *ttmp;
                
                // command
                state = parseBuf(tmp, "", "Gid:");
                parseStr(state, &ttmp, 2);
                if(strlen(ttmp) > 20){
                    ttmp[19] = '\0';
                }
                strcpy(info.command, ttmp);
                free(ttmp);
                
                // user
                state = parseBuf(tmp, "Uid:", "Gid:");
                ttmp = getpwuid(atoi(state + 4))->pw_name;
                if(strlen(ttmp) > 8) {
                    ttmp[7] = '+';
                    ttmp[8] = '\0';
                }
                strcpy(info.user, ttmp);

                // virt
                state = parseBuf(tmp, "VmSize:", "VmLck:");
                parseStr(state, &ttmp, 2);
                info.virt = atoi(ttmp);
                free(ttmp);

                // res
                state = parseBuf(tmp, "VmRSS:", "RssAnon:");
                parseStr(state, &ttmp, 2);
                info.res = atoi(ttmp);
                free(ttmp);

                free(tmp);
            }

            // %mem
            info.mem = info.res / (Mem[0] * 10 / 1024.);
            
            // %cpu


            // pid
            info.pid = atoi(ptr->d_name);
            
            if(info.pid != 0){
                printf("%7d", info.pid);
                printf(" %-8s", info.user);
                printf("%4d", info.pr);
                printf("%4d", info.ni);
                if(info.virt > 9999999){
                    printf("%7.1fg", info.virt/1024./1024.);
                }
                else{
                    printf("%8d", info.virt);
                }
                if(info.res > 999999){
                    printf("%6.1fg", info.res/1024./1024.);
                }
                else{
                    printf("%7d", info.res);
                }
                if(info.shr > 999999){
                    printf("%6.1fg", info.shr/1024./1024.);
                }
                else{
                    printf("%7d", info.shr);
                }
                printf(" %c", info.s);
                printf("%6.1lf", info.cpu);
                printf("%6.1lf", info.mem);
                printf("%4d:%02d.%02d", (info.time/10000/60), (info.time/10000%60), (info.time%10000/100));
                printf(" %s\n", info.command);
            }

            i++;
        }
    }
    closedir(dir);
    return;
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
        getProcInfo();

        time = time == 2 ? 0 : time + 1;
        sleep(1);
    }

    return 0;
}
