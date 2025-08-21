#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <mpi.h>

#define MAX 10000

typedef struct { int id; double x, y; } City;

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
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (rank == 0)
        read_tsp(argv[1]);

    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(cities, sizeof(City) * N, MPI_BYTE, 0, MPI_COMM_WORLD);

    double t1, t2;

    MPI_Barrier(MPI_COMM_WORLD);
    t1 = MPI_Wtime();
    
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

    struct {
        double cost;
        int rank;
    } local = {local_best, rank}, global;

    MPI_Allreduce(&local, &global, 1, MPI_DOUBLE_INT, MPI_MINLOC, MPI_COMM_WORLD);

    if (rank == global.rank)
        memcpy(best_path_par, local_best_path, sizeof(int) * N);
    MPI_Bcast(best_path_par, N, MPI_INT, global.rank, MPI_COMM_WORLD);
    best_cost_par = global.cost;
    
    MPI_Barrier(MPI_COMM_WORLD);
    t2 = MPI_Wtime();

    if (rank == 0) {
        double par_time = t2 - t1;
        print_result("MPI", best_path_par, best_cost_par, par_time);
    }

    MPI_Finalize();
    return 0;
}
