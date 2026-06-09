#include <utils.h>

uint32_t target_pid; //used only for find_by_pid function

char **get_instructions_from_file(char *path) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *content = malloc(file_size + 1);
    if (content == NULL) {
        fclose(file);
        return NULL;
    }
    
    fread(content, 1, file_size, file);
    content[file_size] = '\0';
    fclose(file);
    
    char **instructions = string_split(content, "\n");
    free(content);
    
    return instructions;
}

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
    pcb->instructions = get_instructions_from_file(path); //MODE OF ACCESS: instructions[pcb->context.pc]

    return pcb;
}

bool find_by_pid(void *element) {
    t_pcb *pcb = (t_pcb *) element;
    return pcb->pid == target_pid;
}

void send_memory_update(int ks_fd, uint32_t new_total) {
    t_package *pkg = package_create();
    pkg->op_code = MEMORY_UPDATE;
    package_add(pkg, &new_total, sizeof(uint32_t));
    package_send(pkg, ks_fd);
    package_delete(pkg);
}