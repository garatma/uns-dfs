#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "protocolo.h"
#include "nodo-nodo/socketNodos.h"
#include "nodo-nodo/constantes.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netdb.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include "comunicacion.h"

/*
 typedef struct {
    u_int Mensaje_len;
    char *Mensaje_val;
 }Mensaje; 
*/

#define max_args 3  //Numero maximo de argumentos (-1) cuando se trate de un comando externo
#define max 100  //Numero de caracteres maximo para comando las variables de ambiente

#define AMARILLO "\x1b[33m"
#define ROJO "\x1b[31m"
#define VERDE "\x1b[32m"
#define AZUL "\x1b[34m"
#define NORMAL "\x1b[0m"

/*Declara variables*/
char comando[max]; //comando va a leer el comando que ingrese el usuario
char* args[max_args];
char *path[max];
CLIENT *clnt;
char ip[15];

/*Declara funciones*/
void separarArgumentos();
void listarDirectorio();
void ejecutarCD();
void editor();
void ejecutarMKDIR();
void rm();
void mv();
void cp();
int  cpAux(char*,char*);
char* getMyIp();
void salir();
void help();
/* Structs para el manejo del current working directory */

typedef struct{
    char* name;
    uint size;
}SubDirectorio;
SubDirectorio sd_actual;
SubDirectorio raiz;    	

int isDirectory(const char *path) {
   struct stat statbuf;
   if (stat(path, &statbuf) != 0)
       return 0;
   return S_ISDIR(statbuf.st_mode);
}


int searchFolderAndFile(char* array, char* folder, char* file)
{
    //printf("Entro al searchFolderAndFile, array %s, folder %s, file %s\n",array,folder,file);
    char* carpetaObtenida = strtok(array,",");
    while(carpetaObtenida!=NULL)
    {
	//printf("Buscando...\n");
	char* archivoObtenido = strtok(NULL,",");
	if(!strcmp(carpetaObtenida,folder) && !strcmp(archivoObtenido,file))
	{
	    //printf("Encontre.\n");
	    //Son iguales
	    return 1;
	}
	carpetaObtenida = strtok(NULL,",");
    }
    return 0;
}

