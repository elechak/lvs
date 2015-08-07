
/************************************************************************************
Copyright 2006 Erik Lechak

Extract - a program to translate an lef file into a net list
************************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>

/************************************************************************************
                                                LISTS
************************************************************************************/
typedef struct List{
    void ** data;
    unsigned int size;
    unsigned int length;
} * List;

#define list_iter(LIST,ITER,VAR)   if (LIST && LIST->length) for(ITER=0 ;\
VAR=LIST->data[ITER], ITER<LIST->length ; ITER++)

#define list_clear(LIST) LIST->length = 0;

inline unsigned int list_alloc(List list, unsigned int size){
    list->data = realloc(list->data,sizeof(void *) * size);
    if (! list->data){
        free(list->data);
        return 0;
    }
    list->size = size;
    return size;
}

List list_new(unsigned int size){
    List list = malloc(sizeof(struct List));
    if (! list ) return NULL;
    list->data = NULL;

    if (! list_alloc(list, size) ){
        free(list);
        return NULL;
    }
    list->length = 0;
    return list;
}

inline void list_add(List list, void * p){
    if(list->length == list->size-1){
        list_alloc(list,2*list->size);
    }
    list->data[list->length] = p;
    list->length++;
}

inline void list_add_unique(List list, void * p){
    unsigned int c;
    void * data = NULL;

    list_iter(list, c , data){
        if (data == p) return;
    }

    list_add(list,p);
}


void list_del(List list, void * p){
    register unsigned int c;
    unsigned int length = list->length-1;
    void ** data = list->data;

    for( c=0 ; c < length ; ++c){
        if (*data++ == p){
            list->data[c] = list->data[length];
            list->length--;
            break;
        }
    }

    if (*data == p){
            list->length--;
    }
}

void * list_pop(List list){
    if (list->length){
        list->length--;
        return list->data[list->length];
    }
    return NULL;
}

/***********************************************************************************
            MMAP
************************************************************************************/
typedef struct MMap{
    int fd;
    unsigned long size;
    void * mem;
} MMap;

void close_map(MMap * map){
    munmap(map->mem, map->size);
    close(map->fd);
    map->mem = NULL;
}

MMap read_char_map(char * fname){
    MMap m;
    struct stat stat_info;
    int fd;
    void * fd_mem;

    fd = open(fname, O_RDONLY, 0444 );

    if ( fstat(fd, &stat_info)){
        printf("stat error\n");
        exit(0);
    }

    fd_mem = mmap(0, stat_info.st_size+1, PROT_READ, MAP_PRIVATE, fd, 0);

    m.fd = fd;
    m.size = stat_info.st_size+1;
    m.mem = fd_mem;

    return m;
}

MMap write_map(char * fname, int size, int elementsize){
    MMap m;
    void * fd_mem;
    int saved_mask = umask(0);
    int out = open(fname, O_RDWR | O_CREAT | O_SYNC | O_TRUNC, 0660 );
    umask(saved_mask);

    lseek(out,size * elementsize -1,SEEK_SET );
    write(out,"",1);
    lseek(out,0,SEEK_SET );

    fd_mem = mmap(0,
        size * elementsize,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        out,
        0);

    m.fd = out;
    m.size = size * elementsize;
    m.mem = fd_mem;

    return m;
}

/************************************************************************************
                Program
************************************************************************************/

// Structures

struct Layer;

typedef struct Name{
    char *name;
    struct Layer * layer;
    int xtrans;
    int ytrans;
} * Name;


typedef struct Node{
    int id;
    int type;
    Name name;
    List devices;
    List gates;
} * Node;

typedef struct Layer{
    char name[64];
    unsigned long mask;
    List sjunc;
    List ejunc;
    Node * prev_line;
    Node * current_line;
} * Layer;


// Global variables
int width, height,scale, xoffset, yoffset, h_g, w_g;
Layer layer_a, layer_b;

Layer fets[3][4]; // [transname, poly-layer,diffusion] [ id ]
int num_fets=0;   // # of fet definitions

List nodes       = NULL;
List unused_nodes= NULL;
List layers      = NULL;
List s_junctions = NULL;
List e_junctions = NULL;
List names       = NULL;


