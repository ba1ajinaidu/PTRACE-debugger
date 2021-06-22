#include<stdio.h>
#include<stdlib.h>
#include<stdarg.h>
#include<signal.h>
#include<syscall.h>
#include<errno.h>
#include<vector>
#include<map>
#include<iostream>
#include<string>
#include<sstream>
#include<unistd.h>

#include<sys/ptrace.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/user.h>
#include<sys/reg.h>
#include<sys/personality.h>
#include<sys/stat.h>

using namespace std;

/*
Custom debugger using ptrace, works only on linux OS

To Run the debugger `./debugger path/to/executable`

######### Commads #########

next/ nexti         : To step to next instruction
break 0x123456      : To set a break point at 0x123456
continue / c        : To continue the execution, even c can be used
exit                : To exit the program
infobreak/ i b      : List of break points
info registers/ i r : List of registers with their values


############################ 
*/

map<unsigned int, unsigned int> Breakpoints;

void execute_program(pid_t child, const char* prog_name){
    
    // Disable ASLR on the executable
    personality(ADDR_NO_RANDOMIZE);

    // Trace the execution
    if(ptrace(PTRACE_TRACEME, child, 0, 0)<0)
    {
        cerr<<"[!] error in PTRACE_TRACEME";
        return;
    }

    // pid_t curr_pid = getpid();
    // cout<<"pid in execute_program fn is "<<curr_pid;

    execl(prog_name, prog_name, nullptr);

}


void disassemble(string func, pid_t child){

    int wait_status;
    pid_t new_child = fork();

    wait(&wait_status);
    
    if(new_child ==0){

        
        char * mypid;
        sprintf(mypid, "%d", child);
        printf("pid is %s", mypid);
        string comm = "objdump --disassemble="+func+" "+ "/proc/"+ mypid+ "/exe";
        printf("%s", comm);
        system(comm.c_str());
        
    }
    //kill(new_child, SIGTERM);

}

void Continue(pid_t child){

    int wait_status;
    struct user_regs_struct regs;
    
    ptrace(PTRACE_CONT, child, 0, 0);
    wait(&wait_status);


    if(WIFSTOPPED(wait_status)){
        // printf("Child got a signal: %d\n", (WSTOPSIG(wait_status)) );

        if(WSTOPSIG(wait_status)==5){
            
            ptrace(PTRACE_GETREGS, child, 0, 0);
            printf("[!] Break point hit at 0x%08llx\n", regs.rip);


            // restore data from the breakpoint
            std::map<unsigned int, unsigned int>::iterator iter = Breakpoints.find(regs.rip-1);
            if( iter != Breakpoints.end())
                printf("[+] Address %0x08llx: Data %0x08llx",iter->first, iter->second);

            
            unsigned int addr = Breakpoints.find(regs.rip-1)->first;
            unsigned int data = Breakpoints.find(regs.rip)->second;

            ptrace(PTRACE_POKETEXT, child, (void*)addr, (void*)data);

            // get the rip to the previous instruction
            regs.rip -= 1;

            // Set the registers after modifying rip
            ptrace(PTRACE_SETREGS, child, 0, &regs);

            // Restoring complete
        }

        else{
            perror("[-] Wait");
            return;
        }
        
        printf("[+] child stopped at 0x%08llx\n", regs.rip);

    }
    //cout<<"continue was called";

}


// Handle nexti command

void nexti(pid_t child){
    
    int wait_status;

    if(ptrace(PTRACE_SINGLESTEP, child, 0, 0)<0){
        perror("[!] error in PTRACE_SINGLESTEP");
        return;
    }
    wait(&wait_status);

}


void Set_breakpoint(pid_t child, unsigned int addr){

    // To set a breakpoint to the given address write 'INT 3' trap instruction to the address
    // 0xcc is the opcode for INT 3 
    // peek the data in the addr and then poke it with the 'INT 3/ 0xCC' modified data

    unsigned int data = ptrace(PTRACE_PEEKTEXT, child, (void*)addr, 0);
    unsigned int data_with_trap = (data & 0xffffff00 ) | 0xcc;
    ptrace(PTRACE_POKETEXT, child, (void *)addr, (void *)data_with_trap);
    printf("[+] Breakpoint set at 0x%08x\n", addr);

    // Insert this to the map

    Breakpoints.insert(std::pair<unsigned int, unsigned int> (addr, data));

}


