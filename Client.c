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
/************************************************/
/*              GLOBAL VARIABLES			  	*/
/************************************************/
int fdSocket;
int doneflag=0;
char sentFlag='$';

/************************************************/
/*              FUNCTION HEADERS			  	*/
/************************************************/


int callSocket(char *hostname, unsigned short portnum);
void getLastModificationTime(char * fileName, char* lastTime, long int* second,int index);
int isDirectory( char *path);
int postOrderApply( char* path, int index, char files[][256], char modifTime[][256],long int second[]);
void timeInMiliseconds(double targetTime);
void checkDifference(char sourceFiles[][256], long int sourceFileDate[], char destFiles[][256],long int destFileDate[], char resultArr[][256], int sizeSource, int sizeDest,int* sizeResult);
int isStringInArr(char path[][256], int size, char* target);
int sizepathfun (char *path);
void rek_mkdir(char *path);
void createDir(char directory[256] );
/************************************************/
/*              SIGNAL HANDLERS 			  	*/
/************************************************/

void sigInthandler(){
	char danger='!'; 

	sentFlag = danger;
	fprintf(stderr,"Flag ! olarak set edildi\n");
}


/************************************************/
/*              MAIN OF PROGRAM				    */
/************************************************/
int main( int argc, char* argv[] ){


	float buff;
	int request=getpid();
	int flag = 0;
	char path[256];
	int status;
	char temp;
	int numOfFiles = 0;
	int i=0;
	int fileSize;
	char filesFromServer[500][256];
	char files[500][256];
	char lastModificationTimes[500][256];
	long int lastModifSecondFromServer[500],tempLint;
	long int lastModifSecond[500];
	char space = '\a';
	char sentinal= '@';
	char delim = '\n';
	char differenceArray[500][256];
	int differenceArraySize;
	int j=0;
	int inFilePtr,outFilePtr;
	char readByte;
	int totalByte;
	int intRead = 0;
	int dateIndex;
	int serverTotalByte;
	int fromserverByte=0;
	struct utimbuf timeBufToUpdate;
	char readFlag;
	if (argc != 4)
	{
		fprintf(stderr, "Usage: %s [directory] [ip address] [port] \n",argv[0]);
		exit(0);
	}
	signal(SIGINT, sigInthandler);
	strcpy(path,argv[1]);
	strcat(path,"@");
	fdSocket = callSocket(argv[2],atoi(argv[3]));
	//read(fdSocket,&buff,sizeof(int));
	//fprintf(stderr, "Server'dan okunan deger: %d\n",buff );
	
	if(write(fdSocket,&path,strlen(path)) < 0){
		fprintf(stderr,"Baglanti basarisiz: Server Ayakta Degil\n");
		return 0;
	}

	read(fdSocket,&status,sizeof(int));
	if (status == 0)
	{
		fprintf(stderr,"Baglanti basarisiz: %s pathine baska client bagli\n",path);
		return 0;
	}
	else if(status == 9)
	{
		fprintf(stderr,"Baglanti basarisiz: Threadlerin hepsi dolu\n");
		return 0;
	}
	
	else if (status == 1)
	{
		fprintf(stderr,"Baglanti basarili\n");
	}


	while (1)
	{
		
		read(fdSocket,&readFlag,sizeof(char));
		write(fdSocket,&sentFlag,sizeof(char));
		if (readFlag == '!' || sentFlag == '!')
		{
			fprintf(stderr,"Sinyal Yakaladi Sonlandi BYE\n");	
			//write(fdSocket,&readFlag,sizeof(char));
			close(fdSocket);
			exit(1);
		}
		
		
		numOfFiles = 0;
		while (1)
		{

			
			read(fdSocket,&temp,sizeof(char));
			//fprintf(stderr,"%c",temp);
			if(temp == '@')
			{
				flag = 1;
				break;
			}
			else if(temp == '\a')
			{

				read(fdSocket,&tempLint,sizeof(long int));
				lastModifSecondFromServer[numOfFiles] = tempLint;
				
				
			}
			else if (temp == '\n')
			{
				filesFromServer[numOfFiles][j] = '\0';
				++numOfFiles;

				j=0;
			}
			else
			{
				filesFromServer[numOfFiles][j] = temp;
				++j;
			}
			
			
			
		}
		/*fprintf(stderr,"\n******** Server Side File Info ******\n");
		for ( i = 0; i < numOfFiles; i++)
		{
			fprintf(stderr,"file Name:%s ",filesFromServer[i]);
			fprintf(stderr,"modif time:%ld\n",lastModifSecondFromServer[i]);
		}*/

		fileSize = postOrderApply(argv[1], 0, files,lastModificationTimes,lastModifSecond);
		/*fprintf(stderr,"******** MY  File Info ******\n");
		for ( i = 0; i < fileSize; i++)
		{
			fprintf(stderr,"file Name:%s ",files[i]);
			fprintf(stderr,"modif time:%ld\n",lastModifSecond[i]);
		}*/
		
		checkDifference(filesFromServer,lastModifSecondFromServer,files,lastModifSecond,differenceArray,numOfFiles,fileSize,&differenceArraySize);
		/*fprintf(stderr,"\n******** Difference Array ******\n");
		for ( i = 0; i < differenceArraySize; i++)
		{
			fprintf(stderr,"%s ",differenceArray[i]);
			//fprintf(stderr,"%s",lastModificationTimes[i]);
		}*/
		i = 0;
		while (i < fileSize)
		{
			write(fdSocket,&files[i],strlen(files[i]));
			write(fdSocket,&space,sizeof(char));
			write(fdSocket,&lastModifSecond[i],sizeof(long int));
			write(fdSocket,&delim,sizeof(char));
			++i;
		}
		write(fdSocket,&sentinal,sizeof(char));
		
		for ( i = 0; i < differenceArraySize; i++)
		{
			intRead=0;
			fromserverByte=0;
			//fprintf(stderr,"%s ",differenceArray[i]);
			if (differenceArray[i][0] == '-')
			{
				//fprintf(stderr,"%s aciliyor\n",&differenceArray[i][1]);
				createDir(differenceArray[i]);
				outFilePtr=open(&differenceArray[i][1], O_CREAT | O_WRONLY | O_TRUNC,0777);
				
				if (outFilePtr < 0)
				{
					 fprintf (stderr,"Cannot open out directory '%s'\n", &differenceArray[i][1]);
				}
				
				read(fdSocket,&totalByte,sizeof(int));

				//fprintf(stderr,"Client %d kadar byte okumam lazim\n",totalByte);
				//fprintf(stderr,"**** Karsidan okunan *****\n");
				
				while ( intRead < totalByte )
				{
					intRead += read(fdSocket,&readByte,sizeof(char));
					//fprintf(stderr,"%c-",readByte);
				
					write(outFilePtr,&readByte,sizeof(char));
				}
				fprintf(stderr,"%s Serverdan kopyalandi\n",&differenceArray[i][1]);
				close(outFilePtr);
				dateIndex = isStringInArr(filesFromServer,500,&differenceArray[i][1]);
				timeBufToUpdate.modtime = lastModifSecondFromServer[dateIndex];
				utime(&differenceArray[i][1],&timeBufToUpdate);
			}
			else if (differenceArray[i][0] == '+')
			{


				//fprintf(stderr,"%s aciliyor\n",&differenceArray[i][1]);
				inFilePtr = open(&differenceArray[i][1],O_RDONLY);
				//fprintf(stderr,"%s acildi\n",&differenceArray[i][1]);

				serverTotalByte = sizepathfun(&differenceArray[i][1]);
				write(fdSocket,&serverTotalByte,sizeof(int));
				//fprintf(stderr,"Server %d kadar byte okumasi lazim\n",serverTotalByte);
				while ( fromserverByte  < serverTotalByte )
				{
					fromserverByte +=read(inFilePtr,&readByte,sizeof(char));
					write(fdSocket,&readByte,sizeof(char));
				}
				fprintf(stderr,"%s Server'a yollandi\n",&differenceArray[i][1]);
				close(inFilePtr);
			}
			
			
			//fprintf(stderr,"%s",lastModificationTimes[i]);
		}

		//timeInMiliseconds(3);
	}
	

    return 0;
}
int callSocket(char *hostname, unsigned short portnum)
{
	struct sockaddr_in sa;
	
	struct hostent *hp;
	
	int a, s;
	
	if ((hp= gethostbyname(hostname)) == NULL)
	{

		return(-1);
	}
	
	memset(&sa,0,sizeof(sa));



	sa.sin_family= AF_INET;

	sa.sin_port= htons((__u_short)portnum);

	inet_pton(AF_INET, hostname, &sa.sin_addr.s_addr);


	
	if ((s= socket(AF_INET,SOCK_STREAM,0)) < 0)
		return(-1);

	if (connect(s,(struct sockaddr *)&sa,sizeof sa) < 0)
	{
	 /* connect */
		close(s);
		return(-1);
	} /* if connect */

	return(s);
}
void getLastModificationTime(char * fileName, char* lastTime, long int* second,int index){

	struct stat attr;
	stat(fileName, &attr);
   	strcpy(lastTime,ctime(&attr.st_mtime));
	second[index] = attr.st_mtime;

}


