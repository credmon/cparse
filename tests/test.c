
#include <stdio.h>

int main(void)
{
   int a = 5;
   unsigned int b;

   b = 6;

   /* a comment */
   // another comment
   /* a third
      comment */
  #if 0
   printf("unused code\n");
  #endif
   printf("a: %d\n", a);
   printf("b: %d\n", b);

   return 0;
}