int inicializador()
{
	system("./getFiles.sh"); //Ejecuto el bash para obtener los archivos del nodo
	//Ahora tengo que leer el archivo Log
	FILE *fp = fopen("log", "r");
	if (fp == 0)
	{
		//printf("Ocurrio un error al abrir el archivo.\n");
		return -1;
	}
	else
	{
		//Debo leer el contenido
		char * line = NULL;
		size_t len = 0;
		ssize_t read;
		char* myIp = getMyIp();
		
		Mensaje toSend = {
		    1+strlen(myIp),
		    myIp
		};
		
		Mensaje* msg = get_files_ip_1(&toSend,clnt);
		char* arr =malloc(2048*sizeof(char));
		strcpy(arr, msg->Mensaje_val);
		//printf("Los archivos que recibi son: %s.\n",msg->Mensaje_val);
		//Ahora que obtuve los archivos debo revisar lo que tengo en la carpeta
		getline(&line, &len, fp); //Consumo la primer linea
		while ((read = getline(&line, &len, fp)) != -1) {

			//printf("Retrieved line of length %zu:\n", read);
			//printf("%s", line);
			//Ahora debo obtener el nombre del archivo unicamente
			char *directory;
			/* get the first token */
			directory = strtok(line, "/");
			char* filename = strtok(NULL,"/");
			char *c = strchr(filename, '\n');
			if (c)
			    *c = '\0';
			//printf("El nombre del archivo a ingresar es: %s.\n",filename);
			char direccion[128];
			sprintf(direccion,"%s/%s",directory,filename);
			//printf("La direccion generada es: %s\n",direccion);
			if(!isDirectory(direccion)) //Si es un archivo
			{
			    //printf("Es un archivo.\n");
			    //Deberia primero chequear si existe o no
			    //Ahora debo ingresarlos a la bd en la raiz
			    if(!searchFolderAndFile(arr,"raiz",filename))
			    { //Si no lo tenia
				//printf("Yo no lo habia subido nunca.\n");
				if(!exists(clnt,'1',filename,"raiz")){
				    //printf("No existe asi que lo creo.\n");
				    report_create(clnt, '1', filename, myIp, "raiz");
				}
				else
				{
				    //printf("Ya existe con ese nombre.\n");
				    //Existe en la bd pero no es mio
				    //Debo renombrar hasta que se pueda meter
				    char renombrado[128];
				    int i = 1;
				    int termine = 0;
				    while(!termine)
				    {
					sprintf(renombrado,"%s(%d)",filename,i);
					if(!exists(clnt,'1',renombrado,"raiz"))
					{
					    //printf("El nuevo nombre para el archivo sera: %s.\n",renombrado);
					    termine = 1;
					    report_create(clnt, '1', renombrado, myIp, "raiz");
					}
					else
					    i++;
				    }
				}
			    }
			    else{
				//printf("Ya esta subido en la bd con mi IP.\n");
			    }
			}
			else
			{
			    //printf("Es una carpeta.\n");
			    if(!exists(clnt,'0',filename,"raiz")){
				//printf("No existe, asi que debo crearla.\n");
				//Creo la carpeta, aunque debo ver si existe antes
				report_create(clnt, '0', filename, myIp, "raiz");
			    }
			    //printf("Voy a crear el archivo.\n");
			    char* nuevoFilename = strtok(NULL,"/");
			    if(nuevoFilename != NULL)
			    {
				//printf("El nuevo filename que agarre es: %s\n",nuevoFilename);
				c = strchr(nuevoFilename, '\n');
				if (c)
				    *c = '\0';
				if(!searchFolderAndFile(arr,filename,nuevoFilename))
				{
				    if(!exists(clnt,'1',nuevoFilename,filename))
				    {
					//printf("Voy a crear en carpeta %s.\n",filename);
					report_create(clnt, '1', nuevoFilename, myIp, filename);
					//printf("Cree el archivo.\n");
				    }
				    else
				    {
					//printf("Ya existe con ese nombre.\n");
					//Existe en la bd pero no es mio
					//Debo renombrar hasta que se pueda meter
					char renombrado[128];
					int i = 1;
					int termine = 0;
					while(!termine)
					{
					    sprintf(renombrado,"%s(%d)",filename,i);
					    if(!exists(clnt,'1',renombrado,filename))
					    {
						//printf("El nuevo nombre para el archivo sera: %s.\n",renombrado);
						termine = 1;
						report_create(clnt, '1', renombrado, myIp, filename);
					    }
					    else
						i++;
					}
				    }
				}
				else
				{
				    //printf("Ya esta subido en la bd con mi IP.\n");
				}
			    }
			    
			}
			strcpy(arr,msg->Mensaje_val);
		}
		fclose(fp);
	}
}

void* setPath(){

	
}

