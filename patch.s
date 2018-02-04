
###################
# definitions

.ifdef TARGET

    .if TARGET==1   /* interstate76 */

        .set PATCH1_OFS, 0x499b25
        .set PATCH2_OFS, 0x4C25C0
        .set PATCH3_OFS, -1  /* 0x431F60 */

        /* PATCH4: from import table */
        .set pGetTickCount, 0x4bc100
        .set pGetModuleHandle, 0x4bc0ec
        .set pGetProcAddress, 0x4bc0a0

        .set PATCH5_OFS, 0x4039B8

    .elseif TARGET==2  /* Nitro Pack */

        .set PATCH1_OFS, 0x49AE45
        .set PATCH2_OFS, 0x4F3550
        .set PATCH3_OFS, -1 /* ?? */

        /* PATCH4: from import table */
        .set pGetTickCount, 0x4C111C
        .set pGetModuleHandle, 0x4C1118
        .set pGetProcAddress, 0x4C1138

        .set PATCH5_OFS, 0x432805


    .else
        .print "Wrong Target set( 1=Interstate76, 2=NitroPack)"
        .err
    .endif
.else
.print "No Target set(1=Interstate76, 2=NitroPack)"
.err
.endif


##### common definitions ##########

.set TargetDelay, 42   # 30fps: 1/30 = ~33.333ms per frame, 25fps: 1/25 => 40ms, 24fps: => ~41,66ms

/* "allocated" in patch2 */
.set pSleep, PATCH2_OFS + 0x2C
.set LastTick, PATCH2_OFS + 0x30



##### global var ##############

    .data
    .global _patches
_patches:
    .int patch1, patch2, patch3, patch4, patch5, patch6, 0


####### PATCH 1 ##########################
patch1:
    .int PATCH1_OFS
    .int p1_code
    .int p1_end - p1_code
    .int 0
    .asciz "patch out cpu measurement func(uses privileged instructions to access PIT)"

p1_code:
    mov $200, %eax     # replace:  call <fn> (measure cpu speed)
p1_end:


####### PATCH 2 ###########################
patch2:
    .int PATCH2_OFS
    .int p2_data
    .int p2_end - p2_data
    .int 0
    .asciz "shorten error msg to allocate space for two DWORDs in .data section"

p2_data:
    # orig "Unable Allocate Virtual Memory, aborting Program..." # strip 8 bytes
    .asciz "Unable Allocate Virtual Memory, aborting..."
    .int 0
    .int 0
p2_end:


####### PATCH 3 ##########################
patch3:
    .int PATCH3_OFS
    .int p3_code
    .int p3_end - p3_code
    .int 0
    .asciz "disable write_videolog_txt() func, crashes wine with -d3d"

p3_code:
    ret
p3_end:



###### PATCH 4 ######################################
patch4:
    .int 0                  # let patcher find some free space for it
    .int p4_code
    .int p4_end - p4_code
    .int prepare_patch6     # tell patch6 where our func ended up
    .asciz "add draw_delay() func"

p4_code:
    push %ebx # save
    call *pGetTickCount
    mov $LastTick, %edx
    mov (%edx), %ecx
    test %ecx, %ecx
    jz out
    mov %eax, %ebx
    sub %ecx, %ebx
    cmp $TargetDelay-3, %ebx   # avoid really short calls to Sleep()
    jg out
    mov pSleep, %eax
    test %eax, %eax
    jz fetch_func

do_sleep:
    mov $TargetDelay, %edx
    sub %ebx, %edx
    push %edx
    call *%eax
    call *pGetTickCount
    mov $LastTick, %edx
out:
    mov %eax,(%edx)
    pop %ebx # restore
jmp_cont:
    jmp 0x444444  # fixed up later

str_kernel32:
    .asciz "kernel32"
str_sleep:
    .asciz "Sleep"
fetch_func:
    push %esi       # save
    call ss
ss:
    pop %esi
    sub $ss-str_kernel32, %esi
    push %esi
    call *pGetModuleHandle

    add $str_sleep-str_kernel32,%esi
    push %esi
    push %eax
    call *pGetProcAddress
    pop %esi        # restore
    test %eax, %eax
    jz out
    mov %eax, (pSleep)
    jmp do_sleep
p4_end:

    .text
prepare_patch6:
    mov patch4, %eax
    add $jmp_cont-p4_code, %eax    # VA at jmp ins
    mov %eax, patch6
    mov $0, %eax
    ret


###### PATCH 5 ###############################################
    .data
patch5:
    .int PATCH5_OFS   # call <fn>, replace with call to draw_delay, later chain draw_delay() back to fn
    .int 0
    .int 0
    .int patch_call
    .asciz "hook draw_delay() into frame loop"
target_fn:
    .int 0

    .text
patch_call:
    mov 4(%esp), %eax       # code ptr
    mov patch5, %edx        # VA
    mov patch4, %ecx        # VA of draw_delay()
    push %ecx
    push %edx
    push %eax
    call _set_relative_target
    add $12, %esp
    mov %eax, target_fn

    mov $0, %eax
    ret


###### PATCH 6 ###############################################

    .data
patch6:
    .int -1 # will be set by patch4
    .int 0
    .int 0
    .int patch_jmp
    .asciz "chain draw_delay() back to intercepted fn"

    .text
patch_jmp:
    mov 4(%esp), %eax               # code ptr
    mov patch6, %edx
    mov target_fn, %ecx

    push %ecx
    push %edx
    push %eax
    call _set_relative_target
    add $12, %esp

    mov $0, %eax
    ret
