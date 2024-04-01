/*
    Carlos Conde Vicente
    Diego de Castro Merillas
    Daniel Dominguez Parra
    Iker Botana Vázquez
*/

#include <stdio.h> 
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <mpi.h>

#define NUM_NUMEROSADIVINAR 15
#define NUM_MAX 999999
#define TAG_PROCESSOR_NAME 1 // Nombre del procesador en el que se ejecuta el proceso, MPI_MAX_PROCESSOR_NAME x MPI_CHAR
#define PESO_MIM 10000000 // >,=,<
#define PESO_MEDIO 100000

typedef struct estadisticas_numero{
    int numero;
    int I_MIM;
    int O_MIM;
    double tiempo_calculo;
    double tiempo_total;
    int id_PG;
} estadisticas_numero;

typedef struct estadisticas_proceso{
    int tipo;
    double tiempo_total;
    double tiempo_calculo;
    int porcentaje_calculo;
    int numero_send;
    int numero_recv;
    int numero_iprobe;
} estadisticas_proceso;

// PROCESOS
int procesoES(int *idPA, int numPA, int *idPG, int numPG, int size, int *idPI, int numPI);
void procesoPG(int, int, int);
void procesoPA(int rank);
void procesoPI(int rank);

// FUNCIONES
int messageRecvNumber(int proceso_origen, int etiqueta);
void operation_resolve_number(int pa_number, int number_to_resolve, int id_pa, estadisticas_numero *stats_num, estadisticas_proceso *stats_procesos);
int check_status_PA(int id_pa, int indice, estadisticas_numero *stats_num, estadisticas_proceso *stats_procesos);
void fuerza_espera(unsigned long peso);
void construir_mensaje_estadisticas_parciales(estadisticas_numero *envioStructPG, MPI_Datatype *MPI_envioPG);
void construir_mensaje_estadisticas_totales(estadisticas_proceso *envioStructPG, MPI_Datatype *MPI_envioPES);
void guardarEstadisticas(estadisticas_numero recv_stats_num, estadisticas_numero* stats_numeros);
void guardar_estadisticas_totales(estadisticas_proceso recv_stats_proceso, estadisticas_proceso* stats_procesos, int proceso);
void mostrar_estadisticas_totales(estadisticas_proceso *stats_procesos, int size);
void mostrar_estadisticas_parciales(estadisticas_numero *stats_numeros);



int main(int argc, char **argv) {

    int totalproc, rank, size;
    char **nombres;
    int *ranksPA, *ranksPG, *ranksPI;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 3) {
        perror("Error en el  número de argumentos");
        MPI_Finalize();
        exit(1);
    }
    
    if ((argc == 3 && (atoi(argv[1]) + atoi(argv[2])) >= size) || atoi(argv[1]) < 1 || atoi(argv[2]) < 1 ) {
        perror("Error en el número de argumentos 2");
        MPI_Finalize();
        exit(1);
    }
    
    int numPA = atoi(argv[2]), 
        numPG = atoi(argv[1]), 
        numPI = size - numPA - numPG - 1;

    double tiempo_completo_principio;
    
    if(rank == 0){
        tiempo_completo_principio = MPI_Wtime();

        ranksPA = (int *)malloc(numPA * sizeof(int));
        ranksPG = (int *)malloc(numPG * sizeof(int));
        ranksPI = (int *)malloc(numPI * sizeof(int));

        printf("\n***************************************************\n");
        printf("NUMERO DE PROCESOS      : %d", size);
        printf("\nNUMERO DE GESTORES    : %d", numPG);
        printf("\nUMERO DE  >  =  <     : %d", numPA);
        printf("\nNUMERO TOTAL (+E/S)   : %d", numPA+numPG+1);
        printf("\n***************************************************\n");
        printf("\nP%d:\t E/S", rank);

        for(int i = 0; i < numPG; i++){
            ranksPG[i] = i + 1;
            printf("\nP%d:\t GESTOR NUMERO", ranksPG[i]);
        }

        for(int i = 0; i < numPA; i++){
            ranksPA[i] = numPG + i + 1;
            printf("\nP%d:\t ADIVINA > = <", ranksPA[i]);
        }

        for(int i = 0; i < numPI; i++){
            ranksPI[i] = numPG + numPA + i + 1;
            printf("\nP%d:\t SIN TAREA", ranksPI[i]);
        }
    }

    if(rank == 0){ // PROCESO E/S.
        procesoES(ranksPA, numPA, ranksPG, numPG, size, ranksPI, numPI);
        
    } else if(rank > 0 && rank <= numPG){
        procesoPG(rank, size, numPG);
        
    } else if(rank > numPG && rank <= numPG + numPA){        
        procesoPA(rank);
        
    } else  {
        procesoPI(rank);
    }

    if (rank == 0)
    {
        double tiempo_completo_final = MPI_Wtime() - tiempo_completo_principio;
        printf("Tiempo total de ejecución: %f (%d -> %d PG %d PA)\n", tiempo_completo_final, size, numPG, numPA);
    }

    MPI_Finalize();
    return 0;
}

