
// modified for supporting multi ports, by Bill.Zhang, in 2022-12-28;


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "string.h"
#include <pthread.h>
#include "light_uart.h"

static int rxfd,txfd;
LightUart *pLightUartObj; 

void *capture_keystrokes(void *context) {
    do {
        char c;
        read(0, &c, 1);
        if (c != '\r') {
          write(txfd, &c, 1);
	  //printf("my pid:%d",getpid());

	  fputc(c,pLightUartObj->uartlog);
         }
        
    } while (1);
    return NULL;
}







int main(int argc, char *argv[]) {
    pthread_t tid;
 
    pLightUartObj=new LightUart();
    char *tempVersion=pLightUartObj->GetVersion();
    printf("Ver:%s\n",tempVersion);
    pLightUartObj->InitAll();


    if(argc!=3)
    {

      perror("parameter error!");

      return -1;
    }


    sprintf(pLightUartObj->uartfifo_buffer,"./uart_fifo/uartfifo_%s",argv[1]);
    sprintf(pLightUartObj->uartfifo_tx_buffer,"./uart_fifo/uartfifo-tx_%s",argv[1]);

    //system("/bin/stty raw -echo");
    system("/bin/stty sane ignbrk intr ^k eof ^@");


    if (0 > (rxfd = open (pLightUartObj->uartfifo_buffer, O_RDONLY))) {
        perror("open(./uart_fifo/uartfifo)");
        return -1;
    }
    if (0 > (txfd = open (pLightUartObj->uartfifo_tx_buffer, O_WRONLY))) {
        perror("open(./uart_fifo/uartfifo-tx)");
        return -1;
    }

   
    pthread_create(&tid, NULL, &capture_keystrokes, NULL);

    sprintf(pLightUartObj->tempuartlogfilename,"./%s.log",argv[2]);

    pLightUartObj->uartlog = fopen(pLightUartObj->tempuartlogfilename,"w+");


    while (read(rxfd,&pLightUartObj->c,1) == 1) {
        write(1,&pLightUartObj->c,1);
      if ((pLightUartObj->c != '\r')  && ( (int) pLightUartObj->c != 0)) {
	fputc(pLightUartObj->c,pLightUartObj->uartlog);
        fflush(pLightUartObj->uartlog);
      }
    }
    fclose(pLightUartObj->uartlog);

    return 0;
}

