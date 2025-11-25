#include "../include/query_interpreter.h"
#include "../include/memoria.h" //por acceder_memoria
#include "../include/worker.h"// por manejo de errores

extern bool query_desconectado;


// ****************************************************************************
t_query_interpreter* query_interpreter_crear(t_log* logger){
    t_query_interpreter* interpreter = malloc(sizeof(t_query_interpreter));
    interpreter->pc = 0;
    interpreter->hay_interrupcion = false;
    log_info(logger, "Se creo una query_interpreter");
    return interpreter;
}


// ****************************************************************************
void query_interpreter_ciclo(Pcb* pcb, t_worker* w){
    //me agarro el PC que me enviaron desde kernel y lo guardo en el PC del interpreter. DOnde verdadeeramente va a avanzar va ser en worker, luego si hay interrupciones voy a sobreescribir el pc que me dio kernel (para poner el actual si es que hubo interrupcion)
    w->interpreter->pc = pcb->pc;
    int i = 1;

    log_warning(w->logger, "El archivo_query que se esta ejecutando es %s. Query ID: %d, PC: %d", pcb->nombre_archivo, pcb->query_id, pcb->pc);
    char* instruccion = NULL;
    t_decode* instruccion_decf = NULL;

    for(;;){

        pthread_mutex_lock(&w->flag_error_storage->mutex_error_storage);
        if(w->flag_error_storage->error_storage){
        log_info(w->logger, "Me estoy por salir del ciclo, Storage fallo en alguna operacion");
            w->flag_error_storage->error_storage = false;
            pthread_mutex_unlock(&w->flag_error_storage->mutex_error_storage);
            // if (instruccion) free(instruccion);
            // if (instruccion_decf) destruir_decode(instruccion_decf);
            break;
        }
        pthread_mutex_unlock(&w->flag_error_storage->mutex_error_storage);

        instruccion = fetch(pcb, w); //aca me llega la instruccion completa
        printf("Numero de linea del archivo query - i = %d \n", i++);

        w->interpreter->pc++;  //el pc que avanza es el del interpreter

        instruccion_decf = decode(instruccion, w);

        if(instruccion_decf->fin){ //NO lo hago en el fetch porque ahi todavia no se que instruccion es. REcien en decode, despues de parsear se que se trata de un END.
            executeEnd(w, pcb);                  //puse el == true para no debugear otra vez 
            if (instruccion) free(instruccion);
            if (instruccion_decf) destruir_decode(instruccion_decf);
            break;
        }

        if(instruccion_decf->instruccion_malformada){
            error_instruccion_malformada(w->logger, pcb->query_id, instruccion);
            if (instruccion) free(instruccion);
            if (instruccion_decf) destruir_decode(instruccion_decf);
            break;
        }

        execute(instruccion_decf->parametros, instruccion_decf->ejecuta_instruccion, w, pcb);
        //free(instruccion_decf); revisar con valgrind
        //free(instruccion_decf->parametros);   revisar con valgrind
        free(instruccion);
        instruccion = NULL;
        destruir_decode(instruccion_decf);
        instruccion_decf = NULL;
        
        //checkInterrupt
        pthread_mutex_lock(&mutex_interrupt); 
        if(w->interpreter->hay_interrupcion){ //no entra aca porque al crear query_interpreter se le pone false al hay interrupcion y este cambia recien cuando le llega al hilo de interrupciones
            w->interpreter->hay_interrupcion =false; 
            pthread_mutex_unlock(&mutex_interrupt); 
            interrupt_envio_a_master(pcb, w); //MAndo el PCB para poder actualizarlo con el PC del w (y mandarlo a master)

            // if (instruccion) free(instruccion);
            // if (instruccion_decf) destruir_decode(instruccion_decf);
            
            break;
        }
            pthread_mutex_unlock(&mutex_interrupt);

    }
    // if (instruccion) free(instruccion);
    // if (instruccion_decf) destruir_decode(instruccion_decf);
}


