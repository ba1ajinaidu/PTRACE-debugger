# PTRACE-debugger
A custom debugger for linux based operating systems built using ptrace syscall.

## How to run
Compile the source code
```bash
g++ debugger.cpp -o debugger
```
Execute the debugger with the path to binary file to debug
```bash
./debugger path/to/binary_file
```

## Commands in the debugger

|Command              | Description                                   |
|---------------------|-----------------------------------------------|
| next/ nexti         | To step to next instruction                   |
| break 0x123456      | To set a break point at 0x123456              |
| continue / c        | To continue the execution, even c can be used |
| exit                | To exit the program                           |
| infobreak/ i b      | List of break points                          |
| info registers/ i r | List of registers with their values           |

## Resources
1. [Playing with Ptrace](https://www.linuxjournal.com/article/6100)
2. Liz Rice's talk on debuggers from scratch [part 1](https://youtu.be/TBrv17QyUE0), [part 2](https://youtu.be/ZrpkrMKYvqQ)
3. [Reading registers of a traced process](https://stackoverflow.com/questions/48785758/how-does-gdb-read-the-register-values-of-a-program-process-its-debugging-how)
