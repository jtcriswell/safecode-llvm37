/* Use a message queue to send a pointer which is double freed.
   f1() builds the message queue; f2() reads the data and causes the
   double free. */

#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <string.h>

struct M
{
  long int type;
  char bytes[sizeof(char*)];
};

int mid;

void f1()
{
  char *a;
  struct M msg;

  a = malloc(1000);
  msg.type = 1;
  memcpy(msg.bytes, &a, sizeof(char*));
  mid = msgget(IPC_PRIVATE, 0600);
  msgsnd(mid, &msg, sizeof(char*), 0);
  free(a);
}

void f2()
{
  char *b;
  struct M msg;

  msgrcv(mid, &msg, sizeof(char*), 0, 0);
  memcpy(&b, msg.bytes, sizeof(char*));
  msgctl(mid, IPC_RMID, NULL);
  free(b);
}

int main()
{
  f1();
  f2();
  return 0;
}
