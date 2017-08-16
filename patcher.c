#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>

#include <windows.h>
#include <winnt.h>


typedef int (*fixup_t)(unsigned char* ptr);

extern struct patch *patches;
struct patch {
    DWORD addrVA;
    char *buf;
    DWORD size;
    fixup_t fixup;
    char desc[0];
};

DWORD set_relative_target(unsigned char *code, DWORD VA, DWORD targetVA)
{
    DWORD res = 0;
    DWORD *rel = (DWORD*)(code+1);
    switch(code[0]) {
    case 0xE8: // call near rel32
    case 0xE9: // jmp near rel32
        res = VA + *rel + 5; /* 5 bytes instruction length */
        *rel = targetVA - VA - 5;
        break;
    default:
        printf("set_rel_target failed. unknown ins: %x\n", code[0]);
    }
    return res;
}


static int add_area(IMAGE_NT_HEADERS *nt, const char *section_name, DWORD size)
{
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    IMAGE_SECTION_HEADER *target = NULL;
    DWORD res = 0;

    for( unsigned int i = 0; i < nt->FileHeader.NumberOfSections; i++,sec++) {
        if( strncmp( (const char*)sec->Name, section_name, IMAGE_SIZEOF_SHORT_NAME) == 0 )
            target = sec;
    }
    if(!target) {
        printf("failed to find section: %s\n", section_name);
        return res;
    }

    /* Easy way:
     * raw data is usually aligned to ~0x100 byte boundary leaving hopefully some space */
    if( (target->SizeOfRawData - target->Misc.VirtualSize) >= size ) {
        res =  nt->OptionalHeader.ImageBase + target->VirtualAddress + target->Misc.VirtualSize;
        target->Misc.VirtualSize += size;
        printf("allocating space in %s @%lx (sz:%lx)\n", section_name, res, size);
    } else { // FIXME: handle this
        printf("empty space too small\n");
    }
    return res;
}

static IMAGE_SECTION_HEADER* find_section(IMAGE_NT_HEADERS *nt, DWORD codeVA, DWORD sz)
{
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    codeVA -= nt->OptionalHeader.ImageBase;
    for( unsigned int i = 0; i < nt->FileHeader.NumberOfSections; i++,sec++) {
        if( (codeVA > sec->VirtualAddress)
            && ( (codeVA+sz) <= (sec->VirtualAddress+sec->Misc.VirtualSize)) )
            return sec;
    }
    printf("invalid codeVA: %lx no matching section\n", codeVA);
    return NULL;
}


static int apply_patches(unsigned char* exe)
{
    IMAGE_DOS_HEADER *dos = (void*)exe;
    IMAGE_NT_HEADERS *nt = (void*)(exe + dos->e_lfanew);
    if(nt->Signature != IMAGE_NT_SIGNATURE) {
        printf("no PE file\n");
        return -1;
    }
    DWORD ImageBase = nt->OptionalHeader.ImageBase;

    for(struct patch **p  = &patches; *p; p++) {
        struct patch *patch = *p;

        if(patch->addrVA == -1) {
            printf("skip patch: %s\n",patch->desc);
            continue;
        }
        else if(patch->addrVA == 0) {
            /* try to find some empty space */
            patch->addrVA = add_area(nt, ".text", patch->size);
        }

        printf("patch VA@:%lx sz:%ld: %s\n", patch->addrVA, patch->size, patch->desc);

        IMAGE_SECTION_HEADER *sec = find_section(nt, patch->addrVA, patch->size);
        if(!sec) return -1;
        unsigned char *ptr = exe + sec->PointerToRawData + (patch->addrVA - ImageBase - sec->VirtualAddress);

        if(patch->buf && patch->size)
            memcpy(ptr, patch->buf, patch->size);

        if(patch->fixup) {
            //printf("*fixup ...: %p\n", ptr);
            if( patch->fixup(ptr) ) {
                printf("fixup() failed\n");
                return -1;
            }
        }
        //printf("DONE\n");
    }
    return 0;
}

static int write_file(const char *path, unsigned char* buf, off_t size)
{
    int res = -1;
    int fd = open(path, O_WRONLY | O_CREAT | O_BINARY, 00644);
    if(fd == -1) {
        printf("write result: opem() failed\n");
        return -1;
    }
    ssize_t nb = write(fd, buf, size);
    if(nb != size)
        printf("write result: write() failed\n");
    else
        res = 0;

    close(fd);

    return res;
}

static unsigned char* read_file(const char *in, off_t *size)
{
    unsigned char *buf;
    struct stat st;
    int fd;

    fd = open(in, O_RDONLY | O_BINARY);
    if(fd == -1) {
        printf("open file failed\n");
        return NULL;
    }

    if( fstat(fd, &st) ) {
        printf("stat failed\n");
        return NULL;
    }
    lseek(fd, 0, SEEK_SET);
    *size = st.st_size;
    buf = malloc( st.st_size );
    ssize_t tr = st.st_size;
    unsigned char *p = buf;
    while(tr) {
        ssize_t nb = read(fd, p, tr);
        if(nb < 0) {
            printf("read error: %d\n", nb);
            free(buf);
            buf = NULL;
            break;
        }
        if(nb == 0) break;
        tr -= nb;
        p+= nb;
    }
    close(fd);
    return buf;
}

int main(int argc, char** argv)
{
    int ret = -1;
    off_t size = 0;
    unsigned char *buf;

    if( argc < 3) {
        printf("%s <original exe> <patched exe>\n", argv[0]);
        return 0;
    }


    size = 0;
    buf = read_file(argv[1], &size);
    if(!buf)
        return -1;

    if( apply_patches(buf) == 0) {

        if( write_file(argv[2], buf, size) == 0)
            ret = 0;
    }

    free(buf);
    return ret;
}