// ****************************************************************************
char* fetch(Pcb* pcb, t_worker* w){
    char* buffer_autoselc = NULL;
    size_t tam_autoselc = 0;
    ssize_t leido;

    // LOG AL INICIO DE FETCH
    log_debug(w->logger, "Fetch: pc=%d, interpreter_pc=%d", pcb->pc, w->interpreter->pc);

    //por aca va entrar solamente un proceso que fue interrumpido antes (porque el PC del PCB que me pasaron no va cambiar). POr lo tanto, el getline que va tomar al hacer fetch siempre va ser este, no hay forma de que entre por el de abajo(**) porque se va chequear el PC del PCB que siempre va ser != 0 porque ese nunca se va modificar (ese no es el PC que se avanza, es el que te viene de master)
    if(pcb->pc != 0){
        
        // LOG PARA CASO INTERRUMPIDO
        log_debug(w->logger, "Caso: proceso interrumpido (pc != 0)");

        if(w->interpreter->pc == pcb->pc){//SI esto ocurre significa que es el primer fetch de mi ciclo, porque no avance mi PC de interpreter respecto del pc del PCB
            
            // LOG PARA PRIMER FETCH DESPUÉS DE INTERRUPCIÓN
            log_debug(w->logger, "Primer fetch después de interrupción");
            
            int i = 0;
            for(; i < pcb->pc; i++) {
                do {
                    // Liberar buffer anterior si existe
                    if(buffer_autoselc) {
                        free(buffer_autoselc);
                        buffer_autoselc = NULL;
                        tam_autoselc = 0;
                    }
                    
                    leido = getline(&buffer_autoselc, &tam_autoselc, pcb->archivo);
                    
                    // LOG DESPUÉS DE GETLINE
                    log_debug(w->logger, "Getline iteracion %d: leido=%ld, buffer='%s'", i, leido, buffer_autoselc ? buffer_autoselc : "NULL");
                    
                    if(leido == -1) {
                        if(feof(pcb->archivo)) {
                            free(buffer_autoselc);
                            return strdup("END");
                        } else {
                            log_error(w->logger, "Error de lectura en el archivo de query");
                            free(buffer_autoselc);
                            return NULL;
                        }
                    }
                    
                    // Procesar la línea
                    if(buffer_autoselc) {
                        buffer_autoselc[strcspn(buffer_autoselc, "\n")] = '\0';
                        string_trim(&buffer_autoselc);
                        
                        // LOG DESPUÉS DE PROCESAR LÍNEA
                        log_debug(w->logger, "Linea procesada: '%s'", buffer_autoselc);
                    }
                    
                    // Continuar si es comentario o línea vacía
                } while(buffer_autoselc && (buffer_autoselc[0] == '#' || buffer_autoselc[0] == '\0'));
            }
        }

        //SI es falso que estoy en el PC inicial (con PC inicial nos referimos al que nos pasa el master), osea que estoy en mi segundo fetch
        
        // LOG PARA FETCHES SUBSECUENTES
        log_debug(w->logger, "Fetch subsiguiente después de interrupción");
        
        do {
            // Liberar buffer anterior si existe
            if(buffer_autoselc) {
                free(buffer_autoselc);
                buffer_autoselc = NULL;
                tam_autoselc = 0;
            }
            
            leido = getline(&buffer_autoselc, &tam_autoselc, pcb->archivo);

            // LOG DESPUÉS DE GETLINE
            log_debug(w->logger, "Getline subsiguiente: leido=%ld, buffer='%s'", leido, buffer_autoselc ? buffer_autoselc : "NULL");
            
            if(leido == -1) {
                if(feof(pcb->archivo)) {
                    free(buffer_autoselc);
                    return strdup("END");
                } else {
                    log_error(w->logger, "Error de lectura en el archivo de query");
                    free(buffer_autoselc);
                    return NULL;
                }
            }
            
            // Procesar la línea
            if(buffer_autoselc) {
                buffer_autoselc[strcspn(buffer_autoselc, "\n")] = '\0';
                string_trim(&buffer_autoselc);
                
                // LOG DESPUÉS DE PROCESAR LÍNEA
                log_debug(w->logger, "Linea procesada subsiguiente: '%s'", buffer_autoselc);
            }
            
            // Continuar si es comentario o línea vacía
        } while(buffer_autoselc && (buffer_autoselc[0] == '#' || buffer_autoselc[0] == '\0'));
        
        // LOG ANTES DE RETORNAR
        log_debug(w->logger, "Retornando linea: '%s'", buffer_autoselc ? buffer_autoselc : "NULL");
        
        return buffer_autoselc;
    }
    else{ //est es el de abajo (**)
    //por aca va entrar solamente un proceso que NO fue interrupido (PC = 0)
    
        // LOG PARA PROCESO NUEVO
        log_debug(w->logger, "Caso: proceso nuevo (pc = 0)");
        
        do {
            // Liberar buffer anterior si existe
            if(buffer_autoselc) {
                free(buffer_autoselc);
                buffer_autoselc = NULL;
                tam_autoselc = 0;
            }
            
            leido = getline(&buffer_autoselc, &tam_autoselc, pcb->archivo);

            // LOG DESPUÉS DE GETLINE
            log_debug(w->logger, "Getline proceso nuevo: leido=%ld, buffer='%s'", leido, buffer_autoselc ? buffer_autoselc : "NULL");
            
            if(leido == -1) {
                if(feof(pcb->archivo)) {
                    free(buffer_autoselc);
                    return strdup("END");
                } else {
                    log_error(w->logger, "Error de lectura en el archivo de query");
                    free(buffer_autoselc);
                    return NULL;
                }
            }
            
            // Procesar la línea
            if(buffer_autoselc) {
                buffer_autoselc[strcspn(buffer_autoselc, "\n")] = '\0';
                string_trim(&buffer_autoselc);
                
                // LOG DESPUÉS DE PROCESAR LÍNEA
                log_debug(w->logger, "Linea procesada nuevo: '%s'", buffer_autoselc);
            }
            
            // Continuar si es comentario o línea vacía
        } while(buffer_autoselc && (buffer_autoselc[0] == '#' || buffer_autoselc[0] == '\0'));
        
        // LOG ANTES DE RETORNAR
        log_debug(w->logger, "Retornando linea nuevo proceso: '%s'", buffer_autoselc ? buffer_autoselc : "NULL");
        
        return buffer_autoselc;
    }
}