void ejecutarMKDIR()
{
    if(args[1]==NULL){
		printf("uso: mkdir directorio \n");
    }
	else 
    {
		//Pregunta al coordinador si es valido un directorio con 0.	    


		int maxChar = 512;
		char* cadena = malloc(maxChar * sizeof(char));
		memset(cadena,'\0',1);
		char tipoChar[2];
		sprintf(tipoChar,"%d",0);
		strcat(cadena,tipoChar);
		strcat(cadena,",");
		strcat(cadena,args[1]);
		Mensaje mkdir =
		{
			strlen(cadena),
			cadena
		};
		if(strcmp((char*)path,"/")){ //caso en el que no estoy en root
		    printf("No puedes crear mas niveles de carpetas \n");
		}else{
		    int valid = *exists_1(&mkdir,clnt);
		    //int valid = 0;
		    if(valid)
		    {
			if(valid==1){
			    printf("El directorio ingresado ya existe \n");
			}else{
			    printf("Error DFS: No se pudo crear \n");
			}
		    }
		    else
		    {
			    char size_ip[2];
			    int size = strlen(ip);
			    sprintf(size_ip,"%d",size);
			    char contenido_mensaje[1+sd_actual.size+strlen(ip)+2];
			    strcpy(contenido_mensaje,"0");
			    strcat(contenido_mensaje,args[1]);
			    strcat(contenido_mensaje,size_ip);
			    strcat(contenido_mensaje,(char*)ip);
			    int maxChar = 512;
			    char* cadena = malloc(maxChar * sizeof(char));
			    memset(cadena,'\0',1);
			    char tipoChar[2];
			    sprintf(tipoChar,"%d",0);
			    strcat(cadena,tipoChar);
			    strcat(cadena,",");
			    strcat(cadena,args[1]);
			    strcat(cadena,",");
			    strcat(cadena,ip);
			    Mensaje mkdir_report =
			    {
				    strlen(cadena),
				    cadena
			    };
			    printf("EL contenido de mensaje es %s \n",cadena);
			    int size_dir= strlen(args[1]);
			    char buffer[size_dir+6];
			    strcpy((char*)buffer,"mkdir ");
			    strcat((char*)buffer,(char*)args[1]);
			    system(buffer);
			    report_create_1(&mkdir_report,clnt);
			    
		    }
		}
		
    }
}

void obtenerIP(){
    system("hostname -I > nombre");
    FILE* arch = fopen("nombre","r");
    fscanf(arch,"%s",ip);	
    fclose(arch);
    remove("nombre");
}

int main(int argc, char *argv[]){
    memset(ip,'\0',15);
    obtenerIP();
    char *srv;

    if(argc < 2)
    {
	    printf("El argumento deben ser <ip>\n");
	    exit(1);
    }

    srv = argv[1];

    clnt = clnt_create(srv, PROY2DFS, PROY2DFSVERS,"tcp");

    if(clnt == (CLIENT*)NULL)
    {
		clnt_pcreateerror(srv);
		exit(2);
    }
    
    // por si hay que testear algo temporal
	if(argc > 2 && !strcmp(argv[2], "debug")) {
		// modo debug
	}
	else
		// modo normal
		startListening(clnt);
    
    int seguir=1;

    raiz.name = (char*) malloc(sizeof(char)*5); /* Reservo lugar para '/' y '\0' */
    strcpy(raiz.name,"raiz");
    raiz.size = strlen(raiz.name);
    
 
    sd_actual = raiz;    
    
    strcpy((char*)path,"/");
    inicializador();
    while(seguir){
        printf(" "AZUL"%s "VERDE"$"NORMAL" ",path);
        __fpurge(stdin); //Limpia el buffer de entrada del teclado.
        memset(comando,'\0',max);  //Borra cualquier contenido previo del comando.
        scanf("%[^\n]s",comando);   //Espera hasta que el usuario ingrese algun comando.
        if(strlen(comando)>0){   //verifica que haya ingresado algo seguido de un enter
            separarArgumentos();    //Separar comando de sus argumentos//
            if (strcmp(args[0],"ls")==0){
                listarDirectorio();
            }else if(strcmp(args[0],"exit")==0){
                //seguir=0;
                salir();
            }else if(strcmp(args[0],"cd")==0){
                ejecutarCD();
            }else if(strcmp(args[0],"editor")==0){
                editor();
	    	}else if(strcmp(args[0],"mkdir")==0){
				ejecutarMKDIR();
	    	}else if(strcmp(args[0],"rm")==0){
				rm();
			}else if(strcmp(args[0],"cp")==0){
				cp();
            }
			else if(strcmp(args[0],"mv")==0){
				mv();
	    	}
			else if(strcmp(args[0],"help")==0){
				help();
	    	}
	    	else{
                printf("No se reconoce el comando ingresado\n");
            }
        }
    }
    clnt_destroy(clnt);
}

