#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <omp.h>

#define MAX 10000

typedef struct { int id; double x, y; } City;

typedef struct {
    double cost;
    int path[MAX];
    int thread_id;
    double individual_time;
} ThreadInfo;

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
    for (int i = 0; i < N; i++) printf("%d ", cities[path[i]].id);
    printf("%d\n", cities[path[0]].id);
    printf("Custo: %.0f\n", cost);
    printf("Tempo: %.6f s\n\n", time);
}

int main(int argc, char *argv[]) {
    int num_threads = omp_get_max_threads();
    printf("Numero de threads OpenMP: %d\n\n", num_threads);

    read_tsp(argv[1]);

    ThreadInfo *all_threads = malloc(num_threads * sizeof(ThreadInfo));

    double t1 = omp_get_wtime();
    #pragma omp parallel
    {
        int thread_id = omp_get_thread_num();
        int local_path[MAX];
        double local_best = 1e9;
        double thread_start = omp_get_wtime();

        #pragma omp for
        for (int i = 0; i < N; i++) {
            int path[MAX];
            double cost = nearest_neighbor(i, path);
            if (cost < local_best) {
                local_best = cost;
                memcpy(local_path, path, sizeof(int) * N);
            }
        }

        double thread_end = omp_get_wtime();

        all_threads[thread_id].cost = local_best;
        all_threads[thread_id].thread_id = thread_id;
        all_threads[thread_id].individual_time = thread_end - thread_start;
        memcpy(all_threads[thread_id].path, local_path, N * sizeof(int));

        #pragma omp critical
        {
            if (local_best < best_cost_par) {
                best_cost_par = local_best;
                memcpy(best_path_par, local_path, sizeof(int) * N);
            }
        }
    }
    double t2 = omp_get_wtime();

    for (int i = 0; i < num_threads; i++) {
        if (all_threads[i].cost < 1e9) {
            char label[20];
            sprintf(label, "Thread %d", i);
            print_result(label, all_threads[i].path, all_threads[i].cost, 
                        all_threads[i].individual_time);
        }
    }

    int winner_thread = -1;
    double best_cost = 1e9;
    double best_time = 1e9;
    double max_individual_time = 0;

    for (int i = 0; i < num_threads; i++) {
        if (all_threads[i].cost < 1e9) {
            if (all_threads[i].cost < best_cost || 
               (all_threads[i].cost == best_cost && all_threads[i].individual_time < best_time)) {
                best_cost = all_threads[i].cost;
                best_time = all_threads[i].individual_time;
                winner_thread = i;
            }
            if (all_threads[i].individual_time > max_individual_time) {
                max_individual_time = all_threads[i].individual_time;
            }
        }
    }

    printf("===== RESULTADO =====\n");
    printf("Thread: %d\n", winner_thread);
    
    for (int i = 0; i < N; i++) printf("%d ", cities[all_threads[winner_thread].path[i]].id);
    printf("%d\n", cities[all_threads[winner_thread].path[0]].id);
    printf("Custo: %.0f\n", all_threads[winner_thread].cost);
    printf("Tempo: %.6f s\n", all_threads[winner_thread].individual_time);
    printf("Tempo efetivo da execucao paralela: %.6f s\n", max_individual_time);
    printf("Tempo total (com overhead OpenMP): %.6f s\n", t2 - t1);
    
    free(all_threads);
    return 0;
}