// ****************************************************************************
t_decode* decode(char* instruccion, t_worker* w){
    char** parametros = NULL;
    char* instruccion_copia = string_duplicate(instruccion);

    // Elimina comentarios que comienzan en una linea con codigo
    char* comentario = strchr(instruccion_copia, '#');
    
    if (comentario != NULL) {
        *comentario = '\0';  // Truncar la línea en el comentario
        string_trim(&instruccion_copia);  // Eliminar espacios sobrantes
    }

    parametros = string_n_split(instruccion_copia, 2, " "); 
    free(instruccion_copia);
    //me devuelve un array: parametros[0] = primer elemento de la cadena instruccion, parametros[1] toda el resto de la tira de parametros. El 2 porque lo divido en 2 partes y el " " porque se separa por un espacio
    t_decode* paquete_decode = malloc(sizeof(t_decode));
    paquete_decode->parametros = calloc(1, sizeof(t_instr_param));
    
    if(string_equals_ignore_case(parametros[0], "CREATE")){ //recordar hacer los frees

        char** create_param = aux_separar_file_tag(parametros[1]);

        paquete_decode->parametros->nombre_file = string_duplicate(create_param[0]);
        paquete_decode->parametros->tag = string_duplicate(create_param[1]);
        paquete_decode->ejecuta_instruccion = executeCreate;
        paquete_decode->fin = false;
        paquete_decode->instruccion_malformada = false;

        free(create_param[0]);
        free(create_param[1]);
        free(create_param);
        string_array_destroy(parametros);

        return paquete_decode;
    }
     
    if(string_equals_ignore_case(parametros[0], "TRUNCATE")){
        
        char** truncate_file_tam = NULL;
        char** truncate_file_tag = NULL;

        if (parametros[1] == NULL) {
            log_error(w->logger, "Parametros incompletos para TRUNCATE");
            paquete_decode->instruccion_malformada = true;
            // free(paquete_decode->parametros);
            // free(paquete_decode);
            string_array_destroy(parametros);
            return paquete_decode;
        }

        truncate_file_tam = string_n_split(parametros[1], 2, " ");
        paquete_decode->parametros->tamanio = atoi(truncate_file_tam[1]);
        truncate_file_tag = aux_separar_file_tag(truncate_file_tam[0]);
        paquete_decode->parametros->nombre_file = string_duplicate(truncate_file_tag[0]);
        paquete_decode->parametros->tag = string_duplicate(truncate_file_tag[1]);
        
        paquete_decode->ejecuta_instruccion = executeTruncate;

        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;

        string_array_destroy(truncate_file_tam);
        string_array_destroy(truncate_file_tag);
        string_array_destroy(parametros);

        return paquete_decode;
    }

    if(string_equals_ignore_case(parametros[0], "WRITE")){

        char** write_params_princ = NULL;
        char** write_file_tag = NULL;

        if (parametros[1] == NULL) {
            log_error(w->logger, "Parametros incompletos para WRITE");
            paquete_decode->instruccion_malformada = true;
            // free(paquete_decode->parametros);
            // free(paquete_decode);
            string_array_destroy(parametros);
            return paquete_decode;
        }

        write_params_princ = string_n_split(parametros[1],3," ");

        paquete_decode->parametros->direccion_base = atoi(write_params_princ[1]); //TODO: leak?
        paquete_decode->parametros->contenido = string_duplicate(write_params_princ[2]);

        write_file_tag = aux_separar_file_tag(write_params_princ[0]);
        paquete_decode->parametros->nombre_file = string_duplicate(write_file_tag[0]);
        paquete_decode->parametros->tag = string_duplicate(write_file_tag[1]);
        paquete_decode->ejecuta_instruccion = executeWrite;

        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;

        string_array_destroy(write_params_princ);
        string_array_destroy(write_file_tag);
        string_array_destroy(parametros);

        return paquete_decode;
    }
    
    if(string_equals_ignore_case(parametros[0], "READ")){

        char** read_params_princ = NULL;
        char** read_file_tag = NULL;

        if (parametros[1] == NULL) {
            log_error(w->logger, "Parametros incompletos para READ");
            paquete_decode->instruccion_malformada = true;
            // free(paquete_decode->parametros);
            // free(paquete_decode);
            string_array_destroy(parametros);
            return paquete_decode;
        }

        read_params_princ = string_n_split(parametros[1], 3, " ");
        paquete_decode->parametros->direccion_base = atoi(read_params_princ[1]);
        paquete_decode->parametros->tamanio = atoi(read_params_princ[2]);

        read_file_tag = aux_separar_file_tag(read_params_princ[0]);
        paquete_decode->parametros->nombre_file = string_duplicate(read_file_tag[0]);
        paquete_decode->parametros->tag = string_duplicate(read_file_tag[1]);
        paquete_decode->ejecuta_instruccion = executeRead;

        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;

        string_array_destroy(read_params_princ);
        string_array_destroy(read_file_tag);
        string_array_destroy(parametros);

        return paquete_decode;
    }

    if(string_equals_ignore_case(parametros[0], "TAG")){

        char** tag_params = NULL;
        char** file_tag_org = NULL;
        char** file_tag_dest = NULL;

        if (parametros[1] == NULL) {
            log_error(w->logger, "Parametros incompletos para TAG");
            paquete_decode->instruccion_malformada = true;
            // free(paquete_decode->parametros);
            // free(paquete_decode);
            string_array_destroy(parametros);
            return paquete_decode;
        }
	
		tag_params = string_n_split(parametros[1], 2, " ");

		file_tag_org = aux_separar_file_tag(tag_params[0]);
		file_tag_dest = aux_separar_file_tag(tag_params[1]);
		
		paquete_decode->parametros->nombre_file_org = string_duplicate(file_tag_org[0]);
		paquete_decode->parametros->tag_origen = string_duplicate(file_tag_org[1]);
		paquete_decode->parametros->nombre_file_destino = string_duplicate(file_tag_dest[0]);
		paquete_decode->parametros->tag_destino = string_duplicate(file_tag_dest[1]);
		
		paquete_decode->ejecuta_instruccion = executeTag;
		paquete_decode->fin = false;
		paquete_decode->instruccion_malformada = false;
		
        string_array_destroy(tag_params);
        string_array_destroy(file_tag_org);
        string_array_destroy(file_tag_dest);
        string_array_destroy(parametros);
		
		return paquete_decode;
	}

    if(string_equals_ignore_case(parametros[0], "COMMIT")){
    
        char** commit_param = NULL;
        
        if (parametros[1] == NULL) {
            log_error(w->logger, "Parámetros incompletos para COMMIT");
            paquete_decode->instruccion_malformada = true;
            // free(paquete_decode->parametros);
            // free(paquete_decode);
            string_array_destroy(parametros);
            return paquete_decode;
        }

        commit_param = aux_separar_file_tag(parametros[1]);
        paquete_decode->parametros->nombre_file = string_duplicate(commit_param[0]);
        paquete_decode->parametros->tag = string_duplicate(commit_param[1]);
        paquete_decode->ejecuta_instruccion = executeCommit;
        
        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;

        string_array_destroy(commit_param);
        string_array_destroy(parametros);

        return paquete_decode;
    }

    if(string_equals_ignore_case(parametros[0], "FLUSH")){
        
        char** flush_param = NULL;

        if (parametros[1] == NULL) {
            log_error(w->logger, "Parámetros incompletos para FLUSH");
            paquete_decode->instruccion_malformada = true;
            // free(paquete_decode->parametros);
            // free(paquete_decode);
            string_array_destroy(parametros);
            return paquete_decode;
        }

        flush_param = aux_separar_file_tag(parametros[1]);
        paquete_decode->parametros->nombre_file = string_duplicate(flush_param[0]);
        paquete_decode->parametros->tag = string_duplicate(flush_param[1]);
        paquete_decode->ejecuta_instruccion = executeFlush;
        
        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;

        string_array_destroy(flush_param);
        string_array_destroy(parametros);

        return paquete_decode;
    }

    if(string_equals_ignore_case(parametros[0], "DELETE")){
        
        char** delete_param = NULL;
    
        if (parametros[1] == NULL) {
            log_error(w->logger, "Parámetros incompletos para DELETE");
            paquete_decode->instruccion_malformada = true;
            // free(paquete_decode->parametros);
            // free(paquete_decode);
            string_array_destroy(parametros);
            return paquete_decode;
        }

        delete_param = aux_separar_file_tag(parametros[1]);
        paquete_decode->parametros->nombre_file = string_duplicate(delete_param[0]);
        paquete_decode->parametros->tag = string_duplicate(delete_param[1]);
        paquete_decode->ejecuta_instruccion = executeDelete;

        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;

        string_array_destroy(delete_param);
        string_array_destroy(parametros);

        return paquete_decode;
    }

    if(string_equals_ignore_case(parametros[0], "END")){

        paquete_decode->fin = true;
        paquete_decode->instruccion_malformada = false;
        //free(paquete_decode->parametros);
        string_array_destroy(parametros);

        return paquete_decode;

    }

    // CASO POR DEFECTO para instrucción desconocida
    // lo agrego mayormente porque no quiero ver el warning del compilador
    log_warning(w->logger, "Instrucción desconocida: %s", parametros[0]);
    paquete_decode->fin = false;
    paquete_decode->instruccion_malformada = true;
    free(parametros[0]);
    if (parametros[1]) free(parametros[1]);
    free(parametros);
    return paquete_decode;
 }


