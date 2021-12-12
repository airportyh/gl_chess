int readFile(char *filename, char **fileContents) {
    // https://stackoverflow.com/a/14002993

    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        set_error(1, "%s", strerror(errno));
        *fileContents = NULL;
        return 1;
    }
    CALL(fseek(f, 0, SEEK_END));
    long fsize = ftell(f);
    CALL(fseek(f, 0, SEEK_SET));  /* same as rewind(f); */

    char *string = malloc(fsize + 1);
    int read = fread(string, 1, fsize, f);
    CALL(fclose(f));

    string[read] = 0;
    *fileContents = string;
    return 0;
}