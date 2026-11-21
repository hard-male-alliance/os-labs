#include <elf.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <ulib.h>
#include <unistd.h>

#define STR1(value) #value
#define STR(value) STR1(value)
#define PAYLOAD_ENTRY 0x900000UL
#define PAYLOAD_STATUS 37

extern const unsigned char exec_payload[], exec_payload_end[];

asm(".pushsection .rodata\n"
    "exec_payload:\n"
    "li a0, " STR(SYS_exit) "\n"
    "li a1, " STR(PAYLOAD_STATUS) "\n"
    "ecall\n"
    "exec_payload_end:\n"
    ".popsection\n");

static struct {
    struct elfhdr header;
    struct proghdr load;
    unsigned char payload[64] __attribute__((aligned(4096)));
} image;

int main(void)
{
    size_t size = exec_payload_end - exec_payload;
    assert(size <= sizeof(image.payload));
    image.header = (struct elfhdr) {
        .e_magic = ELF_MAGIC, .e_elf = {2, 1, 1}, .e_type = 2, .e_machine = 243,
        .e_version = 1, .e_entry = PAYLOAD_ENTRY, .e_phoff = sizeof(struct elfhdr),
        .e_ehsize = sizeof(struct elfhdr), .e_phentsize = sizeof(struct proghdr), .e_phnum = 1,
    };
    image.load = (struct proghdr) {
        .p_type = ELF_PT_LOAD, .p_flags = ELF_PF_R | ELF_PF_X, .p_offset = 4096,
        .p_va = PAYLOAD_ENTRY, .p_filesz = size, .p_memsz = size, .p_align = 4096,
    };
    memcpy(image.payload, exec_payload, size);

    int child = fork();
    assert(child >= 0);
    if (child == 0)
    {
        const unsigned char *binary = (const unsigned char *)&image;
        assert(exec("payload", binary, 4096) == -E_INVAL_ELF);
        cprintf("exec preserved the old image after failure.\n");
        exec("payload", binary, 4096 + size);
        panic("exec returned after a valid image");
    }

    int status;
    assert(waitpid(child, &status) == 0 && status == PAYLOAD_STATUS);
    cprintf("exec pass: replacement ELF exited with status %d.\n", status);
    return 0;
}