// ****************************************************************************
void execute(t_instr_param* parametros, void (*ejecuta_instruccion)(t_instr_param*, t_worker*, Pcb*), t_worker* w, Pcb* pcb){
    ejecuta_instruccion(parametros, w, pcb);
}


// ****************************************************************************
char** aux_separar_file_tag(char* cadena){
    char** file_tag;
    file_tag = string_n_split(cadena, 2, ":");
    return file_tag;
    
}


// ****************************************************************************
void executeCreate(t_instr_param* parametros, t_worker* w, Pcb* pcb){

    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_Create = crear_paquete(STORAGE_CREATE_FILE, buffer_generico);
    agregar_a_paquete(paquete_Create, &(pcb->query_id), sizeof(pcb->query_id));
    agregar_a_paquete(paquete_Create, parametros->nombre_file, strlen(parametros->nombre_file) + 1); //parametros->nombre_file es un string por eso pasa sin &
    agregar_a_paquete(paquete_Create, parametros->tag, strlen(parametros->tag) + 1); 
    enviar_paquete(paquete_Create, w->storage_socket, w->logger);
    eliminar_paquete(paquete_Create);
    
    
    rtas_storage(w->storage_socket, w, parametros, pcb);
}


// ****************************************************************************
void executeTruncate(t_instr_param* parametros, t_worker* w, Pcb* pcb){
    
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paqueteTruncate = crear_paquete(STORAGE_TRUNCATE, buffer_generico);
    agregar_a_paquete(paqueteTruncate, &(pcb->query_id), sizeof(pcb->query_id));
    agregar_a_paquete(paqueteTruncate, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paqueteTruncate, parametros->tag, strlen(parametros->tag) + 1);
    agregar_a_paquete(paqueteTruncate, &parametros->tamanio, sizeof(int));
    enviar_paquete(paqueteTruncate, w->storage_socket, w->logger);
    eliminar_paquete(paqueteTruncate);
    

    rtas_storage(w->storage_socket, w, parametros, pcb);
}


