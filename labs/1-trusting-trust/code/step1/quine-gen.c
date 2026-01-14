// convert the contents of stdin to their ASCII values (e.g., 
// '\n' = 10) and spit out the <prog> array used in Figure 1 in
// Thompson's paper.
#include <stdio.h>
#include <string.h>

int main(void) { 
    // put your code here.
    char buffer[100000];
    char line[1000];
    printf("char prog[] = {\n");
    while (fgets(line, sizeof(line), stdin) != NULL) { // read the input file, save it into buffer
        strcat(buffer, line);
    }
    for (int i= 0; buffer[i]; i++) { // print out buffer content in ascii value 
        printf("\t%d,%c", buffer[i], (i+1)%8==0 ? '\n' : ' ');
    }
    printf("0 };\n");
    for (int i= 0; buffer[i]; i++) { // print out the whole code
        printf("%c", buffer[i]);
    }

	return 0;
}