void separarArgumentos(){
    int i;
    for(i=0; i<max_args; i++)   //borra argunmentos antiguos.
        args[i]=NULL;
    const char s[2]=" ";
    char* token=strtok(comando,s);    //separa "comando" en multiples strings segun la aparicion de s.
    for(i=0; i<max_args && token!=NULL; i++){
        args[i]=token;
        token=strtok(NULL,s);   //Avanza hacia el siguiente string.
    }
}

/*1 el cliente solicita a la shell ejecutar cd con el nombre de la carpeta N que desea acceder [COM5]
2 el nodo le pregunta al coordinador si la direccion N es valido [COM3Y6]
3 el coordinador utiliza su tabla para saber si N es una direccion valida para ser accedida [COM8][COM4]
4 si la ruta es valida el coordinador retorna true, false en caso contrario [COM8][COM4]
5 en caso de recibir false muestra por pantalla mensaje de error [COM5]
6 en caso de recibir true, el nodo accede al directorio NR y actualiza su dirActual[COM5]
*/


void ejecutarCD(){
    if(args[1]==NULL){
	printf("uso: cd directorio \n");
    }else //chequear ..
    {
	if(strcmp(args[1], "..")==0){
	    if(strcmp(sd_actual.name,"raiz")){
		memset(path,'\0',max);
		strcpy((char*)path,"/");
		strcpy(sd_actual.name,"raiz");
		sd_actual.size = strlen("raiz");

		//ruta.sub_directorios[1] = NULL;
	    }else{
		printf("No se pueda ir al padre de la raiz \n");
	    }
	}else{
	    //Pregunta al coordinador si es valido un directorio con 0.
	
	    //~ int maxChar = 512;
	    //~ char* cadena = malloc(maxChar * sizeof(char));
	    //~ memset(cadena,'\0',1);
	    //~ char tipoChar[2];
	    //~ sprintf(tipoChar,"%d",0);
	    //~ strcat(cadena,tipoChar);
	    //~ strcat(cadena,",");
	    //~ strcat(cadena,args[1]);
	    //~ Mensaje cd =
	    //~ {
		    //~ strlen(cadena),
		    //~ cadena
	    //~ };
	    //~ int valid = *exists_1(&cd,clnt);
	    
	    int valid = exists(clnt, '0', args[1], NULL);
	    if(valid){
		memset(path,'\0',max);
		strcpy((char*)path,"/");
		sd_actual.size = strlen(args[1]);
		strcpy(sd_actual.name,args[1]);
		strcat((char*)path,sd_actual.name);
		
	    }else{
		printf("El directorio ingresado no existe \n");
	    }
	}
    }
}


void editor(){
    pid_t parent = getpid();
    pid_t pid = fork();

    if (pid == -1)
    {
	// error, failed to fork()
    } 
    else if (pid > 0)
    {
	int status;
	waitpid(pid, &status, 0);
	//En status tenes los posibles errores del editor.
	
	//Dependiendo del error mostrar un mensaje.
    }
    else 
    {
	if(args[1]!=NULL){
	    execl("editor",args[1],sd_actual.name,NULL);
	}else{
	    execl("editor",NULL);
	}
	_exit(EXIT_FAILURE);   //exec never returns
    }
    
}

void listarDirectorio(){

    Mensaje msg_test =
    {
	   sd_actual.size,
	   sd_actual.name,
    };
    
    
    Mensaje* msg_to_rec = ls_1(&msg_test,clnt);
    
    u_int i=0;  
    char mensaje[(*msg_to_rec).Mensaje_len];
    strcpy(mensaje,(*msg_to_rec).Mensaje_val);
    int j=0;

    for(i=0; i<(*msg_to_rec).Mensaje_len; i++){
	while((mensaje[i]!=',') && i<(*msg_to_rec).Mensaje_len){
	    printf("%c",mensaje[j]);
	    i++;
	    j++;
	}
	j++;//saltea la ,
	printf(" ");
    }
    printf("\n");    
}



