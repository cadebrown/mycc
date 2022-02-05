
/* C std */
extern double atof(char* str);
extern void* malloc(size_t sz);

/* Sum 'X[0:N]' */
double fsum(int N, double* X) {
    typeof (*X) res = 0.0;
    int i;
    for (i = 0; i < N; i += 1) res += X[i];
    return res;
}


int main(int argc, char** argv) {
    int i;
    int N = argc - 1;
    double* X = malloc(N * sizeof(double));
    for (i = 0; i < N; i += 1) {
        X[i] = atof(argv[i + 1]);
    }

    printf("%f\n", fsum(N, X));
    return 0;
}

