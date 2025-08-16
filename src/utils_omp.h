#ifndef UTILS_OPENMP_H
#define UTILS_OPENMP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include <limits.h>
#include <omp.h>

typedef struct {
    double x, y;
} Cidade;

// Calcula distância entre duas cidades
double distancia(Cidade a, Cidade b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

// Nova função: descobrir tamanho da instância sem alocar memória
int descobrir_tamanho_instancia(const char *arquivo, int *n) {
    FILE *fp = fopen(arquivo, "r");
    if (!fp) {
        return -1;
    }
    
    char linha[512];
    *n = 0;
    
    // Encontrar DIMENSION
    while (fgets(linha, sizeof(linha), fp)) {
        if (strncmp(linha, "DIMENSION", 9) == 0) {
            if (sscanf(linha, "DIMENSION: %d", n) != 1 && 
                sscanf(linha, "DIMENSION : %d", n) != 1 &&
                sscanf(linha, "DIMENSION:%d", n) != 1) {
                fclose(fp);
                return -1;
            }
            break;
        }
    }
    
    fclose(fp);
    
    // Validação do tamanho
    if (*n <= 0) {
        printf("Erro: DIMENSION inválida (%d)\n", *n);
        return -1;
    }
    
    if (*n > 100000) {  // Limite prático para evitar problemas de memória
        printf("Aviso: Instancia muito grande (%d cidades). Considere usar menos threads.\n", *n);
    }
    
    return 0;
}

// Lê arquivo TSP com alocação dinâmica
int ler_instancia(const char *arquivo, Cidade *cidades, int *n) {
    FILE *fp = fopen(arquivo, "r");
    if (!fp) {
        return -1;
    }
    
    char linha[512];
    int dimensao_encontrada = 0;
    
    // Encontrar DIMENSION (já deve ter sido encontrada antes, mas double-check)
    while (fgets(linha, sizeof(linha), fp)) {
        if (strncmp(linha, "DIMENSION", 9) == 0) {
            int dim_temp;
            if (sscanf(linha, "DIMENSION: %d", &dim_temp) == 1 || 
                sscanf(linha, "DIMENSION : %d", &dim_temp) == 1 ||
                sscanf(linha, "DIMENSION:%d", &dim_temp) == 1) {
                if (dim_temp != *n) {
                    printf("Erro: Inconsistencia no DIMENSION (%d vs %d)\n", *n, dim_temp);
                    fclose(fp);
                    return -1;
                }
                dimensao_encontrada = 1;
            }
        }
        if (strstr(linha, "NODE_COORD_SECTION")) break;
    }
    
    if (!dimensao_encontrada) {
        printf("Erro: DIMENSION nao encontrada no arquivo\n");
        fclose(fp);
        return -1;
    }
    
    // Ler coordenadas
    for (int i = 0; i < *n; i++) {
        int id;
        if (fscanf(fp, "%d %lf %lf", &id, &cidades[i].x, &cidades[i].y) != 3) {
            printf("Erro ao ler coordenadas da cidade %d\n", i + 1);
            fclose(fp);
            return -1;
        }
        
        // Verificar coordenadas válidas
        if (!isfinite(cidades[i].x) || !isfinite(cidades[i].y)) {
            printf("Erro: Coordenadas invalidas na cidade %d: (%.2f, %.2f)\n", 
                   i + 1, cidades[i].x, cidades[i].y);
            fclose(fp);
            return -1;
        }
    }
    
    fclose(fp);
    return 0;
}

// Calcula custo de uma rota
double calcula_custo(Cidade *cidades, int *rota, int n) {
    double custo = 0.0;
    for (int i = 0; i < n - 1; i++)
        custo += distancia(cidades[rota[i]], cidades[rota[i + 1]]);
    custo += distancia(cidades[rota[n - 1]], cidades[rota[0]]);
    return custo;
}

// Próxima permutação
int next_permutation(int *p, int n) {
    int i = n - 2;
    while (i >= 0 && p[i] >= p[i + 1]) i--;
    if (i < 0) return 0;
    
    int j = n - 1;
    while (p[j] <= p[i]) j--;
    
    int temp = p[i]; p[i] = p[j]; p[j] = temp;
    
    for (int a = i + 1, b = n - 1; a < b; a++, b--) {
        temp = p[a]; p[a] = p[b]; p[b] = temp;
    }
    return 1;
}

// ALGORITMO 0: Força bruta versão OpenMP
double forca_bruta_openmp(Cidade *cidades, int n, int thread_id, int num_threads, int *melhor_rota) {
    int *rota = malloc(n * sizeof(int));

    if (!rota) {
        printf("Thread %d: Erro ao alocar memoria para rota\n", thread_id);
        return DBL_MAX;
    }
    
    for (int i = 0; i < n; i++) {
        rota[i] = i;
        melhor_rota[i] = i;
    }
    
    double melhor_custo = DBL_MAX;
    unsigned long long count = 0;
    unsigned long long trabalho_local = 0;
    
    do {
        if (count % num_threads == thread_id) {
            double custo = calcula_custo(cidades, rota, n);
            if (custo < melhor_custo) {
                melhor_custo = custo;
                for (int i = 0; i < n; i++) melhor_rota[i] = rota[i];
            }
            trabalho_local++;
        }
        count++;
    } while (next_permutation(rota + 1, n - 1));
    
    #pragma omp critical
    {
        printf("Thread %d processou %llu permutações\n", thread_id, trabalho_local);
    }
    
    free(rota);
    return melhor_custo;
}

// ALGORITMO 1: Nearest Neighbor com proteções e escalabilidade
double nearest_neighbor(Cidade *cidades, int n, int start, int *rota) {
    if (n <= 0) return DBL_MAX;
    
    int *visitado = calloc(n, sizeof(int));
    if (!visitado) {
        printf("Erro ao alocar memoria para visitado (%d cidades)\n", n);
        return DBL_MAX;
    }
    
    rota[0] = start % n;
    visitado[rota[0]] = 1;
    
    for (int i = 1; i < n; i++) {
        int atual = rota[i-1];
        int proximo = -1;
        double min_dist = DBL_MAX;
        
        for (int j = 0; j < n; j++) {
            if (!visitado[j]) {
                double dist = distancia(cidades[atual], cidades[j]);
                if (isfinite(dist) && dist < min_dist) {
                    min_dist = dist;
                    proximo = j;
                }
            }
        }
        
        if (proximo == -1) {
            // Não encontrou próxima cidade válida
            free(visitado);
            return DBL_MAX;
        }
        
        rota[i] = proximo;
        visitado[proximo] = 1;
    }
    
    free(visitado);
    double custo = calcula_custo(cidades, rota, n);
    return isfinite(custo) ? custo : DBL_MAX;
}

// ALGORITMO 2: 2-opt versão OpenMP
double two_opt_openmp(Cidade *cidades, int n, int thread_id, int num_threads, int *melhor_rota) {
    if (n <= 0) return DBL_MAX;
    
    srand(time(NULL) + thread_id * 1000);
    
    double melhor_custo = DBL_MAX;
    int *rota_temp = malloc(n * sizeof(int));
    if (!rota_temp) {
        printf("Thread %d: Erro ao alocar memoria para rota_temp (%d cidades)\n", thread_id, n);
        return DBL_MAX;
    }
    
    int iteracoes_locais = 0;
    
    // Ajustar número de tentativas baseado no tamanho da instância
    int max_tentativas = (n < 100) ? 10 : (n < 1000) ? 5 : 3;
    int max_iteracoes = (n < 100) ? 5 : (n < 1000) ? 3 : 2;
    int max_2opt = (n < 100) ? 200 : (n < 1000) ? 100 : 50;
    
    int tentativas = 0;
    
    // Cada thread tenta múltiplas soluções iniciais
    for (int start = thread_id % n; tentativas < max_tentativas; start = (start + num_threads) % n, tentativas++) {
        // Parar se já deu uma volta completa nas cidades
        if (tentativas > 0 && start == thread_id % n) break;
        
        double custo_nn = nearest_neighbor(cidades, n, start, rota_temp);
        if (!isfinite(custo_nn)) continue;
        
        // Aplicar 2-opt múltiplas vezes
        for (int iteracao = 0; iteracao < max_iteracoes; iteracao++) {
            int melhorou = 1;
            int tentativas_2opt = 0;
            
            while (melhorou && tentativas_2opt < max_2opt) {
                melhorou = 0;
                iteracoes_locais++;
                tentativas_2opt++;
                
                for (int i = 1; i < n - 1 && !melhorou; i++) {
                    for (int j = i + 1; j < n && !melhorou; j++) {
                        if (j - i == 1) continue;
                        
                        int prev_i = (i - 1 + n) % n;
                        int next_j = (j + 1) % n;
                        
                        double custo_atual = distancia(cidades[rota_temp[prev_i]], cidades[rota_temp[i]]) +
                                           distancia(cidades[rota_temp[j]], cidades[rota_temp[next_j]]);
                        
                        double custo_novo = distancia(cidades[rota_temp[prev_i]], cidades[rota_temp[j]]) +
                                          distancia(cidades[rota_temp[i]], cidades[rota_temp[next_j]]);
                        
                        if (isfinite(custo_atual) && isfinite(custo_novo) && 
                            custo_novo < custo_atual - 1e-9) {
                            
                            for (int a = i, b = j; a < b; a++, b--) {
                                int temp = rota_temp[a];
                                rota_temp[a] = rota_temp[b];
                                rota_temp[b] = temp;
                            }
                            melhorou = 1;
                        }
                    }
                }
            }
            
            double custo = calcula_custo(cidades, rota_temp, n);
            if (isfinite(custo) && custo < melhor_custo) {
                melhor_custo = custo;
                for (int i = 0; i < n; i++) melhor_rota[i] = rota_temp[i];
            }
            
            // Perturbação adaptativa ao tamanho
            if (iteracao < max_iteracoes - 1) {
                int num_perturbacoes = (n < 50) ? 5 : (n < 200) ? 3 : 2;
                for (int k = 0; k < num_perturbacoes; k++) {
                    int a = 1 + rand() % (n-1);
                    int b = 1 + rand() % (n-1);
                    int temp = rota_temp[a];
                    rota_temp[a] = rota_temp[b];
                    rota_temp[b] = temp;
                }
            }
        }
    }
    
    // Se não encontrou nenhuma solução válida, usar nearest neighbor simples
    if (melhor_custo == DBL_MAX) {
        double custo_backup = nearest_neighbor(cidades, n, thread_id % n, melhor_rota);
        if (isfinite(custo_backup)) {
            melhor_custo = custo_backup;
        }
    }
    
    #pragma omp critical
    {
        printf("Thread %d executou %d iteracoes 2-opt\n", thread_id, iteracoes_locais);
    }
    
    free(rota_temp);
    return isfinite(melhor_custo) ? melhor_custo : DBL_MAX;
}

#endif