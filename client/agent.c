#ifdef _WIN32
#include<WinSock2.h>
#include<WS2tcpip.h>
#else
#include <sys/socket.h>
#define closesocket close
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

Inbuf Inbuf_new(const uint8_t* p,size_t l){
    Inbuf buf = {
        p,
        l,
        0,
    };
    return buf;
}

typedef struct {
    const uint8_t* data;
    size_t len;
}Slice;

typedef struct {
    uint8_t* data;
    size_t len;
}SliceMut;

void abort_mem(){
    fputs("failed to allocate",stderr);
    exit(2);
}

void error(const char* err){
    fputs(err,stderr);
    exit(2);
}


void* alloc_or_error(size_t size) {
    void* ptr = malloc(size);
    if(!ptr){
        abort_mem();
    }
    return ptr;
}

SliceMut copy(const Slice* s){
    assert(s);
    SliceMut m;
    m.data = alloc_or_error(s->len+1);
    m.len=s->len;
    for(size_t i=0;i<s->len;i++){
        m.data[i]=s->data[i];
    }
    m.data[s->len]=0; // for printf
    return m;
}

void Outbuf_init(Outbuf* p){
    p->data=NULL;
    p->len=0;
    p->cap=0;
}

void Outbuf_clean(Outbuf*p){
    free(p->data);
    p->data=NULL;
    p->len=0;
    p->cap=0;
}





void Outbuf_append(Outbuf* p,const uint8_t* buf,size_t len) {
    assert(p&&buf);
    while(p->len+len>p->cap){
        if(p->cap==0){
            p->cap=(len >> 1) + 1;
        }
        p->data=realloc(p->data,p->cap*2);
        if(p->data==NULL){
            abort_mem();
        }
        p->cap*=2;
    }
    for(size_t i=0;i<len;i++) {
        p->data[p->len+i]=buf[i];
    }
    p->len+=len;
}

void Outbuf_append_str(Outbuf* p,const char* s) {
    Outbuf_append(p,(const uint8_t*)s,strlen(s));
}

uint8_t* bswap(uint8_t* buf,size_t len) {
    assert((len&0x1) == 0);
    for(size_t i=0;i<(len>>1);i++) {
        char tmp=buf[i];
        buf[i]=buf[len-1-i];
        buf[len-1-i]=tmp;
    }
    return buf;
}

int is_little(){
    int v=1;
    return ((const char*)(&v))[0];
}

#define NETORD(val) (is_little()?bswap((uint8_t*)&val,sizeof(val)):((uint8_t*)&val))  

#define DEFINE_INT_PUT(type) \
void Outbuf_put_##type(Outbuf* b,type##_t val) {\
    Outbuf_append(b,NETORD(val),sizeof(val));\
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

B Inbuf_eof_at(Inbuf* b,size_t i){
    return b->offset+i==b->len;
}

size_t Inbuf_remain(Inbuf* b){
    return b->len-b->offset;
}

#define Inbuf_at_ptr(b,i) ((b)->data+((b)->offset+i))
#define Inbuf_at(b,i) *Inbuf_at_ptr(b,i)
#define Inbuf_progress(b,i) ((b)->offset+=(i))

B Inbuf_expect(Inbuf*b,const char* s){
    size_t l=strlen(s);
    if(Inbuf_remain(b)<l){
        return 0;
    }
    for(size_t i=0;i<l;i++){
        if(Inbuf_at(b,i)!=s[i]){
            return 0;
        }
    }
    return 1;
}



void read_3byte(Inbuf* b,uint32_t* buf,uint32_t* read){
    int l=Inbuf_remain(b) <3?Inbuf_remain(b):3;
    for(int i=0;i<l;i++){
        *buf|= Inbuf_at(b,i) << (8*(3-i-1));
    }
    Inbuf_progress(b,l);
    *read=l;
}

B base64_encode(Inbuf* in, Outbuf* out,uint8_t c62, uint8_t c63 ,B no_padding) {
    size_t count=0;
    while (!Inbuf_eof(in)) {
        uint32_t num=0,red=0;
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
        count++;
    }
    return 1;
}

