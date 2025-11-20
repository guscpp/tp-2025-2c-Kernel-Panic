// worker/src/query_interpreter.c
#include "../include/query_interpreter.h"
#include "../include/memoria.h"
#include "../include/worker.h"
#include "../include/tipos.h"

// Dejo esto de modelo

t_query_interpreter* query_interpreter_crear(t_log* logger){
    t_query_interpreter* interpreter = malloc(sizeof(t_query_interpreter));
    interpreter->pc = 0;
    interpreter->hay_interrupcion = false;
    log_info(logger, "Se creo una query_interpreter");
    return interpreter;
}


void query_interpreter_ciclo(Pcb* pcb, t_worker* w){
    //me agarro el PC que me enviaron desde kernel y lo guardo en el PC del interpreter. DOnde verdadeeramente va a avanzar va ser en worker, luego si hay interrupciones voy a sobreescribir el pc que me dio kernel (para poner el actual si es que hubo interrupcion)
    w->interpreter->pc = pcb->pc;
    int i = 1;

    log_info(w->logger, "El archivo_query que se esta ejecutando es %s. Query ID: %d, PC: %d", pcb->nombre_archivo, pcb->query_id, pcb->pc);
    char* instruccion;
    t_decode* instruccion_decf = malloc(sizeof(t_decode));

    for(;;){

        if(error_memoria){
        log_info(w->logger, "Me estoy por salir del ciclo, porque alguna cosa extrania intento hacer la query en memoria");
            error_memoria = false; 
            break;
        }
        instruccion = fetch(pcb, w); //aca me llega la instruccion completa
        printf("printf - i = %d", i++);

        w->interpreter->pc++;  //el pc que avanza es el del interpreter

        instruccion_decf = decode(instruccion, w);

        if(instruccion_decf->fin){ //NO lo hago en el fetch porque ahi todavia no se que instruccion es. REcien en decode, despues de parsear se que se trata de un END.
            executeEnd(w, pcb);                  //puse el == true para no debugear otra vez 
            break;
        }

        if(instruccion_decf->instruccion_malformada){
                error_instruccion_malformada(w->logger, pcb->query_id, instruccion);
                break;
        }

        execute(instruccion_decf->parametros, instruccion_decf->ejecuta_instruccion, w, pcb);
        //free(instruccion_decf); revisar con valgrind
        //free(instruccion_decf->parametros);   revisar con valgrind
        
        //checkInterrupt
        //comento-mutex pthread_mutex_lock(&mutex_interrupt); 
        if(w->interpreter->hay_interrupcion){ //no entra aca porque al crear query_interpreter se le pone false al hay interrupcion y este cambia recien cuando le llega al hilo de interrupciones
            w->interpreter->hay_interrupcion =false; 
            //comento-mutex pthread_mutex_unlock(&mutex_interrupt); 

            interrupt_envio_a_master(pcb, w); //MAndo el PCB para poder actualizarlo con el PC del w (y mandarlo a master)
            
            break;
        }
         else {
            //comento-mutex pthread_mutex_unlock(&mutex_interrupt);
        }

    }
}

