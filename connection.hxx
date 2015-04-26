
#define COMPILE_TIME_IF()

namespace net 
{
  template<protocol p>
  connection<type::HOST, p>::connection(unsigned port, const callback& c)
  : abstract::connection()
  {
    if(p == protocol::UDP)
      socket_fd = socket(AF_INET,
                         SOCK_DGRAM,
                         IPPROTO_UDP);
    else if(p == protocol::TCP)
      socket_fd = socket(AF_INET,
                         SOCK_STREAM,
                         0);

    if(socket_fd < 0) throw std::logic_error("Socket creation failed");

    bzero((char*) &server_socket, sizeof(server_socket));

    server_socket.sin_family = AF_INET;
    server_socket.sin_addr.s_addr = htonl(INADDR_ANY);
    server_socket.sin_port = htons(port);

    if(bind(socket_fd,
            (struct sockaddr*)&server_socket,
            sizeof(server_socket)))
      throw std::logic_error("Socket bind failed");

    if(p == protocol::TCP)
    {
      listen(socket_fd, 30); // 30 connection in parallel

      std::thread listening_thread ([&] 
      {
        unsigned addr_len;
        while(listening)
        {
          int newfd = accept(socket_fd,
                           (sockaddr_t*) &server_socket,
                           &addr_len);

          std::thread discuss_thread ([&] 
                  {
                  char buf[1024];
                  while(read(newfd, buf, 1024))
                    c(newfd, buf, *this);
                  });
          discuss_thread.detach();
        }
      });

      listening_thread.detach();
    }
    else if(p == protocol::UDP)
    {
      std::thread listening_thread([&] 
                                   {
                                   char buf[1024] = {0};

                                   while(listening)
                                   {
                                   inet_sockaddr_t from;
                                   socklen_t len = sizeof(inet_sockaddr_t);

                                   if (recvfrom(socket_fd,
                                                buf,
                                                1024,
                                                0,
                                                (sockaddr_t*) &from,
                                                &len) == -1)
                                   continue; // or break?
                                   // ACK
                                   if (sendto(socket_fd, buf, 1, 0, (struct sockaddr*) &from, len) == -1)
                                   continue;

                                   //handle data
                                   std::string tmp = buf+2;
                                   c(buf[2], tmp, *this);
                                   }
                                   });
      listening_thread.detach();
    }
  }

  //template<>
  connection<type::HOST, protocol::PIPE>::connection(unsigned port, const callback& c, std::string s)
  : abstract::connection()
  {
    socket_fd = socket(AF_UNIX,
                       SOCK_STREAM,
                       0);

    if(socket_fd < 0) throw std::logic_error("Socket creation failed");

    bzero((char*) &server_socket, sizeof(server_socket));

    server_socket.sun_family = AF_UNIX;
    if(s.empty())
      s = "servXXXXXX";
    else
      s += "XXXXXX";
    file_path = new char[s.size()];
    mkstemp(file_path);

    strcpy(server_socket.sun_path, file_path);

    if(bind(socket_fd,
            (struct sockaddr*)&server_socket,
            sizeof(server_socket)))
      throw std::logic_error("Socket bind failed");

    listen(socket_fd, 30); // 30 connection in parallel

    std::thread listening_thread ([&] 
                                  {
                                  unsigned addr_len;
                                  while(listening)
                                  {
                                  int newfd = accept(socket_fd,
                                                     (sockaddr_t*) &server_socket,
                                                     &addr_len);

                                  std::thread discuss_thread ([&] 
                                                              {
                                                              char buf[1024];
                                                              while(read(newfd, buf, 1024))
                                                              c(newfd, buf, *this);
                                                              });
                                  discuss_thread.detach();
                                  }
                                  });
    listening_thread.detach();
  }

  template<protocol p>
  connection<type::CLIENT,p>::connection(const std::string& addr, unsigned port, const callback& c)
  : abstract::connection()
  {
    if(p == protocol::UDP)
      socket_fd = socket(AF_INET,
                         SOCK_DGRAM,
                         0);
    else if(p == protocol::TCP)
      socket_fd = socket(AF_INET,
                         SOCK_STREAM,
                         0);


    if(socket_fd < 0) throw std::logic_error("Socket creation failed");

    bzero((char *)&server,sizeof(server));

    server.sin_family = AF_INET;
    hp = gethostbyname(addr.c_str());
    bcopy((char*)hp->h_addr,
          (char*)&server.sin_addr,
          hp->h_length);
    server.sin_port = htons(port);

    if(p == protocol::TCP||p == protocol::PIPE)
    {
      if (connect(socket_fd,(struct sockaddr *) &server,sizeof(server)) < 0) 
        throw std::logic_error("Connect failed");
    }
  }
  
  //template<>
  connection<type::CLIENT, protocol::PIPE>::connection(const std::string& addr, unsigned port, const callback& c)
  : abstract::connection()
  {
    socket_fd = socket(AF_UNIX,
                       SOCK_STREAM,
                       0);


    if(socket_fd < 0) throw std::logic_error("Socket creation failed");

    bzero((char *)&server,sizeof(server));

    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, addr.c_str());

    if (connect(socket_fd,(struct sockaddr *) &server,sizeof(server)) < 0) 
      throw std::logic_error("Connect failed");
  }

  template<protocol p>
  void connection<type::CLIENT,p>::send(const std::string& s)
  {
    char buffer[256];
    unsigned length=sizeof(struct sockaddr_in);
    if(p == protocol::UDP)
    {
      sendto(socket_fd,
             s.c_str(),
             s.length(),
             0,
             (const struct sockaddr*)&server,
             sizeof(struct sockaddr));

      inet_sockaddr_t from;
      recvfrom(socket_fd,buffer,256,0,(struct sockaddr *)&from, &length);
    }
    else if(p == protocol::TCP)
    {
      if (write(socket_fd,s.c_str(),s.size()) < 0)
        throw std::logic_error("Write to socket failed"); 
    }
  }


} //!net