
// modified for supporting multi ports, by Bill.Zhang, in 2022-12-28;


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include <pthread.h>

extern "C" {
static int rxfd,txfd;
FILE *uartlog;
void *capture_keystrokes(void *context) {
    do {
        char c;
        read(0, &c, 1);
        if (c != '\r') {
          write(txfd, &c, 1);
	  fputc(c,uartlog);
         }
        
    } while (1);
    return NULL;
}

FILE *in_msg;
FILE *out_rec;

int main(int argc, char *argv[]) {
    char c;
    char buffer[10];
    pthread_t tid;
    long long int offset;
    int res = 0;
    char uartfifo_buffer[256];
    char uartfifo_tx_buffer[256];

/*
    if(argc!=2)
    {

      perror("parameter error!");

      return -1;
    }
*/

    memset(uartfifo_buffer,0,256);
    memset(uartfifo_tx_buffer,0,256);
    sprintf(uartfifo_buffer,"./uart_fifo/uartfifo_%s",argv[1]);
    sprintf(uartfifo_tx_buffer,"./uart_fifo/uartfifo-tx_%s",argv[1]);

    //system("/bin/stty raw -echo");
    system("/bin/stty sane ignbrk intr ^k eof ^@");
/*
    if (0 > (rxfd = open ("./uart_fifo/uartfifo", O_RDONLY))) {
        perror("open(./uart_fifo/uartfifo)");
        return -1;
    }
    if (0 > (txfd = open ("./uart_fifo/uartfifo-tx", O_WRONLY))) {
        perror("open(./uart_fifo/uartfifo-tx)");
        return -1;
    }
*/
    if (0 > (rxfd = open (uartfifo_buffer, O_RDONLY))) {
        perror("open(./uart_fifo/uartfifo)");
        return -1;
    }
    if (0 > (txfd = open (uartfifo_tx_buffer, O_WRONLY))) {
        perror("open(./uart_fifo/uartfifo-tx)");
        return -1;
    }

   
    pthread_create(&tid, NULL, &capture_keystrokes, NULL);

    uartlog = fopen("./uart.log","w+");
//    in_msg  = fopen("./dumpfile", "r");
//    out_rec = fopen("./offset","r");
//    res = fscanf(out_rec,"%lld", &offset);
//
//    fprintf(uartlog, "Offset read from offset file is: %lld\n",offset);
//    fclose(out_rec);
//    fseek(in_msg, offset, SEEK_SET);
//    out_rec = fopen("./offset","w+");
//    fread(buffer, sizeof(char), 1, in_msg);
//    offset = ftell(in_msg);
//    fprintf(uartlog, "0Now offset is: %lld\n",offset);
//    while (!feof(in_msg)) {
//      offset = ftell(in_msg);
////      fprintf(uartlog, "1Now offset is: %lld\r\n",offset);
//      c = buffer[0];
////      fprintf(uartlog, "1 readout c is: %c\r\n",c);
//      if ((c != '\r')  && ( (int) c != 0)) {
//        write(txfd, &c, 1);  
//      }
//
//      fseek(out_rec,0,SEEK_SET);
//      offset = ftell(in_msg);
// //     fprintf(uartlog, "2Now offset is: %lld\r\n",offset);
//      fflush(uartlog);
//      fprintf(out_rec, "%lld\r\n",offset);
//      fflush(out_rec);
//      fread(buffer, sizeof(char), 1, in_msg);
//    }
////    fprintf(uartlog,"Exiting while loop\n");
//    fflush(uartlog);
//    fclose(in_msg);
//    fclose(out_rec);
    

    while (read(rxfd,&c,1) == 1) {
        write(1,&c,1);
      if ((c != '\r')  && ( (int) c != 0)) {
	fputc(c,uartlog);
        fflush(uartlog);
      }
    }
    fclose(uartlog);

    return 0;
}
}
