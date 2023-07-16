#ifdef _WIN32
#include<WinSock2.h>
#else
#include <sys/socket.h>
#endif
#include<stdint.h>
#include<stdlib.h>
#include<assert.h>
#include<stdio.h>


typedef struct {
    uint8_t* data;
    size_t cap;
    size_t len;
} Outbuf;

typedef struct {
    const uint8_t* data;
    size_t len;
    size_t offset;
} Inbuf;

void Outbuf_init(Outbuf* p){
    p->data=NULL;
    p->len=0;
    p->cap=0;
}

void Outbuf_append(Outbuf* p,const uint8_t* buf,size_t len) {
    assert(p&&buf);
    while(p->len+len>p->cap){
        p->data=realloc(p->data,p->len*2);
        if(p->data==NULL){
            fputs("failed to reallocate",stderr);
            exit(2);
        }
    }
    for(size_t i=0;i<len;i++) {
        p->data[p->len+i]=buf[i];
    }
    p->len+=len;
}

char* bswap(char* buf,size_t len) {
    assert(len&0x1 == 0);
    for(size_t i=0;i<(len>>1);i++) {
        char tmp=buf[i];
        buf[i]=buf[len-1-i];
        buf[len-1-i]=tmp;
    }
    return buf;
}

int is_little(){
    int v=1;
    return (const char*)(&v)[0];
}

#define NETORD(val) is_little()?bswap((char*)&val,sizeof(val)):((char*)&val)  

#define DEFINE_INT_PUT(type) \
void Outbuf_put_##type(Outbuf* b,type##_t val) {\
    Outpub_append(b,NETORD(val),sizeof(val));\
}

void Outbuf_put_uint8(Outbuf* b,uint8_t val){
    Outbuf_append(b,&val,1);
}

DEFINE_INT_PUT(uint16)
DEFINE_INT_PUT(uint32)
DEFINE_INT_PUT(uint64)

uint8_t get_char(uint8_t num, uint8_t c62, uint8_t c63 ) {
    if (num < 26) {
        return 'A' + num;
    }
    else if (num >= 26 && num < 52) {
        return 'a' + (num - 26);
    }
    else if (num >= 52 && num < 62) {
        return '0' + (num - 52);
    }
    else if (num == 62) {
        return c62;
    }
    else if (num == 63) {
        return c63;
    }
    return 0;
}

uint8_t get_num(uint8_t c, uint8_t c62 , uint8_t c63 ) {
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    else if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    else if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    else if (c == c62) {
        return 62;
    }
    else if (c == c63) {
        return 63;
    }
    return ~0;
}

// bool
typedef int B;

B Inbuf_eof(Inbuf* b){
    return b->offset==b->len;
}

#define Inbuf_at(b,i) (b)->data[(b)->offset+i]
#define Inbuf_progress(b,i) ((b)->offset+=(i))

void read_3byte(Inbuf* b,uint32_t* buf,uint32_t* read){
    int l=b->len<3?b->len:3;
    for(int i=0;i<l;i++){
        *buf <<= 8;
        *buf|= Inbuf_at(b,i);
    }
    Inbuf_progress(b,l);
    *read=l;
}

B encode(Inbuf* in, Outbuf* out,uint8_t c62, uint8_t c63 ,B no_padding) {
    size_t count=0;
    while (!Inbuf_eof(in)) {
        uint32_t num,red;
        read_3byte(in,&num,&red);
        if (!red) {
            break;
        }
        uint8_t buf[] = {
            (uint8_t)((num >> 18) & 0x3f), 
            (uint8_t)((num >> 12) & 0x3f), 
            (uint8_t)((num >> 6) & 0x3f), 
            (uint8_t)(num & 0x3f)};
        for (int i=0; i < red + 1; i++) {
            Outbuf_put_uint8(out,get_char(buf[i], c62, c63));
        }
        count+=red+1;
    }
    while (!no_padding && count % 4) {
        Outbuf_put_uint8(out,'=');
    }
    return 1;
}

B decode(Inbuf* seq, Outbuf* out, uint8_t c62 , uint8_t c63 ,B consume_padding,B should_eof) {
    B end = 0;
    while (!Inbuf_eof(seq) && !end) {
        size_t redsize = 0;
        uint32_t rep = 0;
        while (redsize < 4 && !Inbuf_eof(seq)) {
            uint8_t n = get_num(Inbuf_at(seq,0), c62, c63);
            if (n == 0xff) {
                end = 1;
                break;
            }
            rep |= ((n << (6 * (3 - redsize))));
            redsize++;
            Inbuf_progress(seq,1);
        }
        char* buf=NETORD(rep);
        Outbuf_append(out,buf+1,redsize-1);
    }
    while (consume_padding &&!Inbuf_eof(seq) && Inbuf_at(seq,0)  == '=') {
        Inbuf_progress(seq,1);
    }
    if(should_eof){
        return Inbuf_eof(seq);
    }
    return 1;
}


int write_http_request(){

}