// Layers
Layer layer_get(char * name){
    unsigned int c, size;
    Layer layer = NULL;

    size = strlen(name);
    list_iter(layers, c, layer){
        if (memcmp(layer->name, name,size) == 0 ) return layer;
    }

    // Didn't find it so we have to create a new one and return it
    layer = malloc(sizeof(struct Layer));
    strcpy(layer->name, name);

    layer->prev_line = NULL;
    layer->current_line = NULL;
    layer->sjunc = list_new(4);
    layer->ejunc = list_new(4);

    layer->mask = 0x1<<layers->length;
    list_add_unique(layers, layer);

    return layer;
}

void layers_init(){
    Layer layer = NULL;
    int c;
    list_iter(layers, c, layer){
        layer->prev_line = calloc(sizeof(Node) * width, 1);
        layer->current_line = calloc(sizeof(Node) * width, 1);
    }
}

// Rules functions
void rules_get(){
    char *rules, *start, token[32];
    unsigned int length, section;
    Layer layer1, layer2;

    MMap rulefile = read_char_map("extract.rules");
    rules = rulefile.mem;

    section=0;
    while(*rules){
        rules += strspn(rules, " \r\n");
        if (*rules == '*'){
            start = ++rules;
            length =  strcspn(rules, " \r\n");

            if ( memcmp(start, "FET", length)==0 ){
                section = 1;
            }else if ( memcmp(start, "S-JUNCTION", length)==0 ){
                section = 2;
            }else if ( memcmp(start, "E-JUNCTION", length)==0 ){
                section = 3;
            }

            rules+=length;
        }else if (*rules == '#') {
            rules +=  strcspn(rules, "\r\n");

        }else{

            layer1= layer2 = NULL;
            while(*rules){

                // GET THE TOKEN
                length =  strcspn(rules, " \r\n"); // get length of string
                // copy the string into token
                memcpy(token, rules, length);
                *(token+length)=0;
                rules+=length; // set rules ahead

                // FET Section
                if (section ==1){

                    if ( ! layer1){ // This is the name of the FET
                        layer1 = layer_get(token);
                        fets[0][num_fets] = layer1;
                    }else if (! layer2){ // This is the Polysilicone layer
                        layer2 = layer_get(token);
                        fets[1][num_fets] = layer2;
                    }else{               // This is the Diffusion
                        fets[2][num_fets] = layer_get(token);
                        num_fets++;
                    }

                // S-JUNCTION Section
                }else if (section ==2){

                    if ( ! layer1){ // Key layer
                        layer1 = layer_get(token);
                        list_add(s_junctions, layer1);
                    }else{ // Junction layer
                        layer2 = layer_get(token);
                    }

                    // add the junction
                    if (layer1 && layer2){
                        list_add_unique(layer1->sjunc, layer2);
                    }
                // E-JUNCTION Section
                }else if (section ==3){

                    if ( ! layer1){ // Key layer
                        layer1 = layer_get(token);
                        list_add(e_junctions, layer1);
                    }else{ // Junction layer
                        layer2 = layer_get(token);
                    }

                    // add the junction
                    if (layer1 && layer2){
                        list_add_unique(layer1->ejunc, layer2);
                    }
                }

                // Just some parsing stuff
                if (*rules == '\n' || *rules=='\r') break;
                rules += strspn(rules, " ");  //skip white space
            }
        }
    }

    close_map(&rulefile);
}

// Nodes
void nodes_print(){
    int c, a;
    Node node, dev;
    list_iter(nodes, c, node){
        node->id = c;
    }

    list_iter(nodes, c, node){
        if (! node->type){
            // OLD OUTPUT
            //~ if (node->name){
                //~ printf("NODE-%i{%s}     ", node->id,node->name->name);
            //~ }else{
                //~ printf("NODE-%i{}     ", node->id);
            //~ }

            //~ list_iter(node->devices, a, dev){
                //~ printf("%s-%i  ", (fets[0][(dev->type-1)])->name,dev->id);
            //~ }

            //~ list_iter(node->gates, a, dev){
                //~ printf("%s-%ig  ", (fets[0][(dev->type-1)])->name,dev->id);
            //~ }
            //~ printf("\n");

            if (node->name){
                printf("%i:'%s'    ", node->id,node->name->name);
            }else{
                printf("%i:''    ", node->id);
            }

            list_iter(node->devices, a, dev){
                printf("%i:'%s'    ", dev->id,(fets[0][(dev->type-1)])->name);
            }

            list_iter(node->gates, a, dev){
                printf("%ig:'%s'    ",dev->id, (fets[0][(dev->type-1)])->name);
            }
            printf("\n");




        }
    }
}

