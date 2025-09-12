#include "storage.h"

typedef struct {
    t_log* logger;
    t_config* config;
    int puerto;
    bool fresh;
    char* punto_montaje;
    int retardo_operacion;
    int retardo_acceso_bloque;
    char* log_level;   
}storage_t;


int main(int argc, char* argv[]) {
    
}

int inicializar_file_system(storage_t* storage){
    if(storage->fresh){
        formatear_fs(storage);
    }
}




int formatear_fs(storage_t* storage){
    return 0;
    
}


