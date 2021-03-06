#ifndef CORE_PROCESS_H
#define CORE_PROCESS_H

#include <lang/lang.h>
#include <lang/Pool.h>
#include <fs/File.h>
#include <fs/devfs/PTY.h>
#include <core/Scheduler.h>
#include <memory/AddressSpace.h>
#include <interrupts/Interrupts.h>
#include <elf.h>
#include <signal.h>


class Thread;

class Process {
public:
    Process(Process* parent, const char* name);
    ~Process();

    Process* clone();
    
    Thread* spawnThread(threadEntryPoint entry, const char* name);

    int attachFile(File* f);
    void closeFile(int fd);
    void realpath(char* p, char* buf);

    void* sbrk(uint64_t size);
    void* sbrkStack(uint64_t size);
    void  allocateStack(uint64_t base, uint64_t size);

    void  requestKill();
    void  setSignalHandler(int signal, struct sigaction* a);
    void  queueSignal(int signal);
    void  executeDefaultSignal(int signal);
    void  runPendingSignals();

    uint64_t brk, stackbrk;
    uint64_t pid, ppid, pgid;
    Process* parent;
    char name[1024];
    char cwd[1024];

    AddressSpace* addressSpace;
    bool isKernel, isPaused;
    
    char exeName[256];
    Pool<Thread*, 1024> threads;
    Pool<File*, 1024> files;
    Pool<struct sigaction*, 64> signalHandlers;
    PTY* pty;
    uint64_t pendingSignals;

    void notifyChildDied(Process* p, uint64_t status);
    uint64_t deadChildPID;
    uint64_t deadChildStatus;
private:
};
#endif