char* fetch(Pcb* pcb, t_worker* w){

    /*
    NO es posible simplificarlo solamente en un solo caso (osea, no puedo generalizar todos los casos y poner un solo for que itere incluso si el pc=0), porque si hago eso, siempre que entre a fetch va volver a entrar al ciclo y voy a leer siempre la misma linea despues del desplazamiento
    */
   //Puede ser posible: posible mejora

    char* buffer_autoselc =  NULL;
    size_t tam_autoselc = 0;

    //por aca va entrar solamente un proceso que fue interrumpido antes (porque el PC del PCB que me pasaron no va cambiar). POr lo tanto, el getline que va tomar al hacer fetch siempre va ser este, no hay forma de que entre por el de abajo(**) porque se va chequear el PC del PCB que siempre va ser != 0 porque ese nunca se va modificar (ese no es el PC que se avanza, es el que te viene de master)
    if(pcb->pc !=0){

        if(w->interpreter->pc  == pcb->pc){//SI esto ocurre significa que es el primer fetch de mi ciclo, porque no avance mi PC de interpreter respecto del pc del PCB
        int i = 0;
        for(; i < pcb->pc; i++)  
            {
            ssize_t leido = getline(&buffer_autoselc, &tam_autoselc, pcb->archivo);
            
            if(leido == -1){
            log_info(w->logger, "Error al leer la instruccion que se ignora (la del for en fetch)");
            free(buffer_autoselc); //Tengo que liberar el malloc de buffer_autoselec que le encargue a getline. El getline se ocupo hacerme el malloc
            return NULL;
            }

            }
        }

        //SI es falso que estoy en el PC inicial (con PC inicial nos referimos al que nos pasa el master), osea que estoy en mi segundo fetch, no entro al for otra vez porque me va tirar lineas abajo en el .txt, entro aca para retomar desde donde me adelanto el for
        
        //Ahora, este ultimo getline va retormar la ultima posicion en la que quedo el anterior getline. BAsicamente va comenzar a leer el archivo desde el PC indicado, ignorando las primeras lineas que logre ignorar con el for. NO hay manera de que se confunda con el getline de abajo(**) porque no puede entrar a este y al de abajo a la vez, estan separados por el if del PCB que nunca cambia
        ssize_t leido = getline(&buffer_autoselc, &tam_autoselc, pcb->archivo);

        buffer_autoselc[strcspn(buffer_autoselc, "\n")] = '\0'; //encuentra un \n en el buffer que lee la linea de instruccion y lo cambio por \0(c)
        string_trim(&buffer_autoselc); //elimina espacios vacios a derecha e izquierda, todos los que pueda encontrar (commins)

        if(leido == -1){
            log_info(w->logger, "Error al leer la primer instruccion del PC != 0");
            free(buffer_autoselc);
            return NULL;
        }
        return buffer_autoselc;
        
    }

    else{ //est es el de abajo (**)
    //por aca va entrar solamente un proceso que NO fue interrupido (PC = 0), EL PC DEL PCB NUNCA CAMBIA. ENtonces, cuando llegue al getline, no va confundirse con el getline de la linea de arriba, porque siempre va entrar por aca y va tomar este getline
    ssize_t leido = getline(&buffer_autoselc, &tam_autoselc, pcb->archivo);

    buffer_autoselc[strcspn(buffer_autoselc, "\n")] = '\0'; //encuentra un \n en el buffer que lee la linea de instruccion y lo cambio por \0 (c)
    string_trim(&buffer_autoselc); //elimina espacios vacios a derecha e izquierda, todos los que pueda encontrar (commins) :D

        if(leido == -1){
            log_info(w->logger, "Error al leer la instruccion");
            free(buffer_autoselc); //Tengo que liberar el malloc de buffer_autoselec que le encargue a getline. El getline se ocupo hacerme el malloc
            return NULL;
        }
        return buffer_autoselc;
    }
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
        paquete_decode->fin = false;  //para poner que al principio es falso que sea "fin". Porque yo intente ponerle el false afuera del ciclo (asi lo tenia que poner una vez sola y aca adentro no lo modificaba). Pero al devolver esto (osea, cuando retornaba decode en el ciclo), pisaba todo el struct de instruccion_decf, incluyendo el false y lo cambiaba a otro numero. POr eso decidi que si lo va pisar, que lo pise con el false
        paquete_decode->instruccion_malformada = false;
        return paquete_decode;
    }
     
    if(string_equals_ignore_case(parametros[0], "TRUNCATE")){
        
        char** truncate_file_tam;
        char** truncate_file_tag;

        truncate_file_tam = string_n_split(parametros[1], 2, " ");
        paquete_decode->parametros->tamanio = atoi(truncate_file_tam[1]);
        truncate_file_tag = aux_separar_file_tag(truncate_file_tam[0]);
        paquete_decode->parametros->nombre_file = truncate_file_tag[0];
        paquete_decode->parametros->tag = truncate_file_tag[1];
        
        paquete_decode->ejecuta_instruccion = executeTruncate;

        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;
        return paquete_decode;
    }

    if(string_equals_ignore_case(parametros[0], "WRITE")){

        char** write_params_princ;
        char** write_file_tag;

        write_params_princ = string_n_split(parametros[1],3," ");

        paquete_decode->parametros->direccion_base = atoi(write_params_princ[1]);
        paquete_decode->parametros->contenido = write_params_princ[2];

        write_file_tag = aux_separar_file_tag(write_params_princ[0]);
        paquete_decode->parametros->nombre_file = write_file_tag[0];
        paquete_decode->parametros->tag = write_file_tag[1];
        paquete_decode->ejecuta_instruccion = executeWrite;

        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;
        return paquete_decode;
    }
    
    if(string_equals_ignore_case(parametros[0], "READ")){

        char** read_params_princ;
        char** read_file_tag;

        read_params_princ = string_n_split(parametros[1], 3, " ");
        paquete_decode->parametros->direccion_base = atoi(read_params_princ[1]);
        paquete_decode->parametros->tamanio = atoi(read_params_princ[2]);

        read_file_tag = aux_separar_file_tag(read_params_princ[0]);
        paquete_decode->parametros->nombre_file = read_file_tag[0];
        paquete_decode->parametros->tag = read_file_tag[1];
        paquete_decode->ejecuta_instruccion = executeRead;

        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;
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

        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;
        return paquete_decode;
    }   
    if(string_equals_ignore_case(parametros[0], "COMMIT")){

        
        char** commit_param;

        commit_param = aux_separar_file_tag(parametros[1]);
        paquete_decode->parametros->nombre_file = commit_param[0];
        paquete_decode->parametros->tag = commit_param[1];
        paquete_decode->ejecuta_instruccion = executeCommit;
        
        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;
        return paquete_decode;
    }
    if(string_equals_ignore_case(parametros[0], "FLUSH")){

        
        char** flush_param;

        flush_param = aux_separar_file_tag(parametros[1]);
        paquete_decode->parametros->nombre_file = flush_param[0];
        paquete_decode->parametros->tag = flush_param[1];
        paquete_decode->ejecuta_instruccion = executeFlush;
        
        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;
        return paquete_decode;
    }
    if(string_equals_ignore_case(parametros[0], "DELETE")){

        
        char** delete_param;

        delete_param = aux_separar_file_tag(parametros[1]);
        paquete_decode->parametros->nombre_file = delete_param[0];
        paquete_decode->parametros->tag = delete_param[1];
        paquete_decode->ejecuta_instruccion = executeDelete;

        paquete_decode->fin = false;  //para poner que al principio es falso que sea fin
        paquete_decode->instruccion_malformada = false;
        return paquete_decode;
    }

    if(string_equals_ignore_case(parametros[0], "END")){
    paquete_decode->fin = true;
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

void execute(t_instr_param* parametros, void (*ejecuta_instruccion)(t_instr_param*, t_worker*, Pcb*), t_worker* w, Pcb* pcb){
    ejecuta_instruccion(parametros, w, pcb);
}

char** aux_separar_file_tag(char* cadena){
    char** file_tag;
    file_tag = string_n_split(cadena, 2, ":");
    return file_tag;
    
}



void executeCreate(t_instr_param* parametros, t_worker* w, Pcb* pcb){

    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_Create = crear_paquete(STORAGE_CREATE_FILE, buffer_generico);
    agregar_a_paquete(paquete_Create, &(pcb->query_id), sizeof(pcb->query_id));
    agregar_a_paquete(paquete_Create, parametros->nombre_file, strlen(parametros->nombre_file) + 1); //parametros->nombre_file es un string por eso pasa sin &
    agregar_a_paquete(paquete_Create, parametros->tag, strlen(parametros->tag) + 1); 
    enviar_paquete(paquete_Create, w->storage_socket, w->logger);
    eliminar_paquete(paquete_Create);
    
    
    //rtas_storage(w->storage_socket, w, CREATE);
    log_info(w->logger, "Llegue a hacer create");
    
}

void executeTruncate(t_instr_param* parametros, t_worker* w, Pcb* pcb){

    
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paqueteTruncate = crear_paquete(STORAGE_TRUNCATE, buffer_generico);
    agregar_a_paquete(paqueteTruncate, &(pcb->query_id), sizeof(pcb->query_id));
    agregar_a_paquete(paqueteTruncate, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paqueteTruncate, parametros->tag, strlen(parametros->tag) + 1);
    agregar_a_paquete(paqueteTruncate, &parametros->tamanio, sizeof(int));
    enviar_paquete(paqueteTruncate, w->storage_socket, w->logger);
    eliminar_paquete(paqueteTruncate);
    
    //rtas_storage(w->storage_socket, w, TRUNCATE);
    log_info(w->logger, "Llegue a hacer truncate");
    
}

void executeWrite(t_instr_param* parametros, t_worker* w, Pcb* pcb){
    size_t tam = strlen(parametros->contenido);
    void* dir = acceder_memoria(
        w->mem,
        pcb->query_id,
        parametros->nombre_file,
        parametros->tag,
        parametros->direccion_base,
        tam,
        true // es escritura
    );
        if (dir) {
        memcpy(dir, parametros->contenido, tam);
        log_info(w->logger, "Query<%d>: Instrucción realizada: WRITE", pcb->query_id);
    }
    if(!dir){
        log_info(w->logger, "INtente hacer algo que no puedo en memoria o storage no me dio la pagina que le pedi. ESte es el de query_interpreter"); //este esta porque el enviar error generico lo puse dentro de memoria.c 
    }

}

void executeRead(t_instr_param* parametros, t_worker* w, Pcb* pcb){
    void* dir = acceder_memoria(
        w->mem,
        pcb->query_id,
        parametros->nombre_file,
        parametros->tag,
        parametros->direccion_base,
        parametros->tamanio,
        false // es lectura
    );
    if (!dir) {
        log_error(w->logger, "Query<%d>: Lectura fallida - File:%s - Tag:%s - Offset:%d",
                  pcb->query_id, parametros->nombre_file, parametros->tag, parametros->direccion_base);
        log_info(w->logger, "INtente hacer algo que no puedo en memoria o storage no me dio la pagina que le pedi. ESte es el de query_interpreter");
        return;
    }

    char* contenido = string_substring(dir, 0, parametros->tamanio);
    log_info(w->logger, "Contenido leído: %s", contenido);

    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_read = crear_paquete(WORKER_READ_RESULT, buffer_generico);

    agregar_a_paquete(paquete_read, &(pcb->query_id), sizeof(int)); //envio querID a master
    agregar_a_paquete(paquete_read, contenido, strlen(contenido)+1); //envio contrenido leido a master
    agregar_a_paquete(paquete_read, parametros->nombre_file, strlen(parametros->nombre_file)+1); //envio file a master para loggear en querycontrol
    agregar_a_paquete(paquete_read, parametros->tag, strlen(parametros->tag)+1); //envio tag a master para loggear en querycontrol
    agregar_a_paquete(paquete_read, &(w->interpreter->pc), sizeof(int));  //envio el pc por las dudas

    enviar_paquete(paquete_read, w->master_socket_distpach, w->logger);
    eliminar_paquete(paquete_read);

    free(contenido);
    log_info(w->logger, "Query<%d>: Instrucción realizada: READ", pcb->query_id);
    
}

void executeTag(t_instr_param* parametros, t_worker* w, Pcb* pcb){

    
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_nuevo_tag = crear_paquete(STORAGE_TAG, buffer_generico);
    agregar_a_paquete(paquete_nuevo_tag, &(pcb->query_id), sizeof(pcb->query_id));
    agregar_a_paquete(paquete_nuevo_tag, parametros->nombre_file_org, strlen(parametros->nombre_file_org)+1);
    agregar_a_paquete(paquete_nuevo_tag, parametros->tag_origen, strlen(parametros->tag_origen)+1);
    agregar_a_paquete(paquete_nuevo_tag, parametros->nombre_file_destino, strlen(parametros->nombre_file_destino)+1);
    agregar_a_paquete(paquete_nuevo_tag, parametros->tag_destino, strlen(parametros->tag_destino)+1);
    enviar_paquete(paquete_nuevo_tag, w->storage_socket, w->logger);
    eliminar_paquete(paquete_nuevo_tag);
    
    //rtas_storage(w->storage_socket, w, TAG);
    log_info(w->logger, "Llegue a hacer tag");
    
}

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
    
    //rtas_storage(w->storage_socket, w, COMMIT);
    log_info(w->logger, "Llegue a hacer commit");

}

