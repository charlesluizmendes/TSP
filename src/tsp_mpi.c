#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <mpi.h> // Biblioteca para programação paralela com MPI

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
    double t0 = MPI_Wtime();  // Tempo inicial
    int *tour = (int*)malloc((n)*sizeof(int));
    int best_len = INT_MAX; int best_start = 1;
    
    // Testa todos os pontos de partida sequencialmente
    for (int s=1; s<=n; ++s) {
        int len = solve_from_start(coord, n, s, tour);
        if (len < best_len) {
            best_len = len;
            memcpy(best_tour_seq, tour, n*sizeof(int));
            best_start = s;
        }
    }
    
    double t = MPI_Wtime() - t0;  // Tempo total
    *best_len_seq = best_len;
    free(tour);
    (void)best_start;  // Suprime warning de variável não usada
    return t;
}

int main(int argc, char **argv) {
    // Verifica argumentos de linha de comando
    if (argc < 2) {
        if (0==0) fprintf(stderr, "Uso: mpirun -np P %s <arquivo.tsp>\n", argv[0]);
        return 1;
    }

    // Inicialização MPI
    int rank, size;
    MPI_Init(&argc, &argv);                    // Inicializa ambiente MPI
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);      // Obtém ID do processo atual
    MPI_Comm_size(MPI_COMM_WORLD,&size);      // Obtém número total de processos

    // Variáveis para métricas de tempo detalhadas
    double t_total0 = MPI_Wtime(), t_total;
    double t_io0=0, t_io=0, t_bcast0=0, t_bcast=0, t_work0=0, t_work=0, t_reduce0=0, t_reduce=0, t_collect0=0, t_collect=0;

    // Leitura TSPLIB (apenas processo 0) + broadcast para todos
    Coord *coord = NULL; int n = 0;

    if (rank == 0) {
        // Apenas processo 0 lê o arquivo
        t_io0 = MPI_Wtime();
        if (!read_tsplib_euc2d(argv[1], &coord, &n)) {
            MPI_Abort(MPI_COMM_WORLD, 1);  // Termina todos os processos se erro
        }
        t_io = MPI_Wtime() - t_io0;
    }

    // Broadcast: distribui dados para todos os processos
    t_bcast0 = MPI_Wtime();
    
    // Primeiro broadcast: dimensão do problema
    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    // Outros processos alocam memória
    if (rank != 0) coord = (Coord*)calloc(n+1, sizeof(Coord));
    
    // Segundo broadcast: coordenadas (2 doubles por ponto, 1-based -> n+1)
    MPI_Bcast(&coord[0], (n+1)*sizeof(Coord), MPI_BYTE, 0, MPI_COMM_WORLD);
    
    t_bcast = MPI_Wtime() - t_bcast0;

    // Preparação para trabalho paralelo
    int *tour = (int*)malloc(n*sizeof(int));              // Tour temporário
    int *best_tour_local = (int*)malloc(n*sizeof(int));   // Melhor tour local
    int best_len_local = INT_MAX;                          // Melhor comprimento local

    // Sincronização antes do trabalho computacional
    MPI_Barrier(MPI_COMM_WORLD);
    
    // Trabalho paralelo: cada processo testa diferentes pontos de partida
    t_work0 = MPI_Wtime();
    
    // Distribuição round-robin dos pontos de partida
    // Processo 0: testa cidades 1, 1+size, 1+2*size, ...
    // Processo 1: testa cidades 2, 2+size, 2+2*size, ...
    // etc.
    for (int s = 1 + rank; s <= n; s += size) {
        int len = solve_from_start(coord, n, s, tour);
        if (len < best_len_local) {
            best_len_local = len;
            memcpy(best_tour_local, tour, n*sizeof(int));
        }
    }
    
    t_work = MPI_Wtime() - t_work0;

    // Redução: encontra o melhor resultado global
    struct { int len; int rank; } in, out;
    in.len = best_len_local; 
    in.rank = rank;

    t_reduce0 = MPI_Wtime();
    // MINLOC encontra o menor comprimento e qual processo o possui
    MPI_Allreduce(&in, &out, 1, MPI_2INT, MPI_MINLOC, MPI_COMM_WORLD);
    t_reduce = MPI_Wtime() - t_reduce0;

    // Identificação do processo vencedor
    int owner = out.rank;                    // Processo que tem a melhor solução
    int best_len_global = out.len;           // Melhor comprimento global
    int *best_tour_global = NULL;

    // Coleta da melhor solução completa
    if (rank == 0) {
        // Processo 0 recebe a melhor solução
        best_tour_global = (int*)malloc(n*sizeof(int));
        t_collect0 = MPI_Wtime();
        
        if (owner == 0) {
            // Se processo 0 tem a melhor solução, apenas copia
            memcpy(best_tour_global, best_tour_local, n*sizeof(int));
        } else {
            // Caso contrário, recebe do processo vencedor
            MPI_Recv(best_tour_global, n, MPI_INT, owner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        t_collect = MPI_Wtime() - t_collect0;
    } else if (rank == owner) {
        // Processo vencedor envia sua solução para processo 0
        MPI_Send(best_tour_local, n, MPI_INT, 0, 0, MPI_COMM_WORLD);
    }

    t_total = MPI_Wtime() - t_total0;

    // Coleta de estatísticas de tempo (máximos de cada fase)
    double max_io=0, max_bcast=0, max_work=0, max_reduce=0, max_collect=0;
    MPI_Reduce(&t_io,      &max_io,      1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_bcast,   &max_bcast,   1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_work,    &max_work,    1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_reduce,  &max_reduce,  1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&t_collect, &max_collect, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // Saída de resultados (apenas processo 0)
    if (rank == 0) {
        // Imprime melhor solução encontrada
        printf("\n=== TSP MPI (NN + 2-Opt) ===\n");
        printf("Instancia: %s  (n=%d)\n", argv[1], n);
        printf("Melhor comprimento: %d\n", best_len_global);
        printf("Tour: ");
        for (int i=0;i<n;i++) printf("%d ", best_tour_global[i]);
        printf("\n");

        // Speedup vs versão sequencial (mesmo algoritmo)
        int *best_tour_seq = (int*)malloc(n*sizeof(int));
        int best_len_seq=0;
        printf("\n--- SPEEDUP ---\n");
        double t_seq = solve_tsp_sequencial(coord, n, best_tour_seq, &best_len_seq);
        double speedup = t_seq / t_total;
        printf("Tempo Sequencial: %.6f s\n", t_seq);
        printf("Tempo Paralelo:   %.6f s\n", t_total);
        printf("Speedup:          %.2fx\n", speedup);
        printf("Resultado:        %s\n", (speedup > 1.0) ? "Paralelo e MAIS RAPIDO!" : "Sequencial e mais rapido");

        // Análise detalhada de gargalos
        printf("\n--- GARGALOS ---\n");
        printf("Leitura (I/O):            %.6f s (%.1f%%)\n", max_io,     (max_io     / t_total)*100.0);
        printf("Broadcast:                %.6f s (%.1f%%)\n", max_bcast,  (max_bcast  / t_total)*100.0);
        printf("Trabalho (NN+2-Opt):      %.6f s (%.1f%%)\n", max_work,   (max_work   / t_total)*100.0);
        printf("Reducao (MINLOC):         %.6f s (%.1f%%)\n", max_reduce, (max_reduce / t_total)*100.0);
        printf("Coleta tour vencedor:     %.6f s (%.1f%%)\n", max_collect,(max_collect/ t_total)*100.0);

        // Conclusões sobre eficiência da paralelização
        printf("\n--- CONCLUSOES ---\n");
        if (max_work > (max_reduce + max_collect))
            printf("Paralelismo eficaz: trabalho util supera overhead de comunicacao\n");
        else
            printf("Paralelismo limitado: comunicacao/organizacao domina\n");

        // Limpeza de memória
        free(best_tour_seq);
        free(best_tour_global);
    }

    // Limpeza de memória (todos os processos)
    free(best_tour_local);
    free(tour);
    free(coord);

    // Finalização MPI
    MPI_Finalize();
    return 0;
}