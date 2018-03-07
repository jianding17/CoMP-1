#include "packageSender.hpp"


PackageSender::PackageSender(int in_thread_num):
frame_id(0), subframe_id(0), thread_num(in_thread_num)
{
    socket_ = new int[thread_num];

    /*Configure settings in address struct*/
    servaddr_.sin_family = AF_INET;
    servaddr_.sin_port = htons(7891);
    servaddr_.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(servaddr_.sin_zero, 0, sizeof(servaddr_.sin_zero));  

    for(int i = 0; i < thread_num; i++)
    {
        int rand_port = rand() % 65536;
        cliaddr_.sin_family = AF_INET;
        cliaddr_.sin_port = htons(rand_port);  // out going port is random
        cliaddr_.sin_addr.s_addr = inet_addr("127.0.0.1");
        memset(cliaddr_.sin_zero, 0, sizeof(cliaddr_.sin_zero));  

        if ((socket_[i] = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { // UDP socket
            printf("cannot create socket\n");
            exit(0);
        }

        /*Bind socket with address struct*/
        if(bind(socket_[i], (struct sockaddr *) &cliaddr_, sizeof(cliaddr_)) != 0)
            perror("socket bind failed");
    }


    buffer_.resize(BS_ANT_NUM);
    for(int i = 0; i < buffer_.size(); i++)
        buffer_[i].resize(buffer_length);

    /* initialize random seed: */
    srand (time(NULL));


    IQ_data = new float*[subframe_num_perframe * BS_ANT_NUM];
    for(int i = 0; i < subframe_num_perframe * BS_ANT_NUM; i++)
        IQ_data[i] = new float[OFDM_FRAME_LEN * 2];
    
    // read from file
    FILE* fp = fopen("data.bin","rb");
    for(int i = 0; i < subframe_num_perframe * BS_ANT_NUM; i++)
        fread(IQ_data[i], sizeof(float), OFDM_FRAME_LEN * 2, fp);
    fclose(fp);
}

PackageSender::~PackageSender()
{
    for(int i = 0; i < subframe_num_perframe * BS_ANT_NUM; i++)
        delete[] IQ_data[i];
    delete[] IQ_data;

    delete[] socket_;
}

void PackageSender::genData()
{
    int cell_id = 0;
    
    for (int j = 0; j < buffer_.size(); ++j) // per antenna
    {
        memcpy(buffer_[j].data(), (char *)&frame_id, sizeof(int));
        memcpy(buffer_[j].data() + sizeof(int), (char *)&subframe_id, sizeof(int));
        memcpy(buffer_[j].data() + sizeof(int) * 2, (char *)&cell_id, sizeof(int));
        memcpy(buffer_[j].data() + sizeof(int) * 3, (char *)&j, sizeof(int));
        //printf("copy IQ\n");
        // waste some time
        for(int p = 0; p < 0; p++)
            rand();

        int data_index = subframe_id * BS_ANT_NUM + j;
        memcpy(buffer_[j].data() + data_offset, (char *)IQ_data[data_index], sizeof(float) * OFDM_FRAME_LEN * 2);   
    }

    subframe_id++;
    if(subframe_id == subframe_num_perframe)
    {
        subframe_id = 0;
        frame_id ++;
        if(frame_id == MAX_FRAME_ID)
            frame_id = 0;
    }
    
}

void PackageSender::loopSend()
{
    auto begin = std::chrono::system_clock::now();
    const int info_interval = 2e1;
    std::vector<int> ant_seq = std::vector<int>(buffer_.size());
    for (int i = 0; i < ant_seq.size(); ++i)
        ant_seq[i] = i;
    //std::iota(ant_seq.begin(), ant_seq.end(), 0);

    int used_socker_id = 0;
    while(true)
    {
        this->genData(); // generate data
        //printf("genData\n");
        //std::random_shuffle ( ant_seq.begin(), ant_seq.end() ); // random perm
        for (int i = 0; i < buffer_.size(); ++i)
        {
            used_socker_id = i % thread_num;
            /* send a message to the server */
            if (sendto(this->socket_[used_socker_id], this->buffer_[ant_seq[i]].data(), this->buffer_length, 0, (struct sockaddr *)&servaddr_, sizeof(servaddr_)) < 0) {
                perror("socket sendto failed");
                exit(0);
            }
        }  
        //printf("send frame %d, subframe %d\n", frame_id, subframe_id);
        if ((frame_id+1) % info_interval == 0 && subframe_id == 0)
        {
            auto end = std::chrono::system_clock::now();
            double byte_len = sizeof(float) * OFDM_FRAME_LEN * 2 * BS_ANT_NUM * subframe_num_perframe * info_interval;
            std::chrono::duration<double> diff = end - begin;
            printf("transmit %f bytes in %f secs, throughput %f MB/s\n", byte_len, diff.count(), byte_len / diff.count() / 1024 / 1024);
            begin = std::chrono::system_clock::now();
        }
    }
    
}