void executeFlush(t_instr_param* parametros, t_worker* w, Pcb* pcb){ //ESto se hace previo a la ejecucion de un commit y de un desalojo de query


    flush_paginas_modificadas(
        w->mem,
        pcb->query_id,
        parametros->nombre_file,
        parametros->tag,
        w->storage_socket
    );
    //rtas_storage(w->storage_socket, w, FLUSH);
    log_info(w->logger, "Query<%d>: Instrucción realizada: FLUSH", pcb->query_id);

}

void executeDelete(t_instr_param* parametros, t_worker* w, Pcb* pcb){
    
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* paquete_delete = crear_paquete(STORAGE_DELETE, buffer_generico);
    agregar_a_paquete(paquete_delete, &(pcb->query_id), sizeof(pcb->query_id));
    agregar_a_paquete(paquete_delete, parametros->nombre_file, strlen(parametros->nombre_file)+1);
    agregar_a_paquete(paquete_delete, parametros->tag, strlen(parametros->tag)+1);
    enviar_paquete(paquete_delete, w->storage_socket, w->logger);
    eliminar_paquete(paquete_delete);
    
    //rtas_storage(w->storage_socket, w, DELETE);
    log_info(w->logger, "Llegue a hacer delete");
    
}

void executeEnd(t_worker* w, Pcb* pcb){ //avisar a master de la finalizacion
    
    log_info(w->logger, "TErmine el proceso. ESpero uno nuevo");
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* aviso_end_query = crear_paquete(WORKER_QUERY_END, buffer_generico);
    agregar_a_paquete(aviso_end_query, &(pcb->query_id), sizeof(int)); //envio queryId
    enviar_paquete(aviso_end_query, w->master_socket_distpach, w->logger);
    eliminar_paquete(aviso_end_query);
    log_info(w->logger, "termine End");
}

void interrupt_envio_a_master(Pcb* pcb_dsp_de_interrupt, t_worker* w){  //Se envia al socket normal
    log_info(w->logger, "Llego una interrupcion, el proceso fue interrumpido. Espero uno nuevo");
    
    t_buffer* buffer_generico = crear_buffer();
    t_paquete* devuelvo_pcb_master = crear_paquete(WORKER_PC_UPDATE, buffer_generico);
    //TAl vez este no haga falta:
    agregar_a_paquete(devuelvo_pcb_master, pcb_dsp_de_interrupt->nombre_archivo, strlen(pcb_dsp_de_interrupt->nombre_archivo)+1);
    
    //Estos dos si:
    agregar_a_paquete(devuelvo_pcb_master, &(pcb_dsp_de_interrupt->query_id), sizeof(int));
    agregar_a_paquete(devuelvo_pcb_master, &(w->interpreter->pc), sizeof(int));
    enviar_paquete(devuelvo_pcb_master, w->master_socket_distpach, w->logger);
    eliminar_paquete(devuelvo_pcb_master);
    
}