char* getMyIp()
{
    char myIp[256];
    struct hostent *host_entry;
    char *IPbuffer; 
    gethostname(myIp,256);
    host_entry = gethostbyname(myIp); 
    IPbuffer = inet_ntoa(*((struct in_addr*) 
                   host_entry->h_addr_list[0])); 
    return IPbuffer;
}

int isValidIpAddress(char *ipAddress)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
    return result;
}

int removeLocal(char* direccion)
{
    int result = remove(direccion);
    return result;
}

int rmAux(char* type,char* file)
{
    if(file == NULL || type == NULL)
	return -1;
    
    int tipoOperacion = 0;
    if(!strcmp(type,"-d"))
    {
	//Voy a borrar un directorio
	tipoOperacion = 0;
    }
    else
    if(!strcmp(type,"-f"))
    {
	//Voy a borrar un archivo
	tipoOperacion = 1;
    }
    else
	return -1;
    char toSend[256];
    strcpy(toSend,"");
    strcat(toSend,file);
    /*
    Mensaje msg_to_send = {
	256,
	toSend
    };
    */
    
    int maxChar = 512;
    char* cadena = malloc(maxChar * sizeof(char));
    memset(cadena,'\0',1);
    char tipoChar[2];
    sprintf(tipoChar,"%d",tipoOperacion);
    strcat(cadena,tipoChar);
    strcat(cadena,",");
    strcat(cadena,toSend);
    strcat(cadena,",");
    strcat(cadena, sd_actual.name);
    //printf("La cadena a enviar es: %s.\n Su longitud es: %d.\n",cadena,strlen(cadena));
    Mensaje msg_to_send = {
	1+strlen(cadena),
	cadena
    };
    
    char* cadena2 = malloc(maxChar * sizeof(char));
    memset(cadena2,'\0',1);
    strcat(cadena2,toSend);
    strcat(cadena2,",");
    strcat(cadena2, sd_actual.name);
    printf("La cadena2 a enviar es: %s.\n Su longitud es: %d.\n",cadena2,strlen(cadena2));
    Mensaje msg_to_send2 = {
	1+strlen(cadena2),
	cadena2
    };
    
    //printf("Voy a hacer el exists.\n");
    int valid = *exists_1(&msg_to_send, clnt);
    if(valid)
    {
	//printf("ES VALID.\n");
	if(!tipoOperacion) //Si es una carpeta debo ver que este vacia
	{
	    Mensaje msg_to_send3 = {
		1+strlen(toSend),
		toSend
		};
	    int isEmpty = *is_empty_1(&msg_to_send3, clnt);
	    if(!isEmpty){
		//printf("No esta vacio.\n");
		return -3;
	    }
	    //printf("Esta vacio.\n");
	}
	else //Si es un archivo
	{
	    //Debo obtener la ip
	    Mensaje* msg_to_rec = getaddress_1(&msg_to_send2, clnt);
	    char* ip = msg_to_rec->Mensaje_val;
	    printf("Recibi una ip %s.\n",ip);
	    //Ahora debo revisar que sea valida
	    if(isValidIpAddress(ip))
	    {
		//Tengo que crear el nuevo paquete con la ip
		char* cadena3 = malloc(maxChar * sizeof(char));
		memset(cadena3,'\0',1);
		char tipoChar2[2];
		sprintf(tipoChar2,"%d",tipoOperacion);
		strcat(cadena3,tipoChar2);
		strcat(cadena3,",");
		strcat(cadena3,toSend);
		strcat(cadena3,",");
		strcat(cadena3, ip);
		strcat(cadena3,",");
		strcat(cadena3, sd_actual.name);
		//printf("La cadena3 a enviar es: %s.\n Su longitud es: %d.\n",cadena3,strlen(cadena3));
		Mensaje msg_to_send3 = {
		    strlen(cadena3),
		    cadena3
		};
		//La ip es valida
		//Ahora debo ver si la ip es mia o no
		if(strcmp(ip,getMyIp()))
		{
		    //printf("no lo tengo yo.\n");
		    //No lo tengo yo
		    if(removeFile(ip,toSend))
		    {
			report_delete_1(&msg_to_send3, clnt);
			return 0;
		    }
		    return -4;
		}
		else
		{
		    //printf("Lo tengo yo.\n");
		    //Lo tengo yo y debo hacer un remove local
		    if(!removeLocal(toSend))
		    {
			report_delete_1(&msg_to_send3, clnt);
			return 0;
		    }
		    return -4;
		}
	    }
	}
	
    
	report_delete_1(&msg_to_send, clnt);
	return 0;
    }
    return -2;
}


