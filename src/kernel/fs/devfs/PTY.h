#ifndef FS_DEVFS_TTY_H
#define FS_DEVFS_TTY_H

#include <lang/lang.h>
#include <fs/File.h>
#include <fs/Pipe.h>


class PTYSlave;
class PTYMaster;


class PTY {
public:
    PTY();

    PTYMaster* openMaster();
    PTYSlave* openSlave();

    Pipe masterWritePipe;
    Pipe masterReadPipe;
};

class PTYMaster : public StreamFile {
public:
    PTYMaster(PTY*);
    virtual int write(const void* buffer, uint64_t count);
    virtual uint64_t read(void* buffer, uint64_t count);
    virtual bool canRead();
private:
    PTY* pty;
};

class PTYSlave : public StreamFile {
public:
    PTYSlave(PTY*);
    virtual int write(const void* buffer, uint64_t count);
    virtual uint64_t read(void* buffer, uint64_t count);
    virtual bool canRead();
    virtual int stat(struct stat* stat);
private:
    PTY* pty;
};



#endif