// ****************************************************************************
void executeWrite(t_instr_param* parametros, t_worker* w, Pcb* pcb){
    size_t tam = strlen(parametros->contenido);
    
    // Establecer contexto para acceder_memoria
    w->mem->memoria_contexto = parametros;
    
    // La funcion acceder_memoria ahora maneja multiples paginas
    void* resultado = acceder_memoria(w->mem, pcb->query_id, parametros->nombre_file, parametros->tag,
                            parametros->direccion_base, tam, true, w);
    if (resultado) {
        log_info(w->logger, "Query<%d>: Instrucción realizada: WRITE %s:%s %d %s", 
                pcb->query_id, parametros->nombre_file, parametros->tag,
                parametros->direccion_base, parametros->contenido);
    } else {
        log_error(w->logger, "Query<%d>: Escritura fallida - File:%s - Tag:%s - Offset:%d",
                 pcb->query_id, parametros->nombre_file, parametros->tag, parametros->direccion_base);
    }
    
    w->mem->memoria_contexto = NULL; // Limpiar contexto
}


// ****************************************************************************
void executeRead(t_instr_param* parametros, t_worker* w, Pcb* pcb) {
    void* dir = acceder_memoria(w->mem, pcb->query_id, parametros->nombre_file, parametros->tag,
                                parametros->direccion_base, parametros->tamanio, false, w);
    if (!dir) {
        log_error(w->logger, "Query<%d>: Lectura fallida - File:%s - Tag:%s - Offset:%d",
                  pcb->query_id, parametros->nombre_file, parametros->tag, parametros->direccion_base);
        return;
    }
    
    char* contenido = string_substring(dir, 0, parametros->tamanio);
    log_info(w->logger, "Contenido leído: %s", contenido);
    
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_read = crear_paquete(WORKER_READ_RESULT, buffer_generico);
    agregar_a_paquete(paquete_read, &(pcb->query_id), sizeof(int)); // envío queryID a master
    agregar_a_paquete(paquete_read, contenido, strlen(contenido)+1); // envío contenido leído a master
    agregar_a_paquete(paquete_read, parametros->nombre_file, strlen(parametros->nombre_file)+1); // para loggear en QueryControl
    agregar_a_paquete(paquete_read, parametros->tag, strlen(parametros->tag)+1); // para loggear en QueryControl
    agregar_a_paquete(paquete_read, &(w->interpreter->pc), sizeof(int)); // envío el PC actual
    
    enviar_paquete(paquete_read, w->master_socket_distpach, w->logger);
    eliminar_paquete(paquete_read);
    free(contenido);
    free(dir); // Liberar el buffer total para lecturas
    
    log_info(w->logger, "Query<%d>: Instrucción realizada: READ", pcb->query_id);
}