Node node_new(int type){
    Node node = NULL;

    node = list_pop(unused_nodes);

    if( ! (node = list_pop(unused_nodes))   ){
        node =  malloc(sizeof(struct Node));
        node->devices = list_new(3);
        node->gates = list_new(3);
        node->name = NULL;
    }

    if (! node ) {
        printf("Could not create node\n");
        exit(0);
    }
    node->type = type;
    list_add(nodes, node);

    return node;
}

inline void node_del(Node n){
    list_clear(n->devices);
    list_clear(n->gates);
    list_del(nodes, n);
    n->name = NULL;
    list_add_unique(unused_nodes, n);
}


inline void node_addgate(Node a , Node b){
    if (a == b) return;

    // Cant merge a device with a device
    if (a->type && b->type){
        printf("Device merge error\n");
        exit(0);
    // A is the device
    }else if (a->type){
        list_add_unique(b->gates, a);

    // B is the device
    }else if (b->type){
        list_add_unique(a->gates, b);
    }
}


inline void node_merge(Node a , Node b){

    Node * line  = NULL;
    Layer layer = NULL;
    Node dev = NULL;
    int c, x;

    if (a == b) return;


    //printf("l X:%i  Y:%i\n", w_g,h_g);

    // Cant merge a device with a device
    if (a->type && b->type){
        printf("Device merge error\n");
        exit(0);
    // A is the device
    }else if (a->type){
        list_add_unique(b->devices, a);

    // B is the device
    }else if (b->type){
        list_add_unique(a->devices, b);

    // No devices so actually merge the nodes
    }else{
        // Move devices from b into a
        list_iter(b->devices, c, dev){
            list_add_unique(a->devices, dev);
        }

        // Move gates from b into a
        list_iter(b->gates, c, dev){
            list_add_unique(a->gates, dev);
        }

        // Merge node names
        // only one node should have a name or there is a design problem
        if (b->name && a->name){
            fprintf(stderr,"Merge Error: \n");

            if (layer_b){
                fprintf(stderr,"    Merging Node %s(%s) with %s(%s) @ %f , %f\n",
                    a->name->name,
                    layer_a->name,
                    b->name->name,
                    layer_b->name,
                    ((double)w_g+xoffset)/scale,
                    ((double)h_g+yoffset)/scale);
            }else{
                fprintf(stderr,"    Merging Node %s(%s) with %s(%s) @ %f , %f\n",
                    a->name->name,
                    layer_a->name,
                    b->name->name,
                    layer_a->name,
                    ((double)w_g+xoffset)/scale,
                    ((double)h_g+yoffset)/scale);
            }

        }else if (b->name){
            a->name = b->name;
        }

        // Exchange all occurances of b with a in the layers current_line
        list_iter(layers, c , layer){
            line = layer->current_line;
            for (x=0; x < width; ++x){
                if (line[x] == b) line[x] = a;
            }
        }

        node_del(b);
    }
}


