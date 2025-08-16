#include <omp.h>
#include "utils.h"

long long fatorial(int n) {
    long long res = 1;
    for (int i = 2; i <= n; i++) res *= i;
    return res;
}

// Gera a i-ésima permutação em ordem lexicográfica
void permutacao_por_indice(int *rota, int n, long long idx) {
    int usados[100] = {0};
    for (int pos = 0; pos < n; pos++) {
        long long fat = fatorial(n - pos - 1);
        int k = idx / fat;
        idx %= fat;
        int count = -1;
        for (int j = 0; j < n; j++) {
            if (!usados[j]) count++;
            if (count == k) {
                rota[pos] = j;
                usados[j] = 1;
                break;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    Cidade cidades[100];
    int n;

    if (argc < 2) {
        printf("Uso: %s <arquivo.tsp>\n", argv[0]);
        return 1;
    }

    ler_instancia(argv[1], cidades, &n);

    long long total_perm = fatorial(n - 1); // fixando a cidade inicial
    double melhor = DBL_MAX;

    #pragma omp parallel
    {
        double melhor_local = DBL_MAX;
        int rota[100];

        #pragma omp for schedule(dynamic)
        for (long long i = 0; i < total_perm; i++) {
            rota[0] = 0; // fixa a cidade inicial
            permutacao_por_indice(rota + 1, n - 1, i);
            double custo = calcula_custo(cidades, rota, n);
            if (custo < melhor_local)
                melhor_local = custo;
        }

        #pragma omp critical
        {
            if (melhor_local < melhor)
                melhor = melhor_local;
        }
    }

    printf("Melhor custo encontrado: %.2f\n", melhor);
    return 0;
}
