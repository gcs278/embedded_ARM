/**
 * \addtogroup httpd
 * @{
 */

/**
 * \file
 *         Web server script interface
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 */

/*
 * Copyright (c) 2001-2006, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * $Id: httpd-cgi.c,v 1.2 2006/06/11 21:46:37 adam Exp $
 *
 */

#include "uip.h"
#include "psock.h"
#include "httpd.h"
#include "httpd-cgi.h"
#include "httpd-fs.h"
//that my bitch
#include "mywebmap.h"

#include <stdio.h>
#include <string.h>

HTTPD_CGI_CALL(file, "file-stats", file_stats);
HTTPD_CGI_CALL(tcp, "tcp-connections", tcp_stats);
HTTPD_CGI_CALL(net, "net-stats", net_stats);
HTTPD_CGI_CALL(rtos, "rtos-stats", rtos_stats );
HTTPD_CGI_CALL(run, "run-time", run_time );
HTTPD_CGI_CALL(io, "led-io", led_io );
HTTPD_CGI_CALL(test, "test", test_out );
HTTPD_CGI_CALL (map, "map-o", map_stats);


static const struct httpd_cgi_call *calls[] = { &file, &tcp, &net, &rtos, &run, &io, &test, &map, NULL };

/*---------------------------------------------------------------------------*/
static
PT_THREAD(nullfunction(struct httpd_state *s, char *ptr))
{
  PSOCK_BEGIN(&s->sout);
  ( void ) ptr;
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
httpd_cgifunction
httpd_cgi(char *name)
{
  const struct httpd_cgi_call **f;

  /* Find the matching name in the table, return the function. */
  for(f = calls; *f != NULL; ++f) {
    if(strncmp((*f)->name, name, strlen((*f)->name)) == 0) {
      return (*f)->function;
    }
  }
  return nullfunction;
}
/*---------------------------------------------------------------------------*/
static unsigned short
generate_file_stats(void *arg)
{
  char *f = (char *)arg;
  return snprintf((char *)uip_appdata, UIP_APPDATA_SIZE, "%5u", httpd_fs_count(f));
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(file_stats(struct httpd_state *s, char *ptr))
{
	//vSemaphoreCreateBinary( mapStruct.SEMForTotalDistance);
  PSOCK_BEGIN(&s->sout);

  PSOCK_GENERATOR_SEND(&s->sout, generate_file_stats, strchr(ptr, ' ') + 1);

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static char mybuff[100];
static char mybuff2[100];
static char mybuff3[10];
static char mybuff4[100];
static char mybuff5[100];
static unsigned short
generate_test_out(void *arg)
{
	// Outputs two buttons, start and stop
	/*char mybuff[1000];
	char mybuff2[100];
	char mybuff3[10];
	char mybuff4[100];
	char mybuff5[100]; */
	//char data[65] = "<button type=\"submit\" name=\"run\" value=\"1\">START</button>";
	//strcat(data, "<button type=\"submit\" name=\"run\" value=\"0\">STOP</button>");
	//strcat(data, "<button type=\"submit\" name=\"run\" value=\"2\">LEFT</button>");
	//strcat(data, "<button type=\"submit\" name=\"run\" value=\"3\">RIGHT</button>");
	//strcat(data, "<dr><button type=\"submit\" name=\"run\" value=\"4\">FIRST RUN</button>");
	//strcat(data, "<button type=\"submit\" name=\"run\" value=\"5\">START MAPPING</button>");
	//strcat(data, "<button type=\"submit\" name=\"run\" value=\"6\">USE MAPPING</button>");
	printf("time");	
	printf("%d\n",mapStruct.deciSeconds);
	sprintf(mybuff3,"%d",  mapStruct.deciSeconds );
	printf(mybuff3);
	sprintf(mybuff4, "%d:",  mapStruct.seconds );
	sprintf(mybuff5,"%d:",  mapStruct.min );
	strcat(uip_appdata, "<br>Elaspe Time :" ); 
	strcat(uip_appdata, mybuff5);
	strcat(uip_appdata, mybuff4);
	strcat(uip_appdata, mybuff3);
	strcat(uip_appdata, "<br>" );
	printf("JUST IN CASE IT CRASH IN HERE I CAN TELL");	 
	//strcat(data, "DATA :");
	//printf("SEMAPHORE TIME\n");
	if( mapStruct.SEMForTotalDistance != NULL){
		if( xSemaphoreTake( mapStruct.SEMForTotalDistance , 10 ) == pdPASS ) {
		//	printf("DEEP INSIDE");
			strcat(uip_appdata, "<br> Total Distance traveled : ");
			sprintf(mybuff, "%d",  mapStruct.totalDistanceTraveled );
			//printf("%d",mapStruct.totalDistanceTraveled);
			strcat(uip_appdata, mybuff);
			strcat(uip_appdata, "<br>");
			//snprintf(mybuff2,sizeof(mybuff2), "%d",  mapStruct.sensor1);
			//strcat(data, mybuff2);
			//sprintf(uip_appdata,data);
			if(	xSemaphoreGive( mapStruct.SEMForTotalDistance ) == pdFALSE )
				{
					//printf("YOU DONE FUCKED UP MARTIN");
				}
		//	printf("GIVE IT AWAY NOW");
		//return strlen(uip_appdata);
		}
	}
	if(mapStruct.SEMForSensors != NULL)
			{
				if( xSemaphoreTake( mapStruct.SEMForSensors , 1 ) == pdPASS ) {
					strcat(uip_appdata, "<br> Sensors 1: ");
					//snprintf(mybuff2,sizeof(mybuff2), "%d,%d,%d,%d",  mapStruct.sensor1,mapStruct.sensor2,mapStruct.sensor3,mapStruct.sensor4 );
					//printf("take\n");
					sprintf(mybuff2,"%d",  mapStruct.sensor1);
					strcat(uip_appdata, mybuff2);
					strcat(uip_appdata, "<br>");
					strcat(uip_appdata, "<br> Sensors 2: ");
					//snprintf(mybuff2,sizeof(mybuff2), "%d,%d,%d,%d",  mapStruct.sensor1,mapStruct.sensor2,mapStruct.sensor3,mapStruct.sensor4 );
					//printf("take\n");
					sprintf(mybuff2, "%d",  mapStruct.sensor2);
					strcat(uip_appdata, mybuff2);
					strcat(uip_appdata, "<br>");
					strcat(uip_appdata, "<br> Sensors 3: ");
					//snprintf(mybuff2,sizeof(mybuff2), "%d,%d,%d,%d",  mapStruct.sensor1,mapStruct.sensor2,mapStruct.sensor3,mapStruct.sensor4 );
					//printf("take\n");
					sprintf(mybuff2, "%d",  mapStruct.sensor3);
					strcat(uip_appdata, mybuff2);
					strcat(uip_appdata, "<br>");
					strcat(uip_appdata, "<br> Sensors 4 : ");
					//snprintf(mybuff2,sizeof(mybuff2), "%d,%d,%d,%d",  mapStruct.sensor1,mapStruct.sensor2,mapStruct.sensor3,mapStruct.sensor4 );
					//printf("take\n");
					sprintf(mybuff2, "%d",  mapStruct.sensor4);
					strcat(uip_appdata, mybuff2);
					strcat(uip_appdata, "<br>");
						if(	xSemaphoreGive( mapStruct.SEMForSensors ) == pdFALSE )
						{
							//printf("YOU DONE FUCKED UP A-AARON");
						}
					}
					//printf("Give\n");
			} 
	/*else{
	    printf("NOPE");	
  		sprintf(uip_appdata,data);
		return strlen(uip_appdata);
	}*/
	//sprintf(uip_appdata,data);
	//printf(data);
	//printf("%d", sizeof(data));
	return strlen(uip_appdata);

  
}
static
PT_THREAD(test_out(struct httpd_state *s, char *ptr))
{
  PSOCK_BEGIN(&s->sout);

  PSOCK_GENERATOR_SEND(&s->sout, generate_test_out, strchr(ptr, ' ') + 1);

  PSOCK_END(&s->sout);
}

static const char closed[] =   /*  "CLOSED",*/
{0x43, 0x4c, 0x4f, 0x53, 0x45, 0x44, 0};
static const char syn_rcvd[] = /*  "SYN-RCVD",*/
{0x53, 0x59, 0x4e, 0x2d, 0x52, 0x43, 0x56,
 0x44,  0};
static const char syn_sent[] = /*  "SYN-SENT",*/
{0x53, 0x59, 0x4e, 0x2d, 0x53, 0x45, 0x4e,
 0x54,  0};
static const char established[] = /*  "ESTABLISHED",*/
{0x45, 0x53, 0x54, 0x41, 0x42, 0x4c, 0x49, 0x53, 0x48,
 0x45, 0x44, 0};
static const char fin_wait_1[] = /*  "FIN-WAIT-1",*/
{0x46, 0x49, 0x4e, 0x2d, 0x57, 0x41, 0x49,
 0x54, 0x2d, 0x31, 0};
static const char fin_wait_2[] = /*  "FIN-WAIT-2",*/
{0x46, 0x49, 0x4e, 0x2d, 0x57, 0x41, 0x49,
 0x54, 0x2d, 0x32, 0};
static const char closing[] = /*  "CLOSING",*/
{0x43, 0x4c, 0x4f, 0x53, 0x49,
 0x4e, 0x47, 0};
static const char time_wait[] = /*  "TIME-WAIT,"*/
{0x54, 0x49, 0x4d, 0x45, 0x2d, 0x57, 0x41,
 0x49, 0x54, 0};
static const char last_ack[] = /*  "LAST-ACK"*/
{0x4c, 0x41, 0x53, 0x54, 0x2d, 0x41, 0x43,
 0x4b, 0};

static const char *states[] = {
  closed,
  syn_rcvd,
  syn_sent,
  established,
  fin_wait_1,
  fin_wait_2,
  closing,
  time_wait,
  last_ack};


static unsigned short
generate_tcp_stats(void *arg)
{
  struct uip_conn *conn;
  struct httpd_state *s = (struct httpd_state *)arg;

  conn = &uip_conns[s->count];
  return snprintf((char *)uip_appdata, UIP_APPDATA_SIZE,
		 "<tr><td>%d</td><td>%u.%u.%u.%u:%u</td><td>%s</td><td>%u</td><td>%u</td><td>%c %c</td></tr>\r\n",
		 htons(conn->lport),
		 htons(conn->ripaddr[0]) >> 8,
		 htons(conn->ripaddr[0]) & 0xff,
		 htons(conn->ripaddr[1]) >> 8,
		 htons(conn->ripaddr[1]) & 0xff,
		 htons(conn->rport),
		 states[conn->tcpstateflags & UIP_TS_MASK],
		 conn->nrtx,
		 conn->timer,
		 (uip_outstanding(conn))? '*':' ',
		 (uip_stopped(conn))? '!':' ');
}
/*---------------------------------------------------------------------------*/
static
PT_THREAD(tcp_stats(struct httpd_state *s, char *ptr))
{

  PSOCK_BEGIN(&s->sout);
  ( void ) ptr;
  for(s->count = 0; s->count < UIP_CONNS; ++s->count) {
    if((uip_conns[s->count].tcpstateflags & UIP_TS_MASK) != UIP_CLOSED) {
      PSOCK_GENERATOR_SEND(&s->sout, generate_tcp_stats, s);
    }
  }

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
static unsigned short
generate_net_stats(void *arg)
{
  struct httpd_state *s = (struct httpd_state *)arg;
  return snprintf((char *)uip_appdata, UIP_APPDATA_SIZE,
		  "%5u\n", ((uip_stats_t *)&uip_stat)[s->count]);
}
				
static
PT_THREAD(net_stats(struct httpd_state *s, char *ptr))
{
  PSOCK_BEGIN(&s->sout);

  ( void ) ptr;
#if UIP_STATISTICS

  for(s->count = 0; s->count < sizeof(uip_stat) / sizeof(uip_stats_t);
      ++s->count) {
    PSOCK_GENERATOR_SEND(&s->sout, generate_net_stats, s);
  }

#endif /* UIP_STATISTICS */

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/

extern void vTaskList( signed char *pcWriteBuffer );
extern char *pcGetTaskStatusMessage( void );
static char cCountBuf[ 128 ];
long lRefreshCount = 0;
static unsigned short
generate_rtos_stats(void *arg)
{
	( void ) arg;
	lRefreshCount++;
	sprintf( cCountBuf, "<p><br>Refresh count = %d<p><br>%s", (int)lRefreshCount, pcGetTaskStatusMessage() );
    vTaskList( uip_appdata );
	strcat( uip_appdata, cCountBuf );

	return strlen( uip_appdata );
}
/*---------------------------------------------------------------------------*/


static
PT_THREAD(rtos_stats(struct httpd_state *s, char *ptr))
{
  PSOCK_BEGIN(&s->sout);
  ( void ) ptr;
  PSOCK_GENERATOR_SEND(&s->sout, generate_rtos_stats, NULL);
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/

char *pcStatus;
unsigned long ulString;

static unsigned short generate_io_state( void *arg )
{
extern long lParTestGetLEDState( void );

	( void ) arg;

	/* Get the state of the LEDs that are on the FIO1 port. */
	if( lParTestGetLEDState() )
	{
		pcStatus = "";
	}
	else
	{
		pcStatus = "checked";
	}

	sprintf( uip_appdata,
		"<input type=\"checkbox\" name=\"LED0\" value=\"1\" %s>LED<p><p>", pcStatus );

	return strlen( uip_appdata );
}
/*---------------------------------------------------------------------------*/

extern void vTaskGetRunTimeStats( signed char *pcWriteBuffer );
static unsigned short
generate_runtime_stats(void *arg)
{
	( void ) arg;
	lRefreshCount++;
	sprintf( cCountBuf, "<p><br>Refresh count = %d", (int)lRefreshCount );
    vTaskGetRunTimeStats( uip_appdata );
	strcat( uip_appdata, cCountBuf );

	return strlen( uip_appdata );
}
/*---------------------------------------------------------------------------*/


static
PT_THREAD(run_time(struct httpd_state *s, char *ptr))
{
  PSOCK_BEGIN(&s->sout);
  ( void ) ptr;
  PSOCK_GENERATOR_SEND(&s->sout, generate_runtime_stats, NULL);
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/


static PT_THREAD(led_io(struct httpd_state *s, char *ptr))
{
  PSOCK_BEGIN(&s->sout);
  ( void ) ptr;
  PSOCK_GENERATOR_SEND(&s->sout, generate_io_state, NULL);
  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
extern void vTaskGetMapWebString( char *pcWriteBuffer);
static char myvarBuf[100];
static unsigned	short
generate_map_stats(void *arg)
{

	//( void ) arg;
	
	// Currently starts at 100
	int xAxis = 100;
	int yAxis = 100;
	//int size=8;
		
	sprintf(uip_appdata, "<script>");

	int i =0;
	for(i = 0 ; i<num_walls; i++) {
		if (web_walls[i].direction == 270)
			xAxis = xAxis + web_walls[i].length;
		else if (web_walls[i].direction == 90)
			xAxis = xAxis - web_walls[i].length;
		else if (web_walls[i].direction == 0)
			yAxis = yAxis + web_walls[i].length;
		else if (web_walls[i].direction == 180)
			yAxis = yAxis - web_walls[i].length;

		strcat(uip_appdata, "ctx.lineTo(");
		sprintf(myvarBuf, "%d",  yAxis);
		printf( "y%d\n",  yAxis);
		strcat(uip_appdata, myvarBuf);
		strcat(uip_appdata, ",");
		sprintf(myvarBuf, "%d",  xAxis);
		printf("x%d\n",  xAxis);
		strcat(uip_appdata, myvarBuf);
		strcat(uip_appdata, ");");
	}

	strcat(uip_appdata, "</script>");
	return strlen( uip_appdata );
	
}
static
PT_THREAD(map_stats(struct httpd_state *s, char *ptr))
{
  PSOCK_BEGIN(&s->sout);

  PSOCK_GENERATOR_SEND(&s->sout, generate_map_stats, strchr(ptr, ' ') + 1);

  PSOCK_END(&s->sout);
} 

/** @} */