void get_names(char * lef){

    char *plef, layer[64], tl_layer[64];
    int length, start;
    double x1,y1,x2,y2,xtl,ytl, tl_x, tl_y;

    Name name = NULL;
    start=1;
    tl_x = tl_y = 0.0;

    plef = lef;
    while(1){

        plef = strpbrk(plef,"PE");
        if (! plef) break;
        length = strcspn(plef, " \r\n;");

        if ((length == 3) && (memcmp("END", plef, 3) == 0)){
            plef += length; // put plef at end of the word END

            if ( ! name ) continue;

            while(*plef == ' ')plef++; // chew up  white space
            length = strcspn(plef, " \r\n;"); // length of name

            if ( name && memcmp(name->name, plef, length) ==0) {
                name = NULL;
            }

            plef += length;
        }else if ((length == 3) && (memcmp("PIN", plef, 3) == 0)){

            plef += length; // put plef at end of the word PIN
            while(*plef == ' ')plef++; // chew up  white space
            length = strcspn(plef, " \r\n;"); // length of name

            name = malloc(sizeof(struct Name));
            name->name = malloc(length+1);
            memcpy(name->name,plef, length);
            name->name[length] = '\0';
            list_add(names, name);
            start=1;
            plef += length;

            // Get the position and layer of the top left corner of this PIN
            while(1){

                plef = strpbrk(plef,"LRE");
                if (! plef) break;
                length = strcspn(plef, " \r\n;");

                if ((length == 5) && (memcmp("LAYER", plef, 5) == 0)){
                    plef += length; // put plef at end of the word RECT
                    while(*plef == ' ')plef++; // chew up  white space
                    length = strcspn(plef, " \r\n;"); // length of name
                    memcpy(layer,plef, length);
                    layer[length] = 0;
                    plef += length;

                }else if ((length == 4) && (memcmp("RECT", plef, 4) == 0)){
                    plef += length; // put plef at end of the word RECT
                    while(*plef == ' ')plef++; // chew up  white space

                    // Get the two points that determine the rectangle
                    x1 = strtod(plef,&plef);
                    y1 = strtod(plef,&plef);
                    x2 = strtod(plef,&plef);
                    y2 = strtod(plef,&plef);

                    // Get the top-left corner rectangle
                    if ( y1 < y2){
                        xtl = x1;
                        ytl = y1;
                    }else if ( y1 > y2){
                        xtl = x2;
                        ytl = y2;
                    }else{
                        xtl = x2;
                        ytl = y2;
                        if (x1 < x2){
                            xtl = x1;
                            ytl = y1;
                        }
                    }

                    // Remember the pinwise top-left corner
                    if (start){
                        tl_x = xtl;
                        tl_y = ytl;
                        strcpy(tl_layer, layer);
                        start = 0;
                    }else {
                        if (ytl < tl_y){
                            tl_y = ytl;
                            tl_x = xtl;
                            strcpy(tl_layer, layer);
                        }else if (ytl == tl_y){
                            if (xtl < tl_x){
                                tl_x = xtl;
                                strcpy(tl_layer, layer);
                            }
                        }
                    }
                }else if ((length == 3) && (memcmp("END", plef, 3) == 0)){
                    plef += length; // put plef at end of the word END
                    while(*plef == ' ')plef++; // chew up  white space
                    break;
                }else{
                    plef += length;
                }

            }

            name->layer = layer_get(tl_layer);
            name->xtrans = (int)(tl_x * scale - xoffset);
            name->ytrans = (int)(tl_y * scale - yoffset);

            //printf("%s   %s  %f (%i)  %f (%i)\n",name->name, tl_layer, tl_x,name->xtrans, tl_y, name->ytrans);

        }else{
            plef += length;
        }
    }
}


// Bitmap functions
inline void draw_rect(unsigned long * bmp, int x1, int y1, int x2, int y2, Layer layer){

    //printf("draw_rect %i %i %i %i\n", x1, y1, x2 ,y2);
    unsigned long * pbmp = NULL;
    unsigned long mask = layer->mask;

    int ystart, yend, xstart,span, y,x;


    if (y1 <= y2){
        ystart = y1;
        yend = y2;
    }else{
        ystart = y2;
        yend = y1;
    }

    if (x1 <= x2){
        xstart = x1;
        span = x2 - x1;
    }else{
        xstart = x2;
        span = x1 - x2;
    }

    for (y = ystart; y <= yend ; ++y){
        // point pbmp at the leftmost pixel at this y
        pbmp = bmp + ((width * y) + xstart);
        for( x = 0 ; x <= span ; ++x ){
            //printf("    >%s %i   %i\n",layer->name, y, x);
            *pbmp++ |=  mask ;
        }
    }
}


