
// modified for supporting multi ports, by Bill.Zhang, in 2022-12-28;

//extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include <unistd.h>
#include <pthread.h>
#include "svdpi.h"
#include "vpi_user.h"
#ifndef PURESIM
	#include "tbxbindings.h"
	#include "tbxmanager.hxx"
#endif
#define  SCOPENAME "Top.uartTransactor"//The hierarchical path of the instance where the DPI-C functions are called
#define  BUFDEP 1024
#define  T_INFO_LEN 25

#define OBJ_NUMBER 20


static char time_info[T_INFO_LEN];


struct thread_para
{
  //unsigned int tid;
  int obj_index;
}thread_para;

//unsigned int threads_buff[OBJ_NUMBER]={0};
static int rxfd[OBJ_NUMBER],txfd[OBJ_NUMBER];
static int pipefd[OBJ_NUMBER][2];
//static char buf[512];
//static char buf[BUFDEP];
static int bufidx[OBJ_NUMBER] = {0};
static int tFlag[OBJ_NUMBER] = {0};
static int line_start_flag[OBJ_NUMBER] = {0};
#ifndef PURESIME
unsigned long long events[OBJ_NUMBER][8000];
unsigned char typein[OBJ_NUMBER][8000];
FILE* record_file[OBJ_NUMBER];
static char obj_name_buffer[OBJ_NUMBER][256];
static char obj_name_buff[OBJ_NUMBER][50];
static int obj_name_buff_count=0;

#endif
#ifdef PURESIM
//extern void setBaudRate(int newBaudRate);
extern void setClocksPerBit(int newClocksPerbit);
extern void setPollingInterval(int newPollingInterval);
extern void setRts(svBit newRts);

//extern unsigned getBaudRate();
extern unsigned getClocksPerBit();
extern unsigned getPollingInterval();
extern unsigned getRts();

extern void putTx(char newTxData);
extern void tbenchTerminate();
#endif
// Call this at init time.
static double timescale_factor = 1.0;
static double precision_conversion_factors[] = {
	1.0, // 1 s (0)
	1.0, // 100 ms (-1)
	1.0, // 10 ms (-2)
	1.0, // 1 ms (-3)
	1.0, // 100 us (-4)
	1.0, // 10 us (-5)
	1.0, // 1 us (-6)
	1.0, // 100 ns (-7)
	1.0, // 10 ns (-8)
	1.0, // 1 ns (-9)
	10.0, // 100 ps (-10)
	100.0, // 10 ps (-11)
	1000.0, // 1 ps (-12)
	10000.0, // 100 fs (-13)
	100000.0, // 10 fs (-14)
	1000000.0, // 1 fs (-15)
};

FILE* in_msg = NULL;
void init_time_library2(){
	int precision_code = -vpi_get( vpiTimePrecision, NULL );
	if( precision_code < 9 )
		printf( "Error: Precisions courser than 1 ns not handled\n" );
	timescale_factor = precision_conversion_factors[precision_code];
}
// Call this whenever you want time in NS
unsigned long long time_in_ns2() {
	static s_vpi_time time = { vpiSimTime, 0, 0, 0.0 };
	vpi_get_time( NULL, &time );
	unsigned long long ret = ( ((unsigned long long)time.high) << 32 ) |
	time.low;
	return (unsigned long long)( (double)ret/timescale_factor+.5 );
}