int postOrderApply( char* path, int index, char files[][256], char modifTime[][256],long int second[])
{

	struct dirent *pathDirent;
	int sizeFile=0;
	char newPath[255] = {0};
	int fileNameSize;
	int temp;
	int result;
	int i;
	char ch;
	char tempA='a';
	DIR *pathDir;
	
	pathDir = opendir(path);
	
    if (pathDir == NULL) 
	{
        fprintf (stderr,"Cannot open directory '%s'\n", path);
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
            strcpy(files[index],newPath);
            getLastModificationTime(files[index],modifTime[index],second,index);
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

void checkDifference(char sourceFiles[][256], long int sourceFileDate[], char destFiles[][256],long int destFileDate[], char resultArr[][256], int sizeSource, int sizeDest,int* sizeResult){

	int i=0;
	int sizeR=0;
	int status;
	char tempPlus[300];
	char tempMinus[300];
/*
	fprintf(stderr,"\n*************************\n");
	for ( i = 0; i < sizeSource; i++)
	{
		fprintf(stderr,"source: %s ",sourceFiles[i]);
		fprintf(stderr,"date: %ld\n",sourceFileDate[i]);
		
	}

	for ( i = 0; i < sizeDest; i++)
	{
		fprintf(stderr,"Dest: %s ",destFiles[i]);
		fprintf(stderr,"date: %ld\n",destFileDate[i]);
		
	}
	
	*/


	for ( i = 0; i < sizeDest; i++)
	{
		
		status = isStringInArr(sourceFiles,sizeSource,destFiles[i]);
		if (status == -1 )
		{
			//fprintf(stderr,"Bu dosya serverda yok:%s\n",destFiles[i]);
			if (isStringInArr(resultArr,sizeR,destFiles[i]) == -1)
			{
				strcpy(resultArr[sizeR],"+");
				strcat(resultArr[sizeR],destFiles[i]);
				fprintf(stderr,"%s dosya servera yollanacak\n",destFiles[i]);
				++sizeR;
			}
			
			
		}
		else {
			if( sourceFileDate[status] > destFileDate[i] )
			{
				strcpy(resultArr[sizeR],"-");
				strcat(resultArr[sizeR],destFiles[i]);
				fprintf(stderr,"%s dosya serverda daha guncel alinacak\n",destFiles[i]);
				++sizeR;
			}
			else if(sourceFileDate[status] < destFileDate[i]){
				strcpy(resultArr[sizeR],"+");
				strcat(resultArr[sizeR],destFiles[i]);
				fprintf(stderr,"%s dosya serverda daha eski yollanacak\n",destFiles[i]);
				++sizeR;
			}

		}
	}

	for ( i = 0; i < sizeSource; i++)
	{
		status = isStringInArr(destFiles,sizeDest,sourceFiles[i]);
		if (status == -1)
		{
			fprintf(stderr,"Bu dosya clientta yok:%s\n",sourceFiles[i]);
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