#include <stdio.h>



int main(void)
{
	double a = 0.1;
	char str[6] = {0};
	(a > 1)?(strcpy(str, "R")):(strcpy(str, "N")); 
	printf("%s\n", str);
	return 0;
}
