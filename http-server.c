
/*
 * http-server.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

static void die(const char *s) { perror(s); exit(1); }

int main(int argc, char **argv)
{
    if (argc != 5) {
        fprintf(stderr, "usage: %s <server_port> <web_root> <mdb-lookup-host> <mdb-lookup-port>\n", argv[0]);
        exit(1);
    }

    unsigned short port = atoi(argv[1]);
    char *webroot = argv[2];
    unsigned short port_mdb = atoi(argv[4]);
    char *serverName= argv[3];
    char *ip;
    char status_code[100];

// ----------------------CONNECTION TO mdb-lookup-server--------------------------
    int sock; // socket descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket failed");

    struct hostent *he;
    // get server ip from server name
    if ((he = gethostbyname(serverName)) == NULL) {
        die("gethostbyname failed");
    }
    ip = inet_ntoa(*(struct in_addr *)he->h_addr);

    // Construct a server address structure

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr)); // must zero out the structure
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(ip);
    servaddr.sin_port        = htons(port_mdb); // must be in network byte order

    // Establish a TCP connection to the server

    if (connect(sock, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0)
        die("connect failed");
    FILE *input = fdopen(sock, "r");


// --------------------CONNECTION TO BROWSER-----------------------------
    // Create a listening socket (also called server socket)

    int servsock;
    if ((servsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        die("socket failed");

    // Construct local address structure

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // any network interface
    addr.sin_port = htons(port);

    // Bind to the local address

    if (bind(servsock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
        die("bind failed");

    // Start listening for incoming connections

    if (listen(servsock, 5 /* queue size for connection requests */ ) < 0)
        die("listen failed");