B base64_decode(Inbuf* seq, Outbuf* out, uint8_t c62 , uint8_t c63 ,B consume_padding,B should_eof) {
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
        uint8_t* buf=NETORD(rep);
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

uint32_t rol(uint32_t word,int shift){
    return ((word) << (shift)) | ((word) >> (sizeof(uint32_t) * 8 - (shift)));
}

void update_sha1(uint32_t* h, const uint8_t* bits) {
    #define ROL(word,shift) rol(word,shift)
    uint32_t w[80] = {0};
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        if (i < 16) {
            w[i]=0;
            for(int j=0;j<4;j++) {
                w[i] |= ((uint32_t)(bits[(i*4)+j]))<< (8*(3-j));
            }
        }
        else {
            w[i] = ROL(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        uint32_t f = 0, k = 0;
        if (i <= 19) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        }
        else if (i <= 39) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        }
        else if (i <= 59) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        }
        else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        unsigned int tmp = ROL(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = ROL(b, 30);
        b = a;
        a = tmp;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}


typedef struct {
    uint32_t hash[5];
    size_t total;
    uint8_t buf[64];
    size_t i;
}SHA1;

void SHA1_init(SHA1* h){
    h->hash[0]=0x67452301;
    h->hash[1]=0xEFCDAB89;
    h->hash[2]=0x98BADCFE;
    h->hash[3]=0x10325476;
    h->hash[4]=0xC3D2E1F0;
    h->total=0;
    memset(h->buf,0,64);
    h->i=0;
}

void SHA1_update(SHA1* s,Inbuf* seq) {
    while (!Inbuf_eof(seq)) {
        size_t initial_i=s->i;
        if(s->i==0){
            memset(s->buf,0,64);
        }
        for(;s->i<64;s->i++){
            if(Inbuf_eof(seq)){
                break;
            }
            s->buf[s->i]=Inbuf_at(seq,0);
            Inbuf_progress(seq,1);
        }
        s->total+=(s->i-initial_i)*8;
        if(s->i!=64){
            return; // suspend
        }
        update_sha1(s->hash, s->buf);
        s->i=0;
    }
}


void SHA1_finish(SHA1* s,Outbuf* out){
#define FLUSH_TOTAL()\
for(int i=0;i<8;i++){\
s->buf[56+i]=(uint8_t)((s->total>>(8*(7-i)))&0xff);\
}
    if(s->i==0){
        memset(s->buf,0,64);
        s->buf[0] = 0x80;
        FLUSH_TOTAL();
        update_sha1(s->hash, s->buf);
    }
    else {
        s->buf[s->i] = 0x80;
        if (s->i < 56) {
            FLUSH_TOTAL();
            update_sha1(s->hash, s->buf);
        }
        else {
            update_sha1(s->hash, s->buf);
            memset(s->buf,0,64);
            FLUSH_TOTAL();
            update_sha1(s->hash, s->buf);
        }
    }
    for (int i=0;i<5;i++ ) {
        Outbuf_put_uint32(out,s->hash[i]);
    }
}

typedef struct {
    SliceMut key;
    SliceMut value;
} Field;


void Field_init(Field* f,const Slice* key,const Slice* value){
    assert(f&&key&&value);
    f->key = copy(key);
    f->value = copy(value);
}

typedef struct {
    Field* fields;
    size_t len;
}HeaderFields;

void HeaderFields_init(HeaderFields* f){
    f->fields=NULL;
    f->len=0;
}

void HeaderFields_add(HeaderFields* f,const Slice* key,const Slice* value) {
    f->fields=realloc(f->fields,sizeof(Field)*(f->len+1));
    if(!f->fields){
        abort_mem();
    }
    Field_init(&f->fields[f->len],key,value);
    f->len+=1;
}

typedef struct {
    HeaderFields fields;
}Response;

int expect_eol(Inbuf* buf){
    return Inbuf_expect(buf,"\r\n")?2:0;
}

void parse_status_line(Inbuf* buf) {
    if(!Inbuf_expect(buf,"HTTP/1.1 101 Switching Protocols\r\n")){
        error("not 101");
    }
    Inbuf_progress(buf,34);
}

int parse_http_fields(Inbuf* buf,HeaderFields* f) {
    assert(f);
    for(;;){
        int eol=expect_eol(buf);
        if(eol){
            Inbuf_progress(buf,eol);
            return 1;
        }
        size_t i=0;
        Slice key,value;
        while(1){
            if(Inbuf_eof_at(buf,i)){
                return 0;
            }
            if(Inbuf_at(buf,i)==':'){
                key.data=Inbuf_at_ptr(buf,0);
                key.len= i;
                i++;
                break;
            }
            i++;
        }
        while(1){
            if(Inbuf_eof_at(buf,i)){
                return 0;
            }
            if(Inbuf_at(buf,i)!=' '){
                break;
            }
            i++;
        }
        int v_start=i;
        while(1){
            if(Inbuf_eof_at(buf,i)){
                return 0;
            }
            if(!Inbuf_eof_at(buf,i+1)&&
            Inbuf_at(buf,i)=='\r'&&
            Inbuf_at(buf,i+1)=='\n'){
                value.data=Inbuf_at_ptr(buf,v_start);
                value.len=i-v_start;
                i+=2;
                break;
            }
            i++;
        }
        HeaderFields_add(f,&key,&value);
        Inbuf_progress(buf,i);
    }
}

void read_socket(SOCKET fd,Inbuf* ibuf,uint8_t* buf,size_t buf_max) {
    ibuf->data=buf;
    memmove(buf,buf+ibuf->offset,ibuf->len - ibuf->offset);
    ibuf->len-=ibuf->offset;
    ibuf->offset=0;
    int res= recv(fd,(char*)buf+ibuf->len,buf_max-ibuf->len,0);
    if(res<=0){
        error("EOF");
    }
    ibuf->len+=res;
}

void read_http_response(SOCKET fd,HeaderFields* f,Inbuf* cmp){
    uint8_t buf[2000];
    Inbuf ibuf = Inbuf_new(buf,0);
    read_socket(fd,&ibuf,buf,2000);
    parse_status_line(&ibuf);
    for(;;) {
        int val=parse_http_fields(&ibuf,f);
        if(val<0){
            error("parse header");
        }
        if(val==1){
            B sec=0;
            for(size_t i=0;i<f->len;i++){
                if(strcmp((const char*)f->fields[i].key.data,"Sec-WebSocket-Accept")==0){
                    if(f->fields[i].value.len!=
                    cmp->len||  
                     memcmp(f->fields[i].value.data,cmp->data,cmp->len)!=0){
                        error("Sec-WebSocket-Accept");
                    }
                    sec=1;
                }
            }
            if(!sec){
                error("no Sec-Websocket-Accept");
            }
            break;
        }
    }
}

void gen_seckey(Outbuf* hdr,Outbuf* hash) {
    SHA1 s;
    const uint8_t* magic=(const uint8_t*)"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const uint8_t* psdrand= (const uint8_t*)"abcdefghijklmnop";
    Inbuf buf = Inbuf_new(psdrand, 16);
    Outbuf tmp;
    Outbuf_init(&tmp);
    if(!base64_encode(&buf,&tmp,'+','/',0)){
        error("base64");
    }
    Outbuf_append_str(hdr,"Sec-WebSocket-Key: ");
    Outbuf_append(hdr,tmp.data,tmp.len);
    Outbuf_append_str(hdr,"\r\n");

    // make sha1
    SHA1_init(&s);

    // append base64 encoded random
    buf = Inbuf_new(tmp.data,tmp.len);
    SHA1_update(&s,&buf);
    // append websocket magic guid
    buf = Inbuf_new(magic,strlen((const char*)magic));
    SHA1_update(&s,&buf);
    
    Outbuf_clean(&tmp);
    SHA1_finish(&s,&tmp);
    buf=Inbuf_new(tmp.data,tmp.len);
    if(!base64_encode(&buf,hash,'+','/',0)){
        error("base64");
    }
    Outbuf_clean(&tmp);
}

void write_websocket_handshake_request(SOCKET fd,Outbuf* hash){
    Outbuf buf;
    Outbuf_init(&buf);
    Outbuf_append_str(&buf,"GET /senda HTTP/1.1\r\n");
    Outbuf_append_str(&buf,"Host: localhost:8090\r\n");
    Outbuf_append_str(&buf,"Origin: http://localhost:8090\r\n");
    Outbuf_append_str(&buf,"Connection: Upgrade\r\n");
    Outbuf_append_str(&buf,"Upgrade: websocket\r\n");
    Outbuf_append_str(&buf,"Sec-Websocket-Version: 13\r\n");
    gen_seckey(&buf,hash);
    Outbuf_append_str(&buf,"\r\n");
    send(fd,(const char*)buf.data,buf.len,0);
    Outbuf_clean(&buf);
}


void read_ws_frame(SOCKET fd, Outbuf* ob){
    uint8_t buf[2000];
    Inbuf ibuf=Inbuf_new(buf,0);
  
}

int main() {
    WSADATA wsa;
    if(WSAStartup(MAKEWORD(2,2),&wsa)!=0){
        error("wsastartup");
    }
    struct addrinfo addr ={0},*res=NULL;
    addr.ai_family=AF_UNSPEC;
    addr.ai_socktype=SOCK_STREAM;
    if(getaddrinfo("localhost","8090",&addr,&res)!=0){
        error("getaddrinfo");
    }

    SOCKET fd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

    if(fd==-1){
        error("socket");
    }

    if(connect(fd,res->ai_addr,res->ai_addrlen)!=0){
        error("connect");
    }
    Outbuf buf;
    Outbuf_init(&buf);
    write_websocket_handshake_request(fd,&buf);
    HeaderFields hf;
    HeaderFields_init(&hf);
    Inbuf cmp = Inbuf_new(buf.data,buf.len);
    read_http_response(fd,&hf,&cmp);

    closesocket(fd);
}