extern "C" {
void *read_keystrokes(void *context) {//read character from txfd, and put it in buffer
    char c;
    struct thread_para *temp_thread_para=(struct thread_para *)context;
    int obj_index=temp_thread_para->obj_index;

	do {

         //printf("my pid:%d, my tid:%u",getpid(),*((unsigned int *)context));
         /*
         // to get tid(thread id);
         unsigned int temp_tid=*((unsigned int *)context);
         // and query to get the obj_index;
         for(int index_i=0;index_i<OBJ_NUMBER;index_i++)
         {
           if(threads_buff[index_i]==temp_tid)
           {
             obj_index=index_i;
             break;
           }
         }
         */

	 read(txfd[obj_index], &c, 1);

	 //printf("obj:%d->%c\n",obj_index,c);


	 write(pipefd[obj_index][1],&c,1);
//	 if((c == '\n')||(c == '\r')){ //'\n' has different meaning in different systems
//                tFlag = 1;
//                write(rxfd,"\n\r",2);
//         }
//         else {
//                write(rxfd, &c, 1);
//         }
	 } while (1);
}

// exported function;
void getbuf(int obj_index,svBitVecVal* buf, int* count, svBit* eom) {

  // Open file "msg" and start streaming in the bytes..
  if(!in_msg) {
      printf("HVL: Opening file \"msg\"..\n");
      if ( (in_msg = fopen("msgb", "r")) == NULL ) {
//      if ( (in_msg = fopen("msg", "r")) == NULL ) {
         printf("Failed to open in_msg file \"msg\"\n!");
         exit(0);
      };
  }
//  svBit t;
//  go(t);
  



  char b[1];
  size_t bytes;
  
  int i = 0;
//  while((b = fgetc(in_msg)) != EOF) {
  while((bytes = fread(b, sizeof(char), 1, in_msg)) != EOF) {
    svPutPartselBit(buf, b[0], 8*i, 8);
//    write(rxfd, &b, 1);
    if(i==39) {
        *count = 40;
        *eom = 0;
        printf("HVL: Sending 40 bytes..\n");
        return;
    }
    i++;
  }

  // Send the remaining bytes with eom.
  svPutPartselBit(buf, 0, 8*i, 8);
  printf("HVL: Sending last %d bytes.\n", i+1);
  *count = i + 1;
  *eom = 1;

  return;
}

// exported function;
char xterm_transmit_chars(int obj_index) {
    svScope scope;
    int i;
    char c;

    

#ifdef PURESIM
    if(read(pipefd[obj_index][0], &c, 1) < 0){ 
		return 0;
	}
    return c;
#else
	static unsigned long long counter[OBJ_NUMBER] ={0};
	static unsigned int event_number[OBJ_NUMBER]={0};
	static unsigned int total_event[OBJ_NUMBER]={0};
	if(counter[obj_index]==0){
		char temprecordfilename[256];
		memset(temprecordfilename,0,256);
		sprintf(temprecordfilename,"./%s_in.log",obj_name_buff[obj_index]);
                //memcpy(temprecordfilename,obj_name_buffer[obj_index],strlen(obj_name_buffer[obj_index]));

		if(TbxManager::IsInReplayMode()){
			printf("In replay mode now\r\n");
						
			//record_file[obj_index] = fopen("./uart_record_replay.log","r");
			record_file[obj_index] = fopen(temprecordfilename,"r");

			i=0;
			while(2==fscanf(record_file[obj_index],"%lld,%d",&events[obj_index][i],&typein[obj_index][i])){
				printf("%lld,%d",events[obj_index][i],typein[obj_index][i]);
				i++;
			}
			total_event[obj_index] = i;
		}
		else 
			//record_file[obj_index] = fopen("./uart_record_replay.log","w+");
			record_file[obj_index] = fopen(temprecordfilename,"w+");
	}
	counter[obj_index]++;
	scope = svGetScope();
	if(TbxManager::IsInReplayMode()){
		if((event_number[obj_index]<total_event[obj_index])&&(counter[obj_index] == events[obj_index][event_number[obj_index]])){
			c = typein[obj_index][event_number[obj_index]];
			event_number[obj_index]++;
			return c;
		}
		else return 0;
	}
	else{
                
		if(read(pipefd[obj_index][0],&c,1)<0){
			
			return 0;
		}
		//printf("run here obj_index:%d.\n",obj_index);

		fprintf(record_file[obj_index], "%lld,%d\r\n",counter[obj_index],c);
		return c;
	}
#endif
}

void read_config(char* file_path)
{
  FILE* fp;
  char temp_buffer[1024];
    // check file;
  if(access(file_path, R_OK)!=0)
  {
    printf("Can't access the file!\n");
  }
  else
  {
    if(!(fp=fopen(file_path,"r")))
    {
      printf("Can't open the file!\n");
    }
    else
    {
      while(!feof(fp))
      {
        memset(temp_buffer,0,1024);
        fgets(temp_buffer,1024,fp);
        
        // get line tag "\n" from temp_buffer;
        char* str1;
        char* str2;
        str1=NULL;
        str2=NULL;
        str1=strtok_r(temp_buffer,",",&str2);
        if((str1!=NULL)&&(str2!=NULL))
        {
          if(strlen(str2)>0)
          {
            //printf("s1:%s, s2:%s\n",str1,str2);
            int temp_seq_number=atoi(str1);
            memset(obj_name_buffer[temp_seq_number],0,256);
            memcpy(obj_name_buffer[temp_seq_number],str2,strlen(str2)-1);
            //printf("%d,%s",temp_seq_number,str2);
          }
        }
      }
      fclose(fp);
    }
  }

}


// exported function;
void xterm_init(int obj_index,const svBitVecVal* obj_name_bits,int byte_count) {

    int sel_item=obj_index;
    char obj_name_bytes[50];

    svScope tempSvScope=svGetScope();
    const char* tempSvScopeName;
    tempSvScopeName=svGetNameFromScope(tempSvScope);
    printf("Getted scope name is:%s\n",tempSvScopeName);
    //
    int in=0;
    char* str1[100];
    char* str2;
    char pBufArray[256];
    memset(pBufArray,0,256);
    memcpy(pBufArray,tempSvScopeName,strlen(tempSvScopeName));
    char* pBuffer;
    pBuffer=pBufArray;


    while((str1[in]=strtok_r(pBuffer,".",&str2))!=NULL)
    {
      pBuffer=str2;
      in++;
    }
    if(in>0)
    {
      printf("entry is:%s\n",str1[in-1]);
    }
    //str1=strtok_r(tempSvScopeName,".",&str2);
   


    obj_name_buff_count=0;
    memset(obj_name_bytes,0,50);
    memset(obj_name_buff[obj_index],0,50);
    memcpy(obj_name_bytes,obj_name_bits,byte_count+1);


    if(byte_count>1)
    {
      for (int temp_i=byte_count;temp_i>0;temp_i--)
      {
        //printf("%02x",obj_name_bytes[temp_i]);
        // reverse the data;
        obj_name_buff[obj_index][obj_name_buff_count]=obj_name_bytes[temp_i];
        obj_name_buff_count++;
      }
      //printf("%llx",*obj_name_bits);
      //printf("\n");
    }
    else
    {
      memcpy(obj_name_buff[obj_index],str1[in-1],strlen(str1[in-1]));
    }

    for(int temp_i=0;temp_i<OBJ_NUMBER;temp_i++)
    {
      memset(obj_name_buffer[temp_i],0,256);
    }

    read_config("uart-xterm.cfg");

/*
    for(int temp_i=0;temp_i<OBJ_NUMBER;temp_i++)
    {
      printf("seq:%d,name:%s\n",temp_i,obj_name_buffer[temp_i]);
    }
*/


    char obj_name[256];
    memset(obj_name,0,256);

/*
    memcpy(obj_name,obj_name_buffer[sel_item],strlen(obj_name_buffer[sel_item]));
    printf("cur:%d,name:%s\n",sel_item,obj_name);
*/

    memcpy(obj_name,obj_name_buff[obj_index],strlen(obj_name_buff[obj_index]));


    char uartfifo_buffer[256];
    char uartfifo_tx_buffer[256];
    char uart_xterm_buffer[256];

    memset(uartfifo_buffer,0,256);
    memset(uartfifo_tx_buffer,0,256);
    memset(uart_xterm_buffer,0,256);


    sprintf(uartfifo_buffer,"./uart_fifo/uartfifo_%d",sel_item);
    sprintf(uartfifo_tx_buffer,"./uart_fifo/uartfifo-tx_%d",sel_item);

    sprintf(uart_xterm_buffer,"xterm -T \"%s\" -e ./uart-xterm %d %s&",obj_name_buff[obj_index],sel_item,obj_name_buff[obj_index]);

/*
    if(strlen(obj_name)==0)
    {
      // program_id, log file name;
      // sprintf(uart_xterm_buffer,"xterm -T \"test_%d\" -e ./uart-xterm %d %d&",sel_item,sel_item,sel_item);
      sprintf(uart_xterm_buffer,"xterm -T \"%s\" -e ./uart-xterm %d %s&",obj_name_buff,sel_item,obj_name_buff);

    }
    else
    {
      // program_id, log file name;
      sprintf(uart_xterm_buffer,"xterm -T \"%s\" -e ./uart-xterm %d %s&",obj_name,sel_item,obj_name);
    }
*/

    pthread_t tid;

    unlink(uartfifo_buffer);//delete /tmp/uartfifo
    if (0 != mkfifo(uartfifo_buffer, 0644)) {
        perror("mkfifo(./uart_fifo/uartfifo)");
        exit(-1);
    }
    unlink(uartfifo_tx_buffer);//delete /tmp/uartfifo-tx
    if (0 != mkfifo(uartfifo_tx_buffer, 0644)) {
        perror("mkfifo(./uart_fifo/uartfifo-tx)");
        exit(-1);
    }
    if(0>(pipe(pipefd[obj_index]))){
    	perror("pipe(pipefd)");
    	exit(-1);
    }
    if (fcntl(pipefd[obj_index][0],F_SETFL, O_NONBLOCK) ==-1){
    	fprintf(stderr, "Call to fcntl failed.\n");
    	exit(1);
    }
    //system("xterm -e ./uart-xterm 1&");//open a new xterm and run uart-xterm
    system(uart_xterm_buffer);

    if (0 > (rxfd[obj_index] = open (uartfifo_buffer, O_WRONLY))) {
        perror("open(./uart_fifo/uartfifo)");
        fflush(stdout);
        exit(-1);
    }

    if (0 > (txfd[obj_index] = open (uartfifo_tx_buffer, O_RDONLY))) {
        perror("open(./uart_fifo/uartfifo-tx)");
        fflush(stdout);
        exit(-1);
    }



/*
    unlink("./uart_fifo/uartfifo_1");//delete /tmp/uartfifo
    if (0 != mkfifo("./uart_fifo/uartfifo_1", 0644)) {
        perror("mkfifo(./uart_fifo/uartfifo)");
        exit(-1);
    }
    unlink("./uart_fifo/uartfifo-tx_1");//delete /tmp/uartfifo-tx
    if (0 != mkfifo("./uart_fifo/uartfifo-tx_1", 0644)) {
        perror("mkfifo(./uart_fifo/uartfifo-tx)");
        exit(-1);
    }
    if(0>(pipe(pipefd))){
    	perror("pipe(pipefd)");
    	exit(-1);
    }
    if (fcntl(pipefd[0],F_SETFL, O_NONBLOCK) ==-1){
    	fprintf(stderr, "Call to fcntl failed.\n");
    	exit(1);
    }
    system("xterm -e ./uart-xterm 1&");//open a new xterm and run uart-xterm
    if (0 > (rxfd = open ("./uart_fifo/uartfifo_1", O_WRONLY))) {
        perror("open(./uart_fifo/uartfifo)");
        fflush(stdout);
        exit(-1);
    }

    if (0 > (txfd = open ("./uart_fifo/uartfifo-tx_1", O_RDONLY))) {
        perror("open(./uart_fifo/uartfifo-tx)");
        fflush(stdout);
        exit(-1);
    }
*/



    //pthread_create(&tid, NULL, &read_keystrokes, NULL);
    struct thread_para temp_thread_para;
    memset(&temp_thread_para,0,sizeof(struct thread_para));
    temp_thread_para.obj_index=obj_index;
    //pthread_create(&tid, NULL, &read_keystrokes, (void *)(&tid));
    pthread_create(&tid, NULL, &read_keystrokes, &temp_thread_para);
    //threads_buff[obj_index]=*((unsigned int *)tid);


    
}


// exported function;
void sendRxToXterm(int obj_index, char b) {

    //printf("run here obj_index:%d,%c\n",obj_index,b);


    if((b=='\n')||(b == '\r')){
		tFlag[obj_index] = 1;
		write(rxfd[obj_index], "\n\r", 2);
		line_start_flag[obj_index] = 1;
	}
	else{
		if(line_start_flag[obj_index] == 1){				
			sprintf(time_info,"%ld ns->%d: ",time_in_ns2(),obj_index);
			line_start_flag[obj_index] = 0;
			write(rxfd[obj_index],time_info,25);
			write(rxfd[obj_index], &b, 1);
		}
		else write(rxfd[obj_index], &b, 1);
	}

}
}
