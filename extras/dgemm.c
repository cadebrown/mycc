/* dgemm.c - Double General Matrix-Matrix product
 *
 * The interface is a bit odd -- running this should have:
 * M N K [A...] [B...]
 * 
 * where you want to multiply a 'MxN' and a 'NxK' matrix.
 * '[A...]' should contain the rows of A, stacked together
 * '[B...]' should contain the rows of B, stacked together
 * 
 * For example, if I wanted to compute the matrix product 'AB' where:
 * A = [1 2 3]
 *     [4 5 6]
 * 
 * B = [7]
 *     [8]
 *     [9]
 * 
 * I would run:
 * $ ./dgemm 2 3 1  1 2 3 4 5 6  7 8 9
 *     50.000 
 *    122.000 
 * 
 * (you can the correctness on wolfram alpha) 
 * https://www.wolframalpha.com/input/?i=%7B%7B1%2C+2%2C+3%7D%2C+%7B4%2C+5%2C+6%7D%7D*%7B%7B7%7D%2C+%7B8%7D%2C+%7B9%7D%7D
 * 
 * @author: Cade Brown <cade@utk.edu>
 */

/* C std */
extern int atoi(char* str);
extern double atof(char* str);
extern void* malloc(size_t sz);

/* DGEMM: multiplies two matrices (in row-major order)
 *
 * A (M, N) (stride: lda)
 * B (N, K) (stride: ldb)
 * C (M, K) (stride: ldc)
 * 
 * Computes:
 *   C = AB
 */
void dgemm(int M, int N, int K, double* A, int lda, double* B, int ldb, double* C, int ldc) {
    int i;
    int j;
    int k;
    for (i = 0; i < M * K; i += 1) C[i] = 0.0;
    for (i = 0; i < M; i += 1) {
        for (k = 0; k < N; k += 1) {
            for (j = 0; j < K; j += 1) {
                int ij = i * ldc + j;
                C[ij] += A[i * lda + k] * B[k * ldb + j];
            }
        }
    }
    return;
}

int main(int argc, char** argv) {
    int i;
    int j;
    if (argc < 1 + 3) {
        printf("usage: %s M N K [A...] [B...] (missing sizes)\n", argv[0]);
        return 1;
    }

    /* Read sizes */
    int M = atoi(argv[1]);
    int N = atoi(argv[2]);
    int K = atoi(argv[3]);
    if (argc != 1 + 3 + M * N + N * K) {
        printf("usage: %s M N K [A:%i] [B:%i] (missing elements!)\n", argv[0], M*N, N*K);
        return 1;
    }

    /* Allocate matrices */
    double* X = malloc(M * N * sizeof(double));
    double* Y = malloc(N * K * sizeof(double));
    double* Z = malloc(M * K * sizeof(double));

    /* Load matrices */
    for (i = 0; i < M * N; i += 1) X[i] = atof(argv[1 + 3 + i]);
    for (i = 0; i < N * K; i += 1) Y[i] = atof(argv[1 + 3 + M * N + i]);

    /* Do matrix computation */
    dgemm(M, N, K, X, N, Y, K, Z, K);

    for (i = 0; i < M; i += 1) {
        for (j = 0; j < K; j += 1) {
            printf("%10.3f ", Z[K * i + j]);
        }
        printf("\n");
    }
    return 0;
}