// PROCESO E/S
int procesoES(int *idPA, int numPA, int *idPG, int numPG, int size, int *idPI, int numPI) {
    int indice = 0;
    int recibidos;
    int myID = 0;
    double tiempo_completo_principio = MPI_Wtime();

    estadisticas_proceso stats;
    stats.tipo = 0;
    stats.tiempo_total = 0;
    stats.tiempo_calculo = 0;
    stats.porcentaje_calculo = 0;
    stats.numero_send = 0;
    stats.numero_recv = 0;
    stats.numero_iprobe = 0;

    estadisticas_proceso recv_stats_proceso;
    MPI_Datatype MPI_envioPES;
    construir_mensaje_estadisticas_totales(&recv_stats_proceso, &MPI_envioPES);

    estadisticas_numero recv_stats_num;
    MPI_Datatype MPI_envioPG;
    construir_mensaje_estadisticas_parciales(&recv_stats_num, &MPI_envioPG);

    estadisticas_proceso *stats_procesos = malloc(size * sizeof(estadisticas_proceso));   

    estadisticas_numero stats_numeros[NUM_NUMEROSADIVINAR];
    
    for(int i = 0; i < NUM_NUMEROSADIVINAR; i++){
        stats_numeros[i].numero = 0;
        stats_numeros[i].I_MIM = 0;
        stats_numeros[i].O_MIM = 0;
        stats_numeros[i].tiempo_calculo = 0;
        stats_numeros[i].tiempo_total = 0;
        stats_numeros[i].id_PG = 0;
    }

    srand(time(NULL));

    for(int i = 0; i < NUM_NUMEROSADIVINAR; i++)
        stats_numeros[i].numero = (rand()+2145+i) % NUM_MAX + 1; // Lista de números aleatorios a adivinar 

    // Indicar a los PG cuántos PA hay y sus ID
    for (int i = 0; i < numPG; i++) {     
        stats.numero_send++;
        MPI_Send(&numPA, 1, MPI_INT, idPG[i], 0, MPI_COMM_WORLD); // Envío el número de PA
        stats.numero_send++;
        MPI_Send(idPA, numPA, MPI_INT, idPG[i], 0, MPI_COMM_WORLD); // Envío los IDs de los PA
    }    
   
    // Envío de tareas inicial a todos los PG
    MPI_Status status;
    indice = 0;
    recibidos = 0;
    
    for (int i = 0; i < numPG; i++) {
        if (indice < NUM_NUMEROSADIVINAR) {
            stats.numero_send++;
            MPI_Send(&stats_numeros[indice++].numero, 1, MPI_INT, idPG[i], 0, MPI_COMM_WORLD);
        }
    }
    
    // Bucle de envío de números   
    while (recibidos < NUM_NUMEROSADIVINAR) {
        stats.numero_recv++;
        MPI_Recv(&recv_stats_num, 1, MPI_envioPG, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        recibidos++;
        
        guardarEstadisticas(recv_stats_num, stats_numeros);
        
        if(indice < NUM_NUMEROSADIVINAR){
            stats.numero_send++;
            MPI_Send(&stats_numeros[indice++].numero, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
        }
        
        mostrar_estadisticas_parciales(stats_numeros);
    }
    
    // Esperamos por la recepcion de todos los procesos gestores, entonces, cuando termine de recibir todos los mensajes, se envia a PA para que finalice
    // Iremos recibiendo uno a uno, y cuando tengamos todos, enviamos el mensaje a PA para que finalice
    int aux=-1;
    for(int i=0; i<numPG; i++){
        stats.numero_send++;
        MPI_Send(&aux, 1, MPI_INT, idPG[i], 0, MPI_COMM_WORLD);
        stats.numero_recv++;
        MPI_Recv(&recv_stats_proceso, 1, MPI_envioPES, idPG[i], 1, MPI_COMM_WORLD, &status);
        guardar_estadisticas_totales(recv_stats_proceso, stats_procesos, idPG[i]);        
    }
    
    //Como ya hemos recibido todos, entendemos que ya ha terminado y enviamos el mensaje a PA para que finalice  
    for(int i=0 ; i<numPA;i++){
        stats.numero_send++;
        MPI_Send(&aux, 1, MPI_INT, idPA[i], 1, MPI_COMM_WORLD);
        stats.numero_recv++;
        MPI_Recv(&recv_stats_proceso, 1, MPI_envioPES, idPA[i], 1, MPI_COMM_WORLD, &status);
        guardar_estadisticas_totales(recv_stats_proceso, stats_procesos, idPA[i]);
    }
   
    int acabar = -1;    
    for(int i=0; i<numPI; i++){
        stats.numero_send++;
        MPI_Send(&acabar, 1, MPI_INT, idPI[i], 1, MPI_COMM_WORLD);
        stats.numero_recv++;
        MPI_Recv(&recv_stats_proceso, 1, MPI_envioPES, idPI[i], 1, MPI_COMM_WORLD, &status);
        guardar_estadisticas_totales(recv_stats_proceso, stats_procesos, idPI[i]);
    } 
   
    stats.tiempo_total = MPI_Wtime() - tiempo_completo_principio;
    guardar_estadisticas_totales(stats, stats_procesos, 0);
    mostrar_estadisticas_totales(stats_procesos, size);

    return  0;
}

// PROCESO PG
void procesoPG(int mi_rango, int num_procesos, int numPG){
    
    // Espera a que el PES les identifique como PG o bien les mande acabar.
    int proceso_saber;
    int proc_llegada = 0;
    MPI_Status status;
    int origin_process = 0;
    int etiqueta = 0;
    int check = -1;
    int numIDsPA;
    int *idPAs;
    double tiempo_completo_principio = MPI_Wtime();

    estadisticas_proceso stats;
    stats.tipo = 1;
    stats.tiempo_total = 0;
    stats.tiempo_calculo = 0;
    stats.porcentaje_calculo = 0;
    stats.numero_send = 0;
    stats.numero_recv = 0;
    stats.numero_iprobe = 0; 
    
    MPI_Datatype MPI_envioPES;
    construir_mensaje_estadisticas_totales(&stats, &MPI_envioPES);

    MPI_Datatype MPI_envioPG;
    estadisticas_numero stats_numero;
   
    stats_numero.id_PG = mi_rango;
    stats_numero.numero = 0;
    stats_numero.I_MIM = 0;
    stats_numero.O_MIM = 0;
    stats_numero.tiempo_calculo = 0;
    stats_numero.tiempo_total = 0;

    construir_mensaje_estadisticas_parciales(&stats_numero, &MPI_envioPG);

    // Recibe el numero de PAs que hay
    MPI_Recv(&numIDsPA, 1, MPI_INT, origin_process, etiqueta, MPI_COMM_WORLD, &status);
    stats.numero_recv++;

    // Reservo memoria para el arrray que me va a llegar
    idPAs = (int *)malloc(numIDsPA * sizeof(int));
    if(idPAs == NULL){
        perror("Error en la reserva de memoria");
        exit(1);
    }

    // Recibe un array con los id de los PAs que hay
    MPI_Recv(idPAs, numIDsPA, MPI_INT, origin_process, etiqueta, MPI_COMM_WORLD, &status);
    stats.numero_recv++;
    
    etiqueta = 0;
    origin_process = 0;
    int number;

    number = messageRecvNumber(origin_process, etiqueta);
    stats.numero_recv++;

    // Si el numero es -1, el proceso finaliza
    while(number != -1){
        stats_numero.id_PG = mi_rango;
        stats_numero.numero = number;
        stats_numero.I_MIM = 0;
        stats_numero.O_MIM = 0;
        stats_numero.tiempo_calculo = 0;
        stats_numero.tiempo_total = 0;
        double tmp_inic_num = MPI_Wtime();

        // Si llega aquí, el proceso pg tiene un número. Por tanto, pasamos a comprobar la lista de procesos PA para ver si hay alguno libre
        while (check == -1)
        {
            for(int enum_id_pa = 0 ; enum_id_pa<numIDsPA ; enum_id_pa++){
                // Bucle que esperará hasta que haya un proceso PA libre
                check = check_status_PA(idPAs[enum_id_pa], enum_id_pa, &stats_numero, &stats);
                    
                if(check == -1){
                    // Proceso PA ocupado, pasamos al siguiente de la lista
                    continue;   
                }
                
                else{
                    // Proceso PA libre
                    operation_resolve_number(check, number, idPAs[enum_id_pa], &stats_numero, &stats);
                    check = 0;
                    break;
                }
            }
        }
        
        stats_numero.tiempo_total = MPI_Wtime() - tmp_inic_num;
        stats.numero_send++;
        
        MPI_Send(&stats_numero, 1, MPI_envioPG, 0, 0, MPI_COMM_WORLD);
        number = messageRecvNumber(origin_process, etiqueta);
        
        check = -1;
    }
    
    // Hemos enviado las estadisiticas por lo que sabemos que ha terminado y finalizamos
    stats.tiempo_total = MPI_Wtime() - tiempo_completo_principio;
    stats.porcentaje_calculo = stats_numero.tiempo_calculo / stats.tiempo_total * 100;
    stats.numero_send++;
    MPI_Send(&stats, 1, MPI_envioPES, 0, 1, MPI_COMM_WORLD);
    
    return;
}

// PROCESO PA
void procesoPA(int myrank){

    int numMin = 0;
    int numMax = NUM_MAX;
    int numMedio = 0;
    int finalizar = 0;
    int info_proc;
    MPI_Status status;

    estadisticas_proceso stats;
    stats.tipo = 2;
    stats.tiempo_total = 0;
    stats.tiempo_calculo = 0;
    stats.porcentaje_calculo = 0;
    stats.numero_send = 0;
    stats.numero_recv = 0;
    stats.numero_iprobe = 0;

    MPI_Datatype MPI_envioPES;
    construir_mensaje_estadisticas_totales(&stats, &MPI_envioPES);

    int state_var = 0; // 0 --> libre; -1 --> ocupado. Inicialmente esta a 0 para que entre en el bucle
    int variable_for_the_count;
    int pg_id = -1;
    int tag;
    double tiempo_completo_principio = MPI_Wtime();
    double tiempo_calculo_principio;
    int numero_ocupado = 0;
    
    while(1){
        if(state_var == 0){
            numMax = NUM_MAX;
            numMin = 0;
            MPI_Probe(MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status); // Bloquea el proceso PA para esperar si el proceso PG está libre --> Etiqueta = 2 para recibir el mensaje de PA
            pg_id = status.MPI_SOURCE;
            stats.numero_recv++;
            MPI_Recv(&info_proc, 1, MPI_INT, pg_id, 1, MPI_COMM_WORLD, &status);
            
            if (info_proc == -1) {
                stats.tiempo_total = MPI_Wtime() - tiempo_completo_principio;
                stats.porcentaje_calculo = stats.tiempo_calculo / stats.tiempo_total * 100;
                stats.numero_send++;
                MPI_Send(&stats, 1, MPI_envioPES, 0, 1, MPI_COMM_WORLD);
                return;
            }
            
            numMedio = numMin+(numMax - numMin)/2;         
            stats.numero_send++;
            tiempo_calculo_principio = MPI_Wtime();
            
            MPI_Send(&numMedio, 1, MPI_INT, pg_id, 3, MPI_COMM_WORLD);  
          
            state_var = -1;  // PA está ocupado
            numero_ocupado = 0;
     
        } else if (state_var == -1) {
            MPI_Iprobe(MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &finalizar, &status);
            stats.numero_iprobe++;
            
            if (finalizar != 0) {
                numero_ocupado++;
                int emissor = status.MPI_SOURCE;
                stats.numero_recv++;
                MPI_Recv(&info_proc, 1, MPI_INT, emissor, 1, MPI_COMM_WORLD, &status);
               
                stats.numero_send++;
                MPI_Send(&state_var, 1, MPI_INT, emissor, 3, MPI_COMM_WORLD);
            }
        }
        
        // Recibe el mensaje del proceso PG
        stats.numero_recv++;
        MPI_Recv(&variable_for_the_count, 1, MPI_INT, pg_id, 4, MPI_COMM_WORLD, &status);

        fuerza_espera(PESO_MIM);

        if(variable_for_the_count == 0){
            numMin = numMedio;
            numMedio = numMin+(numMax - numMin)/2;
        }

        else if (variable_for_the_count == 2)
        {
            numMax = numMedio;
            numMedio = numMin+(numMax - numMin)/2;
        }

        if(variable_for_the_count == 1){
            state_var = 0;
            stats.tiempo_calculo = stats.tiempo_calculo + MPI_Wtime() - tiempo_calculo_principio;
            
            stats.numero_send++;
            MPI_Send(&numero_ocupado, 1, MPI_INT, pg_id, 5, MPI_COMM_WORLD);
        }
        
        else
        {
            //pregunta a PG por el numero medio
            stats.numero_send++;
            MPI_Send(&numMedio, 1, MPI_INT, pg_id, 5, MPI_COMM_WORLD);   
        }
    }
}

// PROCESO PI 
void procesoPI(int myrank){

    int buf;
    MPI_Status status;
    
    // Calcular el tiempo total
    estadisticas_proceso stats;
    MPI_Datatype MPI_envioPES;
    construir_mensaje_estadisticas_totales(&stats, &MPI_envioPES);
   
    stats.tipo = 3;
    stats.tiempo_total = 0;
    stats.tiempo_calculo = 0;
    stats.porcentaje_calculo = 0;
    stats.numero_send = 0;
    stats.numero_recv = 0;
    stats.numero_iprobe = 0;
    
    double tmp_inic = MPI_Wtime();
    
    // Recv bloqueante hasta que se diga que se para el proceso que se quiere finalizar
    MPI_Recv(&buf, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
    stats.numero_recv = stats.numero_recv + 1;  

    // Enviar las estadísticas totales al PES
    stats.numero_send = stats.numero_send + 1;
    stats.tiempo_total = MPI_Wtime() - tmp_inic;
    stats.porcentaje_calculo = stats.tiempo_calculo / stats.tiempo_total * 100;
    MPI_Send(&stats, 1, MPI_envioPES, 0, 1, MPI_COMM_WORLD);
    
    return;
}

// FUNCIONES
void operation_resolve_number(int pa_number, int number_to_resolve, int id_pa, estadisticas_numero *stats_num, estadisticas_proceso *stats_proceso){

    MPI_Status status;
    int valor_a_enviar;
    double tiempo_calculo_comienzo = MPI_Wtime();
    
    while(pa_number != number_to_resolve){
        fuerza_espera(PESO_MEDIO);
        stats_num->I_MIM++;

        if( number_to_resolve > pa_number){
            // Numero del proceso PA menor que el numero a resolver
            valor_a_enviar = 0;
            stats_proceso->numero_send++;
            MPI_Send(&valor_a_enviar, 1, MPI_INT, id_pa, 4, MPI_COMM_WORLD);
        }
        
        else if (number_to_resolve < pa_number){
            // Numero del proceso PA mayor que el numero a resolver
            valor_a_enviar = 2;
            stats_proceso->numero_send++;
            MPI_Send(&valor_a_enviar, 1, MPI_INT, id_pa, 4, MPI_COMM_WORLD);
        }   

        // Recibir nuevo número
        stats_proceso->numero_recv++;
        MPI_Recv(&pa_number, 1, MPI_INT, id_pa, 5, MPI_COMM_WORLD, &status);
    }

    // Proceso PA adivina el número
    stats_num->tiempo_calculo = MPI_Wtime() - tiempo_calculo_comienzo;
    stats_proceso->tiempo_calculo = stats_proceso->tiempo_calculo + stats_num->tiempo_calculo;
    valor_a_enviar = 1;
    stats_proceso->numero_send++;
    MPI_Send(&valor_a_enviar, 1, MPI_INT, id_pa, 4, MPI_COMM_WORLD);
    stats_proceso->numero_recv++;
    MPI_Recv(&pa_number, 1, MPI_INT, id_pa, 5, MPI_COMM_WORLD, &status);
    stats_num->O_MIM = pa_number;
    
    return;
}

int check_status_PA(int id_pa, int indice, estadisticas_numero *stats_num, estadisticas_proceso *stats_procesos){

    MPI_Status status;
    int respuesta;

    // Mandar mensaje al proceso PA con el id, para resolver un nuevo numero. Tag = 2
    stats_procesos->numero_send++;
    MPI_Send(&id_pa, 1, MPI_INT, id_pa, 1, MPI_COMM_WORLD);

    // Recibo bloqueante para esperar respuesta del proceso PA --> number = 0 --> libre; number = 1 --> ocupado. Tag = 3 
    stats_procesos->numero_recv++;
    MPI_Recv(&respuesta, 1, MPI_INT, id_pa, 3, MPI_COMM_WORLD, &status);
    
    if(respuesta == -1){
        return -1;
    }
    
    else if(respuesta >= 0 && respuesta <= NUM_MAX){
        stats_num->I_MIM++;
        return respuesta;
    }
    
    else{
        return -1;
    }
}

int messageRecvNumber(int proceso_origen, int etiqueta){
    
    MPI_Status estado;
    int mensaje_recibido;
    MPI_Recv(&mensaje_recibido, 1, MPI_INT, proceso_origen, 0, MPI_COMM_WORLD, &estado);

    // Comprobación para ver si el número es válido
    if(mensaje_recibido > 0 && mensaje_recibido < NUM_MAX){
        return mensaje_recibido;
    }

    if(mensaje_recibido == -1){
        return -1;
    }

    return -1;

}

void fuerza_espera(unsigned long peso){

    for (unsigned long i = 1; i < 1 * peso; i++)
    {
        int x = sqrt(i);
    }
}

void construir_mensaje_estadisticas_parciales(estadisticas_numero *envioStructPG, MPI_Datatype *MPI_envioPG){

    MPI_Datatype tipos[6];
    int longitudes[6];
    MPI_Aint direcc[7];
    MPI_Aint desplaz[6];

    tipos[0] = MPI_INT;
    tipos[1] = MPI_INT;
    tipos[2] = MPI_INT;
    tipos[3] = MPI_DOUBLE;
    tipos[4] = MPI_DOUBLE;
    tipos[5] = MPI_INT;

    longitudes[0] = 1;
    longitudes[1] = 1;
    longitudes[2] = 1;
    longitudes[3] = 1;
    longitudes[4] = 1;
    longitudes[5] = 1;

    MPI_Get_address(&(envioStructPG->numero), &direcc[0]);
    MPI_Get_address(&(envioStructPG->I_MIM), &direcc[1]);
    MPI_Get_address(&(envioStructPG->O_MIM), &direcc[2]);
    MPI_Get_address(&(envioStructPG->tiempo_calculo), &direcc[3]);
    MPI_Get_address(&(envioStructPG->tiempo_total), &direcc[4]);
    MPI_Get_address(&(envioStructPG->id_PG), &direcc[5]);

    desplaz[0] = 0;
    desplaz[1] = direcc[1] - direcc[0];
    desplaz[2] = direcc[2] - direcc[0];
    desplaz[3] = direcc[3] - direcc[0];
    desplaz[4] = direcc[4] - direcc[0];
    desplaz[5] = direcc[5] - direcc[0];

    MPI_Type_create_struct(6, longitudes, desplaz, tipos, MPI_envioPG);
    MPI_Type_commit(MPI_envioPG);
}

void construir_mensaje_estadisticas_totales(estadisticas_proceso *envioStructPG, MPI_Datatype *MPI_envioPES){

    MPI_Datatype tipos[7];
    int longitudes[7];
    MPI_Aint direcc[8];
    MPI_Aint desplaz[7];

    tipos[0] = MPI_INT;
    tipos[1] = MPI_DOUBLE;
    tipos[2] = MPI_DOUBLE;
    tipos[3] = MPI_INT;
    tipos[4] = MPI_INT;
    tipos[5] = MPI_INT;
    tipos[6] = MPI_INT;

    longitudes[0] = 1;
    longitudes[1] = 1;
    longitudes[2] = 1;
    longitudes[3] = 1;
    longitudes[4] = 1;
    longitudes[5] = 1;
    longitudes[6] = 1;

    MPI_Get_address(&(envioStructPG->tipo), &direcc[0]);
    MPI_Get_address(&(envioStructPG->tiempo_total), &direcc[1]);
    MPI_Get_address(&(envioStructPG->tiempo_calculo), &direcc[2]);
    MPI_Get_address(&(envioStructPG->porcentaje_calculo), &direcc[3]);
    MPI_Get_address(&(envioStructPG->numero_send), &direcc[4]);
    MPI_Get_address(&(envioStructPG->numero_recv), &direcc[5]);
    MPI_Get_address(&(envioStructPG->numero_iprobe), &direcc[6]);

    desplaz[0] = 0;
    desplaz[1] = direcc[1] - direcc[0];
    desplaz[2] = direcc[2] - direcc[0];
    desplaz[3] = direcc[3] - direcc[0];
    desplaz[4] = direcc[4] - direcc[0];
    desplaz[5] = direcc[5] - direcc[0];
    desplaz[6] = direcc[6] - direcc[0];

    MPI_Type_create_struct(7, longitudes, desplaz, tipos, MPI_envioPES);
    MPI_Type_commit(MPI_envioPES);
}

void guardarEstadisticas(estadisticas_numero recv_stats_num, estadisticas_numero* stats_numeros) {
    
    for(int i = 0; i < NUM_NUMEROSADIVINAR; i++){
        if(stats_numeros[i].numero == recv_stats_num.numero){
            stats_numeros[i] = recv_stats_num;
        }
    }
}

void guardar_estadisticas_totales(estadisticas_proceso recv_stats_proceso, estadisticas_proceso* stats_procesos, int proceso) {

    stats_procesos[proceso] = recv_stats_proceso;
}

void mostrar_estadisticas_totales(estadisticas_proceso *stats_procesos, int size){

    double tiempo_total_total = 0;
    double tiempo_calculo_total = 0;
    int porcentaje_calculo_total = 0;
    int numero_send_total = 0;
    int numero_recv_total = 0;
    int numero_iprobe_total = 0;

    printf("\n\n\n***************************************************");
    printf("\nESTADISTICAS FINALES DE LOS PROCESOS");
    printf("\nPROCESO\tTIPO\tT_TOTALl\tT_CALCULO\t%%CALC\tN_SEND\tN_RECV\tN_IPROBE\n");

    for(int i = 0; i < size; i++){
        printf("\n%d\t%d\t%f\t%f\t%d%%\t%d\t%d\t%d\n", i, stats_procesos[i].tipo,stats_procesos[i].tiempo_total, stats_procesos[i].tiempo_calculo, 
        stats_procesos[i].porcentaje_calculo, stats_procesos[i].numero_send, stats_procesos[i].numero_recv, stats_procesos[i].numero_iprobe);

        tiempo_total_total += stats_procesos[i].tiempo_total;
        tiempo_calculo_total += stats_procesos[i].tiempo_calculo;
        numero_send_total += stats_procesos[i].numero_send;
        numero_recv_total += stats_procesos[i].numero_recv;
        numero_iprobe_total += stats_procesos[i].numero_iprobe;
    }

    porcentaje_calculo_total = tiempo_calculo_total / tiempo_total_total * 100;
    printf("\nTotal----\t%f\t%f\t%d%%\t%d\t%d\t%d\n", tiempo_total_total, tiempo_calculo_total, 
    porcentaje_calculo_total, numero_send_total, numero_recv_total, numero_iprobe_total);
    printf("\n***************************************************\n");
    
    return;
}

void mostrar_estadisticas_parciales(estadisticas_numero *stats_numeros){

    int I_MIM_total = 0;
    int O_MIM_total = 0;
    double tiempo_calculo_total = 0;
    double tiempo_total_total = 0;

    printf("\n\n\nNN\tNumero\tI_MIM\tO_MIM\tTiempo Calculo\tTiempo Total\tProceso PG\n");

    for(int i = 0; i < NUM_NUMEROSADIVINAR; i++){
        printf("\n%d\t%d\t%d\t%d\t%f\t%f\tprocesoPG->%d\n", i, stats_numeros[i].numero, stats_numeros[i].I_MIM, 
        stats_numeros[i].O_MIM, stats_numeros[i].tiempo_calculo, 
        stats_numeros[i].tiempo_total, stats_numeros[i].id_PG);

        I_MIM_total += stats_numeros[i].I_MIM;
        O_MIM_total += stats_numeros[i].O_MIM;
        tiempo_calculo_total += stats_numeros[i].tiempo_calculo;
        tiempo_total_total += stats_numeros[i].tiempo_total;
    }
    
    printf("\nTotal----\t%d\t%d\t%f\t%f\n", I_MIM_total, O_MIM_total, tiempo_calculo_total, tiempo_total_total);
   
}
