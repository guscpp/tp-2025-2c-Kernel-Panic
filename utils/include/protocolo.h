#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdint.h>
#include <commons/string.h>

typedef enum
{
	CPU_HANDSHAKE,  //Desde la cpu
    MASTER_HANDSHAKE,
    STORAGE_HANDSHAKE,
    STORAGE_TAMANO_BLOQUE,
    WORKER_HANDSHAKE
} op_code;

#endif /* PROTOCOLO_H_ */