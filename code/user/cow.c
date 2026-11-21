#include <stdio.h>
#include <ulib.h>

static volatile int data __attribute__((aligned(4096))) = 17;
static volatile int exit_status __attribute__((aligned(4096))) = -1;

int main(void)
{
    int writer = fork();
    assert(writer >= 0);
    if (writer == 0)
    {
        assert(data == 17);
        data = 29;
        yield();
        assert(data == 29);
        return 42;
    }

    int reader = fork();
    assert(reader >= 0);
    if (reader == 0)
    {
        for (int i = 0; i < 8; i++)
        {
            assert(data == 17 && exit_status == -1);
            yield();
        }
        return 0;
    }

    yield();
    assert(data == 17);
    data = 31;
    assert(waitpid(writer, (int *)&exit_status) == 0);
    assert(data == 31 && exit_status == 42);
    assert(waitpid(reader, NULL) == 0);
    cprintf("cow pass: fork isolation and wait copyout.\n");
    return 0;
}
