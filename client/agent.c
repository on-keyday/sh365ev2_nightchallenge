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

typedef struct {
    const uint8_t* data;
    size_t len;
}Slice;

typedef struct {
    uint8_t* data;
    size_t len;
}SliceMut;

SliceMut copy(const Slice* s){
    assert(s);
    SliceMut m;
    m.data = alloc_or_error(s->len);
    m.len=s->len;
    for(size_t i=0;i<s->len;i++){
        m.data[i]=s->data[i];
    }
    return m;
}

void Outbuf_init(Outbuf* p){
    p->data=NULL;
    p->len=0;
    p->cap=0;
}

void abort_mem(){
    fputs("failed to allocate",stderr);
    exit(2);
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

B Inbuf_eof_at(Inbuf* b,size_t i){
    return b->offset+i==b->len;
}

size_t Inbuf_remain(Inbuf* b){
    return b->len-b->offset;
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

B base64_encode(Inbuf* in, Outbuf* out,uint8_t c62, uint8_t c63 ,B no_padding) {
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

void update_sha1(uint32_t* h, const uint8_t* bits) {
    #define ROL(word,shift) (word << shift) | (word >> (sizeof(word) * 8 - shift));
    uint32_t w[80] = {0};
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        if (i < 16) {
            w[i]=0;
            for(int j=0;j<4;j++) {
                w[i] <<=8;
                w[i] |= bits[(i*4)+j];
            }
        }
        else {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
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
        unsigned int tmp = rol(a, 5) + f + e + k + w[i];
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
#define FLUSH_TOTAL() 
for(int i=0;i<8;i++){\
s->buf[56+i]=(uint8_t)((s->total>>(8*(7-i)))&0xff);\
}
    if(s->i==0){
        memset(s->buf,0,64);
        s->buf[0] = 0x80;
        FLUSH_TOTAL();
        updatec_sha1(s->hash, s->buf);
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
    return 1;
}

typedef struct {
    SliceMut key;
    SliceMut value;
} Field;

void* alloc_or_error(size_t size) {
    void* ptr = malloc(size);
    if(!ptr){
        abort_mem();
    }
    return ptr;
}

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
    uint16_t status;
    HeaderFields fields;
}Response;

void parse_http_field(Inbuf* buf,HeaderFields* f) {
    assert(f);
    size_t i=0;
    for(;;){
        Slice key,value;
        while(!Inbuf_eof())
    }
}

int write_http_request(){

}