#include <omp.h>
#include "utils_omp.h"

// Estrutura para armazenar resultado de cada thread
typedef struct {
    double custo;
    int *rota;
    int thread_id;
    double tempo;   // Adicionar tempo individual
} ResultadoThread;

int main(int argc, char *argv[]) {
    int n, algoritmo;
    Cidade *cidades = NULL;
    int num_threads = omp_get_max_threads();
    
    if (argc < 3) {
        printf("Uso: %s <arquivo.tsp> <algoritmo> [num_threads]\n", argv[0]);
        printf("Algoritmos: 0=forca bruta, 1=nearest neighbor, 2=2-opt\n");
        return 1;
    }
    
    algoritmo = atoi(argv[2]);
    
    if (argc >= 4) {
        num_threads = atoi(argv[3]);
        if (num_threads <= 0 || num_threads > omp_get_max_threads()) {
            printf("Número de threads inválido. Usando %d threads.\n", omp_get_max_threads());
            num_threads = omp_get_max_threads();
        }
    }
    
    omp_set_num_threads(num_threads);
    
    // Ler arquivo TSP
    if (descobrir_tamanho_instancia(argv[1], &n) != 0) {
        printf("Erro ao ler dimensões do arquivo %s\n", argv[1]);
        return 1;
    }
    
    cidades = malloc(n * sizeof(Cidade));
    if (!cidades) {
        printf("Erro ao alocar memoria para %d cidades\n", n);
        return 1;
    }
    
    if (ler_instancia(argv[1], cidades, &n) != 0) {
        printf("Erro ao ler arquivo %s\n", argv[1]);
        free(cidades);
        return 1;
    }
    
    printf("TSP com %d cidades, algoritmo %d, %d threads OpenMP\n", n, algoritmo, num_threads);
    
    // Preparar estruturas para resultados
    ResultadoThread *resultados = malloc(num_threads * sizeof(ResultadoThread));
    for (int i = 0; i < num_threads; i++) {
        resultados[i].rota = malloc(n * sizeof(int));
        resultados[i].custo = DBL_MAX;
        resultados[i].thread_id = i;
        resultados[i].tempo = 0.0;  // Inicializar tempo
    }
    
    double tempo_inicio = omp_get_wtime();
    
    // Executar algoritmo escolhido com paralelização OpenMP
    if (algoritmo == 0) {
        // Força bruta com OpenMP
        printf("Executando forca bruta com OpenMP...\n");
        
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            double tempo_thread_inicio = omp_get_wtime();
            
            double melhor_custo = forca_bruta_openmp(cidades, n, tid, num_threads, resultados[tid].rota);
            resultados[tid].custo = melhor_custo;
            resultados[tid].tempo = omp_get_wtime() - tempo_thread_inicio;
        }
        
    } else if (algoritmo == 1) {
        // CORRIGIDO: Nearest neighbor com medição de tempo correta
        printf("Executando nearest neighbor com OpenMP...\n");
        
        // Criar array local para melhor resultado por thread
        double *melhor_custo_thread = malloc(num_threads * sizeof(double));
        int **melhor_rota_thread = malloc(num_threads * sizeof(int*));
        
        for (int i = 0; i < num_threads; i++) {
            melhor_custo_thread[i] = DBL_MAX;
            melhor_rota_thread[i] = malloc(n * sizeof(int));
        }
        
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            double tempo_thread_inicio = omp_get_wtime();
            
            // Cada thread processa um subconjunto das cidades iniciais
            #pragma omp for schedule(dynamic)
            for (int start = 0; start < n; start++) {
                int *rota_temp = malloc(n * sizeof(int));
                double custo = nearest_neighbor(cidades, n, start, rota_temp);
                
                // Atualizar melhor resultado da thread atual
                if (custo < melhor_custo_thread[tid]) {
                    melhor_custo_thread[tid] = custo;
                    for (int i = 0; i < n; i++) {
                        melhor_rota_thread[tid][i] = rota_temp[i];
                    }
                }
                
                free(rota_temp);
            }
            
            // Salvar resultado da thread
            resultados[tid].custo = melhor_custo_thread[tid];
            for (int i = 0; i < n; i++) {
                resultados[tid].rota[i] = melhor_rota_thread[tid][i];
            }
            resultados[tid].tempo = omp_get_wtime() - tempo_thread_inicio;
        }
        
        // Limpeza
        for (int i = 0; i < num_threads; i++) {
            free(melhor_rota_thread[i]);
        }
        free(melhor_rota_thread);
        free(melhor_custo_thread);
        
    } else {
        // 2-opt com OpenMP
        printf("Executando 2-opt com OpenMP...\n");
        
        #pragma omp parallel
        {
            int tid = omp_get_thread_num();
            double tempo_thread_inicio = omp_get_wtime();
            
            double melhor_custo = two_opt_openmp(cidades, n, tid, num_threads, resultados[tid].rota);
            resultados[tid].custo = melhor_custo;
            resultados[tid].tempo = omp_get_wtime() - tempo_thread_inicio;
        }
    }
    
    double tempo_total = omp_get_wtime() - tempo_inicio;
    
    
    // Encontrar melhor resultado global
    double melhor_custo_global = DBL_MAX;
    int thread_vencedora = -1;
    
    printf("\n=== DETALHES POR THREAD ===\n");
    for (int i = 0; i < num_threads; i++) {
        double custo_display = (resultados[i].custo == DBL_MAX) ? -1.0 : resultados[i].custo;
        printf("Thread %d: custo=%.2f, tempo=%.6fs", i, custo_display, resultados[i].tempo);
        
        if (resultados[i].custo < melhor_custo_global) {
            melhor_custo_global = resultados[i].custo;
            thread_vencedora = i;
            printf(" ** VENCEDOR **");
        }

        printf("\n");
    }
    
    // Mostrar resultados
    if (thread_vencedora != -1) {
        printf("\n=== ROTA VENCEDORA (Thread %d) ===\n", thread_vencedora);
        printf("Custo: %.2f\n", melhor_custo_global);
        printf("Rota: ");
        for (int i = 0; i < n; i++) {
            printf("%d", resultados[thread_vencedora].rota[i]);
            if (i < n - 1) printf(" -> ");
        }
        printf(" -> %d\n", resultados[thread_vencedora].rota[0]);
    } else {
        printf("\nNenhuma thread encontrou solucao valida!\n");
    }
    
    // Calcular métricas de tempo CORRIGIDAS
    if (thread_vencedora != -1) {
        double tempo_max = resultados[0].tempo;
        double tempo_min = resultados[0].tempo;
        double tempo_soma = 0;
        
        for (int i = 0; i < num_threads; i++) {
            if (resultados[i].tempo > tempo_max) tempo_max = resultados[i].tempo;
            if (resultados[i].tempo < tempo_min) tempo_min = resultados[i].tempo;
            tempo_soma += resultados[i].tempo;
        }
        
        double tempo_medio = tempo_soma / num_threads;
        
        printf("\n=== METRICAS DE PARALELIZACAO ===\n");
        printf("Melhor custo encontrado: %.2f (Thread %d)\n", melhor_custo_global, thread_vencedora);
        printf("Numero de threads: %d\n", num_threads);
        printf("Tempo total (wall clock): %.6f segundos\n", tempo_total);
        printf("Tempo maximo (thread): %.6f segundos\n", tempo_max);
        printf("Tempo medio (thread):  %.6f segundos\n", tempo_medio);
        printf("Tempo minimo (thread): %.6f segundos\n", tempo_min);
        
        if (num_threads > 1 && tempo_total > 0) {
            double variacao_tempo = tempo_max > 0 ? ((tempo_max - tempo_min) / tempo_max) * 100 : 0;
            double balanceamento = tempo_max > 0 ? (tempo_min / tempo_max) * 100 : 0;
            double eficiencia_uso = tempo_max > 0 ? (tempo_medio / tempo_max) * 100 : 0;
            
            // Speedup corrigido: tempo sequencial estimado / tempo paralelo real
            double speedup_real = (tempo_total > 0.0) ? (tempo_soma / tempo_total) : 0.0;
            double eficiencia_paralela = (num_threads > 0) ? (speedup_real / num_threads) * 100.0 : 0.0;
            
            printf("\n=== BALANCEAMENTO DE CARGA ===\n");
            printf("Variacao de tempo: %.1f%%\n", variacao_tempo);
            printf("Balanceamento:      %.1f%%\n", balanceamento);
            printf("Eficiencia de uso: %.1f%%\n", eficiencia_uso);
            
            printf("\n=== SPEEDUP ===\n");
            // Speedup estimado baseado no trabalho total vs tempo de parede
            double speedup_estimado = tempo_soma / tempo_total;
            double eficiencia_estimada = (speedup_estimado / num_threads) * 100.0;
            
            printf("Speedup estimado: %.2fx\n", speedup_estimado);
            printf("Eficiencia estimada: %.1f%%\n", eficiencia_estimada);

        } else {
            printf("\n=== EXECUCAO SEQUENCIAL ===\n");
            printf("Executando com 1 thread (sem paralelizacao)\n");
        }
    }
    
    // Limpeza
    for (int i = 0; i < num_threads; i++) {
        free(resultados[i].rota);
    }
    free(resultados);
    free(cidades);
    
    return 0;
}