void Info_registers(pid_t child){

    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, child, 0, 0);



    printf("######## Regsiters Info ########\n");
    printf(" RAX:  0x%08llx\n RBX:  0x%08llx\n RCX:  0x%08llx\n RDX:  0x%08llx\n RSP:  0x%08llx\n RBP:  0x%08llx\n RSI:  0x%08llx\n RDI:  0x%08llx\n RIP:  0x%08llx\n"
    , regs.rax,regs.rbx,regs.rcx,regs.rdx,regs.rsp,regs.rbp,regs.rsi,regs.rdi,regs.rip );

}


void List_breakpoints(){
    
    printf("\n   ######## Breakpoints ######## \n\n");
    int i =0;
    for(auto bp : Breakpoints){

        printf("-> Breakpoint %d : 0x%08x\n", ++i, bp.first);

    }

    printf("\n");
}


string last_command;

// Handle commands
void handle_command(pid_t child, string command){
    
    //cout<<"Handling command "<<command;

    if(command == "exit"){       
        kill(child, SIGKILL);
        printf("\n ######## Debugger Stopped ######## \n");
        exit(0);
    }

    else if(command.find("disass")==0 || command.find("dis")==0){
        std::size_t fn_name = command.find_last_of(' ');
        string func = command.substr(fn_name+1);
        
        printf(" ");
        disassemble(func, child);
    }
        
    else if(command == "continue" || command == "c") 
        Continue(child); 
    
    else if(command == "next" || command == "nexti")
        nexti(child);

    else if(command.find("break") == 0){

        // Get the last string (hopefully the address- 0x#####) // break 0x#####
        istringstream iss(command);

        std::size_t Lastword = command.find_last_of(' ');
        string addr = command.substr(Lastword+1);


        // convert the addr from string to unsigned int
        unsigned int uiaddr;
        std::stringstream hexstring;
        hexstring << std::hex << addr;
        hexstring >> uiaddr;

        //printf("----Breakpoint set at 0x%08x------\n", uiaddr);

        Set_breakpoint(child, uiaddr);

    }

    else if(command == "info registers" || command == "i r"){
        //cout<<command;
        Info_registers(child);
    }

    else if(command == "info breakpoints" || command == "i b"){
        List_breakpoints();
    }

    else if(command.empty()){
        last_command = command;
        nexti(child);
    }

    else{
        printf("[!] Unkonown command entered\n");
        command = "";
    }

    last_command = command;
    return;
    
}
int main(int argc, char **argv){
    
    int wait_status;
    
    char * executable;
    string command;

    if(argc < 2){
        cout << "\nPath to a executable file was not provided \n"<<"\nUsage: ./debugger path/to/executable/file \n";
        return -1;
    }
    executable = argv[1];

    // check if the provided file exists in the given path
    struct stat info;
    if(stat(executable, &info)!=0)
    {
        printf("\n '%s' file doesn't exist, please provide a proper file \n", executable);
        return -1;
    }


    pid_t child = fork();

    // pid_t cur_pid = getpid();
    // cout<<"Pid in main fn is"<<cur_pid;

    // execute the binary in child process

    if(child == 0){
        execute_program(child, executable);
    }

    // start debugging session
    
    printf("\n ######## Debugger started ######## \n\n");
    wait(&wait_status);


    while(WIFSTOPPED(wait_status)){
        
        // get all register values
        
        struct user_regs_struct regs;
        ptrace(PTRACE_GETREGS, child, 0, &regs);

        // extract value of rip register, which contains adress of the current instruction
        unsigned instr = ptrace(PTRACE_PEEKTEXT, child, regs.rip, 0);
        printf("0x%08llx >> ", regs.rip);

        // prompt for a new command
        getline(cin, command);

        //cout<<command<<"\n";
        // Handle the new command
        handle_command(child, command);

    }

}