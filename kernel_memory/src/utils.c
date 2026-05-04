#include <utils.h>

uint32_t target_pid;

t_pcb *create_pcb(uint32_t pid, char *path) {
    t_pcb *pcb = malloc(sizeof(t_pcb));
    
    pcb->pid = pid;
    pcb->context.pc = 0;
    pcb->context.ax = 0;
    pcb->context.bx = 0;
    pcb->context.cx = 0;
    pcb->context.dx = 0;
    pcb->context.eax = 0;
    pcb->context.ebx = 0;
    pcb->context.ecx = 0;
    pcb->context.edx = 0;
    pcb->context.si = 0;
    pcb->context.di = 0;
    pcb->instruction_path = path;

    return pcb;
}

bool find_by_pid(void *element) {
    t_pcb *pcb = (t_pcb *) element;
    return pcb->pid == target_pid;
}