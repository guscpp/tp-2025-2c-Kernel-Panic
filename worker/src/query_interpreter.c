// worker/src/query_interpreter.c
#include "../include/query_interpreter.h"
//#include "../include/memoria.h"
#include "../include/worker.h"
#include "../include/tipos.h"

// Dejo esto de modelo
/*
t_query_interpreter* query_interpreter_crear() {
    t_query_interpreter* interpreter = malloc(sizeof(t_query_interpreter));
    interpreter->pc = 0;
    return interpreter;
}
*/

void query_interpreter_ciclo(Pcb* pcb, t_worker* w){
    log_info(w->logger, "El archivo_query que se esta ejecutando es %s. Query ID: %d, PC: %d", pcb->nombre_archivo, pcb->query_id, pcb->pc);
    char* instruccion;
    t_decode* instruccion_decf = malloc(sizeof(t_decode));
    for(;;){
        instruccion = fetch(pcb, w);
        

        pcb->pc++; //agregar la idea de que puede venir un pc != 0

        instruccion_decf = decode(instruccion, w);

        if(instruccion_decf->fin){ //NO lo hago en el fetch porque ahi todavia no se que instruccion es. REcien en decode, despues de parsear se que se trata de un END.
            executeEnd(w);
            break;
        }

        execute(instruccion_decf->parametros, instruccion_decf->ejecuta_instruccion, w);

    }

}

char* fetch(Pcb* pcb, t_worker* w){
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
    paquete_decode->fin = true;
    return paquete_decode;
    }
 
}

void execute(t_instr_param* parametros, void (*ejecuta_instruccion)(t_instr_param*, t_worker*), t_worker* w){
    ejecuta_instruccion(parametros, w);
}

char** aux_separar_file_tag(char* cadena){
    char** file_tag;
    file_tag = string_n_split(cadena, 2, ":");
    return file_tag;
    
}



void executeCreate(t_instr_param* parametros, t_worker* w){
    
    int tamanio = 0;
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_Create = crear_paquete(STORAGE_CREATE_FILE, buffer_generico);
    agregar_a_paquete(paquete_Create, parametros->nombre_file, strlen(parametros->nombre_file) + 1); //parametros->nombre_file es un string por eso pasa sin &
    agregar_a_paquete(paquete_Create, &tamanio, sizeof(int));
    enviar_paquete(paquete_Create, w->storage_socket, w->logger);
    eliminar_paquete(paquete_Create);
    
    
    log_info(w->logger, "Llegue a hacer create");
}

void executeTruncate(t_instr_param* parametros, t_worker* w){


    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paqueteTruncate = crear_paquete(STORAGE_TRUNCATE, buffer_generico);
    agregar_a_paquete(paqueteTruncate, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paqueteTruncate, parametros->tag, strlen(parametros->tag +1));
    agregar_a_paquete(paqueteTruncate, &parametros->tamanio, sizeof(int));
    enviar_paquete(paqueteTruncate, w->storage_socket, w->logger);
    eliminar_paquete(paqueteTruncate);
    
    log_info(w->logger, "Llegue a hacer truncate");
}

void executeWrite(t_instr_param* parametros, t_worker* w){
    /*
    -----Creo que la logica en este caso seria solamente escribir en memoria. Deberia limitarse solamente a esto y en caso de no encontrarlo en memoria y tenga que pedirlo a storage, la memoria interna es la que deberia ocuparse de todo eso, desde aca solo mando a escrbir, el como eso sucede ya no es mi problema (es decir, de alguna forma, la memoria interna debe encargarse de recuperar esas paginas y escribirlo). Me refiero a que toda esa logica iria dentro de escribir_en_memoria()

    escribir_en_memoria(parametros.nombre_file, parametros.tag, parametros.direccion_base, parametros.contenido);

    ----No creo que deba ir algo como lo de abajo por la explicacion de arriba:
    bool en_memoria = paginas_estan_en_Memoria()
    if(en_memoria){
        escribir_en_memoria(parametros.nombre_file, parametros.tag, parametros.direccion_base, parametros.contenido);
    }
    solicitar_contenido_storage()
    escribir_en_memoria()

*/
    log_info(w->logger, "Llegue a hacer write");
}

