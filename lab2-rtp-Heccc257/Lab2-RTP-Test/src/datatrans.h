
bool sendRTP(int src, rtp_packet_t *pack, sockaddr_in dest);
rtp_packet_t* revRTP(int my, sockaddr_in *fr, char *rcvbuf);