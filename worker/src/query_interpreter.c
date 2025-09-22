// worker/src/query_interpreter.c
#include "../include/query_interpreter.h"
#include "../include/memoria.h"
#include "../include/worker.h"

// Dejo esto de modelo
t_query_interpreter* query_interpreter_crear() {
    t_query_interpreter* interpreter = malloc(sizeof(t_query_interpreter));
    interpreter->pc = 0;
    return interpreter;
}

void query_interpreter_ciclo(PCB* pcb, t_worker* w){
    log_info(w->logger, "El archivo_query que se esta ejecutando es %s. Query ID: %d, PC: %d", pcb->nombre_archivo, pcb->query_id, pcb->pc);
    char* instruccion;
    t_decode* instruccion_decf = malloc(sizeof(t_decode));
    for(;;){
        instruccion = fetch(pcb, w);

        pcb->pc++; //agregar la idea de que puede venir un pc != 0

        instruccion_decf = decode(instruccion, w);
        execute(instruccion_decf->parametros, instruccion_decf->ejecuta_instruccion);

    }

}

char* fetch(PCB* pcb, t_worker* w){
    char* buffer_autoselc =  NULL;
    size_t tam_autoselc = 0;
    ssize_t leido = getline(&buffer_autoselc, &tam_autoselc, pcb->archivo);
    if(leido == -1){
        log_info(w->logger, "Error al leer la instruccion");
        free(buffer_autoselc); //Tengo que liberar el malloc de buffer_autoselec que le encargue a getline. El getline se ocupo hacerme el malloc
        return NULL;
    }
    return buffer_autoselc;
}

t_decode* decode(char* instruccion, t_worker* w){
    char** parametros;
    parametros = string_n_split(instruccion, 2, " "); 
    //me devuelve un array: parametros[0] = primer elemento de la cadena instruccion, parametros[1] toda el resto de la tira de parametros. El 2 porque lo divido en 2 partes y el " " porque se separa por un espacio
    t_decode* paquete_decode = malloc(sizeof(t_decode));
    paquete_decode->parametros = malloc(sizeof(t_instr_param)); 

    
    if(string_equals_ignore_case(parametros[0], "CREATE")){ //recordar hacer los frees

        char** create_param;
        create_param = aux_separar_file_tag(parametros[1]);
        paquete_decode->parametros->nombre_file = create_param[0];
        paquete_decode->parametros->tag = create_param[1];
        paquete_decode->ejecuta_instruccion = executeCreate;
        return paquete_decode;
    }
     
    if(string_equals_ignore_case(parametros[0], "TRUNCATE")){
        
        char** truncate_file_tam;
        char** truncate_file_tag;

        truncate_file_tam = string_n_split(parametros[1], 2, " ");
        paquete_decode->parametros->tamanio = truncate_file_tam[1];
        truncate_file_tag = aux_separar_file_tag(truncate_file_tam[0]);
        paquete_decode->parametros->nombre_file = truncate_file_tag[0];
        paquete_decode->parametros->tag = truncate_file_tag[1];
        
        paquete_decode->ejecuta_instruccion = executeTruncate;
        return paquete_decode;
    }

    if(string_equals_ignore_case(parametros[0], "WRITE")){

        char** write_params_princ;
        char** write_file_tag;

        write_params_princ = string_n_split(parametros[1],3," ");

        paquete_decode->parametros->direccion_base = write_params_princ[1];
        paquete_decode->parametros->contenido = write_params_princ[2];

        write_file_tag = aux_separar_file_tag(write_params_princ[0]);
        paquete_decode->parametros->nombre_file = write_file_tag[0];
        paquete_decode->parametros->tag = write_file_tag[1];
        paquete_decode->ejecuta_instruccion = executeWrite;

        return paquete_decode;
    }
    
    if(string_equals_ignore_case(parametros[0], "READ")){

        char** read_params_princ;
        char** read_file_tag;

        read_params_princ = string_n_split(parametros[1], 3, " ");
        paquete_decode->parametros->direccion_base = read_params_princ[1];
        paquete_decode->parametros->tamanio = read_params_princ[2];

        read_file_tag = aux_separar_file_tag(read_params_princ[0]);
        paquete_decode->parametros->nombre_file = read_file_tag[0];
        paquete_decode->parametros->tag = read_file_tag[1];
        paquete_decode->ejecuta_instruccion = executeRead;
            
        return paquete_decode;
    }
    if(string_equals_ignore_case(parametros[0], "TAG")){

        
        char** tag_params;
        char** file_tag_org;
        char** file_tag_dest;

        tag_params = string_n_split(parametros[1], 2, " ");
        file_tag_org = aux_separar_file_tag(tag_params[0]);
        paquete_decode->parametros->nombre_file_org = file_tag_org[0];
        paquete_decode->parametros->tag_origen = file_tag_org[1];

        file_tag_dest = aux_separar_file_tag(tag_params[1]);
        paquete_decode->parametros->nombre_file_destino = file_tag_dest[0];
        paquete_decode->parametros->tag_destino = file_tag_dest[1];
        paquete_decode->ejecuta_instruccion = executeTag;

        return paquete_decode;
    }   
    if(string_equals_ignore_case(parametros[0], "COMMIT")){

        
        char** commit_param;

        commit_param = aux_separar_file_tag(parametros[1]);
        paquete_decode->parametros->nombre_file = commit_param[0];
        paquete_decode->parametros->tag = commit_param[1];
        paquete_decode->ejecuta_instruccion = executeCommit;

        return paquete_decode;
    }
    if(string_equals_ignore_case(parametros[0], "FLUSH")){

        
        char** flush_param;

        flush_param = aux_separar_file_tag(parametros[1]);
        paquete_decode->parametros->nombre_file = flush_param[0];
        paquete_decode->parametros->tag = flush_param[1];
        paquete_decode->ejecuta_instruccion = executeFlush;
        
        return paquete_decode;
    }
    if(string_equals_ignore_case(parametros[0], "DELETE")){

        
        char** delete_param;

        delete_param = aux_separar_file_tag(parametros[1]);
        paquete_decode->parametros->nombre_file = delete_param[0];
        paquete_decode->parametros->tag = delete_param[1];
        paquete_decode->ejecuta_instruccion = executeDelete;
        return paquete_decode;
    }

    if(string_equals_ignore_case(parametros[0], "END")){
    //creo que se puede sacar
    return NULL;
    }
 
}

void execute(t_instr_param* parametros, void (*ejecuta_instruccion)(t_instr_param*)){
    ejecuta_instruccion(parametros);
}

char** aux_separar_file_tag(char* cadena){
    char** file_tag;
    file_tag = string_n_split(cadena, 2, ":");
    return file_tag;
    
}


void executeCreate(){

}

void executeTruncate(){

}

void executeWrite(){

}

void executeRead(){

}

void executeTag(){

}

void executeCommit(){

}

void executeFlush(){

}

void executeDelete(){

}