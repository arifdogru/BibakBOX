#define _POSIX_SOURCE
#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <utime.h>

#define LOGFILENAME "MainLog.log"
#define PATHPERLOG "Log.log"
/************************************************/
/*              GLOBAL VARIABLES			  	*/
/************************************************/

int mainSocket,miniservFd,port;
char pathArr[400][256];
int pathArrSize;
char allServicesPath[400][256];
int servicePathSize;
pthread_t miniServerThreads[50];
FILE* mainLogFilePtr;

int threadIndex;
pthread_mutex_t threadIndexMutex;
char flagArr[50];
pthread_mutex_t allMutexes[50];
int threadPoolSize;
int numOfCreatedThreads;
char serverPath[256];
typedef struct
{
	char path[256];
	int fd;
	char serverDir[256];
	int mutexIndex;

}MiniServerParams;

MiniServerParams miniServerParamsArray[50];
  struct tm *timeinfo;
  time_t rawtime;  
  char s[64];
	char mainLogFileName[256];
/************************************************/
/*              FUNCTION HEADERS			  	*/
/************************************************/
int establish(unsigned short portnum);
void initPathArr();
void printPaths();
int isInPathArr(char* path);
void* miniServerFunc(void* param);
void timeInMiliseconds(double targetTime);
int postOrderApply( char* path, int index, char files[][256], char modifTime[][256],long int  second[]);
int isDirectory( char *path);
void getLastModificationTime(char * fileName, char* lastTime, long int* second,int index);
void checkDifference(char sourceFiles[][256], long int sourceFileDate[], char destFiles[][256],long int destFileDate[], char resultArr[][256], int sizeSource, int sizeDest,int* sizeResult);
int isStringInArr(char path[][256], int size, char* target);
int sizepathfun (char *path);
void rek_mkdir(char *path);
void createDir(char directory[256] );
int searchInIntegerArray(int source[], int target, int size);
void removeFromPathArr(char *target);
void initFlagArray(char ch, int size);
/************************************************/
/*              SIGNAL HANDLERS 			  	*/
/************************************************/

void sigInthandler(int sigNo){
	int i;
	if (sigNo == SIGPIPE)
	{
		fprintf(stderr,"SIGPIPE GELDI\n");
	}
	fprintf(mainLogFilePtr,"Ctrl-c Geldi yeni islem alinmayacak !!!\n");
	fprintf(stderr,"Ctrl-c Geldi threadler kapatiliyor..\n");
	for ( i = 0; i < 50; i++)
	{
		pthread_mutex_lock(&allMutexes[i]);

		flagArr[i] = '!';			

		pthread_mutex_unlock(&allMutexes[i]);
		
	}

	//fprintf(stderr,"Ctrl-c threadlere kapama flag i yollandi ..\n");
	for ( i = 0; i < numOfCreatedThreads; i++)
	{
		//fprintf(stderr,"Suan bagli thread sayisi:%d\n",threadIndex);
		pthread_join(miniServerThreads[i],NULL);
	}
	fprintf(stderr,"\n************* Thread pool artik bos server kapaniyor *************\n");

	fprintf(mainLogFilePtr,"\n************* Statistic *************\n");
	fprintf(mainLogFilePtr," Toplam %d tane client'a hizmet verildi \n",numOfCreatedThreads);
	fprintf(mainLogFilePtr, "Asagidaki pathler icin hizmet verildi\n");
	fprintf(stderr," Toplam %d tane client'a hizmet verildi \n",numOfCreatedThreads);
	fprintf(stderr, "Asagidaki pathler icin hizmet verildi\n");
	for ( i = 0; i < servicePathSize; i++)
	{
		fprintf(mainLogFilePtr, "%d-) %s\n",i+1,allServicesPath[i]);
		fprintf(stderr, "%d-) %s\n",i+1,allServicesPath[i]);
	}
	
	//fprintf(mainLogFilePtr," -Ayni Anda- Toplam %d tane client'a hizmet verildi \n",threadIndex);
	fprintf(mainLogFilePtr,"\n************* Thread pool artik bos server kapandi *************\n\n\n");
	close(mainSocket);
	//perror("mainSocket");
	fclose(mainLogFilePtr);
	//perror("mainLogFile");
	exit(0);
}



