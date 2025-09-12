#include "query_control.h"

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Uso: ./bin/query [archivo_config] [archivo_query] [prioridad]\n");
        return EXIT_FAILURE;
    }
    t_query_control* qc = inicializar_query_control(argc, argv);
    
    verificar_query_control(qc);

    if (conectar_al_master(qc) == -1) {
        liberar_query_control(qc);
        return EXIT_FAILURE;
    }
    
    enviar_query_al_master(qc);

    procesar_respuestas_master(qc);

    close(qc->master_socket);
    terminar_programa(qc->logger, qc->config);
    
    return EXIT_SUCCESS;
}