void executeRead(t_instr_param* parametros, t_worker* w){

    /*
    //La logica seria como la de arriba, esta funcion de encarga de traer esa lectura como sea:
    //char* contenido_leido = leer_de_memoria(parametros->nombre_file, parametros->tag, parametros->direccion_base, parametros->tamanio)
    t_paquete* paqueteLectura = crear_paquete(WORKER_READ_RESULT, buffer_generico);
    agregar_a_paquete(paqueteLectura, contenido_leido, strlen(contenido_leido)+1);
    enviar_paquete(paqueteLectura, w->master_socket, w->logger); 
    eliminar_paquete(paqueteLectura); 
    */

    log_info(w->logger, "Llegue a hacer read");
}

void executeTag(t_instr_param* parametros, t_worker* w){

    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_nuevo_tag = crear_paquete(STORAGE_TAG, buffer_generico);
    agregar_a_paquete(paquete_nuevo_tag, parametros->nombre_file_org, strlen(parametros->nombre_file_org)+1);
    agregar_a_paquete(paquete_nuevo_tag, parametros->tag_origen, strlen(parametros->tag_origen)+1);
    agregar_a_paquete(paquete_nuevo_tag, parametros->nombre_file_destino, strlen(parametros->nombre_file_destino)+1);
    agregar_a_paquete(paquete_nuevo_tag, parametros->tag_destino, strlen(parametros->tag_destino)+1);
    enviar_paquete(paquete_nuevo_tag, w->storage_socket, w->logger);
    eliminar_paquete(paquete_nuevo_tag);
    
    log_info(w->logger, "Llegue a hacer tag");
}

void executeCommit(t_instr_param* parametros, t_worker* w){

    executeFlush(parametros, w); //executeFLush necesita el nombreDelFIle y el tag, y yo aca en commit tengo esos parametros
    
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_commit = crear_paquete(STORAGE_COMMIT, buffer_generico);
    agregar_a_paquete(paquete_commit, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paquete_commit, parametros->tag, strlen(parametros->tag)+1);
    enviar_paquete(paquete_commit, w->storage_socket, w->logger);
    eliminar_paquete(paquete_commit);

    log_info(w->logger, "Llegue a hacer commit");
}

void executeFlush(t_instr_param* parametros, t_worker* w){ //ESto se hace previo a la ejecucion de un commit y de un desalojo de query

    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_flush = crear_paquete(STORAGE_FLUSH, buffer_generico);
    agregar_a_paquete(paquete_flush, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paquete_flush, parametros->tag, strlen(parametros->tag)+1);
    enviar_paquete(paquete_flush, w->storage_socket, w->logger);
    eliminar_paquete(paquete_flush);
    
    log_info(w->logger, "Llegue a hacer flush");
}

void executeDelete(t_instr_param* parametros, t_worker* w){

    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_delete = crear_paquete(STORAGE_DELETE, buffer_generico);
    agregar_a_paquete(paquete_delete, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paquete_delete, parametros->tag, strlen(parametros->tag)+1);
    enviar_paquete(paquete_delete, w->storage_socket, w->logger);
    eliminar_paquete(paquete_delete);
    
    log_info(w->logger, "Llegue a hacer delete");
}

void executeEnd(t_worker* w){ //avisar a master de la finalizacion

    t_buffer* buffer_generico = crear_buffer();
    t_paquete* aviso_end_query = crear_paquete(WORKER_QUERY_END, buffer_generico);
    enviar_paquete(aviso_end_query, w->master_socket, w->logger);
    eliminar_paquete(aviso_end_query);
}