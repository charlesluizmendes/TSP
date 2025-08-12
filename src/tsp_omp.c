#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <omp.h>  // Biblioteca para programação paralela com OpenMP

// Estrutura para armazenar coordenadas x,y de cada cidade
typedef struct { double x, y; } Coord;

// ---------- Utilidades ----------

/**
 * Calcula distância euclidiana entre duas cidades e arredonda para inteiro
 * @param c: array de coordenadas das cidades
 * @param i: índice da primeira cidade
 * @param j: índice da segunda cidade
 * @return: distância euclidiana arredondada
 */
static inline int euc_2d(const Coord *c, int i, int j) {
    double dx = c[i].x - c[j].x;  // Diferença em x
    double dy = c[i].y - c[j].y;  // Diferença em y
    return (int) lround(sqrt(dx*dx + dy*dy));  // Distância euclidiana arredondada
}

/**
 * Calcula o comprimento total de um tour (caminho completo)
 * @param coord: coordenadas das cidades
 * @param tour: array com a ordem das cidades visitadas
 * @param n: número de cidades
 * @return: comprimento total do tour
 */
int tour_length(const Coord *coord, const int *tour, int n) {
    long sum = 0;
    for (int i = 0; i < n; ++i) {
        int a = tour[i];           // Cidade atual
        int b = tour[(i+1)%n];     // Próxima cidade (volta para 0 no final)
        sum += euc_2d(coord, a, b); // Adiciona distância entre as cidades
    }
    return (int)sum;
}

// ---------- TSPLIB (EUC_2D) ----------

/**
 * Lê arquivo no formato TSPLIB95 com coordenadas euclidianas 2D
 * @param path: caminho para o arquivo .tsp
 * @param out: ponteiro para array de coordenadas (será alocado)
 * @param n: ponteiro para número de cidades (será preenchido)
 * @return: 1 se sucesso, 0 se erro
 */
int read_tsplib_euc2d(const char *path, Coord **out, int *n) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "Erro ao abrir %s\n", path); return 0; }

    char line[512], type[64] = {0};
    int dim = -1;
    int in_coords = 0;

    // Primeiro passe: ler cabeçalho até NODE_COORD_SECTION
    while (fgets(line, sizeof(line), f)) {
        // Procura pela dimensão do problema (número de cidades)
        if (sscanf(line, "DIMENSION : %d", &dim) == 1) continue;
        if (sscanf(line, "DIMENSION: %d", &dim) == 1) continue;
        
        // Procura pelo tipo de peso das arestas
        if (sscanf(line, "EDGE_WEIGHT_TYPE : %63s", type) == 1) continue;
        if (sscanf(line, "EDGE_WEIGHT_TYPE: %63s", type) == 1) continue;
        
        // Procura pela seção de coordenadas
        if (strstr(line, "NODE_COORD_SECTION")) { in_coords = 1; break; }
    }
    
    // Validações do cabeçalho
    if (dim <= 0) { fprintf(stderr, "DIMENSION invalida\n"); fclose(f); return 0; }
    if (strcmp(type, "EUC_2D") != 0) {
        fprintf(stderr, "EDGE_WEIGHT_TYPE=%s nao suportado (use EUC_2D)\n", type);
        fclose(f); return 0;
    }
    if (!in_coords) { fprintf(stderr, "NODE_COORD_SECTION nao encontrado\n"); fclose(f); return 0; }

    // Aloca memória para coordenadas (1-based indexing, por isso dim+1)
    Coord *coord = (Coord*)calloc(dim+1, sizeof(Coord));
    if (!coord) { fclose(f); return 0; }

    // Lê coordenadas das cidades
    int id; double x,y; int count=0;
    char *ok;  // Variável não utilizada (possível remnant de código anterior)
    while (count < dim && fgets(line, sizeof(line), f)) {
        if (strstr(line, "EOF")) break;  // Fim do arquivo TSPLIB
        
        // Lê linha no formato: ID X Y
        if (sscanf(line, "%d %lf %lf", &id, &x, &y) == 3) {
            if (id >=1 && id <= dim) { 
                coord[id].x = x; 
                coord[id].y = y; 
                count++; 
            }
        }
    }
    fclose(f);
    
    // Verifica se leu todas as coordenadas
    if (count != dim) { fprintf(stderr, "Coord insuficientes (%d/%d)\n", count, dim); free(coord); return 0; }
    *out = coord; *n = dim; return 1;
}

// ---------- Heurísticas (NN + 2-Opt) ----------

/**
 * Algoritmo Nearest Neighbor (Vizinho Mais Próximo)
 * Constrói um tour começando de uma cidade específica
 * @param coord: coordenadas das cidades
 * @param n: número de cidades
 * @param start: cidade inicial
 * @param tour: array onde será armazenado o tour resultante
 */
