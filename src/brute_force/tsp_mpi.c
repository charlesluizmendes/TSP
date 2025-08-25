#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <mpi.h>

#define MAX 20

typedef struct { 
    int id; 
    double x, y; 
} City;

typedef struct {
    double cost;
    int path[MAX];
    int rank;
    double individual_time;
} ProcessInfo;

City cities[MAX];
int N;

double dist(City a, City b) {
    return round(sqrt((a.x - b.x)*(a.x - b.x) + (a.y - b.y)*(a.y - b.y)));
}

void read_tsp(const char *filename) {
    FILE *f = fopen(filename, "r");
    char line[100];
    while (fgets(line, sizeof(line), f))
        if (strncmp(line, "NODE_COORD_SECTION", 18) == 0) break;
    while (fscanf(f, "%d %lf %lf", &cities[N].id, &cities[N].x, &cities[N].y) == 3)
        N++;
    fclose(f);
}

void tsp(int level, double cost, int *path, int *vis, 
        double *best_cost, int *best_path) {
    if (level == N) {
        cost += dist(cities[path[N-1]], cities[path[0]]);
        if (cost < *best_cost) {
            *best_cost = cost;
            memcpy(best_path, path, N * sizeof(int));
        }
        return;
    }
    for (int i = 0; i < N; i++) {
        if (!vis[i]) {
            vis[i] = 1;
            path[level] = i;
            tsp(level + 1, cost + dist(cities[path[level-1]], cities[i]),
                path, vis, best_cost, best_path);
            vis[i] = 0;
        }
    }
}

void print_result(const char *label, int *path, double cost, double time) {
    printf("%s:\n", label);
    for (int i = 0; i < N; i++) printf("%d ", cities[path[i]].id);
    printf("%d\n", cities[path[0]].id);
    printf("Custo: %.0f\n", cost);
    printf("Tempo: %.6f s\n", time);
    printf("\n");
}

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        printf("Numero de processos MPI: %d\n\n", size);
    }   

    if (rank == 0) read_tsp(argv[1]);
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(cities, sizeof(City)*MAX, MPI_BYTE, 0, MPI_COMM_WORLD);
    
    double t1, t2;
    double process_start, process_end;

    double local_best = 1e9;
    int local_path[MAX];
    int path[MAX], vis[MAX];
    
    ProcessInfo local_info, *all_info = NULL;
    if (rank == 0) {
        all_info = malloc(size * sizeof(ProcessInfo));
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    t1 = MPI_Wtime();
    process_start = MPI_Wtime();

    for (int i = 1 + rank; i < N; i += size) {
        memset(vis, 0, sizeof(vis));
        path[0] = 0; path[1] = i;
        vis[0] = vis[i] = 1;
        tsp(2, dist(cities[0], cities[i]), path, vis,
            &local_best, local_path);
    }

    process_end = MPI_Wtime();

    struct { 
        double cost; 
        double time;
        int rank; 
    } local_result = {local_best, process_end - process_start, rank};
    
    struct { 
        double cost; 
        double time;
        int rank; 
    } *all_results = NULL;
    
    if (rank == 0) {
        all_results = malloc(size * sizeof(typeof(local_result)));
    }
    
    MPI_Gather(&local_result, sizeof(local_result), MPI_BYTE, 
               all_results, sizeof(local_result), MPI_BYTE, 0, MPI_COMM_WORLD);
    
    int winner_rank = rank;
    
    if (rank == 0) {
        double best_cost = 1e9;
        double best_time = 1e9;
        
        for (int i = 0; i < size; i++) {
            if (all_results[i].cost < 1e9) {
                if (all_results[i].cost < best_cost || 
                   (all_results[i].cost == best_cost && all_results[i].time < best_time)) {
                    best_cost = all_results[i].cost;
                    best_time = all_results[i].time;
                    winner_rank = all_results[i].rank;
                }
            }
        }
        free(all_results);
    }
    
    MPI_Bcast(&winner_rank, 1, MPI_INT, 0, MPI_COMM_WORLD);
    
    local_info.cost = local_best;
    local_info.rank = rank;
    local_info.individual_time = process_end - process_start;
    memcpy(local_info.path, local_path, MAX * sizeof(int));
    
    int global_best_path[MAX];
    if (rank == winner_rank) {
        memcpy(global_best_path, local_path, MAX * sizeof(int));
    }
    MPI_Bcast(global_best_path, MAX, MPI_INT, winner_rank, MPI_COMM_WORLD);

    MPI_Gather(&local_info, sizeof(ProcessInfo), MPI_BYTE, 
               all_info, sizeof(ProcessInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    t2 = MPI_Wtime();

    if (rank == 0) {
        for (int i = 0; i < size; i++) {
            if (all_info[i].cost < 1e9) {
                char label[20];
                sprintf(label, "Processo %d", i);
                print_result(label, all_info[i].path, all_info[i].cost, all_info[i].individual_time);
            }
        }

        double winner_cost = 0;
        double winner_time = 0;
        for (int i = 0; i < size; i++) {
            if (all_info[i].rank == winner_rank) {
                winner_cost = all_info[i].cost;
                winner_time = all_info[i].individual_time;
                break;
            }
        }

        printf("===== RESULTADO =====\n");
        printf("Processo: %d\n", winner_rank);
        
        for (int i = 0; i < N; i++) printf("%d ", cities[global_best_path[i]].id);
        printf("%d\n", cities[global_best_path[0]].id);
        printf("Custo: %.0f\n", winner_cost);
        printf("Tempo: %.6f s\n", winner_time);
        printf("Tempo total da execucao paralela: %.6f s\n", t2 - t1);
        
        free(all_info);
    }

    MPI_Finalize();
    return 0;
}