void rm()
{
    int result = rmAux(args[1],args[2]);
    switch (result)
    {
	case -1: 
	    printf("Uso: rm <-d/-f> <ruta archivo/directorio>\n");
	    break;
	case -2:
	    printf("El nombre de archivo/carpeta no es valido.\n");
	    break;
	case -3:
	    printf("Fallo al borrar '%s': El directorio no esta vacio.\n");
	    break;
	case -4:
	    printf("Ocurrio un error y no se puede eliminar.\n");
	    break;
    }
}

void cp()
{
    int result = cpAux(args[1],args[2]);
    switch (result)
    {
		case 0:
			printf("Finaliza cpAux correctamente.\n");
			break;
		case -1: 
			printf("Uso: rm <-d/-f> <ruta archivo/directorio>\n");
			break;
		case -2:
			printf("Error: Archivo de entrada incorrecto.\n");
			break;
		case -3:
			printf("Error: Directorio de salida incorrecto.\n");
			break;
		case -4:
			printf("Error: En la comunicacion con el nodo origen.\n");
			break;
		case -5:
			printf("Directorio debe ser absoluto (/Directoriorio).\n");
			break;
		case -6:
			printf("Error: Archivo ya existe en destino.\n");
			break;
    }
}

int cpAux(char* origen,char* destino)
{	

    if(origen == NULL || destino == NULL){
		return -1;
	}
    
    //Verificacion de si el archivo origen existe

    //Creo la ruta completa del archivo origen para enviarle al copyFile
	int maxChar = 512;
	char* rutaOrigen = malloc(maxChar * sizeof(char));
	memset(rutaOrigen,'\0',1);
	if (strcmp(sd_actual.name, "raiz")) {
		strcat(rutaOrigen,"/");
		strcat(rutaOrigen, sd_actual.name);
	}
    strcat(rutaOrigen,"/");
    strcat(rutaOrigen,origen);

    //Control de archivo de entrada
	int validArchivoOrigen = 0;
    validArchivoOrigen = exists(clnt, TIPOARCHIVO, origen, sd_actual.name);
	if(validArchivoOrigen)
    {
	    //Control de directorio de salida - (CarpetaX sin /)
	    if (destino[0] != '/')
	    {
	    	//Directorio debe ser absoluto (/Directoriorio)
	    	return -5;
	    }

	    //controlo que exista un directorio de destino
	    //Me fijo si es la raiz
	    //TODO: setear destino = raiz
	    int rutaCompara = strcmp(destino,"/");
	    int rutaCompara2 = strcmp(destino,"raiz");
	    int validCarpetaDestino = 0;
	    int validArchivoDestino = 0;
	    int esRaiz = 0;
	    if (rutaCompara == 0 || rutaCompara2 == 0)
	    {
	    	validCarpetaDestino = 1;
	    	validArchivoDestino = exists(clnt, TIPOARCHIVO, origen, "raiz");
			esRaiz=1;
	    }
	    else{
	    	validCarpetaDestino = exists(clnt, TIPOCARPETA, destino, NULL);
	    	validArchivoDestino = exists(clnt, TIPOARCHIVO, origen, destino);
	    }
	    
	    if(validCarpetaDestino){
	    	
	    	//Control si existe el archivo en el destino(cp no reemplaza)
	    	if (validArchivoDestino != 0)
	    	{
	    		//Error: Archivo ya existe en destino
	    		return -6;
	    	}

	    	//En este punto ya se pasaron todos los controles sobre el archivo origen y destino
	    	//Obtengo IP del nodo que tiene el archivo origen

			if (esRaiz){
				char * ip = getaddress(clnt, origen, "raiz");
			}
			else {
				char * ip = getaddress(clnt, origen, destino);

			}
			
			//Si no es la raiz, le saco la barra a la direccion destino
		    if (esRaiz == 0)
		    {
				int i;
				for(i=1;i<strlen(destino);i++)
				{
					destino[i-1]=destino[i];
				}
					destino[i-1]='\0';
			}

			printf("Mensaje para copyFile %s \n", ip);
			printf("ip msg: %s \n", ip);
			printf("rutaO msg: %s\n", rutaOrigen);
			printf("rutaD msg: %s\n", destino);

			//TODO: DESCOMENTAR LUEGO PARA REALIZAR LA COPIA FISICA
			//int resCopy = copyFile(ip, rutaOrigen, destino);
			int resCopy =1;
			//if (resCopy == ACK)
			if (resCopy == 1)
			{	
			    int result = 0;
			    if (esRaiz){
					printf("resport create: el destino es la raiz, no se crea el archivo \n");
				    //result = report_create(clnt, TIPOARCHIVO, origen, ip, NULL);
			    }
			    else {
			        result = report_create(clnt, TIPOARCHIVO, origen, ip, destino);
					printf("cp correcto");
			    }
                            
			    /*
					Manejo de errores del report create, no especifica que retorna

					if (result != 0 ) // TODO: HUBO error
					{
						printf("Error: En report_create \n");
						return -10;
					}
                */
			}
			else
			{
				//Error: En la comunicacion con el nodo origen
				return -4;
			}
	    }
	    else{
	    	//Error: Directorio de salida incorrecto
	    	return -3;
	    }
    }
	else{
		//Error: Nombre de archivo origen incorrecto
		return -2;
	}
    return 0;
}