void nearest_neighbor(const Coord *coord, int n, int start, int *tour) {
    char *used = (char*)calloc(n+1, 1);  // Array para marcar cidades visitadas
    int cur = start;                      // Cidade atual
    
    // Inicializa tour
    for (int i=0;i<n;i++) tour[i]=0;
    
    // Constrói tour visitando sempre a cidade mais próxima não visitada
    for (int i=0;i<n;i++) {
        tour[i] = cur;      // Adiciona cidade atual ao tour
        used[cur] = 1;      // Marca como visitada
        
        // Encontra próxima cidade mais próxima não visitada
        int best=-1, bestd=INT_MAX;
        for (int j=1;j<=n;j++) if(!used[j]) {
            int d = euc_2d(coord, cur, j);
            if (d < bestd) { bestd=d; best=j; }
        }
        cur = (best==-1)? start : best;  // Próxima cidade (ou volta ao início se terminou)
    }
    free(used);
}

/**
 * Tenta uma iteração do algoritmo 2-Opt
 * Testa todas as possíveis trocas de segmentos para melhorar o tour
 * @param coord: coordenadas das cidades
 * @param tour: tour atual (será modificado se houver melhoria)
 * @param n: número de cidades
 * @return: 1 se houve melhoria, 0 caso contrário
 */
int try_2opt_once(const Coord *coord, int *tour, int n) {
    int improved = 0;
    int old = tour_length(coord, tour, n);  // Comprimento atual
    
    // Testa todas as possíveis trocas de segmentos
    for (int i=1; i<n-1; ++i) {
        for (int k=i+1; k<n; ++k) {
            // Inverte segmento [i..k]
            int len = k - i + 1;
            for (int a=0; a<len/2; ++a) {
                int tmp = tour[i+a];
                tour[i+a] = tour[k-a];
                tour[k-a] = tmp;
            }
            
            // Verifica se melhorou
            int newlen = tour_length(coord, tour, n);
            if (newlen < old) { 
                improved = 1; 
                old = newlen; 
            }
            else {
                // Desfaz a troca se não melhorou
                for (int a=0; a<len/2; ++a) {
                    int tmp = tour[i+a];
                    tour[i+a] = tour[k-a];
                    tour[k-a] = tmp;
                }
            }
        }
    }
    return improved;
}

/**
 * Algoritmo 2-Opt completo
 * Aplica melhorias locais até não conseguir mais melhorar
 * @param coord: coordenadas das cidades
 * @param tour: tour inicial (será otimizado)
 * @param n: número de cidades
 */
void two_opt(const Coord *coord, int *tour, int n) {
    // Loop até não haver mais melhorias
    for (;;) {
        if (!try_2opt_once(coord, tour, n)) break;
    }
}

/**
 * Resolve TSP para um ponto de partida específico
 * Combina Nearest Neighbor + 2-Opt
 * @param coord: coordenadas das cidades
 * @param n: número de cidades
 * @param start: cidade inicial
 * @param tour_out: array onde será armazenado o melhor tour
 * @return: comprimento do melhor tour encontrado
 */
int solve_from_start(const Coord *coord, int n, int start, int *tour_out) {
    nearest_neighbor(coord, n, start, tour_out);  // Constrói tour inicial
    two_opt(coord, tour_out, n);                  // Otimiza com 2-Opt
    return tour_length(coord, tour_out, n);       // Retorna comprimento final
}

// ---------- Sequencial (para speedup) ----------

/**
 * Resolve TSP sequencialmente testando todos os pontos de partida
 * Usado para calcular speedup da versão paralela
 * @param coord: coordenadas das cidades
 * @param n: número de cidades
 * @param best_tour_seq: array onde será armazenado o melhor tour
 * @param best_len_seq: ponteiro onde será armazenado o melhor comprimento
 * @return: tempo de execução em segundos
 */
double solve_tsp_sequencial(const Coord *coord, int n, int *best_tour_seq, int *best_len_seq) {
    double t0 = omp_get_wtime();  // Tempo inicial (usa timer OpenMP)
    int *tour = (int*)malloc(n*sizeof(int));
    int best_len = INT_MAX;
    
    // Testa todos os pontos de partida sequencialmente
    for (int s=1; s<=n; ++s) {
        int len = solve_from_start(coord, n, s, tour);
        if (len < best_len) {
            best_len = len;
            memcpy(best_tour_seq, tour, n*sizeof(int));
        }
    }
    
    double t = omp_get_wtime() - t0;  // Tempo total
    *best_len_seq = best_len;
    free(tour);
    return t;
}

