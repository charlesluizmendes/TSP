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
    printf("%-25s: %d\n",  "P", P);
    printf("%-25s: %.6f\n", "T_seq (s)", T_seq);
    printf("%-25s: %.6f\n", "T_par (s)", T_par);
    printf("%-25s: %.6f\n", "Speedup (S)", S);
    printf("%-25s: %.2f%%\n", "Eficiencia (%)", E);
    printf("%-25s: %.6f ( = P * T_par )\n", "Custo paralelo", cost_par_time);
    printf("%-25s: %.6f ( = P*T_par - T_seq )\n", "Overhead abs (s)", overhead_abs);
    printf("%-25s: %.6f (fracao de T_seq)\n", "Overhead rel", overhead_rel);
    printf("%-25s: %.6f\n", "Karp-Flatt epsilon", epsilon);
    printf("%-25s: %s (tolerancia 10%%)\n", "Custo-otimal?", cost_optimal ? "SIM" : "NAO");

    if (epsilon >= 0.0 && isfinite(epsilon)) {
        printf("%-25s: %.6f\n", "Karp-Flatt epsilon", epsilon);
    } else {
        printf("%-25s: N/A (P <= 1)\n", "Karp-Flatt epsilon");
    }

    printf("%-25s: %s (tolerancia 10%%)\n", "Custo-otimal?", cost_optimal ? "SIM" : "NAO");

    // Comparacao de qualidade/custo de solucao, se informado
    if (C_seq >= 0.0 && C_par >= 0.0) {
        double delta = C_par - C_seq;
        double pct  = (C_seq != 0.0) ? (delta / C_seq) * 100.0 : 0.0;
        printf("\n=== Qualidade da Solucao ===n");
        printf("%-25s: %.6f\n", "C_seq", C_seq);
        printf("%-25s: %.6f\n", "C_par", C_par);
        printf("%-25s: %.6f (%+.2f%%)\n", "Delta (par - seq)", delta, pct);
    } else {
        printf("\n(Qualidade nao avaliada: informe C_seq e C_par >= 0 para comparar)\n");
    }

    return 0;
}
