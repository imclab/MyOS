#include <core/Thread.h>
#include <core/Processor.h>
#include <core/Process.h>
#include <kutils.h>


Thread::Thread(Process *p) {
    static int tid = 0;
    id = tid++;
    esp = eip = NULL;
    process = p;
    process->threads->insertLast(this);
    dead = false;
}

void Thread::die() {
    dead = true;
    while (true)
        Processor::idle();
}