void draw(char * lef, unsigned long * bmp){

    char *plef, layer[64];
    int length;
    plef = lef;
    double x1,y1,x2,y2;

    while(1){
        plef = strpbrk(plef,"LR");

        if (! plef) break;

        length = strcspn(plef, " \r\n;");

        if ((length == 4) && (strncmp("RECT", plef, 4) == 0)){

            plef += length; // put plef at end of the word layer
            while(*plef == ' ')plef++; // chew up  white space

            x1 = strtod(plef,&plef);// * scale - xoffset;
            y1 = strtod(plef,&plef);// * scale - yoffset;
            x2 = strtod(plef,&plef);// * scale - xoffset;
            y2 = strtod(plef,&plef);// * scale - yoffset;

            draw_rect(bmp,
                (int)(x1 * scale - xoffset),
                (int)(y1 * scale - yoffset),
                (int)(x2 * scale - xoffset),
                (int)(y2 * scale - yoffset),
                layer_get(layer));


        }else if ((length == 5) && (strncmp("LAYER", plef, 5) == 0)){
            plef += length; // put plef at end of the word layer
            while(*plef == ' ')plef++; // chew up  white space
            length = strcspn(plef, " \r\n;"); // length of layer name
            memcpy(layer,plef, length);
            layer[length] = '\0';
        }else{
            plef += length;
        }
    }
}

void modifyBMP(unsigned long * bmp){
    unsigned int c, size, n;
    unsigned long *pbmp, mask, fet, diff;

    size = width * height;

    for (n =0 ; n < num_fets; ++n){
        fet = fets[0][n]->mask;
        diff = fets[2][n]->mask;
        mask = fets[1][n]->mask | diff;

        pbmp = bmp;
        for  (c=0; c < size; ++c){
            if ((*pbmp & mask) == mask){
                *pbmp = (*pbmp | fet) &~ diff;
            }
            pbmp++;
        }
    }
}

// Create the bitmap from the lef file
MMap createBMP(char * lef){

    char *plef = lef;
    int length;
    float x1,y1,x2,y2;
    MMap bmp;

    float xsize = 0.0;
    float ysize = 0.0;
    float xmax = 0.0;
    float ymax = 0.0;
    float xmin = 0.0;
    float ymin = 0.0;
    float xsizemax = 0.0;
    float xsizemin = 50000.0;
    float ysizemax = 0.0;
    float ysizemin = 50000.0;

    while(1){
        plef = strpbrk(plef,"LR");

        if (! plef) break;

        length = strcspn(plef, " \r\n;");

        if ((length == 4) && (strncmp("RECT", plef, 4) == 0)){
            plef += length; // put plef at end of the word layer
            while(*plef == ' ')plef++; // chew up  white space
            x1 = strtod(plef,&plef) * scale;
            y1 = strtod(plef,&plef) * scale;
            x2 = strtod(plef,&plef) * scale;
            y2 = strtod(plef,&plef) * scale;

            xsize = x2 - x1;
            ysize = y2 - y1;

            if (x1 > xmax) xmax = x1;
            if (x1 < xmin) xmin = x1;
            if (x2 > xmax) xmax = x2;
            if (x2 < xmin) xmin = x2;

            if (y1 > ymax) ymax = y1;
            if (y1 < ymin) ymin = y1;
            if (y2 > ymax) ymax = y2;
            if (y2 < ymin) ymin = y2;

            if (xsize > xsizemax) xsizemax = xsize;
            if (xsize < xsizemin) xsizemin = xsize;

            if (ysize > ysizemax) ysizemax = ysize;
            if (ysize < ysizemin) ysizemin = ysize;
        }
        plef += length;
    }

    // Set global Variables width and height
    width =  xmax - xmin +1;
    height = ymax - ymin +1;

    xoffset = xmin;
    yoffset = ymin;

    bmp =  write_map("extract.bmp", width * height , sizeof(unsigned long));
    get_names(lef);
    draw(lef, bmp.mem);
    modifyBMP(bmp.mem);
    return bmp;
}

void scan_line(Layer layer, unsigned long * pbmp){
    Node current_node = NULL;
    Node *prev_line = NULL;
    Node *current_line = NULL;
    Node *tmp = NULL;
    unsigned long i = layer->mask;
    unsigned long pixel;
    int w;

    int type = 0;
    for(w =0 ; w < num_fets; ++w){
        if (fets[0][w] == layer){
            type = w+1;
            break;
        }
    }

    // swap previous line adn this line
    tmp = layer->prev_line;
    prev_line = layer->prev_line = layer->current_line;
    current_line = layer->current_line = tmp;

    for(w = 0 ; w<width ;++w ){

        pixel = *pbmp++;

        if ( pixel & i){

            if (current_node){              // There is currently a node to our left
                if (prev_line[w]){

                    // MERGE current node with node from previous line
                    if (current_node != prev_line[w]){
                        layer_a = layer;
                        layer_b = NULL;
                        w_g = w;
                        node_merge(current_node, prev_line[w]);
                    }

                }
            }else{                          // there is no node to our left
                if (prev_line[w]){                // there is a node above
                    // EXPAND down the node from the previous line
                    current_node = prev_line[w];
                }else{                            //no node above
                    //CREATE a new node
                    current_node = node_new(type);
                }
            }

        }else{
            current_node = NULL;
        }

        current_line[w] = current_node;
    }
}