void mv()
{
    if(args[1]==NULL){
	printf("uso: mv archivo o mv archivo directorio \n");
    }else 
    {
	/*
	 * si el archivo y el directorio existen procedo
	 * */	
	char* archivo = malloc(50*sizeof(char));
	strcpy(archivo,args[1]);
	char* destino = malloc(100*sizeof(char));
	strcpy(destino,"raiz");
	int archivo_valido=exists(clnt, '1', archivo, sd_actual.name);
	int directorio_existe=1;	
	int directorio_valido=1;
	if (args[2]!= NULL)
	{
	    strcpy(destino,args[2]);
	    directorio_existe=exists(clnt, '0', destino, NULL);
	    directorio_valido=0;
	    int i = 0;
	    char *pch=strchr(destino,'/');
	    if (pch==NULL)
	    {
		directorio_valido=1;
	    }
	}	
	if (archivo_valido==1)
	{
	    if (directorio_valido)
	    {
		if (directorio_existe==1)
		{
		    //no tengo que crear el directorio
		    /*
		     * como el directorio existe solo tengo que actualizarle el padre
		     * */
		    //printf("Destino = %s.\n",destino);
		    int res_update_directory= report_update_directory(clnt,archivo,sd_actual.name,destino);
		    //printf("El resultado es: %d .\n",res_update_directory);
		}
		else
		{
		    //printf("Destino = %s.\n",destino);
		    //printf("Debo crear la carpeta.\n");
		    // como el directorio no eixste lo creo y despues actualizo el directorio del archivo
		    int res_create= report_create(clnt,'0',destino,"",NULL);
		    int res_update_directory= report_update_directory(clnt,archivo,sd_actual.name,destino);
		}
	    }
	    else
	    {
		printf("Solo puedes crear un nivel de carpeta en el root. Uso: mv archivo raiz/directorio \n");
	    }	    
	}
	else
	{
	    printf("??El archivo no es v??lido!\n");
	}
	 
    }
}