int main(int argc, char **argv) {
    // Verifica argumentos de linha de comando
    if (argc < 2) {
        fprintf(stderr, "Uso: OMP_NUM_THREADS=k %s <arquivo.tsp>\n", argv[0]);
        return 1;
    }

    // Variáveis para métricas de tempo
    double t_total0 = omp_get_wtime(), t_total;    // Tempo total de execução
    double t_io0=0, t_io=0, t_par0=0, t_par=0;     // Tempos de I/O e região paralela
    double max_thread_work = 0.0;                  // Tempo da thread mais lenta

    // Leitura do arquivo TSPLIB (sequencial)
    Coord *coord = NULL; int n=0;
    t_io0 = omp_get_wtime();
    if (!read_tsplib_euc2d(argv[1], &coord, &n)) return 1;
    t_io = omp_get_wtime() - t_io0;

    // Preparação para paralelismo
    int *best_tour = (int*)malloc(n*sizeof(int));  // Melhor tour global
    int best_len = INT_MAX;                         // Melhor comprimento global

    // Início da região paralela
    t_par0 = omp_get_wtime();

    #pragma omp parallel
    {
        // Variáveis privadas de cada thread
        int *tour = (int*)malloc(n*sizeof(int));              // Tour temporário da thread
        int *local_best_tour = (int*)malloc(n*sizeof(int));   // Melhor tour da thread
        int local_best = INT_MAX;                              // Melhor comprimento da thread

        double t0 = omp_get_wtime();  // Tempo inicial da thread

        // Loop paralelo com balanceamento dinâmico
        // schedule(dynamic): Cada thread pega uma iteração quando está livre
        // Ideal para cargas de trabalho desbalanceadas (2-Opt varia muito)
        #pragma omp for schedule(dynamic)
        for (int s=1; s<=n; ++s) {
            // Cada thread testa diferentes pontos de partida
            int len = solve_from_start(coord, n, s, tour);
            if (len < local_best) {
                local_best = len;
                memcpy(local_best_tour, tour, n*sizeof(int));
            }
        }

        double t_thread = omp_get_wtime() - t0;  // Tempo de trabalho da thread

        // Seção crítica: apenas uma thread por vez pode executar
        // Atualiza resultado global e estatísticas
        #pragma omp critical
        {
            // Atualiza melhor solução global se necessário
            if (local_best < best_len) {
                best_len = local_best;
                memcpy(best_tour, local_best_tour, n*sizeof(int));
            }
            
            // Atualiza tempo máximo de thread (para detectar desbalanceamento)
            if (t_thread > max_thread_work) max_thread_work = t_thread;
        }

        // Limpeza da memória da thread
        free(local_best_tour);
        free(tour);
    }
    // Fim da região paralela - todas as threads terminaram

    t_par = omp_get_wtime() - t_par0;    // Tempo total da região paralela
    t_total = omp_get_wtime() - t_total0; // Tempo total de execução

    // Saída de resultados
    printf("\n=== TSP OpenMP (NN + 2-Opt) ===\n");
    printf("Instancia: %s  (n=%d)\n", argv[1], n);
    printf("Melhor comprimento: %d\n", best_len);
    printf("Tour: ");
    for (int i=0;i<n;i++) printf("%d ", best_tour[i]);
    printf("\n");

    // Cálculo e exibição do speedup vs versão sequencial
    int *best_tour_seq = (int*)malloc(n*sizeof(int));
    int best_len_seq=0;
    printf("\n--- SPEEDUP ---\n");
    double t_seq = solve_tsp_sequencial(coord, n, best_tour_seq, &best_len_seq);
    double speedup = t_seq / t_total;
    printf("Tempo Sequencial: %.6f s\n", t_seq);
    printf("Tempo Paralelo:   %.6f s\n", t_total);
    printf("Speedup:          %.2fx\n", speedup);
    printf("Resultado:        %s\n", (speedup > 1.0) ? "Paralelo e MAIS RAPIDO!" : "Sequencial e mais rapido");

    // Análise de gargalos e overhead
    printf("\n--- GARGALOS ---\n");
    printf("Leitura (I/O):            %.6f s (%.1f%%)\n", t_io,  (t_io  / t_total)*100.0);
    printf("Regiao paralela:          %.6f s (%.1f%%)\n", t_par, (t_par / t_total)*100.0);
    printf("Pior thread (NN+2-Opt):   %.6f s (dentro da regiao)\n", max_thread_work);

    // Conclusões sobre eficiência da paralelização
    printf("\n--- CONCLUSOES ---\n");
    if (t_par > t_io)
        printf("Paralelismo eficaz: trabalho util supera overhead de I/O\n");
    else
        printf("Paralelismo limitado: I/O domina o tempo total\n");

    // Limpeza de memória
    free(best_tour_seq);
    free(best_tour);
    free(coord);
    return 0;
}