void scan( unsigned long * bmp){
    int h;
    int c,a,b,is_gate,w;
    unsigned long * pbmp = bmp;
    unsigned long mask;
    Name name;

    Layer junc_a, junc_b, layer;
    Node node_a, node_b,state_b;

    layers_init(); // allocate memory for the scanlines

    for (h = 0; h<height; ++h){

        // Scanline for each layer
        list_iter(layers, c, layer){
            scan_line(layer, pbmp);
        }

        // Set the names of the Nodes
        list_iter(names, c,name){
            if (name->ytrans == h){
                // TODO: remove name from list to increase performance
                node_a = name->layer->current_line[name->xtrans];
                if (node_a){
                    node_a->name = name;
                }else{
                    printf("Name Error: %s  %i  %i\n", name->name, name->xtrans, name->ytrans);
                }
            }
        }

        pbmp+=width;

        // Surface-Junctions side A
        list_iter(s_junctions , a, junc_a){

            is_gate = 0;
            // side B
            list_iter(junc_a->sjunc, b, junc_b){

                mask = junc_a->mask | junc_b->mask; // mask used to determine if this is a gate junction



                for(w =0 ; w < num_fets; ++w){
                    if ((fets[0][w]->mask | fets[1][w]->mask) == mask){
                        is_gate = 1;
                        //printf("    %s  %s %lu \n",fets[0][w]->name,fets[1][w]->name, mask);
                        break;
                    }
                }

                // step through their width
                for( c=0 ; c < width; ++c){

                    node_a = junc_a->current_line[c];
                    if (! node_a) continue;

                    node_b = junc_b->current_line[c];
                    if (! node_b) continue;

                    // you only get to here if node_a and node_b exist

                    // need to merge nodes
                    if (node_a != node_b){
                        layer_a = junc_a;
                        layer_b = junc_b;
                        h_g = h;
                        if (is_gate){
                            node_addgate(node_a,node_b);
                        }else{
                            node_merge(node_a, node_b);
                        }
                    }
                }
            }
        }

        // Edge-Junctions side A
        // for transistors this will be the diffusion
        list_iter(e_junctions , a, junc_a){

            // side B
            // for transistors this will be the transistor
            list_iter(junc_a->ejunc, b, junc_b){

                // initial state is the value of the first device pixel
                state_b = junc_b->current_line[0];

                // step through their width
                for( c=0 ; c < width; ++c){
                    node_a = junc_a->current_line[c];
                    node_b = junc_b->current_line[c];


                    // diffusion below transistor
                    if(node_a && junc_b->prev_line[c]){
                        node_merge(junc_b->prev_line[c], node_a);
                    }

                    // diffusion above transistor
                    if (node_b && junc_a->prev_line[c] ){
                        node_merge(junc_a->prev_line[c], node_b);
                    }


                    if ((unsigned long)state_b ^ (unsigned long)node_b){
                        if (state_b){
                            // diffusion to the right of transistor
                            if ( junc_a->current_line[c] ){
                                node_merge(junc_a->current_line[c], state_b);
                            }

                        }else{
                            // diffusion to the left of transistor
                            if ( junc_a->current_line[c-1] ){
                                node_merge(junc_a->current_line[c-1], node_b);
                            }
                        }

                        state_b = node_b;
                    }
                }

            }
        }
    }
}

int main(int argc, char **argv){

    scale = 10;

    nodes       = list_new(64);
    unused_nodes= list_new(64);
    layers      = list_new(16);
    s_junctions = list_new(8);
    e_junctions = list_new(8);
    names       = list_new(4);

    rules_get();

    MMap lef = read_char_map(argv[1]);
    MMap bmp = createBMP(lef.mem);

    close_map(&lef);
    scan(bmp.mem);
    close_map(&bmp);

    nodes_print();
    return 0;
}