// ****************************************************************************
void executeTag(t_instr_param* parametros, t_worker* w, Pcb* pcb){
    log_debug(w->logger, "Query<%d>: FETCH - Program Counter:%d - TAG %s:%s %s:%s", 
             pcb->query_id, w->interpreter->pc, 
             parametros->nombre_file_org, parametros->tag_origen,
             parametros->nombre_file_destino, parametros->tag_destino);
    
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_nuevo_tag = crear_paquete(STORAGE_TAG, buffer_generico);
    
    agregar_a_paquete(paquete_nuevo_tag, &(pcb->query_id), sizeof(pcb->query_id));
    agregar_a_paquete(paquete_nuevo_tag, parametros->nombre_file_org, strlen(parametros->nombre_file_org)+1);
    agregar_a_paquete(paquete_nuevo_tag, parametros->tag_origen, strlen(parametros->tag_origen)+1);
    agregar_a_paquete(paquete_nuevo_tag, parametros->nombre_file_destino, strlen(parametros->nombre_file_destino)+1);
    agregar_a_paquete(paquete_nuevo_tag, parametros->tag_destino, strlen(parametros->tag_destino)+1);
    
    enviar_paquete(paquete_nuevo_tag, w->storage_socket, w->logger);
    eliminar_paquete(paquete_nuevo_tag);
    
    rtas_storage(w->storage_socket, w, parametros, pcb);
    
}


