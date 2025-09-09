#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

#define MAX 10000

typedef struct { int id; double x, y; } City;

typedef struct {
    double cost;
    int path[MAX];
    int rank;
    double individual_time;
} ProcessInfo;

City cities[MAX];
int N;
int best_path_par[MAX];
double best_cost_par = 1e9;

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

double nearest_neighbor(int start, int *path) {
    int visited[MAX] = {0};
    path[0] = start;
    visited[start] = 1;
    double cost = 0.0;

    for (int i = 1; i < N; i++) {
        int last = path[i - 1], next = -1;
        double min_d = 1e9;
        for (int j = 0; j < N; j++) {
            if (!visited[j]) {
                double d = dist(cities[last], cities[j]);
                if (d < min_d) {
                    min_d = d;
                    next = j;
                }
            }
        }
        path[i] = next;
        visited[next] = 1;
        cost += min_d;
    }
    cost += dist(cities[path[N - 1]], cities[start]);
    return cost;
}

void print_result(const char *label, int *path, double cost, double time) {
    printf("%s:\n", label);
    printf("Custo: %.0f\n", cost);
    printf("Tempo: %.6f s\n\n", time);
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0) {
        printf("Numero de processos MPI: %d\n\n", size);
        read_tsp(argv[1]);
    }

    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(cities, sizeof(City) * N, MPI_BYTE, 0, MPI_COMM_WORLD);

    double t1, t2;
    double process_start, process_end;

    MPI_Barrier(MPI_COMM_WORLD);
    t1 = MPI_Wtime();
    process_start = MPI_Wtime();
    
    int local_best_path[MAX];
    double local_best = 1e9;

    for (int i = rank; i < N; i += size) {
        int path[MAX];
        double cost = nearest_neighbor(i, path);
        if (cost < local_best) {
            local_best = cost;
            memcpy(local_best_path, path, sizeof(int) * N);
        }
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
            if (all_results[i].cost < best_cost || 
               (all_results[i].cost == best_cost && all_results[i].time < best_time)) {
                best_cost = all_results[i].cost;
                best_time = all_results[i].time;
                winner_rank = all_results[i].rank;
            }
        }
        free(all_results);
    }
    
    MPI_Bcast(&winner_rank, 1, MPI_INT, 0, MPI_COMM_WORLD);

    ProcessInfo local_info, *all_info = NULL;
    if (rank == 0) {
        all_info = malloc(size * sizeof(ProcessInfo));
    }

    local_info.cost = local_best;
    local_info.rank = rank;
    local_info.individual_time = process_end - process_start;
    memcpy(local_info.path, local_best_path, N * sizeof(int));

    MPI_Gather(&local_info, sizeof(ProcessInfo), MPI_BYTE, 
               all_info, sizeof(ProcessInfo), MPI_BYTE, 0, MPI_COMM_WORLD);

    if (rank == winner_rank)
        memcpy(best_path_par, local_best_path, sizeof(int) * N);
    MPI_Bcast(best_path_par, N, MPI_INT, winner_rank, MPI_COMM_WORLD);
    
    MPI_Barrier(MPI_COMM_WORLD);
    t2 = MPI_Wtime();

    if (rank == 0) {
        for (int i = 0; i < size; i++) {
            char label[20];
            sprintf(label, "Processo %d", i);
            print_result(label, all_info[i].path, all_info[i].cost, all_info[i].individual_time);
        }

        double winner_cost = 0;
        double winner_time = 0;
        double max_individual_time = 0;
        
        for (int i = 0; i < size; i++) {
            if (all_info[i].rank == winner_rank) {
                winner_cost = all_info[i].cost;
                winner_time = all_info[i].individual_time;
            }
            if (all_info[i].individual_time > max_individual_time) {
                max_individual_time = all_info[i].individual_time;
            }
        }

        printf("===== RESULTADO =====\n");
        printf("Processo: %d\n", winner_rank);
        
        printf("Custo: %.0f\n", winner_cost);
        printf("Tempo: %.6f s\n", winner_time);

        printf("Tempo efetivo da execucao paralela: %.6f s\n", max_individual_time);
        printf("Tempo total (com overhead MPI): %.6f s\n", t2 - t1);
        
        free(all_info);
    }

    MPI_Finalize();
    return 0;
}