/************************************************/
/*              MAIN OF PROGRAM				    */
/************************************************/
int main( int argc, char *argv[] ){

    int fd,socket;
	char readPath[256];
	char serverDir[256];
	char serverDirParam[256];
	char serverCreateCharArr[256];

	char ch;
	int zero = 0;
	int one = 1;
	int nine = 9;
	MiniServerParams miniServerParam;
	int i=0;
	int indexForMutex=0;
	
    threadIndex=0;
	numOfCreatedThreads=0;
	if (argc != 4)
	{
		fprintf(stderr, "Usage: %s [directory] [threadPool size] [port] \n",argv[0]);
		exit(0);
	}
	
	initFlagArray('$',50);
	signal(SIGINT, sigInthandler);
	signal(SIGPIPE, sigInthandler);

    port = atoi(argv[3]);
	threadPoolSize = atoi(argv[2]);
    strcpy(serverDirParam,argv[1]);
    strcpy(serverPath,argv[1]);
	strcpy(serverCreateCharArr,"+");
	strcat(serverCreateCharArr,serverPath);
	if (serverPath[strlen(serverPath)-1] != '/')
	{
		strcat(serverCreateCharArr,"/");
	}
	




	socket=establish(port);
	if(socket == -1){
		fprintf(stderr, "socket dolu \n" );
		return -1;
	}

	createDir(serverCreateCharArr);

	strcpy(mainLogFileName,argv[1]);
	strcat(mainLogFileName,"/");
	strcat(mainLogFileName,LOGFILENAME);

	//Get time server started as string
  	time(&rawtime);
  	timeinfo = localtime ( &rawtime );
  	strftime(s, sizeof(s), "%c", timeinfo);

	
	mainLogFilePtr=fopen(mainLogFileName,"a+");
	if(mainLogFilePtr == NULL){
		fprintf(stderr,"%s File Create Error\n",mainLogFileName);
		exit(1);
	}
	//Log start
	fprintf(mainLogFilePtr, "*** Server Started at %s and at port %d with %d threads ***\n",s ,port,threadPoolSize);
	

	mainSocket = socket;

	servicePathSize = 0;

	fprintf(stderr, "Server yeni baglanti icin %d portunda bekliyor\n", port);

	initPathArr();
    while (1)
    {
		//fprintf(stderr,"Suan bagli thread sayisi:%d\n",threadIndex);
        i=0;
		fprintf(stderr,"Main Server Yeni baglanti bekliyor\n");
        fd= accept(socket,NULL,NULL);
        if(fd == -1){
            fprintf(stderr, "fd= %d\n",fd );
            return -1;
        }
		fprintf(stderr,"Main Server'a Yeni baglanti istegi geldi\n");
		//Log new connection
		fprintf(mainLogFilePtr, "Main Server'a Yeni Baglanti Istegi Geldi\n");
		
        miniservFd = fd;
		do
		{
			read(fd,&ch,sizeof(char));
			if(ch != '@')
			{
				readPath[i] = ch;
				++i;
			}
		} while (ch  != '@');
		readPath[i] = '\0';
		if ( isInPathArr(readPath) == 0 )
		{
			if ( threadIndex < threadPoolSize)
			{
				fprintf(mainLogFilePtr, "%s Pathi ile Baglanti Saglandi\n",readPath);
				fprintf(stderr, "%s Pathi ile Baglanti Saglandi\n",readPath);
				strcpy(pathArr[pathArrSize],readPath);
				strcpy(allServicesPath[servicePathSize],readPath);
				++pathArrSize;
				++servicePathSize;
				write(fd,&one,sizeof(int));

				miniServerParamsArray[indexForMutex].fd =fd; 
				
				strcpy(miniServerParamsArray[indexForMutex].serverDir,serverDirParam);
				strcpy(miniServerParamsArray[indexForMutex].path,readPath);
				miniServerParamsArray[indexForMutex].mutexIndex = indexForMutex;
				pthread_create(&miniServerThreads[indexForMutex],NULL,miniServerFunc,&miniServerParamsArray[indexForMutex]);
				/// Bunun icinde mutex kullanip path arrayinden eleman silinince kaydirmak gerekirmi vs
				++indexForMutex;
				++numOfCreatedThreads;
			}else
			{
				fprintf(mainLogFilePtr, "Threadler Dolu Baglanti Reddedildi\n");
				fprintf(stderr, "Threadler Dolu Baglanti Reddedildi\n");
				write(fd,&nine,sizeof(int));
			}
		}
		else
		{
			fprintf(mainLogFilePtr, "%s Path Icin Aktif Baglanti Var Yeni Baglanti Reddedildi\n",readPath);
			fprintf(stderr, "%s Path Icin Aktif Baglanti Var Yeni Baglanti Reddedildi\n",readPath);
			write(fd,&zero,sizeof(int));
		}
		fprintf(stderr, "Aktif Baglintili Pathler\n");
		printPaths();
    }
    

    return 0;
}

