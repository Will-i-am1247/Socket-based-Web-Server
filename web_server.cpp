// **************************************************************************************
// * Echo Strings (echo_s.cc)
// * -- Accepts TCP connections and then echos back each string sent.
// **************************************************************************************
#include "web_server.h"
#include <iostream>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>
#include <regex>

#include "logging.h"

using namespace std;

int readRequest(int sockFd, string* filename) {
  char buffer[1024];
  ssize_t takenBytes = read(sockFd, buffer, sizeof(buffer) - 1);

  if (takenBytes <= 0) {
    DEBUG << "Failed reading request sending a 400 Bad Request." << ENDL;
    return 400;
  }

  buffer[takenBytes] = '\0';
  string request(buffer);

  //find header
  size_t headerEnd = request.find("\r\n\r\n");
  if (headerEnd == string::npos) {
    DEBUG << "Bad request sending 400 Bad Request." << ENDL;
    return 400;
  }

  //look fro request
  string requestLine = request.substr(0, headerEnd);
  regex validFilePattern("GET\\s+/(file\\d\\.html|image\\d\\.jpg)\\s+HTTP/\\d\\.\\d");
  regex genericPattern("GET\\s+/([^\\s]+)\\s+HTTP/\\d\\.\\d");
  smatch match;

  if (regex_search(requestLine, match, validFilePattern) || regex_search(requestLine, match, genericPattern)) {
    *filename = match[1].str();
    return 200;
  } else {
    DEBUG << "Incorrect formating sending 400 Bad Request." << ENDL;
    return 400;
  }
}

void sendLine(int sockFd, const string& line) {
  send(sockFd, line.c_str(), line.size(), 0);
}

void send404(int sockFd) {
  //this is for the 404 error
  string error404 = "HTTP/1.0 404 Not Found\r\n"
                   "Content-Type: text/html\r\n\r\n";
  string note = "<html><note>404 Not Found</note></html>";

  sendLine(sockFd, error404);
  sendLine(sockFd, note);

  DEBUG << "File not found sending 404 Not Found." << ENDL;
}

void send400(int sockFd) {
  string response = "HTTP/1.0 400 Bad Request\r\n\r\n";
  sendLine(sockFd, response);
}

void send200(int sockFd, const string& filename) {
  struct stat file;

  //check for file
  if (stat(filename.c_str(), &file) != 0) {
    DEBUG << "File not found sending 404 Not Found." << ENDL;
    send404(sockFd);
    return;
  }

  string error404 = "HTTP/1.0 200 OK\r\n";
  string contentType;

  size_t extensionPos = filename.find_last_of('.');
  if (extensionPos != string::npos) {
    string extension = filename.substr(extensionPos);
    if (extension == ".html") {
      contentType = "text/html";
    } else if (extension == ".jpg") {
      contentType = "image/jpeg";
    }
  }

   if (!contentType.empty()) {
    error404.append("Content-Type: ").append(contentType).append("\r\n");
  }
  error404.append("Content-Length: ").append(to_string(file.st_size)).append("\r\n\r\n");
  sendLine(sockFd, error404);

  //buffer size
  const size_t bufferSize = 1024;
  char buffer[bufferSize];

  //open the file for reading
  int fileDescriptor = open(filename.c_str(), O_RDONLY);
  if (fileDescriptor == -1) {
    DEBUG << "Failed to open " << filename << " sending 404 error response." << ENDL;
    send404(sockFd);
    return;
  }

  //tead and send file
  while (true) {
    ssize_t takenBytes = read(fileDescriptor, buffer, bufferSize);
    if (takenBytes < 0) {
      DEBUG << "Error reading " << filename << " failed to transfer." << ENDL;
      break;
    }
    if (takenBytes == 0) {
      break;
    }
    if (write(sockFd, buffer, takenBytes) < 0) {
      DEBUG << "Error sending data to " << filename << ENDL;
      break;
    }
  }

  close(fileDescriptor);
}




// **************************************************************************************
// * processConnection()
// **************************************************************************************
int processConnection(int sockFd) {
  string filename;
  int serverReturn = readRequest(sockFd, &filename);

  //chck return code and handle the request depending on 400 or 200
  switch (serverReturn) {
    case 400: 
      send400(sockFd);
      
    case 200: 
      send200(sockFd, filename);
       
    default:
      //just in case there are any return codes that were not mathcing
      DEBUG << "Different return code: " << serverReturn << ". Closing connection." << ENDL;
      break;
  }

  close(sockFd);
  return 0;
}
    


