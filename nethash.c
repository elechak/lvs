
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define STORAGE 4
#define XNODESIZE 1000

struct Node{
    //int id;
    unsigned long order;
    unsigned long label_hash;
    struct Node * conn[STORAGE];
};

typedef struct Node * Node;

Node nodes = NULL;
Node xnodes = NULL;
unsigned long xnode_index = 0;
unsigned long limit = 64;
unsigned long num_nodes = 10;
char * infile;



inline void node_append(Node node_a, Node node_b){
    Node xnode = NULL;

    while(1){

        if (node_a->order < STORAGE){
            node_a->conn[ node_a->order ] = node_b;
            node_a->order++;
            break;
        }else if (node_a->order == STORAGE ){
            xnode = xnodes + xnode_index++;

            // need to make more xnodes for connection space
            if ( xnode_index == XNODESIZE){
                xnodes= calloc(XNODESIZE, sizeof(struct Node));
                xnode_index = 0;
            }

            // move last node into first position of xnode
            xnode->conn[xnode->order++] = node_a->conn[STORAGE-1];
            // append the new node to xnode
            xnode->conn[xnode->order++] = node_b;
            // set the last node to xnode
            node_a->conn[ STORAGE-1 ] = xnode;
            node_a->order++;
            break;
        }else{
            node_a->order++;
            node_a = node_a->conn[STORAGE-1];
        }
    }
}


inline void node_connect(unsigned long a, unsigned long b){
    node_append(nodes+a, nodes+b);
    if (a != b) node_append(nodes+b, nodes+a);
}

Node node_getconnected( Node n, unsigned long index){

    if (index >= n->order) return NULL;

    while(1){
        if (n->order > STORAGE){
            if (index < (STORAGE-1)){
                return n->conn[index];
            }else{
                n = n->conn[STORAGE-1];
                index -= (STORAGE-1);
            }
        }else{
            return n->conn[index];
        }
    }
}


int cmpn(const void * a, const void * b){
    return (*(unsigned long *)a - *(unsigned long *)b);
}


unsigned long node_hash( Node n,Node from, unsigned long strength,unsigned long jump){

    int c;
    Node node=NULL;
    unsigned long hash, value1 , value2;


    if (! n->order) return 0; // a lonely node returns 0


    jump++; // how many hops did it take to get here
    strength++; // how strong is the signal when it gets here

    value1 = value2 = n->label_hash;


    if (strength > limit) return n->label_hash ^ (n->order * jump + strength);


    c = 0;

    if (from){
        for(; c < n->order; ++c){
            node = node_getconnected(n,c);
            if (node == from){
                ++c;
                goto nofrom;
            }
            hash = node_hash(node, n, strength * n->order, jump);
            value1 ^= hash;
            value2 += hash;

        }
    }else{
        nofrom:
        for(; c < n->order; ++c){
            hash = node_hash(node_getconnected(n,c), n, strength * n->order, jump);
            value1 ^= hash;
            value2 += hash;
        }
    }

    value1 = n->label_hash ^ value2 ^ (value1<<8) ^ (strength<<4) ^ (jump<<2) ^ n->order;

    //rotate the hash
    value1 = ((value1<<3) | value1>>(sizeof(unsigned long)-3));

    return value1;
}



void options(int argc, char **argv){
    int count;

    for (count =1; count <argc; count ++){
        if (argv[count][0] == '-'){
            //starts with a '-' so this is a flag

            if (strcmp(argv[count]+1, "size")==0){
                count++;
                num_nodes = atol(argv[count]);

            }else if (strcmp(argv[count]+1, "depth")==0){
                count++;
                limit = atol(argv[count]);
            }

        }else{
            infile= argv[count];
        }
    }
}


void label_hash(Node n, char * string){
    unsigned long hash = 0;

    //fprintf(stderr, "LABEL = %s ",string);

    while( *string ){
        hash ^= *string++;
        hash = ((hash<<1) | hash>>(sizeof(unsigned long)-1));
    }

    n->label_hash = hash;
}


int main(int argc, char **argv){
    char line[1024];
    char *pline;
    unsigned long id, vid, c;
    int length;

    // collect and implement the options
    options(argc, argv);

    // initialize the node stacks.
    nodes = calloc(num_nodes, sizeof(struct Node));
    xnodes= calloc(XNODESIZE, sizeof(struct Node));

    FILE * fp = fopen(infile, "r");

    c=0;
    // Connect the nodes
    while( fgets(line,1024, fp) ) {
        //fprintf(stderr,"    LINE %lu\n", ++c);
        //fflush(stderr);

        pline = line;

        //skip this line if comment
        if (*pline == '#'){
            continue;

        }else if (*pline == 'L'){


            pline++;
            id = strtoul( pline,&pline,10);
            while(*pline == ' ') ++pline; // chew up  white space
            length = strcspn(pline, " \r\n"); // length of name
            label_hash( &(nodes[id]), pline);
            pline +=length;


        }else{

            //fprintf(stderr,"    C");
            //fflush(stderr);

            // Get a node (first one on the line) call it "id"
            id = strtoul( pline,&pline,10);
            if (*pline == '\n') continue;

            //fprintf(stderr," %lu", id);
            //fflush(stderr);


            // Now connect the other nodes on this line to "id"
            while(1){
                vid = strtoul( pline,&pline,10);

                //fprintf(stderr," (%lu) ", vid);
                //fflush(stderr);

                node_connect(id , vid);
                if (*pline == '\n') break;
            }

            //fprintf(stderr,"----C\n");
            //fflush(stderr);
        }
    }

    // Create Map file
    unsigned long *hash;
    char * buffer = malloc(strlen(infile)+ 10);

    *buffer = 0;
    strcat(buffer,infile);
    strcat(buffer, ".hash");

    int saved_mask = umask(0);
    int out = open(buffer, O_RDWR | O_CREAT | O_SYNC, 0660 );
    umask(saved_mask);

    lseek(out,num_nodes * sizeof(unsigned long)-1,SEEK_SET );
    write(out,"",1);
    lseek(out,0,SEEK_SET );

    hash = mmap(0,
        num_nodes * sizeof(unsigned long),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        out,
        0);


    // write the hash
    for (id =0 ; id < num_nodes; id ++){
        //fprintf(stderr,"ID= %lu\n", id);
        //fflush(stderr);
        hash[id] = node_hash(nodes+id,NULL, 0, 0);
    }

    munmap(hash, num_nodes * sizeof(unsigned long));
    close(out);

    return 0;
}