// ****************************************************************************
void executeCommit(t_instr_param* parametros, t_worker* w, Pcb* pcb){
    
    executeFlush(parametros, w, pcb); //executeFLush necesita el nombreDelFIle y el tag, y yo aca en commit tengo esos parametros
    log_info(w->logger, "Este es el flush antes del commit");

    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_commit = crear_paquete(STORAGE_COMMIT, buffer_generico);

    agregar_a_paquete(paquete_commit, &(pcb->query_id), sizeof(pcb->query_id));
    agregar_a_paquete(paquete_commit, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paquete_commit, parametros->tag, strlen(parametros->tag)+1);

    enviar_paquete(paquete_commit, w->storage_socket, w->logger);
    eliminar_paquete(paquete_commit);
    

    rtas_storage(w->storage_socket, w, parametros, pcb);

}


// ****************************************************************************
void executeFlush(t_instr_param* parametros, t_worker* w, Pcb* pcb){ //ESto se hace previo a la ejecucion de un commit y de un desalojo de query

    flush_paginas_modificadas(w->mem, pcb->query_id, parametros->nombre_file,
                              parametros->tag, w->storage_socket);


    rtas_storage(w->storage_socket, w, parametros, pcb);

}


// ****************************************************************************
void executeDelete(t_instr_param* parametros, t_worker* w, Pcb* pcb){
    
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_delete = crear_paquete(STORAGE_DELETE, buffer_generico);

    agregar_a_paquete(paquete_delete, &(pcb->query_id), sizeof(pcb->query_id));
    agregar_a_paquete(paquete_delete, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paquete_delete, parametros->tag, strlen(parametros->tag)+1);

    enviar_paquete(paquete_delete, w->storage_socket, w->logger);
    eliminar_paquete(paquete_delete);
    

    rtas_storage(w->storage_socket, w, parametros, pcb);
    
}