void salir()
{
	printf("??Seguro que deseas salir del sistema? y/n \n");
	char seleccion[2];
	scanf("%s",seleccion);
	seleccion[1] = '\0';
	if (strcmp(seleccion,"y") == 0)
	{
		printf("Apagando el sistema..\n");
		char* myIp = getMyIp();//"192.168.1.31";
		char* mis_documentos=get_my_documents(clnt,myIp);
		printf("Mis documentos: %s\n",mis_documentos);
		char* copia_mis_documentos=malloc(2048*sizeof(char));
		strcpy(copia_mis_documentos,mis_documentos);
		int raiz=0;
		char* delimiter = ",";
		char* filename=malloc(512*sizeof(char));
		char* carpeta=malloc(512*sizeof(char));
		
		carpeta = strtok(copia_mis_documentos,delimiter);
		while(carpeta != NULL)
		{
		    filename = strtok(NULL,delimiter);
		    printf("Voy a eliminar %s/%s\n",carpeta,filename);
		    
		    
		    int res_report_delete=report_delete(clnt,'1',filename,myIp,carpeta);
		    if (strcmp(carpeta,"raiz")!=0 && is_empty(clnt,carpeta))
		    {// si la carpeta esta vacia la borro
			    printf("La carpeta %s quedo vacia.\n",carpeta);
			    int res_report_delete=report_delete(clnt,'0',carpeta,myIp,NULL);
		    }
		    carpeta = strtok(NULL,delimiter);
		}
		
		
		
		
		/*
		
		filename=strtok(copia_mis_documentos, delimiter);
		while(filename!=NULL && raiz)
		{			
			if (strcmp(filename,"0"))
			{
				//viene una carpeta, termine con los archivos de la raiz
				raiz=0;
			}
			else
			{				
				int res_report_delete=report_delete(clnt,'1',filename,"raiz");
			    
			}
			filename=strtok(NULL, delimiter);			
		}
		carpeta=strtok(NULL, delimiter);
		while (carpeta!=NULL)
		{
			int carpeta_terminada=0;
			while (!carpeta_terminada)
			{
				filename=strtok(NULL, delimiter);
				if (strcmp(filename,"0"))
				{
					//viene una carpeta, termine con los archivos de la carpeta actual
					carpeta_terminada=1;
				}
				else
				{				
					int res_report_delete=report_delete(clnt,'1',filename,carpeta);
					
				}
			}
			if (is_empty(clnt,carpeta))
			{// si la carpeta esta vacia la borro
				int res_report_delete=report_delete(clnt,'0',carpeta,NULL);
			}
			carpeta=strtok(NULL, delimiter);
		}			*/
		printf("??Hasta la pr??xima!");
		exit(0);
	}
	else
	{
		printf("Nos alegra que sigas con nosotros\n");
	}		
}

void help()
{
	printf("Comandos disponibles:\n");
	printf("- " AMARILLO "mv " ROJO "[origen] " AZUL "[directorio destino]" NORMAL ": mover archivo/directorio a otro directorio. \n");
	printf("- " AMARILLO "cp " ROJO "[archivo origen] " AZUL "[directorio destino]" NORMAL ": copiar archivo/directorio a otro directorio. \n");
	printf("- " AMARILLO "rm " VERDE "[-f/-d] " ROJO "[archivo/directorio]" NORMAL ": eliminar un archivo o un directorio vac??o. \n");
	printf("- " AMARILLO "ls" NORMAL ": listar el directorio actual. \n");
	printf("- " AMARILLO "cd " ROJO "[directorio]" NORMAL ": cambiar de directorio. \n");
	printf("- " AMARILLO "editor " ROJO "[archivo]" NORMAL ": editar un archivo existente o nuevo. \n");
	printf("- " AMARILLO "mkdir " ROJO "[directorio]" NORMAL ": crear un directorio nuevo. \n");
	printf("- " AMARILLO "exit" NORMAL ": salir del DFS. \n");
}
