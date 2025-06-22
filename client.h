#ifndef __CLIENT__
#define __CLIENT__

class Client{
    public:
        int recebe_arquivo(int sockfd, const char* file_name, unsigned char seq);
};

#endif