// ****************************************************************************
void executeEnd(t_worker* w, Pcb* pcb){ //avisar a master de la finalizacion
    
    log_info(w->logger, "Query<%d>: Finalización exitosa de la Query", pcb->query_id);

    t_buffer* buffer_generico = crear_buffer();
    t_paquete* aviso_end_query = crear_paquete(WORKER_QUERY_END, buffer_generico);

    agregar_a_paquete(aviso_end_query, &(pcb->query_id), sizeof(int)); //envio queryId
    
    enviar_paquete(aviso_end_query, w->master_socket_distpach, w->logger);
    eliminar_paquete(aviso_end_query);
    
    // Cerrar archivo de la query
    if(pcb->archivo != NULL) {
        fclose(pcb->archivo);
        pcb->archivo = NULL;
    }
    
    // Liberar memoria
    //free(pcb->nombre_archivo);
    //free(pcb);
}


// ****************************************************************************
void interrupt_envio_a_master(Pcb* pcb_dsp_de_interrupt, t_worker* w){  //Se envia al socket normal
    log_info(w->logger, "Llego una interrupcion, el proceso fue interrumpido. Espero uno nuevo");
    
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* devuelvo_pcb_master;
    if(query_desconectado){
        devuelvo_pcb_master = crear_paquete(WORKER_QUERY_DESCONECTADO, buffer_generico);
    }else{
        devuelvo_pcb_master = crear_paquete(WORKER_PC_UPDATE, buffer_generico);
    }
    //TAl vez este no haga falta:
    agregar_a_paquete(devuelvo_pcb_master, pcb_dsp_de_interrupt->nombre_archivo, strlen(pcb_dsp_de_interrupt->nombre_archivo)+1);
    
    //Estos dos si:
    agregar_a_paquete(devuelvo_pcb_master, &(pcb_dsp_de_interrupt->query_id), sizeof(int));
    agregar_a_paquete(devuelvo_pcb_master, &(w->interpreter->pc), sizeof(int));
    enviar_paquete(devuelvo_pcb_master, w->master_socket_distpach, w->logger);
    eliminar_paquete(devuelvo_pcb_master);
    
}


// ****************************************************************************
void destruir_decode(t_decode* dec) {
    if (dec == NULL) return;

    if (dec->parametros != NULL) {
        // Libero cada campo de parametros que no sea NULL
        if (dec->parametros->nomb_instr) {
            free(dec->parametros->nomb_instr);
            dec->parametros->nomb_instr = NULL;
        }
        if (dec->parametros->nombre_file) {
            free(dec->parametros->nombre_file);
            dec->parametros->nombre_file = NULL;
        }
        if (dec->parametros->tag) {
            free(dec->parametros->tag);
            dec->parametros->tag = NULL;
        }
        if (dec->parametros->contenido) {
            free(dec->parametros->contenido);
            dec->parametros->contenido = NULL;
        }
        if (dec->parametros->nombre_file_org) {
            free(dec->parametros->nombre_file_org);
            dec->parametros->nombre_file_org = NULL;
        }
        if (dec->parametros->tag_origen) {
            free(dec->parametros->tag_origen);
            dec->parametros->tag_origen = NULL;
        }
        if (dec->parametros->nombre_file_destino) {
            free(dec->parametros->nombre_file_destino);
            dec->parametros->nombre_file_destino = NULL;
        }
        if (dec->parametros->tag_destino) {
            free(dec->parametros->tag_destino);
            dec->parametros->tag_destino = NULL;
        }
        
        free(dec->parametros);
        dec->parametros = NULL;
    }
    
    free(dec);
}