// **************************************************************************************
// * main()
// * - Sets up the sockets and accepts new connection until processConnection() returns 1
// **************************************************************************************

int main (int argc, char *argv[]) {

  // ********************************************************************
  // * Process the command line arguments
  // ********************************************************************
  int opt = 0;
  while ((opt = getopt(argc,argv,"d:")) != -1) {
    
    switch (opt) {
    case 'd':
      LOG_LEVEL = std::stoi(optarg);;
      break;
    case ':':
    case '?':
    default:
      std::cout << "useage: " << argv[0] << " -d <num>" << std::endl;
      exit(-1);
    }
  }

  // *******************************************************************
  // * Creating the inital socket is the same as in a client.
  // ********************************************************************
  int listenFd = -1;
  // Call socket() to create the socket you will use for lisening.
  listenFd = socket(AF_INET, SOCK_STREAM, 0);




  if (listenFd < 0){
    FATAL << "Socket cannot be created" << ENDL;
    exit(-1);
  }
  // ********************************************************************
  // * The bind() and calls take a structure that specifies the
  // * address to be used for the connection. On the cient it contains
  // * the address of the server to connect to. On the server it specifies
  // * which IP address and port to lisen for connections.
  // ********************************************************************
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));

  // *** assign 3 fields in the servadd struct sin_family, sin_addr.s_addr and sin_port
  // *** the value your port can be any value > 1024.
  
  //zero the whole thing
  bzero(&servaddr, sizeof(servaddr));

  //IPv4 Protocol Family
  servaddr.sin_family = AF_INET;

  //Let the system pick the IP address
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  //You pick a random high-numbered port
  #define PORT 7341
  servaddr.sin_port = htons(PORT);


  //spit out debug for socket
  DEBUG << "Calling Socket() assigned file descriptor " << listenFd << ENDL;

  int bind (int sockfd, const struct sockaddr *my_addr, socklen_t addrlen);

  // ********************************************************************
  // * Binding configures the socket with the parameters we have
  // * specified in the servaddr structure.  This step is implicit in
  // * the connect() call, but must be explicitly listed for servers.
  // ********************************************************************
  bool bindSuccesful = false;

  while (!bindSuccesful) {



    // ** Call bind()
    // You may have to call bind multiple times if another process is already using the port
    // your program selects.

    if (bind (listenFd, (sockaddr *) &servaddr, sizeof(servaddr)) == 0) {
      DEBUG << "Calling bind" << "(" << listenFd << ", " << (sockaddr *) &servaddr << ", " << sizeof(servaddr) << ")" << ENDL;


      //call the port
      INFO << "Using port " << PORT << ENDL;
      //std::cout << "bind() successful: " << std::endl;
      bindSuccesful = true;
    }

    else {
        int rnd = 1000 + (rand() % 10000);

        servaddr.sin_port = htons(rnd);
    }
  }
  // *** DON'T FORGET TO PRINT OUT WHAT PORT YOUR SERVER PICKED SO YOU KNOW HOW TO CONNECT.


  // ********************************************************************
  // * Setting the socket to the listening state is the second step
  // * needed to being accepting connections.  This creates a queue for
  // * connections and starts the kernel listening for connections.
  // ********************************************************************
  int listenQueueLength = 1;
  // ** Cal listen()
  DEBUG << "Calling listen" << "(" << listenFd << ", " << listenQueueLength << ")" << ENDL;

  if (listen(listenFd, listenQueueLength) < 0) { 

    FATAL << "listen() failed: " << strerror(errno) << ENDL;
    exit(-1);
  }
  // ********************************************************************
  // * The accept call will sleep, waiting for a connection.  When 
  // * a connection request comes in the accept() call creates a NEW
  // * socket with a new fd that will be used for the communication.
  // ********************************************************************
  bool quitProgram = false;
  while (!quitProgram) {
    int connFd = -1;

    // Call the accept() call to check the listening queue for connection requests.
    // If a client has already tried to connect accept() will complete the
    // connection and return a file descriptor that you can read from and
    // write to. If there is no connection waiting accept() will block and
    // not return until there is a connection.
    // ** call accept() 

    DEBUG << "Calling accept(" << listenFd << "NULL, NULL" << ")" << ENDL;

      if ((connFd = accept(listenFd, (sockaddr *) NULL, NULL)) < 0) {
        FATAL << "accept() failed: " << strerror(errno) << ENDL;

        quitProgram = true;
      }
    

    // Now we have a connection, so you can call processConnection() to do the work.
    quitProgram = processConnection(connFd);
   
    close(connFd);
  }

  close(listenFd);

}
