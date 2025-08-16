#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <time.h>

typedef struct {
    double x, y;
} Cidade;

// Calcula distância entre duas cidades
double distancia(Cidade a, Cidade b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

// Lê arquivo TSP
int ler_instancia(const char *arquivo, Cidade *cidades, int *n) {
    FILE *fp = fopen(arquivo, "r");
    if (!fp) {
        return -1;
    }
    
    char linha[256];
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
        }
        if (strstr(linha, "NODE_COORD_SECTION")) break;
    }
    
    if (*n <= 0 || *n > 1000) {
        fclose(fp);
        return -1;
    }
    
    // Ler coordenadas
    for (int i = 0; i < *n; i++) {
        int id;
        if (fscanf(fp, "%d %lf %lf", &id, &cidades[i].x, &cidades[i].y) != 3) {
            fclose(fp);
            return -1;
        }
        
        // Verificar coordenadas válidas
        if (!isfinite(cidades[i].x) || !isfinite(cidades[i].y)) {
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

// ALGORITMO 0: Força bruta com métricas
double forca_bruta(Cidade *cidades, int n, int rank, int size, int *melhor_rota) {
    int *rota = malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) {
        rota[i] = i;
        melhor_rota[i] = i;
    }
    
    double melhor_custo = DBL_MAX;
    unsigned long long count = 0;
    unsigned long long trabalho_local = 0;
    
    do {
        if (count % size == rank) {
            double custo = calcula_custo(cidades, rota, n);
            if (custo < melhor_custo) {
                melhor_custo = custo;
                for (int i = 0; i < n; i++) melhor_rota[i] = rota[i];
            }
            trabalho_local++;
        }
        count++;
    } while (next_permutation(rota + 1, n - 1));
    
    // Coletar estatísticas de trabalho
    unsigned long long trabalho_total, trabalho_max, trabalho_min;
    MPI_Reduce(&trabalho_local, &trabalho_total, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&trabalho_local, &trabalho_max, 1, MPI_UNSIGNED_LONG_LONG, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&trabalho_local, &trabalho_min, 1, MPI_UNSIGNED_LONG_LONG, MPI_MIN, 0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("Permutacoes processadas: %llu\n", trabalho_total);
        printf("Distribuicao: min=%llu, max=%llu, variacao=%.1f%%\n", 
               trabalho_min, trabalho_max, 
               100.0 * (trabalho_max - trabalho_min) / trabalho_max);
    }
    
    free(rota);
    return melhor_custo;
}

// ALGORITMO 1: Nearest Neighbor com proteções
double nearest_neighbor(Cidade *cidades, int n, int rank, int *rota) {
    if (n <= 0 || n > 1000) return DBL_MAX;
    
    int *visitado = calloc(n, sizeof(int));
    if (!visitado) return DBL_MAX;
    
    int start = rank % n;
    
    rota[0] = start;
    visitado[start] = 1;
    
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

// ALGORITMO 2: 2-opt com proteções
double two_opt(Cidade *cidades, int n, int rank, int size, int *melhor_rota) {
    if (n <= 0 || n > 1000) return DBL_MAX;
    
    srand(time(NULL) + rank * 1000);
    
    double melhor_custo = DBL_MAX;
    int *rota_temp = malloc(n * sizeof(int));
    if (!rota_temp) return DBL_MAX;
    
    int iteracoes_locais = 0;
    
    // CORREÇÃO: Se rank >= n, usar rank % n para garantir que todos os processos trabalhem
    int tentativas = 0;
    
    // Cada processo tenta múltiplas soluções iniciais
    for (int start = rank % n; tentativas < 5; start = (start + size) % n, tentativas++) {
        // Parar se já deu uma volta completa nas cidades
        if (tentativas > 0 && start == rank % n) break;
        
        double custo_nn = nearest_neighbor(cidades, n, start, rota_temp);
        if (!isfinite(custo_nn)) continue;
        
        // Aplicar 2-opt múltiplas vezes
        for (int iteracao = 0; iteracao < 3; iteracao++) {
            int melhorou = 1;
            int tentativas_2opt = 0;
            
            while (melhorou && tentativas_2opt < 100) {
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
            
            // Perturbação
            if (iteracao < 2) {
                for (int k = 0; k < 5; k++) {
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
        double custo_backup = nearest_neighbor(cidades, n, rank % n, melhor_rota);
        if (isfinite(custo_backup)) {
            melhor_custo = custo_backup;
        }
    }
    
    // Coletar estatísticas de trabalho
    int iteracoes_total, iteracoes_max, iteracoes_min;
    MPI_Reduce(&iteracoes_locais, &iteracoes_total, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&iteracoes_locais, &iteracoes_max, 1, MPI_INT, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&iteracoes_locais, &iteracoes_min, 1, MPI_INT, MPI_MIN, 0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        printf("Iteracoes 2-opt: %d total, min=%d, max=%d\n", 
               iteracoes_total, iteracoes_min, iteracoes_max);
    }
    
    free(rota_temp);
    return isfinite(melhor_custo) ? melhor_custo : DBL_MAX;
}

#endif