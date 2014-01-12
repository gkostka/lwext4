
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>

#include <lcd_log.h>

int _getpid(void)
{
    return 1;
}

int _kill(int pid, int sig)
{
    errno = EINVAL;
    return -1;
}

void _exit (int status)
{
    _kill(status, -1);
    while (1) {}                /* Make sure we hang here */
}

int _write(int file, char *ptr, int len)
{
    int todo;

    for (todo = 0; todo < len; todo++)
    {
        __io_putchar( *ptr++ );
    }

    /* Implement your write code here, this is used by puts and printf for example */
    return len;
}

caddr_t _sbrk(int incr)
{
    extern char __heap_end asm("__heap_end");
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == 0)
        heap_end = &__heap_end;

    prev_heap_end = heap_end;
    if ((unsigned int)(heap_end + incr) > (0x20000000 + 0x20000))
    {
        abort();
        errno = ENOMEM;
        return (caddr_t) -1;
    }

    heap_end += incr;

    return (caddr_t) prev_heap_end;
}

int _close(int file)
{
    return -1;
}


int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    return 1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

int _read(int file, char *ptr, int len)
{
    return 0;
}

int _open(char *path, int flags, ...)
{
    /* Pretend like we always fail */
    return -1;
}

int _wait(int *status)
{
    errno = ECHILD;
    return -1;
}

int _unlink(char *name)
{
    errno = ENOENT;
    return -1;
}


int _stat(char *file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _link(char *old, char *new)
{
    errno = EMLINK;
    return -1;
}

int _fork(void)
{
    errno = EAGAIN;
    return -1;
}

int _execve(char *name, char **argv, char **env)
{
    errno = ENOMEM;
    return -1;
}
