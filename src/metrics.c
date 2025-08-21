#include <stdio.h>
#include <math.h>

int main(void) {
    double T_seq, T_par;
    double C_seq, C_par;
    int P;

    printf("=== Metricas de Desempenho ===\n\n");

    printf("Numero de processadores/threads (P): ");
    if (scanf("%d", &P) != 1 || P <= 0) {
        fprintf(stderr, "Erro: P deve ser inteiro positivo.\n");
        return 1;
    }

    printf("Tempo sequencial T_seq (s): ");
    if (scanf("%lf", &T_seq) != 1 || T_seq <= 0.0) {
        fprintf(stderr, "Erro: T_seq deve ser > 0.\n");
        return 1;
    }

    printf("Tempo paralelo T_par (s): ");
    if (scanf("%lf", &T_par) != 1 || T_par <= 0.0) {
        fprintf(stderr, "Erro: T_par deve ser > 0.\n");
        return 1;
    }

    printf("Custo/qualidade sequencial C_seq (opcional, -1 para ignorar): ");
    if (scanf("%lf", &C_seq) != 1) {
        fprintf(stderr, "Erro de entrada.\n");
        return 1;
    }

    printf("Custo/qualidade paralelo C_par (opcional, -1 para ignorar): ");
    if (scanf("%lf", &C_par) != 1) {
        fprintf(stderr, "Erro de entrada.\n");
        return 1;
    }

    // Métricas básicas
    double S = T_seq / T_par;                    // Speedup
    double E = (S / (double)P) * 100.0;          // Eficiência em %
    double cost_par_time = (double)P * T_par;    // Custo em tempo do paralelo
    double overhead_abs = cost_par_time - T_seq; // Overhead Absoluto
    double overhead_rel = overhead_abs / T_seq;  // Overhead Relativo

    // Karp–Flatt epsilon (fração serial efetiva)
    double epsilon = -1.0;
    if (P > 1) {
        epsilon = (1.0 / S - 1.0 / (double)P) / (1.0 - 1.0 / (double)P);
    }

    // Custo-otimalidade (heurística: dentro de 10%)
    int cost_optimal = fabs(cost_par_time - T_seq) <= 0.10 * T_seq;

    printf("\n=== Resultados ===\n");
    printf("P                 : %d\n", P);
    printf("T_seq (s)         : %.6f\n", T_seq);
    printf("T_par (s)         : %.6f\n", T_par);
    printf("Speedup (S)       : %.6f\n", S);
    printf("Eficiencia (%%)   : %.2f%%\n", E);
    printf("Custo paralelo    : %.6f ( = P * T_par )\n", cost_par_time);
    printf("Overhead abs (s)  : %.6f ( = P*T_par - T_seq )\n", overhead_abs);
    printf("Overhead rel      : %.6f (fracao de T_seq)\n", overhead_rel);

    if (epsilon >= 0.0 && isfinite(epsilon)) {
        printf("Karp–Flatt epsilon: %.6f\n", epsilon);
    } else {
        printf("Karp–Flatt epsilon: N/A (P <= 1)\n");
    }

    printf("Custo-otimal?     : %s (tolerancia 10%%)\n", cost_optimal ? "SIM" : "NAO");

    // Comparacao de qualidade/custo de solucao, se informado
    if (C_seq >= 0.0 && C_par >= 0.0) {
        double delta = C_par - C_seq;
        double pct  = (C_seq != 0.0) ? (delta / C_seq) * 100.0 : 0.0;
        printf("\n--- Qualidade da Solucao ---\n");
        printf("C_seq             : %.6f\n", C_seq);
        printf("C_par             : %.6f\n", C_par);
        printf("Delta (par - seq) : %.6f ( %+ .2f%% )\n", delta, pct);
    } else {
        printf("\n(Qualidade nao avaliada: informe C_seq e C_par >= 0 para comparar)\n");
    }

    return 0;
}
