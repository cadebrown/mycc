
/* C std */
extern double atof(char* str);
extern double sin(double x);

int main(int argc, char** argv) {
    if (argc != 1 + 1) {
        printf("usage: %s [x]\n", argv[0]);
        return 1;
    }
    printf("%f\n", sin(atof(argv[1])));
    return 0;
}