// -----------------------------------------LOOP TO READ REQUESTS----------------------------------

    int clntsock;
    socklen_t clntlen;
    struct sockaddr_in clntaddr;

    int r;

    char requestLine[4096];

    while (1) {

        snprintf(status_code, sizeof(status_code), "200 OK");

        // Accept an incoming connection

        clntlen = sizeof(clntaddr); // initialize the in-out parameter

        if ((clntsock = accept(servsock,
                        (struct sockaddr *) &clntaddr, &clntlen)) < 0)
            die("accept failed");

        // accept() returned a connected socket (also called client socket)
        // and filled in the client's address into clntaddr


        // Receive the HTTP request

            r = recv(clntsock, requestLine, sizeof(requestLine), 0);
            if (r < 0)
            {
                    close(clntsock);
                    continue;
            }
            else if (r == 0)
            {
                    close(clntsock);
                    continue;
            }
            else
            {
                    requestLine[r]= '\0';
            }


        // Separating HTTP request

            char *token_separators = "\t \r\n"; // tab, space, new line
            char *method = strtok(requestLine, token_separators);
            char *requestURI_tmp = strtok(NULL, token_separators);
            char *httpVersion = strtok(NULL, token_separators);


            char buf[4096];

            //making sure that each of the components is parsed correctly
            if(method == NULL || requestURI_tmp == NULL || httpVersion == NULL)
            {
                    snprintf(buf, sizeof(buf),"HTTP/1.0 501 Not Implemented\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>\r\n");

                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");

                    snprintf(status_code, sizeof(status_code), "501 Not Implemented");

                    printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI_tmp, httpVersion, status_code); //log error to output

                    close(clntsock);
                    continue;

            }


            char requestURI[strlen(requestURI_tmp)+11];
            strcpy(requestURI, requestURI_tmp);
            requestURI[strlen(requestURI_tmp)+10]= '\0';


            if(strcmp(requestURI, "/mdb-lookup") == 0)
            {

                    printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion, status_code);

                    snprintf(buf, sizeof(buf),"HTTP/1.0 200 OK\r\n\r\n""<h1>mdb-lookup</h1>\r\n"
        "<p>\r\n"
        "<form method=GET action=/mdb-lookup>\r\n"
        "lookup: <input type=text name=key>\r\n"
        "<input type=submit>\r\n"
        "</form>\r\n"
        "<p>\r\n");

                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");


                    close(clntsock);
                    continue;
            }
            else if(strstr(requestURI, "/mdb-lookup?key="))
            {
                    char buf[4096];
                    char query[1000];
                    int i;
                    int j= 0;
                    for(i= 16; i< strlen(requestURI); i++, j++)
                    {
                            query[j]= requestURI[i];
                    }
                    query[j]= '\0';
                    printf("looking up [%s]: %s \"%s %s %s\" %s\n", query, inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion, status_code);

                    query[j]= '\n';
                    snprintf(buf, sizeof(buf),"HTTP/1.0 200 OK\r\n\r\n""<h1>mdb-lookup</h1>\r\n"
        "<p>\r\n"
        "<form method=GET action=/mdb-lookup>\r\n"
        "lookup: <input type=text name=key>\r\n"
        "<input type=submit>\r\n"
        "</form>\r\n"
        "<p>\r\n");

                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");

                    snprintf(buf, sizeof(buf), "<p><table border>\r\n");
                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");


                    char lookup_result[4096];
                    int color= 1;


                    if(send(sock, query, j+1, 0) != j+1)
                            die("send failed");

                    while(1)
                    {
                            fgets(lookup_result, sizeof(lookup_result), input);
                            if(lookup_result[0]== '\n')
                            {
                                    break;
                            }

                            if(color%2==1)
                            {
                                    snprintf(buf, sizeof(buf), "<tr><td>");
                                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                                            die("send failed");
                            }
                            else
                            {
                                    snprintf(buf, sizeof(buf), "<tr><td bgcolor=yellow>");
                                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                                            die("send failed");

                            }
                            snprintf(buf, sizeof(buf), "\r\n");
                            if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                                             die("send failed");


                            color++;

                            int k; // size of lookup_result

                            for(k= 0; k< sizeof(lookup_result); k++)
                            {
                                    if(lookup_result[k] == '\n')
                                            break;
                            }

                            snprintf(buf, sizeof(buf), "%s", lookup_result);
                            if (send(clntsock, buf, k, 0) != k)
                                    die("send failed");

                            snprintf(buf, sizeof(buf), "\r\n");
                            if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                                    die("send failed");


                    }
                    snprintf(buf, sizeof(buf), "</table>");
                             if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                                              die("send failed");

                    close(clntsock);
                    continue;
            }
            else
            {
// ------------------------------------------------------------------------

        // 1. Check for invalid GET
            char buf[4096];

            if(strcmp(method, "GET") == 0)
            {
            }
            else
            {
                    snprintf(buf, sizeof(buf),"HTTP/1.0 501 Not Implemented\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>\r\n");

                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");

                    snprintf(status_code, sizeof(status_code), "501 Not Implemented");

                    printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion, status_code); //log error to output

                    close(clntsock);
                    continue;
            }


        // 2. Check for HTTP/1.0 OR 1.1

            if((strcmp(httpVersion, "HTTP/1.0") == 0) || (strcmp(httpVersion, "HTTP/1.1") == 0))
            {
            }
            else
            {
                    snprintf(buf, sizeof(buf),"HTTP/1.0 501 Not Implemented\r\n\r\n<html><body><h1>501 Not Implemented</h1></body></html>\r\n");

                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");

                    snprintf(status_code, sizeof(status_code),"501 Not Implemented");

                    printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion, status_code); //log error to output

                    close(clntsock);
                    continue;
            }


        // 3. Check if URI starts with "/"
           if(requestURI[0] != '/')
           {
                    snprintf(buf, sizeof(buf),"HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>\r\n");

                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");

                    snprintf(status_code, sizeof(status_code), "400 Bad Request");

                    printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion, status_code); //log error to output

                    close(clntsock);
                    continue;
           }


        // 4. Check if URI has "/../"
           if(strstr(requestURI, "/../"))
           {
                    snprintf(buf, sizeof(buf),"HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>\r\n");

                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");

                    snprintf(status_code, sizeof(status_code), "400 Bad Request");

                    printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion, status_code); //log error to output

                    close(clntsock);
                    continue;
           }



        // 5. Check if URI's last three characters are "/.."
           char lastThreeChar[4];
           int i,j=0;
           for(i= strlen(requestURI)-3; i<strlen(requestURI); i++, j++)
           {
                   lastThreeChar[j]= requestURI[i];
           }
           lastThreeChar[j]= '\0';


           if(strcmp(lastThreeChar, "/..") == 0)
           {
                    snprintf(buf, sizeof(buf),"HTTP/1.0 400 Bad Request\r\n\r\n<html><body><h1>400 Bad Request</h1></body></html>\r\n");

                    if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");

                    snprintf(status_code, sizeof(status_code), "400 Bad Request");

                    printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion, status_code); //log error to output

                    close(clntsock);
                    continue;
           }


        // 6. If "/" at end
           if(requestURI[strlen(requestURI)-1] == '/')
           {
                   strcat(requestURI, "index.html");
           }


        // 7. Check if file or directory
           int isDir= 0; //default is 'is a file'
           struct stat statbuf;
           char webroot_tmp[1000];
           strcpy(webroot_tmp, webroot);

           if(stat(strcat(webroot_tmp, requestURI), &statbuf))
           {
           }
           else
           {

           if (S_ISREG(statbuf.st_mode))
           {
           }
           else if(S_ISDIR(statbuf.st_mode))
           {
                   isDir= 1;
           }

           if(isDir)
           {
                   strcat(requestURI, "/index.html");
           }
           }


        // 8. Checking to open file
           char webroot_new[1000];
           strcpy(webroot_new, webroot);
           strcat(webroot_new, requestURI);
           FILE *fp= fopen(webroot_new, "rb");
           if(fp == NULL)
           {
                   snprintf(buf, sizeof(buf),"HTTP/1.0 404 Not Found\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>\r\n");
                   if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");

                    snprintf(status_code, sizeof(status_code), "404 Not Found");

                    printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion, status_code); //log error to output

                    close(clntsock);
                    continue;

           }


// -------------------------------------------------------------------------

           // Logging status code to output in 200 OK case


           printf("%s \"%s %s %s\" %s\n", inet_ntoa(clntaddr.sin_addr), method, requestURI, httpVersion, status_code);

           // Sending HTML file error code

           char file_buf[4096];
           if(!(strcmp(status_code, "200 OK")))
           {
                   snprintf(buf, sizeof(buf),"HTTP/1.0 200 OK\r\n\r\n");
                   if (send(clntsock, buf, strlen(buf), 0) != strlen(buf))
                            die("send failed");
                   size_t n;
                   while ((n = fread(file_buf, 1, sizeof(file_buf), fp)) > 0)
                   {
                           if (send(clntsock, file_buf, n, 0) != n)
                                   die("send content failed");
                   }
           }


           // Finally, close the client connection and file and go back to accept()

           fclose(fp);
           close(clntsock);
            }
    }

    close(sock);
    fclose(input);
    return 0;
}