int establish(unsigned short portnum)
{
	char myname[ HOST_NAME_MAX];

	int s;

	struct sockaddr_in serverAdress;

	struct hostent *hp;

	memset(&serverAdress, 0, sizeof(struct sockaddr_in));

	gethostname(myname,HOST_NAME_MAX);
	//fprintf(stderr, "%s\n", myname );
	hp= gethostbyname(myname);

	if (hp == NULL) 
		return -1;

	serverAdress.sin_family= AF_INET;
	
	serverAdress.sin_port= htons(portnum);
	serverAdress.sin_addr.s_addr=htonl(INADDR_ANY);
 	
	if ((s= socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	if (bind(s,(struct sockaddr *)&serverAdress,sizeof(struct sockaddr_in)) < 0)
	{
		close(s);
		return -1;	
	}
	
	listen(s, 10);
	
	return s;
}
void initPathArr(){
	int i;
	for ( i = 0; i < pathArrSize; i++)
	{
		strcpy(pathArr[i],"");
	}
}
void printPaths(){
	int i;
	for ( i = 0; i < pathArrSize; i++)
	{
		fprintf(stderr,"%s\n",pathArr[i]);
	}
	

}
int isInPathArr(char* path){

	int flag =0;
	int i;
	for ( i = 0; i < pathArrSize; i++)
	{
		if (strcmp(pathArr[i],path) == 0)
		{
			flag = 1;
			break;
		}
		
	}
	return flag;

}
void* miniServerFunc(void* param){
	char files[500][256];
	char filesFromClient[500][256];
	char lastModificationTimes[500][256];
	long int lastModifSecond[500];
	long int lastModifSecondFromClient[500];
	char differenceArray[500][256];
	int differenceArraySize;
	MiniServerParams miniParams;
	miniParams = (*(MiniServerParams*)param);
	char temp;
	int size=0;
	int tempLint;
	int i=0,j=0,k;
	int numOfFiles=0;
	char delim = '\n';
	char space = '\a';
	char sentinal = '@';
	int inFilePtr,outFilePtr;
	char readByte;
	int sentByte;
	int totalByte = 0;
	int readSocketByte=0;
	int readLoop=0;
	char *dirNamePtr;
	int count=0;
	int dateIndex;
	char willCreatDir[50];
	struct stat st = {0};
	char tempDirForSent[256];
	char serverDirectory[256];
	struct utimbuf timeBufToUpdate;
	char myPath[256];
	char charArrToCreate[256];
	char pathWithPlus[256];
	FILE* pathPerLogPTR;
	char pathLogFileName[256];
	sigset_t set;
	
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set,SIGPIPE);	
	pthread_sigmask(SIG_BLOCK, &set, NULL);

	pthread_mutex_lock(&threadIndexMutex);
	++threadIndex;
	pthread_mutex_unlock(&threadIndexMutex);
	//fprintf(stderr,"mini Server param dir:%s \n",miniParams.serverDir);
	strcpy(myPath,serverPath);
	strcat(myPath,"/");
	strcat(myPath,miniParams.path);
	strcpy(pathWithPlus,"+");
	strcat(pathWithPlus,myPath);
	strcat(pathWithPlus,"/");
	createDir(pathWithPlus);


	strcpy(pathLogFileName,myPath);
	strcat(pathLogFileName,"_");
	strcat(pathLogFileName,PATHPERLOG);
	pathPerLogPTR=fopen(pathLogFileName,"a+");
	if(pathPerLogPTR == NULL){
		fprintf(stderr,"%s File Create Error\n",pathLogFileName);
		exit(1);
	}

	while (1)
	{	
		size = postOrderApply(myPath,0,files,lastModificationTimes,lastModifSecond);
		
		/*fprintf(stderr,"\n******** MY FILES INFO ******\n");
		for ( i = 0; i < size; i++)
		{
			fprintf(stderr,"%s ",files[i]);
			fprintf(stderr,"time second:%ld \n",lastModifSecond[i]);
			//fprintf(stderr,"%s",lastModificationTimes[i]);

		}*/
		i = 0;
		// Client a sorun olup olmadigini ilettigi adim
		pthread_mutex_lock(&allMutexes[miniParams.mutexIndex]);
		write(miniParams.fd,&flagArr[miniParams.mutexIndex],sizeof(char));
		
		if (read(miniParams.fd,&temp,sizeof(char)) == 0)
		{
			fprintf(stderr, " cipetpet\n ");
		}
		
		//fprintf(stderr,"Yolladigim flag:%c\n",flagArr[miniParams.mutexIndex]);
		if (flagArr[miniParams.mutexIndex] == '!' || temp == '!' )
		{

			pthread_mutex_unlock(&allMutexes[miniParams.mutexIndex]);
			pthread_mutex_destroy(&allMutexes[miniParams.mutexIndex]);
			removeFromPathArr(miniParams.path);
			//fprintf(stderr,"Path arrayinden silinen:%s\n",miniParams.path);
			pthread_mutex_lock(&threadIndexMutex);
			--threadIndex;
			pthread_mutex_unlock(&threadIndexMutex);
			fprintf(mainLogFilePtr,"%s Baglantili client sonlandi\n",miniParams.path);
			close(miniParams.fd);
			fclose(pathPerLogPTR);
			//perror("close ettim");
			return NULL;
		}
		pthread_mutex_unlock(&allMutexes[miniParams.mutexIndex]);
		// Client a sorun olup olmadigini ilettigi adim 
		// SON
		
		// Server client tarafinda sorun olup olmadigini okudugu bolum
		
		//fprintf(stderr,"Gelen flag:%c\n",temp);
		//fprintf(stderr,"Client hala bagli\n");
		
		while (i < size )
		{
			//timeInMiliseconds(1);
			//strcpy(tempDirForSent,strchr(files[i],'/'));
			//fprintf(stderr,"%s : %s\n",directory,&tempDirForSent[1]);
			write(miniParams.fd,&files[i],strlen(files[i]));
			write(miniParams.fd,&space,sizeof(char));
			write(miniParams.fd,&lastModifSecond[i],sizeof(long int));
			write(miniParams.fd,&delim,sizeof(char));
			++i;
		}
		write(miniParams.fd,&sentinal,sizeof(char));
		numOfFiles = 0;
		while (1)
		{

			read(miniParams.fd,&temp,sizeof(char));
			if(temp == '@')
			{
				break;
			}
			else if(temp == '\a')
			{

				read(miniParams.fd,&tempLint,sizeof(long int));
				lastModifSecondFromClient[numOfFiles] = tempLint;
			}
			else if (temp == '\n')
			{
				filesFromClient[numOfFiles][j] = '\0';
				++numOfFiles;
				j=0;
			}
			else
			{
				filesFromClient[numOfFiles][j] = temp;
				++j;
			}
			
		}

		//fprintf(stderr,"path:%s",miniParams.path);
		/*fprintf(stderr,"\n******** Client Side File Info ******\n");
		for ( i = 0; i < size; i++)
		{
			fprintf(stderr,"%s ",filesFromClient[i]);
			fprintf(stderr,"time second:%ld \n",lastModifSecondFromClient[i]);
			//fprintf(stderr,"%s",lastModificationTimes[i]);
		}*/
		//strcpy(serverDirectory,strtok(miniParams.path,"/"));
		//strcat(serverDirectory,"/");
		checkDifference(files,lastModifSecond,filesFromClient,lastModifSecondFromClient,differenceArray,size,numOfFiles,&differenceArraySize);
		/*fprintf(stderr,"\n******** Difference Array ******\n");
		for ( i = 0; i < differenceArraySize; i++)
		{
			fprintf(stderr,"%s ",differenceArray[i]);
			//fprintf(stderr,"%s",lastModificationTimes[i]);
		}*/
		for ( i = 0; i < differenceArraySize; i++)
		{
			readSocketByte=0;
			readLoop=0;
			//fprintf(stderr,"%s ",differenceArray[i]);
			if (differenceArray[i][0] == '-')
			{
				strcpy(charArrToCreate,"+");
				strcat(charArrToCreate,serverPath);
				strcat(charArrToCreate,"/");
				strcat(charArrToCreate,&differenceArray[i][1]);
				//fprintf(stderr,"%s okuma modunda aciliyor\n",&differenceArray[i][1]);
				inFilePtr = open(&charArrToCreate[1],O_RDONLY);
				//fprintf(stderr,"%s okuma modunda acildi\n",&charArrToCreate[1]);

				sentByte = sizepathfun(&charArrToCreate[1]);
				//fprintf(stderr,"Client %d kadar byte okuman lazim\n",sentByte);
				write(miniParams.fd,&sentByte,sizeof(int));
				
				while ( readLoop < sentByte )
				{
				 	readLoop += read(inFilePtr,&readByte,sizeof(char));
					write(miniParams.fd,&readByte,sizeof(char));
				}
				
				close(inFilePtr);
				fprintf(mainLogFilePtr," %s dosyasi client'a kopyalandi\n",&differenceArray[i][1]);
				fprintf(pathPerLogPTR," %s dosyasi client'a kopyalandi\n",&differenceArray[i][1]);
				fprintf(stderr," %s dosyasi client'a kopyalandi\n",&differenceArray[i][1]);
				
			}
			else if (differenceArray[i][0] == '+')
			{
				//fprintf(stderr,"%s aliniyor\n",&differenceArray[i][1]);
				/*strcpy(serverDirectory,"-");
				strcat(serverDirectory,miniParams.serverDir);
				strcat(serverDirectory,&differenceArray[i][1]);*/
				strcpy(charArrToCreate,"+");
				strcat(charArrToCreate,serverPath);
				strcat(charArrToCreate,"/");
				strcat(charArrToCreate,&differenceArray[i][1]);

				createDir(charArrToCreate);
				
				
				outFilePtr=open(&charArrToCreate[1], O_CREAT | O_WRONLY | O_TRUNC,0777);
				//fprintf(stderr,"%s alim icin olusturuldu\n",&charArrToCreate[1]);
				if (outFilePtr < 0)
				{
					fprintf (stderr,"Cannot open out directory '%s'\n", &charArrToCreate[1]);
					exit(0);
				}
				
				read(miniParams.fd,&totalByte,sizeof(int));

				//fprintf(stderr, "Server %d byte okumam lazim\n",totalByte);
				while ( readSocketByte < totalByte )
				{
					readSocketByte += read(miniParams.fd,&readByte,sizeof(char));
					write(outFilePtr,&readByte,sizeof(char));
				}
				close(outFilePtr);
				fprintf(mainLogFilePtr," %s dosyasi server'a kopyalandi\n",&differenceArray[i][1]);
				fprintf(pathPerLogPTR," %s dosyasi server'a kopyalandi\n",&differenceArray[i][1]);
				fprintf(stderr," %s dosyasi server'a kopyalandi\n",&differenceArray[i][1]);
				
				dateIndex = isStringInArr(filesFromClient,500,&differenceArray[i][1]);
				timeBufToUpdate.modtime = lastModifSecondFromClient[dateIndex];
				utime(&charArrToCreate[1],&timeBufToUpdate);
			}
			
			
			//fprintf(stderr,"%s",lastModificationTimes[i]);
		}


		//timeInMiliseconds(3);

	}
	




}

void timeInMiliseconds(double targetTime){
  struct timeval stop, start;
  
  double secs = 0;

  gettimeofday(&start, NULL);
  do
  {
    gettimeofday(&stop, NULL);
    secs = (double)(stop.tv_usec - start.tv_usec) / 1000000 + (double)(stop.tv_sec - start.tv_sec);
  } while (secs < targetTime);
}


void getLastModificationTime(char * fileName, char* lastTime, long int* second,int index){

	struct stat attr;
	stat(fileName, &attr);
   	strcpy(lastTime,ctime(&attr.st_mtime));
	//fprintf(stderr,"Time:%d\n",attr.st_mtime);
	second[index] = attr.st_mtime;

}

int postOrderApply( char* path, int index, char files[][256], char modifTime[][256],long int second[])
{

	struct dirent *pathDirent;
	int sizeFile=0;
	char newPath[255] = {0};

	DIR *pathDir;
	FILE *resultFptr;
	int fileNameSize;
	int temp;
	int result;
	pid_t childpid;
	int i;
	char ch;
	char tempA='a';

	pathDir = opendir(path);
	
    if (pathDir == NULL) 
	{
        fprintf (stderr,"Cannot open directory Post Order %s\n", path);
        exit(0);
    }
    
	while ((pathDirent = readdir(pathDir)) != NULL)
	{
        fileNameSize=strlen(pathDirent->d_name);
		if (pathDirent->d_name[0]!='.' && pathDirent->d_name[fileNameSize-1]!='~')
        {
            strcpy(newPath,path);	
		    strcat(newPath,"/");    
            strcat(newPath,pathDirent->d_name);
			strcpy(files[index],&newPath[strlen(serverPath)+1]);
            getLastModificationTime(newPath,modifTime[index],second,index);
            //strcat(files[index],"/");
            //strcat(files[index],pathDirent->d_name);
        }
		
		if(!isDirectory(newPath) && pathDirent->d_name[0]!='.' && pathDirent->d_name[fileNameSize-1]!='~')
		{	
            index++;
		}
		if(isDirectory(newPath) && pathDirent->d_name[0]!='.' && pathDirent->d_name[fileNameSize-1]!='~' && pathDirent->d_type != DT_FIFO && pathDirent->d_type != DT_LNK )
		{
			index = postOrderApply(newPath,index,files,modifTime,second);
		}
		
	}//while end
   	closedir(pathDir);
	return index;
}

int isDirectory( char *path) 
{
   struct stat statbuf;

   if (stat(path, &statbuf) == -1)
      return 0;
  
   else
      return S_ISDIR(statbuf.st_mode);
}

void checkDifference(char sourceFiles[][256], long int sourceFileDate[], char destFiles[][256],long int destFileDate[], char resultArr[][256], int sizeSource, int sizeDest,int* sizeResult){

	int i=0;
	int sizeR=0;
	int status;
	char tempPlus[300];
	char tempMinus[300];

	/*fprintf(stderr,"\n************** Check Differecn Func  ***********\n");
	for ( i = 0; i < sizeSource; i++)
	{
		fprintf(stderr,"source: %s ",sourceFiles[i]);
		fprintf(stderr,"date: %ld\n",sourceFileDate[i]);
		
	}

	for ( i = 0; i < sizeDest; i++)
	{
		fprintf(stderr,"Dest: %s ",destFiles[i]);
		fprintf(stderr,"date: %ld\n",destFileDate[i]);
		
	}*/
	
	


	for ( i = 0; i < sizeDest; i++)
	{
		
		status = isStringInArr(sourceFiles,sizeSource,destFiles[i]);
		if (status == -1 )
		{
			fprintf(stderr,"Bu dosya bende yok:%s\n",destFiles[i]);
			fprintf(mainLogFilePtr," %s dosyasi serverda yok\n",destFiles[i]);
			
			if (isStringInArr(resultArr,sizeR,destFiles[i]) == -1)
			{
				strcpy(resultArr[sizeR],"+");
				strcat(resultArr[sizeR],destFiles[i]);
				fprintf(stderr,"dosya alinacak %s\n",destFiles[i]);
				++sizeR;
			}
		}
		else {
			if( sourceFileDate[status] > destFileDate[i] )
			{
				strcpy(resultArr[sizeR],"-");
				strcat(resultArr[sizeR],destFiles[i]);
				//fprintf(stderr," dosya alinacak %s\n",destFiles[i]);
				fprintf(mainLogFilePtr," %s Dosyasi Serverda Daha Guncel\n",destFiles[i]);
				++sizeR;
			}
			else if(sourceFileDate[status] < destFileDate[i]){
				strcpy(resultArr[sizeR],"+");
				strcat(resultArr[sizeR],destFiles[i]);
				fprintf(mainLogFilePtr," %s Dosyasi Clientta Daha Guncel\n",destFiles[i]);
				++sizeR;
			}

		}
	}

	for ( i = 0; i < sizeSource; i++)
	{
		status = isStringInArr(destFiles,sizeDest,sourceFiles[i]);
		if (status == -1)
		{
			fprintf(stderr,"%s dosyasi clientta yok\n",sourceFiles[i]);
			fprintf(mainLogFilePtr,"%s dosya clientta yok\n",sourceFiles[i]);
			if(isStringInArr(resultArr,sizeR,sourceFiles[i]) == -1){
				strcpy(resultArr[sizeR],"-");
				strcat(resultArr[sizeR],sourceFiles[i]);
				++sizeR;
			}
		}
		else
		{
			if (destFileDate[status] > sourceFileDate[i])
			{
				strcpy(tempPlus,"+");
				strcat(tempPlus,destFiles[status]);

				strcpy(tempMinus,"-");
				strcat(tempMinus,destFiles[status]);
				
				if (isStringInArr(resultArr,sizeR,tempPlus) == -1 && isStringInArr(resultArr,sizeR,tempMinus) == -1 )
				{
					strcpy(resultArr[sizeR],"+");
					strcat(resultArr[sizeR],sourceFiles[i]);
					fprintf(mainLogFilePtr,"%s Dosyasi Clientta Daha Guncel\n",destFiles[i]);
					++sizeR;
				}
				
			}
			else if(destFileDate[status] < sourceFileDate[i])
			{
				strcpy(tempPlus,"+");
				strcat(tempPlus,sourceFiles[i]);

				strcpy(tempMinus,"-");
				strcat(tempMinus,sourceFiles[i]);

				if (isStringInArr(resultArr,sizeR,tempPlus) == -1 && isStringInArr(resultArr,sizeR,tempMinus) == -1 )
				{
					strcpy(resultArr[sizeR],"-");
					strcat(resultArr[sizeR],sourceFiles[i]);
					fprintf(mainLogFilePtr,"%s Dosyasi Serverda Daha Guncel\n",destFiles[i]);
					++sizeR;
				}
				
			}
		}
	}

	*sizeResult = sizeR;

}
int isStringInArr(char path[][256], int size, char* target){

	int flag =0;
	int i;
	for ( i = 0; i < size; i++)
	{
		if (strcmp(path[i],target) == 0)
		{
			flag = 1;
			break;
		}
		
	}
	if (flag == 0)
	{
		return -1;
	}
	else
	{
		return i;
	}
	
}

int sizepathfun (char *path){
    int sizeInt =0;	
	double sizeDouble = 0;
	struct stat st;
   	lstat(path, &st);

		sizeDouble = st.st_size;
		sizeInt = sizeDouble;
		return sizeInt;

}

void rek_mkdir(char *path)
{
	struct stat st = {0};
  char *sep = strrchr(path, '/' );
  if(sep != NULL) {
    *sep = 0;
    rek_mkdir(path);
    *sep = '/';
  }


	if (stat(path, &st) == -1) {
		mkdir(path, 0777);
	}
  
}

void createDir(char directory[256] ){
	int i, temp=0,count=0;
	char willCreatDir[256];
	for ( i = 0; i < strlen(directory); i++)
	{
		if (directory[i] == '/')
		{
			count++;
		}
		
	}
	//fprintf(stderr,"num of / :%d \n",count);
	i=0;
	while (temp < count)
	{
		willCreatDir[i] = directory[i+1];
		
		if (directory[i+1] == '/')
		{
			++temp;
		}
		++i;
	}
	willCreatDir[i] = '\0';
		
	rek_mkdir(willCreatDir);
}
int searchInIntegerArray(int source[], int target, int size){

	int i;
	for ( i = 0; i < size; i++)
	{
		if (source[i] == target)
		{
			return i;
		}
		
	}
	return -1;

}

void initFlagArray(char ch, int size){
	int i;
	for ( i = 0; i < size; i++)
	{
		flagArr[i] = ch;
	}
	

}

void removeFromPathArr(char *target){
	int i, size, pos;
    size = pathArrSize;
    pos = isStringInArr(pathArr,pathArrSize,target);

    if(pos < 0 || pos > size)
    {
        printf("Invalid position! Please enter position between 1 to %d", size);
    }
    else
    {
        for(i=pos; i<size-1; i++)
        {
            
            strcpy(pathArr[i],pathArr[i+1]);
        }
        strcpy(pathArr[size-1]," ");
        pathArrSize--;
    }
	
}